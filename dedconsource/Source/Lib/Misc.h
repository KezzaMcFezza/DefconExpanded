/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_Misc_INCLUDED
#define DEDCON_Misc_INCLUDED

#ifndef DEBUG
#define NDEBUG
#endif

// various stuff used all over the place
#include <assert.h>
#include <string>
#include "Forward.h"

//! determines whether a is a bigger version than b. Returns 1 if a is bigger, -1 if b is bigger, 0 if they are equal.
int CompareVersions( char const * a, char const * b );
int CompareVersions( std::string const & a, std::string const & b );

#endif // DEDCON_Misc_INCLUDED
