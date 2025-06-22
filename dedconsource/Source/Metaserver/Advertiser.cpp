/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "Advertiser.h"

#include "Lib/Codes.h"

#include "Recordings/AutoTagWriter.h"
#include "Recordings/TagReader.h"
#include "Recordings/GameHistory.h"
#include "GameSettings/GameSettings.h"
#include "Server/Client.h"
#include "Lib/Globals.h"
#include "Multiwinia/Map.h"
#include "Server/Host.h"
#include "Main/Log.h"

#include <sstream>

// flag for port forwarding
IntegerSetting portForwarding( Setting::Network, "PortForwarding", 0, 1 );

Advertiser::Advertiser()
{
    bumpIDThis = 0;
    next.GetTime();
    next.AddSeconds(2);
}

bool Advertiser::TimeToSend( int delay )
{
    Time current;
    current.GetTime();

    if ( ( current - next ).Milliseconds() < 0 && bumpID == bumpIDThis )
    {
        return false;
    }
    if ( bumpIDThis != bumpID )
        next = current;

    next.AddSeconds(delay);

    bumpIDThis = bumpID;

    return true;
}

#ifdef DEBUG
static StringSetting advertiseKey( Setting::Internal, "AdvertiseKey" );
#endif

static IntegerSetting advertisePlayers( Setting::ExtendedGameOption, "AdvertisePlayers", 0 );

static IntegerSetting ratingFluctuation( Setting::Player, "RatingFluctuation", 0 );

static IntegerSetting allStarMinRating( Setting::Player, "AllStarMinRating", 0 );

#ifdef COMPILE_DEDCON
static float const * GetScoreFluctuations2()
{
#if VANILLA
    static float fluctuations[6];
    for ( int i = 5; i >= 0; --i )
#elif EIGHTPLAYER
    static float fluctuations[8];
    for (int i = 7; i >= 0; --i)
#elif TENPLAYER
    static float fluctuations[10];
    for (int i = 9; i >= 0; --i)
#endif
    {
        fluctuations[i] = (float(rand())/float(RAND_MAX)) * 2 - 1;
    }

    return fluctuations;
}

static float const * GetScoreFluctuations()
{
    static float const * ret = GetScoreFluctuations2();
    return ret;
}

static float GetScoreFluctuation( int team )
{
    assert ( 0 <= team && team < 6 );
    return GetScoreFluctuations()[ team ];
}
#endif

void InternetAdvertiser::WriteRecording( TagWriter & writer )
{
#ifdef COMPILE_DEDWINIA
    Host::GetHost().Say( "End of recording." );
    writer.WriteInt( "randomseed", RandomSeed() );
    writer.WriteInt( "gamemode", settings.gameMode );
    writer.WriteInt( "maphash", Map::GetHash() );
    writer.WriteInt( "hostid", Host::GetHost().GetID() );
    writer.WriteString( "servername", settings.serverName );
#endif
}

bool InternetAdvertiser::ParsePlayback( std::string const & key, TagReader & reader )
{
#ifdef COMPILE_DEDWINIA
    if ( key == "randomseed" )
    {
        SetRandomSeed( reader.ReadInt() );
        return true;
    }
    if ( key == "gamemode" )
    {
        settings.gameMode.Set( reader.ReadInt() );
        return true;
    }
    if ( key == "maphash" )
    {
        Map::SetHash( reader.ReadInt() );
        return true;
    }
    if ( key == "hostid" )
    {
        Host::GetHost().SetID( reader.ReadInt() );
        return true;
    }
    if ( key == "servername" )
    {
        settings.serverName.Set( reader.ReadString() );
        return true;
    }
#endif
    return false;
}

void Advertiser::WriteRandom( TagWriter & writer )
{
#ifdef COMPILE_DEDWINIA
    Time const & start = Time::GetStartTime();
    LongLong time;
    time.higher = 0;
    time.lower = start.Seconds();
    writer.WriteLong( C_METASERVER_START_TIME, time );
    writer.WriteInt( C_METASERVER_RANDOM_ID, RandomSeed() );
#endif
}

