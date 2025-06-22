/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "Globals.h"

#include <stdlib.h>
#include "Main/Log.h"
// #include "Socket.h"
#include "GameSettings/GameSettings.h"
#include "Recordings/GameHistory.h"
#include "Network/Socket.h"

ServerInfo serverInfo;
DemoLimits demoLimits;

void DemoLimits::ApplyLimits()
{
    // demo settings restrictions.
    if ( set && demoMode && !applied )
    {
        applied = true;
        Log::Err() << "Applying demo limits.\n";
        
        if ( !allowServers && !gameHistory.Playback() )
        {
            Log::Err() << "Hosting DEMO games has been disabled on metaserver order.\n";
            exit(1);
        }

        settings.RestrictDemo();
        
        // this must get into a new sequence
        gameHistory.RequestBump();
        
        settings.SendSettings();
    }
}

Socket lanSocket;
Socket serverSocket;

