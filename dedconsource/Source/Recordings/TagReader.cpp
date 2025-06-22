/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "TagReader.h"

#include "TagParser.h"
#include "Main/Log.h"
#include <iomanip>
#include "CompressedTag.h"
#include "Network/Exception.h"
#include <sstream>

// procedure: CheckLeft( size ) followed by exactly size calls to ReadRawCharUnsafe.
inline void TagReader::CheckLeft( int size )
{
    if( left < size )
    {
        throw ReadException( "Read over end of buffer" );
    }

    left -= size;
}

inline unsigned char TagReader::ReadRawCharUnsafe()
{
    return *(read++);
}

inline unsigned short TagReader::ReadRawWordUnsafe()
{
    unsigned int little = ReadRawCharUnsafe();
    unsigned int big    = ReadRawCharUnsafe();

    return little + (big << 8);
}

//! prepares to read from the compressed buffer
TagReader::TagReader( CompressedTag const & tag )
        : read( tag.Buffer() ), left ( tag.Size() ), atomsLeft(-1), nestedLeft(-1), obsolete( false ), parent(0) {}

//! start reading
std::string TagReader::Start()
{
    assert( atomsLeft < 0 );
    char leadIn = ReadRawChar();
    if ( leadIn != '<' )
        throw ReadException( "'<' expected at start of tag." );

    std::string type = ReadRawString();
    atomsLeft = ReadRawChar();

    return type;
}

//! started already?
bool TagReader::Started() const
{
    return ( atomsLeft >= 0 );
}

//! start to read nested tags
void TagReader::Nest()
{
    assert( atomsLeft == 0 );
    nestedLeft = ReadRawChar();
}

//! mark this reader as obsolete; no reads can be done from it in the future, and the destructor won't complain about not having read everything.
void TagReader::Obsolete()
{
    obsolete = true;
}

//! finish reading
void TagReader::Finish()
{
    if ( !obsolete )
    {
        assert( nestedLeft == 0 );
        assert( atomsLeft == 0 );

        if ( ReadRawChar() != '>' )
            throw ReadException( "'>' expected at end of tag." );

        // retransfer read position
        if ( parent )
        {
            parent->read = read;
            parent->left = left;
            parent->obsolete = false;
        }

        obsolete = true;
    }
}

//! prepares to take over reading from another reader
TagReader::TagReader( TagReader & other, bool nested )
        : read( other.read ), left ( other.left ), atomsLeft(-1), nestedLeft(-1), obsolete(false), parent(0)
{
    assert( !other.obsolete );

    if ( nested )
    {
        other.obsolete = true;

        assert( other.atomsLeft == 0 );

        // read number of nested tags
        if ( other.nestedLeft < 0 )
            other.nestedLeft = ReadRawChar();

        // we'll be reading one
        assert( other.nestedLeft > 0 );
        other.nestedLeft --;
        parent = &other;
    }
    else
    {
        // transfer rest of data
        atomsLeft = other.atomsLeft;
        nestedLeft = other.nestedLeft;
        parent = other.parent;
    }
}

//! read a byte
unsigned char TagReader::ReadChar()
{
    if ( ReadRawChar() != 3 )
        throw ReadException( "not a char at this point" );

    return ReadRawChar();
}

//! read a bool
bool TagReader::ReadBool()
{
    if ( ReadRawChar() != 5 )
        throw ReadException( "not a boolean at this point" );

    return ReadRawChar();
}

//! read a word
unsigned short int TagReader::ReadWord()
{
    // guessing the magic value
    if ( ReadRawChar() != 2 )
        throw ReadException( "not a word at this point" );

    return ReadRawWord();
}

//! read an integer value
unsigned int TagReader::ReadInt()
{
    if ( ReadRawChar() != 1 )
        throw ReadException( "not an integer at this point" );

    return ReadRawInt();
}

//! read time?
LongLong TagReader::ReadLong()
{
    unsigned char id = ReadRawChar();
    if ( id != 9 )
        throw ReadException( "not a long at this point" );

    return ReadRawLong();
}

//! read an integer value
void TagReader::ReadIntArray( std::vector< int > & s )
{
    if ( ReadRawChar() != 8 )
        throw ReadException( "not a integer array at this point" );

    return ReadRawIntArray( s );
}

//! read a float value
float TagReader::ReadFloat()
{
    if ( ReadRawChar() != 2 )
        throw ReadException( "not a float at this point" );

    return ReadRawFloat();
}

//! read a string
std::string TagReader::ReadString()
{
    if ( ReadRawChar() != 4 )
        throw ReadException( "not a string at this point" );

    return ReadRawString();
}

//! read a string
std::wstring TagReader::ReadUnicodeString()
{
    if ( ReadRawChar() != 10 )
        throw ReadException( "not a unicode string at this point" );

    return ReadRawUnicodeString();
}

//! read coordinates?
double TagReader::ReadDouble()
{
    unsigned char id = ReadRawChar();
    if ( id != 7 )
        throw ReadException( "not a double at this point" );

    return ReadRawDouble();
}

//! read the key part of an atom
std::string TagReader::ReadKey()
{
    assert( atomsLeft > 0 );
    --atomsLeft;

    return ReadRawString();
}

//! read raw data
void TagReader::ReadRaw( int & len, unsigned char const * & data )
{
    if ( ReadRawChar() != 6 )
        throw ReadException( "not a raw data chunk at this point" );

    ReadRawRaw( len, data );
}