void Advertiser::WritePacket( AutoTagWriter & writer, bool publishKey )
{
    // count
    int numPlayers, numSpectators, numClients;
    Client::Count( numClients, numPlayers, numSpectators );

    // get superpowers
    std::vector< SuperPower * > powers;
    SuperPower::GetSortedByScore( powers );

    // count them again
    int totalPlayers = numPlayers = 0;
    bool scoreSet = false;
    for ( std::vector< SuperPower * >::const_iterator i = powers.begin(); i != powers.end(); ++i )
    {
        // don't count players with low score in all star mode
        if ( serverInfo.started || settings.allStarTime <= 0 || (*i)->GetRating() >= allStarMinRating )
        {
            totalPlayers++;
            if ( (*i)->GetHuman() )
            {
                numPlayers++;
            }
        }

        scoreSet |= (*i)->ScoreSet();
    }

    scoreSet &= serverInfo.started;

    int maxSpectators = settings.maxSpectators;
    // the metaserver does not like empty servers
    if (!(numSpectators+numPlayers) )
    {
        ++numSpectators;
        ++maxSpectators;
    }

    if ( numSpectators > maxSpectators )
    {
        maxSpectators = numSpectators;
    }
  
    // during all star time, pretend the server is never full
    if ( settings.allStarTime > 0 && numSpectators >= maxSpectators && !serverInfo.started )
    {
        maxSpectators = numSpectators + 1;
    }

    // do the real work
    writer.Open( C_METASERVER );

    // write the server name
    {
        std::ostringstream name;
        name << settings.serverName.Get();

        // append player names
        bool colon = true;
        int count = advertisePlayers;
        if ( count > 0 )
        {
            for ( std::vector< SuperPower * >::const_iterator i = powers.begin(); i != powers.end(); ++i )
            {
                if ((*i)->GetName().size() == 0)
                {
                    continue;
                }

                if ( colon )
                {
                    name << " : ";
                    colon = false;
                }
                else
                {
                    name << ", ";
                }

                if ( count-- > 0 )
                {
                    name << (*i)->GetName();
                }
                else
                {
                    name << "...";
                    break;
                }
            }
        }

        writer.WriteString( C_METASERVER_SERVER_NAME, name.str() );
    }

    settings.WriteServerInfo(writer);
    WriteRandom(writer);

    // server IP. if set incorrectly, connecting with a doubleclick in the metaserver list will not work from your LAN.
    writer.WriteString( C_METASERVER_LAN_IP, settings.serverLANIP );

    int LANPort = settings.serverPort;
    if( !publishKey && lanSocket.Bound() )
    {
        LANPort = lanSocket.GetPort();
    }
    writer.WriteInt( C_METASERVER_PORT, LANPort );

    // the number of total players goes here, including AIs
    writer.WriteChar( C_METASERVER_TOTAL_PLAYERS, totalPlayers );

#ifdef COMPILE_DEDCON
    writer.WriteChar( C_METASERVER_DEMO_MODE, demoLimits.demoMode ? 1 : 0 );
#endif
    writer.WriteChar( C_METASERVER_MAX_PLAYERS, gameHistory.Playback() ? 1 : settings.maxTeams.Get() );
    writer.WriteChar( C_METASERVER_NUM_SPECTATORS, numSpectators );
#ifdef COMPILE_DEDCON
    writer.WriteChar( C_METASERVER_MAX_SPECTATORS, maxSpectators );
#endif

    bool started = serverInfo.started;
    if ( gameHistory.GameTime() <= 0 )
    {
        started = false;
    }

#ifdef DEBUG
    if ( !advertiseKey.IsDefault() )
    {
        started = false;
    }
#endif

    writer.WriteChar( C_METASERVER_GAME_STARTED, ( started ? 1 : 0 ) + ( serverInfo.ended ? 1 : 0 ) );
    // if port forwarding is enabled, the public port is supposed to
    // be identical with the private port. Always.
    writer.WriteInt( C_METASERVER_PUBLIC_PORT, ( portForwarding || serverInfo.publicPort <= 0 ) ? settings.serverPort : serverInfo.publicPort );


    writer.WriteChar( C_METASERVER_GAME_MODE, settings.gameMode );

#ifdef COMPILE_DEDWINIA
    writer.WriteInt( C_METASERVER_MAP_HASH, Map::GetHash() );
    if ( !settings.mapName.IsDefault() )
    {
        writer.WriteString( C_METASERVER_MAP_NAME, settings.mapName.Get() );
    }
#endif

#ifdef COMPILE_DEDCON
    writer.WriteChar( C_METASERVER_SCORE_MODE, settings.scoreMode );
#endif

    if ( started )
    {
        writer.WriteInt( C_METASERVER_GAME_TIME, gameHistory.GameTime()/10 );
    }

#ifdef COMPILE_DEDCON
    writer.WriteChar( C_METASERVER_NUM_PLAYERS, gameHistory.Playback() ? 0 : numPlayers );

    if ( !settings.serverMods.IsDefault() )
    {
        writer.WriteString( C_METASERVER_MODS, settings.serverMods );
    }
#endif
    // send fake password
    if ( !settings.serverPassword.IsDefault() )
    {
#ifdef COMPILE_DEDCON
        writer.WriteString( C_METASERVER_PASSWORD, "yes" );
#else
        writer.WriteChar( C_METASERVER_HASPASSWORD, 1 );
#endif
    }

    if ( publishKey )
    {
#ifdef DEBUG
        if ( !advertiseKey.IsDefault() )
        {
            writer.WriteString( C_LOGIN_KEY, advertiseKey );
        }
        else
#endif
        {
            writer.WriteString( C_LOGIN_KEY, settings.GetServerKey() );
        }
    }

    writer.WriteString( C_DC_TAG_TYPE, C_METASERVER_PLAYERS );

#ifdef COMPILE_DEDCON
    // write the client information
    {
        TagWriter playerWriter( writer, C_PLAYER_LIST );

        int player = powers.size()-1;
        for ( std::vector< SuperPower * >::const_iterator i = powers.begin(); i != powers.end(); ++i )
        {
            // write individual player info into nested tag
            char title[2] = { '0', 0 };
            title[0] = player + '0';
            TagWriter writer( playerWriter, title );
            writer.WriteString( C_PLAYER_LIST, (*i)->GetPrintName() );

            float score = (*i)->GetScore();
            if (!(*i)->ScoreSet() )
            {
                score = (*i)->GetRating();
                if ( score != 0 )
                {
                    score += GetScoreFluctuation( (*i)->GetTeamID() ) * ratingFluctuation.Get();
                }
            }

            writer.WriteInt( "ps", int(score) );
            writer.WriteInt( "pa", (*i)->GetAllianceID() );

            --player;
        }
    }
#endif

    writer.Close();
}

