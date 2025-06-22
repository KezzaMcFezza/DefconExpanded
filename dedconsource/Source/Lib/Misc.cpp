/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "Misc.h"


//! determines whether a is a bigger version than b. Returns 1 if a is bigger, -1 if b is bigger, 0 if they are equal.
int CompareVersions( char const * a, char const * b )
{
    // versions are equal
    if ( !*a && !*b )
        return 0;

    // whitespace is smaller than anything else ( 1.42 beta1 < 1.42 )
    if ( isspace(*a) && !isspace(*b) )
        return -1;
    if ( !isspace(*a) && isspace(*b) )
        return 1;

    // otherwise, longer versions are bigger ( 1.42 > 1.4 )
    if ( *a && !*b )
        return 1;
    if ( *b && !*a )
        return -1;

    assert( a && b );

    // the rest is handled alphabetically
    if ( *a > *b )
        return 1;
    else if ( *a < *b )
        return -1;
    
    // recurse
    return CompareVersions( a+1, b+1 );
}

int CompareVersions( std::string const & a, std::string const & b )
{
    return CompareVersions( a.c_str(), b.c_str() );
}
