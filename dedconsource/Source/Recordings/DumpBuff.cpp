/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "DumpBuff.h"

#include <iomanip>
#include "Lib/Codes.h"
#include "TagReader.h"
#include "TagParser.h"
#include "GameSettings/Settings.h"
#include "Network/Exception.h"
#include "Main/Log.h"
#include "Lib/Unicode.h"

#include "CompressedTag.h"

static char const * LookupCodeEx( char const * code, char const * hint = 0 )
{
#ifdef XDEBUG
    return LookupCode( code, hint );
#else
    return code;
#endif
}

//! tag parser that prints messages in a readable format
class TagParserPrint: public TagParser
{
    std::string hint;
public:
    explicit TagParserPrint( std::ostream & s )
    :stream(s)
    {}

    explicit TagParserPrint( OStreamWrapper & s )
    :stream( s.GetStream() ? *s.GetStream() : std::cout )
    {}

protected:
    virtual void OnType( std::string const & type )
    {
        hint = LookupCodeEx(type.c_str());
        stream << hint << ' ';
    }

    // called when value-key pairs of the various types are encountered.
    // default action: ignore.
    virtual void OnChar( std::string const & key, int value )
    {
        stream << LookupCodeEx(key.c_str(), hint.c_str()) << "=0x" << std::hex << value << std::dec << ' ';
    }

    virtual void OnInt( std::string const & key, int value )
    {
        stream << LookupCodeEx(key.c_str(), hint.c_str()) << '=' << value << ' ';
    }

    virtual void OnLong( std::string const & key, LongLong const & value )
    {
        stream << LookupCodeEx(key.c_str(), hint.c_str()) 
               << "=L" << std::fixed << std::setprecision(0) << value.lower + double(value.higher)*double(0x10000)*double(0x10000) << ' ';
    }

    virtual void OnIntArray( std::string const & key, std::vector<int> const & value )
    {
        stream << LookupCodeEx(key.c_str(), hint.c_str()) << " = ( ";
        std::vector<int>::const_iterator i = value.begin();
        if ( i != value.end() )
        {
            stream << *i;
            ++i;
        }
        while( i != value.end() )
        {
            stream << ", " << *i;
            ++i;
        }
        stream << " ) ";
    }

    virtual void OnFloat( std::string const & key, float value )
    {
        stream << LookupCodeEx(key.c_str(), hint.c_str()) << '=' << value << ' ';
    }

    virtual void OnString( std::string const & key, std::string const & value )
    {
        if ( key == C_TAG_TYPE )
        {
            hint = LookupCodeEx(value.c_str(), hint.c_str());
            stream << "TAG_TYPE=" << hint << " ";
        }
        else if ( key == C_MW_TAG_TYPE )
        {
            hint = LookupCodeEx(value.c_str(), hint.c_str());
            stream << "MW_TAG_TYPE=" << hint << " ";
        }
        else if ( key == C_DC_TAG_TYPE )
        {
            hint = LookupCodeEx(value.c_str(), hint.c_str());
            stream << "DC_TAG_TYPE=" << hint << " ";
        }
        else
        {
            stream << LookupCodeEx(key.c_str(), hint.c_str()) << "=\"" << value << "\" ";
        }
    }

    virtual void OnUnicodeString( std::string const & key, std::wstring const & value )
    {
        stream << LookupCodeEx(key.c_str(), hint.c_str()) << "=L\"" << value << "\" ";
    }

    virtual void OnBool( std::string const & key, bool value )
    {
        stream << LookupCodeEx(key.c_str(), hint.c_str()) << "=b" << value << ' ';
    }

    virtual void OnDouble( std::string const & key, double const & value )
    {
        stream << LookupCodeEx(key.c_str(), hint.c_str()) << "=d" << value << ' ';
    }

    virtual void OnRaw( std::string const & key, unsigned char const * data, int len )
    {
        stream << LookupCodeEx(key.c_str(), hint.c_str()) << "=(";
        while ( len-- > 0 )
        {
            stream << std::hex << std::setw(3) << static_cast< unsigned int>( *data++ );
        }
        stream << std::dec << ") ";
    }

    // checkpoints
    virtual void OnStart( TagReader & reader )
    {
        stream << '<';
    }

    virtual void AfterStart( TagReader & reader ){}
    virtual void BeforeNest( TagReader & reader ){}

    virtual void OnFinish( TagReader & reader )
    {
        stream << '>';
    }

    // called for nested tags
    virtual void OnNested( TagReader & reader )
    {
        // print nested tags, too
        Parse( reader );
        stream << ' ';
    }
private:
    std::ostream & stream;
};

void dumpbufinternal( TagReader & reader, std::ostream & stream )
{
    try
    {
        TagParserPrint parser( stream );
        parser.Parse( reader );
        reader.Finish();
        stream << '\n';
        assert( reader.BytesLeft() <= 1 );
    }
    catch( ReadException const & e )
    {
        stream << " | error while parsing.\n";
    }
}

void dumpbuf( TagReader & reader )
{
     if ( Log::Out().GetWrapper().GetStream() )
     {
         TagReaderCopy copy(reader);
         dumpbufinternal(copy, *Log::Out().GetWrapper().GetStream() );
     }
     if ( Log::Out().GetWrapper().GetSecondaryStream() )
     {
         TagReaderCopy copy(reader);
         dumpbufinternal(copy, *Log::Out().GetWrapper().GetSecondaryStream() );
     }
}

void dumpbuf( unsigned char const * recbuf, int recbuflen )
{
    TagReader reader( (unsigned char const *) recbuf, recbuflen );
    dumpbuf(reader);
    reader.Obsolete();

    /*
    std::ostream & stream = std::cout;
    stream << "(";
    while ( recbuflen-- > 0 )
    {
        stream << std::hex << std::setw(3) << static_cast< unsigned int>( *recbuf++ );
    }
    stream << std::dec << ")\n";
    */
}

void dumpbuf( char const * recbuf, int recbuflen )
{
    dumpbuf( (unsigned char const *)recbuf, recbuflen );
}

