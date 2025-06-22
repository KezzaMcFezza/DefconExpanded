/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "Main/config.h"

#include "CompressedTag.h"
#include "DumpBuff.h"
#include "TagWriter.h"
#include "TagReader.h"
#include "Main/Log.h"
#include "Network/Exception.h"
#include <iostream>
#include <cstring>

#ifdef XDEBUG
// simple compressor
class Packer
{
public:
    typedef unsigned char Token;
    typedef unsigned short Code;

    enum
    {
        CodeBits = 15,
        NumCodes = 1 << CodeBits,
        NumTokens = 1 << 8
    };

    // string lookup tree
    struct StringTree
    {
        // the tree branches
        StringTree * branches[ NumTokens ];

        // code up to now
        Code code;

        // token added here
        Token token;

        // the parent
        StringTree * parent;

        // add a branch
        StringTree * AddBranch( Token token, Code code )
        {
            StringTree * add = new StringTree( code ); 
            add->token = token;
            add->parent = this;
            return branches[token] = add;
        }

        explicit StringTree( int code_ )
        : code( code_ ), token(0), parent( 0 )
        {
            for( int i = NumTokens-1; i >= 0; --i )
            {
                branches[i] = 0;
            }
        }

        ~StringTree()
        {
            for( int i = NumTokens-1; i >= 0; --i )
            {
                delete branches[i];
            }
        }
    };

    typedef std::vector< StringTree * > Dictionary;

    Packer()
    : previous( 0 ), root( 0 )
    {
        for( int i = NumTokens-1; i >= 0; --i )
        {
            root.AddBranch( i, i );
        }
    }

    // generate one code from a token stream
    Code CompressOne( Token const * & tokens, size_t & len )
    {
        // find the biggest string
        StringTree * string = FindString( tokens, len );

        // if the stream is not at its end, add the next token to a new string
        if( len > 0 )
        {
            previous = AddString( string, *tokens );
        }

        // and return the code
        return string->code;
    }

    // uncompress one code
    void UncompressOne( Code code, Token * & tokens, size_t & len )
    {
        StringTree * string = NULL;
        if ( code < NumTokens )
        {
            // verbatim token. Write directly.
            *(tokens++) = code;
            len--;
            string = root.branches[code];
        }
        else
        {
            assert ( (int)dictionary.size() > code - NumTokens );

            // known string. Fetch it.
            string = dictionary[ code - NumTokens ];

            // and write it.
            WriteStringTree( string, tokens, len );
        }

        if ( previous )
        {
            // find the first character in the current string
            StringTree * run = string;
            while( run->parent && run->parent != &root )
            {
                run = run->parent;
            }
            
            // add a new string
            AddString( previous, run->token );
        }

        // remember previous string
        previous = string;
    }

    // reading bitwise from an input buffer
    class BitIStream
    {
    public:
        BitIStream( unsigned char const * buffer, size_t size )
        : current( buffer ), currentBit( 0 ), bytesLeft( size )
        {}

        bool Eof()
        {
            return bytesLeft * 8 - currentBit < 8;
        }

        Code Get()
        {
            // first, get 8 bits
            Code get = Get( 8 );

            // extended bit?
            if ( get >= 0x80 )
            {
                // get extra bits
                get &= 0x7F;
                get |= Get( CodeBits-7 ) << 7;
            }
            return get;
        }
    private:
        unsigned int Get( unsigned char bits )
        {
            unsigned int ret = 0;
            unsigned char fullbits = bits;
            unsigned char shift = 0;
            while ( bits > 0 )
            {
                ret |= ( *current >> currentBit ) << shift;
                unsigned int read = 8 - currentBit;
                shift += read;
                if ( read > bits )
                {
                    read = bits;
                }
                bits -= read;
                currentBit += read;
                
                if ( currentBit >= 8 )
                {
                    ++current;
                    --bytesLeft;
                    currentBit -= 8;
                }
            }

            ret &= ( ( 1 << fullbits ) - 1 );
            return ret;
        }

        // the current write position
        unsigned char const * current;
        unsigned char currentBit;

        // the number of bytes left to read
        size_t bytesLeft;
    };

    void Uncompress( BitIStream & s, Token * & tokens, size_t & len )
    {
        while( !s.Eof() && len > 0 )
        {
            UncompressOne( s.Get(), tokens, len );
        }
    }

    // reading bitwise from an input buffer
    class BitOStream
    {
    public:
        BitOStream( std::vector< unsigned char > & receiver_ )
        : receiver( receiver_ ), currentBit( 0 ), current( 0 )
        {}

        ~BitOStream()
        {
            Finish();
        }

        void Put( Code code )
        {
            if ( code < 0x80 )
            {
                // simple ASCII char
                Put( code, 8 );
            }
            else
            {
                // extended char, put extra
                Put( ( code & 0x7F ) | 0x80, 8 );
                Put( ( code >> 7 ), CodeBits - 7 );
            }
        }

        void Finish()
        {
            if ( currentBit > 0 )
            {
                receiver.push_back( current );
            }
            currentBit = 0;
        }
    private:
        void Put( Code code, unsigned char bits )
        {
            while ( bits > 0 )
            {
                current |= code << currentBit;
                unsigned int written = 8 - currentBit;
                code >>= written;
                if ( written > bits )
                {
                    written = bits;
                }
                bits -= written;
                currentBit += written;
                
                if ( currentBit >= 8 )
                {
                    receiver.push_back( current );
                    currentBit -= 8;
                    current = 0;
                }
            }
        }