unsigned char TagReader::ReadRawChar()
{
    CheckLeft( 1 );

    return ReadRawCharUnsafe();
}

unsigned short int TagReader::ReadRawWord()
{
    CheckLeft( 2 );

    return ReadRawWordUnsafe();
}

unsigned int TagReader::ReadRawInt()
{
    CheckLeft( 4 );

    unsigned int little = ReadRawWordUnsafe();
    unsigned int big    = ReadRawWordUnsafe();

    return little + (big << 16);
}

void TagReader::ReadRawIntArray( std::vector<int> & s )
{
    int size = ReadRawChar();
    s.clear();
    s.reserve(size);
    for( int i = 0; i < size; ++i )
    {
        s.push_back( ReadRawInt() );
    }
}

float TagReader::ReadRawFloat()
{
    // hope this is it
    assert( sizeof( float ) == sizeof( int ) );
    union { float f; int i; } u;

    u.i = ReadRawInt();
    return u.f;
}

double TagReader::ReadRawDouble()
{
    DoubleUnion u;

    assert( sizeof(u.l) == sizeof(double) );

    u.l.lower  = ReadRawInt();
    u.l.higher = ReadRawInt();
 
    return u.d;
}

LongLong TagReader::ReadRawLong()
{
    LongLong l;

    l.lower  = ReadRawInt();
    l.higher = ReadRawInt();

    return l;
}

char const * TagReader::ReadRawString()
{
#define MAXSTATICLEN 256
    static char buffer[ MAXSTATICLEN+1 ];
    static char * pBuffer = 0;
    if ( buffer != pBuffer )
    {
        delete[] pBuffer;
        pBuffer = buffer;
    }

    unsigned int len = ReadRawChar();
    //extra long strings
    if ( len == 255 )
    {
        len = ReadRawInt();
    }

    CheckLeft( len );

    if ( len > MAXSTATICLEN )
    {
        pBuffer = new char[len+1];
    }

    // std::stringstream s;
    for( unsigned int i = 0; i < len; ++i )
    {
        char c = ReadRawCharUnsafe();
        pBuffer[i] = ( c ? c : '_' );
    }
    pBuffer[len] = 0;

    return pBuffer;
}

std::wstring TagReader::ReadRawUnicodeString()
{
    unsigned int len = ReadRawChar();
    //extra long strings
    if ( len == 255 )
    {
        len = ReadRawInt();
    }

    std::wostringstream s;
    for( int i = len-1; i>=0; --i )
    {
        wchar_t c = ReadRawWord();
        s.put( c ? c : '_' );
    }

    return s.str();
}

//! read raw data
void TagReader::ReadRawRaw( int & len, unsigned char const * & data )
{
    len = ReadRawInt();

    data = read;
    read += len;
    left -= len;
}

//! help the parser parse anything
void TagReader::ParseAny( TagParser & parser, std::string const & key )
{
    unsigned char t = ReadRawChar();
    std::vector< int > s;
    switch (t)
    {
    case 1: parser.OnInt(key,ReadRawInt()); break;
    case 2: parser.OnFloat(key,ReadRawFloat()); break;
    case 3: parser.OnChar(key,ReadRawChar()); break;
    case 4: parser.OnString(key,ReadRawString()); break;
    case 8: ReadRawIntArray(s); parser.OnIntArray(key,s); break;
    case 10: parser.OnUnicodeString(key,ReadRawUnicodeString()); break;
    case 5: parser.OnBool(key,ReadRawChar()); break; // bools?
    case 7: parser.OnDouble(key,ReadRawDouble()); break; // coordinates?
    case 9: parser.OnLong(key,ReadRawLong()); break; // time?

    case 6:
        {
            // crap, this is another way to put a tag inside another tag, as an atom!
            int len = ReadRawInt();
#ifdef COMPILE_DEDCON
            if( *read != '<' )
#endif
            {
                parser.OnRaw( key, read, len );
                read += len;
                left -= len;
                break;
            }
            TagReader subReader( read, len );
            read += len;
            left -= len;
            parser.Parse( subReader );
        }
        break;
    default:
        {
            std::cout << "\n";
            while ( left > 0 )
            {
                std::cout << std::hex << std::setw(3) << (unsigned int)ReadRawChar();
            }
            std::cout << "\n";
            std::cout.flush();

            throw ReadException( "unknown data type" );
        }
    }
}

//! read anything and ignore it
void TagReader::ReadAny()
{
    unsigned char t = ReadRawChar();
    std::vector<int> s;
    switch (t)
    {
    case 1: ReadRawInt(); break;
    case 2: ReadRawFloat(); break;
    case 3: ReadRawChar(); break;
    case 4: ReadRawString(); break;
    case 8: ReadRawIntArray(s); break;
    case 10: ReadRawUnicodeString(); break;
    case 5: ReadRawChar(); break; // bools?
    case 7: ReadRawDouble(); break; // coordinates?
    case 9: ReadRawLong(); break; // time?

    case 6:
        {
            // crap, this is another way to put a tag inside another tag, as an atom!
            int len = ReadRawInt();
            read += len;
            left -= len;
        }
        break;
    default:
        {
            std::cout << "\n";
            while ( left > 0 )
                std::cout << std::hex << std::setw(3) << (unsigned int)ReadRawChar();
            std::cout << "\n";
            std::cout << "\n";
            std::cout << "\n";
            std::cout.flush();

            throw ReadException( "unknown data type" );
        }
    }
}

//! dump the contents
void TagReader::Dump(){}
