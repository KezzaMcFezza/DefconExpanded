/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_Unicode_INCLUDED
#define DEDCON_Unicode_INCLUDED

#include "Misc.h"
#include "Forward.h"

// unicode helper class. All conversions assume UFT8 encoding for basic strings
class Unicode
{
public:
    //! write a wide string to a regular stream
    static void Write( std::ostream & stream, std::wstring const & string );

    //! write a regular string to a wide stream
    static void Write( std::wostream & stream, std::string const & string );

    //! convert wide to narrow
    static std::string Convert( std::wstring const & string );

    //! convert narrow to wide
    static std::wstring Convert( std::string const & string );
};

// convenience: don't bother what kind of stream you write to
namespace std
{
    inline ostream & operator << ( ostream & s, wstring const & w )
    {
        Unicode::Write( s, w );
        return s;
    }

    inline wostream & operator << ( wostream & s, string const & w )
    {
        Unicode::Write( s, w );
        return s;
    }
}

#endif // DEDCON_Unicode_INCLUDED