void Advertiser::Force()
{
    ++bumpID;
}

int Advertiser::bumpID = 1;

void LanAdvertiser::Advertise()
{
    if ( !TimeToSend( 10 ) )
    {
        return;
    }

    // try to determine LAN IP
    lanIP.Send();

    if ( !settings.advertiseOnLAN )
    {
        return;
    }

    if ( settings.serverLANIP.IsDefault() )
    {
        return;
    }

#ifdef COMPILE_DEDWINIA
    if( !lanSocket.Bound() )
    {
        // open LAN socket
        int port = settings.serverPort.Get();
        port += 10 + (rand() % 10);
        for( int tries = 20; tries > 0; --tries )
        {
            port += ( 3 + rand() % 5 );
            lanSocket.Open();
            lanSocket.SetPort(port);
            lanSocket.SetIP( settings.serverLANIP.Get() );
            if ( lanSocket.Bind() )
            {
                break;
            }
            else
            {
                lanSocket.Close();
            }
        }
        if( lanSocket.Bound() )
        {
            if( Log::GetLevel() >= 2 )
            {
                Log::Out() << "Opened LAN socket on port " << port << ".\n";
            }
        }
    }
#endif

    AutoTagWriter writer;
    WritePacket( writer, false );

    // broadcast the message
    sockaddr_in broadcastaddr;
    broadcastaddr.sin_family = AF_INET;
    broadcastaddr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcastaddr.sin_port = htons(5009);

    // delegate to usual send function
    writer.SendTo( serverSocket, broadcastaddr, METASERVER_ENCRYPTION );
}

static bool advertisedOnInternet = false;

void InternetAdvertiser::Advertise()
{
    if ( !settings.advertiseOnInternet )
    {
        return;
    }

    // demo servers don't get displayed by the metaserver once they started, so why bother advertising them?
    if ( serverInfo.started && demoLimits.demoMode && gameHistory.GameTime() > 0 )
    {
        return;
    }

    if ( !Reachable() )
    {
        Init();
    }

    if ( !TimeToSend( ServerTTL()-5 ) )
    {
        return;
    }

    if ( serverInfo.publicPort < 0 )
    {
        // return;
    }

    // QueryData( C_SERVER_TTL );

    // write game info
    if ( settings.serverLANIP.Get().size() > 0 || lanIP.numTries >= 4 )
    {
        // Log::Err() << "bla\n";

        AutoTagWriter writer;
        WritePacket( writer, true );

        // send the message to the metaserver
        Send( writer, metaserverSocket );

        advertisedOnInternet = true;
    }
    else
    {
        lanIP.Send();
    }

    // write match
    if ( !portForwarding || serverInfo.publicPort < 0 )
    {
        AutoTagWriter writer(C_METASERVER_MATCH,"aa", C_DC_TAG_TYPE );
        writer.WriteInt   ("ae", 0); // game count
        settings.WriteServerInfo(writer);

        writer.WriteString( C_LOGIN_KEY, settings.GetServerKey() );

        Send( writer, serverSocket );
    }
}

InternetAdvertiser::~InternetAdvertiser()
{
    Stop();
}

void InternetAdvertiser::Stop()
{
    if ( !settings.advertiseOnInternet || !advertisedOnInternet )
    {
        return;
    }

#ifndef COMPILE_DEDCON
    // log out
    AutoTagWriter writer(C_METASERVER,C_METASERVER_LOGOUT, C_DC_TAG_TYPE );
    writer.WriteInt( C_METASERVER_PUBLIC_PORT, ( portForwarding || serverInfo.publicPort <= 0 ) ? settings.serverPort : serverInfo.publicPort );

    Send( writer, metaserverSocket );
    Send( writer, metaserverSocket );
    Send( writer, metaserverSocket );
#endif
}
