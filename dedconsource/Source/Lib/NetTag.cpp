/*
 *  * DedCon: Dedicated Server for Defcon (and Multiwinia)
 *  *
 *  * Copyright (C) 2008 Manuel Moos
 *  *
 *  */

// #include "AutoTagReader.h"
#include "Recordings/AutoTagWriter.h"
#include "Main/Log.h"
#include "Network/Socket.h"
#include "GameSettings/Settings.h"
#include "Recordings/DumpBuff.h"

#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
IntegerSetting packetLoss( Setting::Network, "PacketLoss", 0, 100 );
#endif

void AutoTagWriter::SendTo( Socket & socket, sockaddr_in const & target, ENCRYPTION_VERSION encryption ) const
{
    // make sure everything is wrapped up
    Close();

    // and send
    bool lost = false;
#ifdef DEBUG
    lost = rand() < ( RAND_MAX/100 ) * packetLoss;
#endif
    if ( !lost )
    {
        socket.SendTo( target, buffer, Size(), encryption );
    }
#ifdef DEBUG
    else
    {
        if ( Log::GetLevel() >= 4 )
            Log::Out() << "lost:";
    }

    if ( Log::GetLevel() >= 4 )
    {
        Log::Out() << ntohs(socket.LocalAddr().sin_port) << " -> " << inet_ntoa(target.sin_addr) << ":" << ntohs(target.sin_port) << "\t";
        dumpbuf( buffer, Size() );
    }

    if ( Log::NetLog().GetWrapper().IsSet() )
    {
        // bend output to network log
        OStreamSetter setter ( Log::Out().GetWrapper(), Log::NetLog().GetWrapper().GetStream() );
        setter.SetSecondary( 0 );
        setter.SetTimestampOut( true );

        Log::Out() << "-> " << inet_ntoa(target.sin_addr) << ":" << ntohs(target.sin_port) << "\t";
        dumpbuf( buffer, Size() );
    }
#endif
}