        // the current write position
        std::vector< unsigned char > & receiver;
        unsigned char currentBit;
        char current;
    };

    void Compress( BitOStream & s, Token const * & tokens, size_t & len )
    {
        while( len > 0 )
        {
            s.Put( CompressOne( tokens, len ) );
        }
        s.Finish();
    }
private:
    // finds the longest string at the given position of given maximal token length
    StringTree * FindString( Token const * & tokens, size_t & len )
    {
        StringTree * current = &root;
        StringTree * next;
        while( len > 0 && ( next = current->branches[ *tokens ] ) && next != previous )
        {
            current = next;
            ++tokens;
            --len;
        }
            
        return current;
    }

    // adds a new string
    StringTree * AddString( StringTree * parent, Token next )
    {
        Code code = dictionary.size() + NumTokens;
        if ( code < NumCodes )
        {
            // std::cout << code << " = " << parent->code << " + " << next << '\n';

            StringTree * ret = parent->AddBranch( next, code );
            dictionary.push_back( ret );
            return ret;
        }
        else
        {
            return 0;
        }
    }

    void WriteStringTree( StringTree const * string, Token * & tokens, size_t & len )
    {
        // simple recursion to reverse the order
        if ( string->parent )
        {
            WriteStringTree( string->parent, tokens, len );
            *(tokens++) = string->token;
            --len;
        }
    }

    // the last decoded string
    StringTree * previous;

    StringTree root;
    Dictionary dictionary;
};

class TestPack
{
public:
    static void Test()
    {
        char const * testSequenceIn = "ABCABCABCABC";

        char testSequenceOut[ 1000 ];

        {
            size_t lenIn = strlen( testSequenceIn )+1;
            size_t lenOut = 100;

            Packer packer, unpacker;

            Packer::Token const * in = (Packer::Token const *)( testSequenceIn );
            Packer::Token * out = (Packer::Token *)( testSequenceOut );

            int codes = 0;
            while( lenIn > 0 )
            {
                unpacker.UncompressOne( packer.CompressOne( in, lenIn ), out, lenOut );
                ++codes;
            }

            assert( 0 == strcmp( testSequenceIn, testSequenceOut ) );
        }

        // std::cout << "\n";

        {
            size_t lenIn = strlen( testSequenceIn )+1;
            size_t lenOut = 100;

            Packer packer, unpacker;

            Packer::Token const * in = (Packer::Token const *)( testSequenceIn );
            Packer::Token * out = (Packer::Token *)( testSequenceOut );

            std::vector< unsigned char > packed;
            Packer::BitOStream o( packed );
            packer.Compress( o, in, lenIn );
            Packer::BitIStream i( &packed[0], packed.size() );
            unpacker.Uncompress( i, out, lenOut );

            assert( 0 == strcmp( testSequenceIn, testSequenceOut ) );
        }
    }

    TestPack()
    {
        Test();
    }
};
    
static TestPack testPack;
#endif

typedef std::vector< unsigned char > DataVector;

// saves a data block
void CompressedTag::Save( std::ostream & s, unsigned char const * buffer, size_t size )
{
    // write tag length (take care of endianness)
    unsigned char len[4];
    TagWriter writer( len, sizeof(len) );
    writer.WriteRawInt( size );
    s.write( (char *)len, sizeof(len) );
    writer.Obsolete();

    s.write( (char *)buffer, size );
}

//! saves tag to stream
void CompressedTag::Save( std::ostream & s ) const
{
    Save(s, &content[0], content.size() );
}

// reads a data block in two steps
size_t CompressedTag::ReadSize( std::istream & s )
{
    // read tag length (take care of endianness)
    unsigned char len[4]={0,0,0,0};
    s.read( (char *)len, sizeof(len) );
    TagReader reader( len, sizeof(len) );
    size_t size = reader.ReadRawInt(); 
    reader.Obsolete();

    return size;
}

void CompressedTag::Load( std::istream & s, unsigned char * buffer, size_t size )
{
    if ( s.eof() )
    {
        Log::Err() << "Unexpected end of file while reading.\n";
        throw QuitException( -1 );
    }

    if ( !s.good() || s.fail() )
    {
        Log::Err() << "Unexpected error while reading.\n";
        throw QuitException( -1 );
    }

    s.read( (char *)buffer, size );
}

//! loads tag from stream
void CompressedTag::Load( std::istream & s, unsigned int maxSize )
{
    size_t size = ReadSize( s );

    if ( !s.eof() )
    {
        if ( maxSize > 0 && size > maxSize )
        {
            Log::Err() << "Too large tag size encountered. This is not a recording.\n";
            throw QuitException( -1 );
        }

        // read content
        content.resize( size );

        Load( s, &content[0], size ); 
    }

    // if ( Log::GetLevel() > 3 )
#if 0
    {
        try
        {
            if ( Size() > 0 )
            {
                dumpbuf( Buffer(), Size() + 1 );
            }
        }
        catch(...)
        {
        }
    }
#endif
}

