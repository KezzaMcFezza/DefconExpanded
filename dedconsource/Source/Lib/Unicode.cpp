/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "Unicode.h"
#include <string>
#include <iostream>
#include <sstream>

//! write a wide string to a regular stream
void Unicode::Write( std::ostream & stream, std::wstring const & string )
{
    for ( std::wstring::const_iterator i = string.begin(); i != string.end(); ++i )
    {
        wchar_t w = *i;
        if ( w < 0x80 )
        {
            // easy
            stream.put( w );
        }
        else if ( w < 0x800 )
        {
            // bit wranging
            stream.put( 0xC0 | ( w >> 6   ) );
            stream.put( 0x80 | ( w & 0x3F ) );
        }
        else
        {
            // last case, the bigger values don't even wit a wchar_t
            stream.put( 0xE0 | ( w >> 12 ) );
            stream.put( 0x80 | ( ( w >> 6 ) & 0x3F ) );
            stream.put( 0x80 | ( w & 0x3F ) );
        }
    }
}


//! write a regular string to a wide stream
void Unicode::Write( std::wostream & stream, std::string const & string )
{
    // the next wchar to write
    wchar_t next = 0;
    int overhead = 0;
    for ( std::string::const_iterator i = string.begin(); i != string.end(); ++i )
    {
        unsigned char c = *i;
        if ( ( c & 0xC0 ) == 0x80 )
        {
            // continuation
            assert( overhead > 0 );
            next = ( next << 6 ) | ( c & 0x3F );

            // all components read?
            if ( --overhead == 0 )
            {
                stream.put(next);
            }
        }
        else
        {
            assert( overhead == 0 );
            if ( ( c & 0x80 ) == 0 )
            {
                // easy, single char
                stream.put(c);
            }
            else if ( ( c & 0xE0 ) == 0xC0 )
            {
                // double-byte sequence
                next = c & 0x1F;
                overhead = 1;
            }
            else if ( ( c & 0xF0 ) == 0xE0 )
            {
                // tripple-byte sequence
                next = c & 0x0F;
                overhead = 2;
            }
            else if ( ( c & 0xF8 ) == 0xF0 )
            {
                // quadruple-byte sequence (bad, but oh well, what can we do?)
                next = c & 0x07;
                overhead = 3;
            }
        }
    }

    assert( overhead == 0 );
}

//! convert wide to narrow
std::string Unicode::Convert( std::wstring const & string )
{
    std::ostringstream s;
    Write( s, string );
    return s.str();
}

//! convert narrow to wide
std::wstring Unicode::Convert( std::string const & string )
{
    std::wostringstream s;
    Write( s, string );
    return s.str();
}
