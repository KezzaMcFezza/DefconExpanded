/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_Map_INCLUDED
#define DEDCON_Map_INCLUDED

#ifdef COMPILE_DEDWINIA

#include "Forward.h"

class Map
{
public:
    static int GetHash();
    static void SetHash( int hash );
    static bool DemoRestricted(); //!< checks demo restrictions. Returns true if restrictions are met.
    static bool CheckDemo(); //!< checks demo restrictions; reverts if they're not met.
    static void UpdateHash(); //!< update map hash on map change
    static void Lock(); //!< locks the map from now on
    static void Players( int numPlayers, bool final ); //!< call when the number of players changed

    //! sends the map to a client
    static void SendMap( Client & client );
};

#endif

#endif // DEDCON_Map_INCLUDED
