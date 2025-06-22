/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_Globals_INCLUDED
#define DEDCON_Globals_INCLUDED

#include "Misc.h"
#include <vector>

// information about this server
struct ServerInfo
{
    std::string publicIP;  //!< our public IP address
    int publicPort;        //!< our public port
    bool dedicated;        //!< dedicated or proxy mode?

    int keyID;             //!< the ID of the server authentication key

    bool started;          //!< has the game started?
    bool ended;            //!< has the game ended?
    bool everyoneReady;    //!< everyone ready to play?
    int  everyoneReadySince; //!< if yes, since when?
    bool unstoppable;        //!< set when the countdown has progressed so far that it cannot be aborted any more
    int  totalPlayers;       //!< total number of players (with AIs)

    ServerInfo():publicPort(-1), dedicated( false ), keyID(-2), started( false ), ended( false ), everyoneReady( false ), everyoneReadySince( -1 ), unstoppable( false ), totalPlayers(0){}
};

extern ServerInfo serverInfo;

// limits for demo key users
struct DemoLimits
{
    int maxGameSize, maxPlayers;
    bool allowServers;
    bool demoMode;       // whether this server needs to stay in demo mode
    bool demoRestricted; // whether this server allows demo players

#ifdef COMPILE_DEDWINIA
    std::vector<int> demoMaps; // which map (hashes) are allowed
    int demoModes;             // bitfield of allowed modes (bit 0: domination,...)
#endif

    bool set, applied;

    DemoLimits()
  : maxGameSize(2)
#ifdef COMPILE_DEDCON
  , maxPlayers(1), allowServers(true)
#else
  , maxPlayers(0), allowServers(false)
#endif
  , demoMode(false), demoRestricted(false)
#ifdef COMPILE_DEDWINIA
  , demoModes(6)
#endif
  , set( false ), applied( false ){}

    void ApplyLimits();
};

extern DemoLimits demoLimits;

extern Socket serverSocket;
extern Socket lanSocket;

#endif // DEDCON_Globals_INCLUDED
