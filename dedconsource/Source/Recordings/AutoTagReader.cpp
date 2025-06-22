/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "AutoTagReader.h"

#include <stdlib.h>

#include "Lib/Misc.h"
#include "GameSettings/Settings.h"
#include "DumpBuff.h"
#include "Lib/Globals.h"
#include "Main/Log.h"

#ifdef DEBUG
extern IntegerSetting packetLoss;
#endif

AutoTagReaderBuffer::AutoTagReaderBuffer( Socket & socket_ )
: socket( socket_ )
{
    len = 0;

    // read
    len  = socket.RecvFrom( sender, buffer, BUFSIZE );

    bool lost = false;
#ifdef DEBUG
    lost = rand() < ( RAND_MAX/100 ) * packetLoss;
#endif
    if ( lost )
    {
        len = 0;
    }
}

sockaddr_in const & AutoTagReaderBuffer::Sender() const
{
    return sender;
}
 
void AutoTagReaderBuffer::SendTo( Socket & socket, sockaddr_in const & target, ENCRYPTION_VERSION encryption ) const
{
    // and send
    socket.SendTo(target, buffer, len, encryption );

#ifdef DEBUG
    if ( Log::GetLevel() >= 4 )
    {
        Log::Out() << "-> " << inet_ntoa(target.sin_addr) << ":" << ntohs(target.sin_port) << "\t";
        dumpbuf( buffer, len );
    }
#endif
}

bool AutoTagReaderBuffer::Received() const { return len > 0; }

AutoTagReader::AutoTagReader( Socket & socket )
        : AutoTagReaderBuffer( socket )
        , TagReader( buffer, len )
{
    // nobody will read an empty buffer
    if ( !Received() )
        Obsolete();

#ifdef DEBUG
    else
    {
        if ( Log::GetLevel() >= 4 && serverInfo.dedicated )
        {
            Log::Out() << ntohs(socket.LocalAddr().sin_port) << " ";
            Dump();
        }

        if ( Log::NetLog().GetWrapper().IsSet() )
        {
            // bend output to network log
            OStreamSetter setter( Log::Out().GetWrapper(), Log::NetLog().GetWrapper().GetStream() );
            setter.SetSecondary( 0 );
            setter.SetTimestampOut( true );
            Dump();
        }
    }
#endif
}

AutoTagReader::~AutoTagReader()
{
}

//! dump the contents
void AutoTagReader::Dump()
{
#ifdef DEBUG
    Log::Out() << "<- " << inet_ntoa(sender.sin_addr) << ":" << ntohs(sender.sin_port) << "\t";
    dumpbuf( buffer, len );
#endif
}
