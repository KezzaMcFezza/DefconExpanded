/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "Main/config.h"

#include "MetaServer.h"

#include "Lib/Codes.h"

#include "Server/Client.h"
#include "Recordings/AutoTagReader.h"
#include "Recordings/AutoTagWriter.h"
#include "Lib/Clock.h"
#include "Lib/Globals.h"
#include "GameSettings/GameSettings.h"
#include "Network/Exception.h"
#include "Main/Log.h"
#include "Advertiser.h"
#include "Multiwinia/Map.h"

#include <sstream>
#include <iomanip>

#include <stdlib.h>
#include <string.h>

//! checks whether the build has expired (parameter: seconds since epoch)
void CheckExpired( int time )
{
#ifdef SVN_BUILD

// #ifdef DEBUG
    return;
// #endif

   int expiry = 62;

#ifdef COMPILE_DEDCON
    expiry *= 2;
#endif

#ifdef MACOSX
   expiry *= 2;
#endif

    int expire = BUILD_TIME + 60 * 60 * 24 * ( expiry + 1 );
    int hoursleft = ( expire - time )/(60 * 60);
    if ( hoursleft < 0 )
    {
        Log::Err() << "This build is expired.\n";
        exit(-1);
    }

    static int lastHours = hoursleft;

    static bool reported = false;
    if ( reported )
    {
        if( hoursleft < lastHours - 48 || hoursleft < lastHours/2 )
        {
            Log::Err() << "Whoops, what I meant to say was:\n";
            lastHours = hoursleft;
        }
        else
        {
            return;
        }
    }
    reported = true;

    if ( hoursleft >= 48 )
    {
        int daysleft = hoursleft/24;
        Log::Err() << "This build will expire in " << daysleft << " days.\n";
    }
    else if ( hoursleft >= 2 )
    {
        Log::Err() << "This build will expire in " << hoursleft << " hours.\n";
    }
    else if ( hoursleft == 1 )
    {
        Log::Err() << "This build will expire in one hour.\n";
    }
    else if ( hoursleft == 0 )
    {
        Log::Err() << "This build will expire about now.\n";
    }

#endif
}

InternetAdvertiser * metaServer = 0;

MetaServerQuery::MetaServerQuery()
        : answered( false )
{
    lastSent.GetTime();
    lastSent.AddSeconds(-10);
}

void MetaServerQuery::WasForMe() // call when a query answer was for you.
{
    answered = true;
}

// called for nested tags
void MetaServerQuery::OnNested( TagReader & reader )
{
    throw ReadException( "Metaserver answers are not supposed to have nested tags." );
}

static void QueryData( char const * data, Socket & socket, sockaddr_in target )
{
    AutoTagWriter writer( C_METASERVER, C_VALUE_QUERY, C_DC_TAG_TYPE );
    writer.WriteString( C_DATA_TYPE, data );
    settings.WriteServerInfo( writer );

    writer.SendTo( socket, target, METASERVER_ENCRYPTION );
}

class MetaServerQueryDemoLimits: public MetaServerQueryList
{
public:
    MetaServerQueryDemoLimits()
    {
        CheckExpired( Time::GetStartTime().Seconds() );
    }
protected:
    virtual void OnInt( std::string const & key, int value )
    {
        if ( !good )
        {
            // ignore the rest of the message
            return;
        }
        else if ( key == C_MAX_DEMO_GAME_SIZE )
        {
#ifdef COMPILE_DEDWINIA
            demoLimits.maxGameSize = 4;
#endif
#ifdef COMPILE_DEDCON
#if VANILLA
            demoLimits.maxGameSize = value;
#elif EIGHTPLAYER
            demoLimits.maxGameSize = 8;
#elif TENPLAYER
            demoLimits.maxGameSize = 10;
#endif
#endif
        }
        else if ( key == C_MAX_DEMO_PLAYERS )
        {
#ifdef COMPILE_DEDWINIA
            demoLimits.maxPlayers = 4;
#endif
#ifdef COMPILE_DEDCON
#if VANILLA
            demoLimits.maxPlayers = value;
#elif EIGHTPLAYER
            demoLimits.maxPlayers = 8;
#elif TENPLAYER
            demoLimits.maxPlayers = 10;
#endif
#endif
        }
#ifdef COMPILE_DEDWINIA
        else if ( key == C_DEMO_MODES )
        {
            demoLimits.demoModes = value;
        }
#endif
        else if ( key == C_METASERVER_RECEIVETIME )
        {
            CheckExpired( value );
        }
    }
    virtual void OnChar( std::string const & key, int value )
    {
        if ( !good )
        {
            // ignore the rest of the message
            return;
        }
    }
    virtual void OnIntArray( std::string const & key, std::vector<int> const & value )
    {
        if ( !good )
        {
            // ignore the rest of the message
            return;
        }
#ifdef COMPILE_DEDWINIA
        else if ( key == C_DEMO_MAPS )
        {
            demoLimits.demoMaps = value;
        }
#endif
    }
    virtual void OnBool( std::string const & key, bool value )
    {
        if ( !good )
        {
            // ignore the rest of the message
            return;
        }
        else if ( key == C_ALLOW_DEMO_SERVERS )
        {
#ifdef COMPILE_DEDWINIA
            demoLimits.allowServers = true;
#else
            demoLimits.allowServers = value;
#endif
        }
    }
    virtual void OnString( std::string const & key, std::string const & value )
    {
        if ( key == C_TAG_TYPE || key == C_DC_TAG_TYPE )
        {
            good &= ( value == C_VALUE_QUERY );
        }
        else if ( key == C_DATA_TYPE )
        {
            good &= ( value == "DemoLimits" );
        }
        else if ( !good )
        {
            // ignore the rest of the message
            return;
        }
    }
    virtual void OnFinish( TagReader & reader )
    {
        if ( !good )
            return;

        // mark message as processed
        WasForMe();

        if ( Log::GetLevel() >= 2 )
        {
            Log::Out() << "Demo limits received."
#ifdef COMPILE_DEDCON
                       << " MaxGameSize = " << demoLimits.maxGameSize
                       << ", MaxDemoPlayers = " << demoLimits.maxPlayers
                       << ", AllowServers = " << ( demoLimits.allowServers ? "yes" : "no" )
#endif
#ifdef COMPILE_DEDWINIA
                       << " DemoModes = " << demoLimits.demoModes
                       << ", " << demoLimits.demoMaps.size() << " allowed maps"
#endif
                       << ".\n";
        }

        // apply demo limits
        demoLimits.set = true;
        demoLimits.ApplyLimits();

#ifdef COMPILE_DEDWINIA
        // recheck whether the server should be demo restricted
        settings.CheckRestrictions();

        Map::CheckDemo();
#endif
    }
private:
    virtual void DoSend( Socket & socket, sockaddr_in const & target )
    {
        // QueryData( "LatestVersion", socket, target );
        // QueryData( "MOTD", socket, target );
        // QueryData( "UpdateURL", socket, target );
        // QueryData( C_SERVER_TTL, socket, target );
        QueryData( "DemoLimits", socket, target );
    }
};

class MetaServerQueryKey: public MetaServerQueryList
{
public:
    MetaServerQueryKey( std::string const & key_ )
        : key( key_ ), keyID( -1 ), me( 0 )
    {
    }

    MetaServerQueryKey * WorthBothering()
    {
        if ( key.size() < 10 )
        {
            keyID = -1;
            // way too short!
            OnAnswer( -1, "key too short" );
            return 0;
        }

        if ( key[0] == 'D' && key[1] == 'E' && key[2] == 'M' && key[3] == 'O' )
        {
            // demo key, not worth bothering.
            // if ( Log::GetLevel() >= 3 )
            // Log::Out() << "Not checking demo key " << key << ".\n";

            OnAnswer( keyID = -2, "only a demo key" );

            delete this;
            return 0;
        }

        return this;
    }

protected:
    virtual void OnInt( std::string const & key, int value )
    {
        if ( key == C_LOGIN_KEY_ID )
        {
            keyID = value;
#ifdef METASERVER_OUTAGE
            keyID = -1;
#endif
        }
        else if ( key == C_LOGIN_METASERVER_ACCEPTED )
        {
            me = value;
        }
    }
    virtual void OnString( std::string const & key, std::string const & value )
    {
        if ( key == C_TAG_TYPE || key == C_DC_TAG_TYPE )
        {
            good &= ( value == C_LOGIN_METASERVER_ACCEPTED );
        }
        else if ( key == C_LOGIN_KEY )
        {
            good &= ( value == this->key );
        }
    }
    virtual void OnFinish( TagReader & reader )
    {
        // seen problems? quit
        if (!good)
            return;

        /* key me codes accrding no NeoThermic PM:

           7:54:09] Chris Delay says: -3 = revoked 
           -4 = key not found 
           [17:54:31] Ashley Pinner says: Ahh. Is there a -2 status? 
           [17:54:53] Chris Delay says: -2 = banned 
         */

#ifdef METASERVER_OUTAGE
        me = 1;
#endif

        // handle the answer
        char const * invalid = 0;
        switch( me )
        {
        case 1:
            break; // success
        case -1:
            invalid = "completely invalid (wrong format or checksum failed)";
            break;
        case -2:
            invalid = "key banned";
            break;
        case -3:
            invalid = "key revoked";
            break;
        case -4:
            invalid = "key not found";
            break;
        case -7:
            invalid = "invalid version";
            break;
        default:
            invalid = "unknown reason";
        }

        reader.Obsolete();
        OnAnswer( keyID, invalid );

        // and mark message as processed
        WasForMe();
    }

    // called when the answer arrives
    virtual void OnAnswer( int keyID, char const * invalid ) = 0;
    virtual void OnSend( TagWriter & writer ){};

private:
    virtual void DoSend( Socket & socket, sockaddr_in const & target )
    {
        AutoTagWriter writer( C_METASERVER );
        writer.WriteString ( C_DC_TAG_TYPE, C_LOGIN_METASERVER );
        writer.WriteString( C_METASERVER_KEY, key );
        writer.WriteInt( C_LOGIN_KEY_ID, keyID );
        settings.WriteServerInfo( writer );

        // do additional stuff
        OnSend( writer );

        writer.SendTo( socket, target, METASERVER_ENCRYPTION );
    }

    std::string const & GetKey() const { return key; }

    std::string key; // the key to query
protected:
    int keyID;       // the numeric ID of the key
    int me;          // content of the 'me' tag, probably the key validity
};

class MetaServerQueryServerKey: public MetaServerQueryKey
{
public:
    MetaServerQueryServerKey()
    :MetaServerQueryKey( settings.GetServerKey() )
    {
    }
protected:
    // called when the answer arrives
    virtual void OnAnswer( int keyID, char const * invalid )
    {
        serverInfo.keyID = keyID;

        // metaserver problem: key seems to be OK, but keyID lookup unavailable
        if ( keyID < 0 && me == 1 )
        {
            keyID = 0;
        }

        if ( keyID < 0 || invalid )
        {
            std::string crippledkey = std::string( settings.GetServerKey(), 0, 13 ) + "-XXXXXX-XXXXXX-XXX";

            if ( keyID == -2 )
            {
                Log::Err() << "Only got demo key \"" << crippledkey << "\".\n";
            }
            else if ( invalid )
            {
                Log::Err() << "Authentication of server key \"" 
                           << crippledkey << "\" failed, " << invalid << ".\n";
            }
            else
            {
                Log::Err() << "Authentication of server key \"" 
                           << crippledkey << "\" failed, unknown reason.\n";
            }

#ifndef DEBUG
            if ( keyID != -2 )
            {
                // can't handle failed authentication. boo! Exit.
                throw QuitException(1);
            }
#endif

            demoLimits.demoMode = true;

#ifdef COMPILE_DEDWINIA
            Map::CheckDemo();
#endif

            demoLimits.ApplyLimits();
        }
        else
        {
            if ( Log::GetLevel() >= 2 )
                Log::Out() << "Server key authentication succeeded, key id = " << keyID << ".\n";

            // inform the admin system
            Client::OnServerKeyID();
        }
    }
private:
};

// checks the validity of client keys
class MetaServerQueryClientKey: public MetaServerQueryKey
{
public:
    MetaServerQueryClientKey( Client * client_ )
            :MetaServerQueryKey( client_->key ), client( client_ )
    {
        keyID = client->keyID;
    }
protected:
    virtual void OnSend( TagWriter & writer )
    {
        // write the client's IP
        writer.WriteString( "da", inet_ntoa( client->saddr.sin_addr ) );
    };

    // called when the answer arrives
    virtual void OnAnswer( int keyID, char const * invalid )
    {
        // special code for demo keys
        if ( keyID == -2 )
        {
            client->id.SetDemo( true );
        }
        else if ( me != 1 && ( keyID < 0 || keyID != client->keyID || invalid ) )
        {
            if ( invalid )
            {
                Log::Err() << "Authentication of " << *client << "'s key failed, " << invalid << ".\n";
            }
            else
            {
                Log::Err() << "Authentication of " << *client << "'s key failed for an unknown reason.\n";
            }

            // authentication failed
            client->Quit( SuperPower::AuthenticationFailed );
            client = 0;
        }
        else if ( keyID >= 0 )
        {
            // check if the ID was used before
            for ( Client::const_iterator c = Client::GetFirstClient(); c; ++c )
            {
                if ( c->keyID == keyID && &(*c) != client )
                {
                    Log::Err() << "Authentication of " << *client << "'s key succeeded, but it's a duplicate.\n";
#ifndef XDEBUG
                    client->Quit( SuperPower::DuplicateKey );
                    return;
#endif
                }
            }
        }
    }
private:
    Client * client;
};

// checks the validity of client keys
class MetaServerQueryServerTTL: public MetaServerQueryList
{
public:
    MetaServerQueryServerTTL( int & serverTTL )
    : serverTTL( serverTTL )
    {
        good = false;
    }
protected:
    virtual void OnString( std::string const & key, std::string const & value )
    {
        if ( key == C_DATA_TYPE )
        {
            good = ( value == C_SERVER_TTL );
        }

        if ( !good )
            return;

        if ( key == C_SERVER_TTL )
        {
            std::istringstream s( value );
            s >> serverTTL;
        }
    }

    virtual void DoSend( Socket & socket, sockaddr_in const & target )
    {
        QueryData( C_SERVER_TTL, socket, target );
    }

    virtual void OnFinish( TagReader & reader )
    {
        if ( !good )
            return;

        // mark message as processed
        WasForMe();
    }
private:
    int & serverTTL; //!< the time a server info survives
    bool good;       //!< set to false if anything fishy happened so far
};

static IntegerSetting requireMetaserver( Setting::Flow, "RequireMetaServer", 0 );

// call if the metaserver is (temporarily) unavailable
static void MetaServerUnavailable()
{
    if ( requireMetaserver )
    {
        Log::Err() << "Thinking again, can't do much without contact to the metaserver. Goodbye.\n";
        throw QuitException(1);
    }
}

#ifdef COMPILE_DEDCON
char const * metaServerName = "metaserver.introversion.co.uk";
// char const * metaServerName = "simamo.de";
int const metaServerPort = 5008;
#endif

#ifdef COMPILE_DEDWINIA
char const * metaServerName = "metaserver-mw.introversion.co.uk";
int const metaServerPort = 5013;
#endif

MetaServer::MetaServer()
{
    memset(&metaserver,0,sizeof(struct sockaddr_in));

    metaserver.sin_family = AF_INET;
    metaserver.sin_addr.s_addr = INADDR_BROADCAST;
    metaserver.sin_port = htons( metaServerPort );

    reachable = false;
    serverTTL = 20;
}

bool GetHostByName( char const * hostname, sockaddr_in & addr )
{
    struct hostent *hostentry;
    hostentry = gethostbyname ( hostname );
    if ( hostentry )
    {
        addr.sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];
        return true;
    }

    return false;
}

void MetaServer::Init()
{
    metaserverSocket.Open();
    metaserverSocket.SetPort(5010);
    metaserverSocket.Bind();
    if ( !metaserverSocket.Bound() )
    {
        metaserverSocket.SetPort(0);
        metaserverSocket.Bind();
    }

    // look up hostname
    if( GetHostByName( metaServerName, metaserver ) )
    {
        if ( Log::GetLevel() >= 2 )
            Log::Out() << "Metaserver IP: " << inet_ntoa(metaserver.sin_addr) << "\n";

        // open socket to communicate with metaserver
        // metaserverSocket.Open();
        // metaserverSocket.Bind();
        reachable = true;

#ifndef COMPILE_DEDWINIA
        // ask about the TTL of servers
        AddQuery( new MetaServerQueryServerTTL( serverTTL ) );
#endif
    }
    else
    {
        static bool log = true;
        if ( log )
        {
            Log::Err() << "Metaserver name lookup failed, won't do metaserver queries for now (will retry later).\n";
            MetaServerUnavailable();
            log = false;
        }
    }
}

MetaServer::~MetaServer()
{
}

class TagParserMatchMaker: public TagParserLax
{
public:
    struct Client
    {
        // the target address
        sockaddr_in target;

        Client()
        {
            memset(&target,0,sizeof(struct sockaddr_in));
            target.sin_family = AF_INET;
        }

        void SetPort( int port )
        {
            target.sin_port=htons((unsigned short) port);
        }

        void SetAddress( std::string const & address )
        {
            target.sin_addr.s_addr=inet_addr( address.c_str() );
        }

        int GetPort() const
        {
            return ntohs(target.sin_port);
        }

        std::string GetAddress() const
        {
            return inet_ntoa( target.sin_addr );
        }
    };

    TagParserMatchMaker()
    {
    }
protected:
    virtual void OnType( std::string const & type_ )
    {
        type = type_;
    }

    virtual void OnInt( std::string const & key, int value )
    {
        if ( key == C_METASERVER_PUBLIC_PORT )
        {
            a.SetPort(value);
        }
        else if ( key == C_METASERVER_NAT_PORT )
        {
            b.SetPort(value);
        }
    }

    virtual void OnString( std::string const & key, std::string const & value )
    {
        if ( key == C_TAG_TYPE )
        {
            clas = value;
        }
        else if ( key == C_DC_TAG_TYPE )
        {
            clas = value;
        }
        else if ( key == "da" )
        {
            a.SetAddress(value);
        }
        else if ( key == C_METASERVER_PUBLIC_IP )
        {
            b.SetAddress(value);
        }
    }

    virtual void OnNested( TagReader & reader )
    {
        reader.Obsolete();
        throw ReadException( "Matchmaking data is not supposed to have nested tags." );
    }

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        // write answer packet to client, bogus content just to pierce the firewall
        if ( type == C_METASERVER_MATCH )
        { 
            if ( clas == "ab" )
            {
                if ( b.GetAddress() != serverInfo.publicIP || b.GetPort() != serverInfo.publicPort )
                {
                    serverInfo.publicIP   = b.GetAddress();
                    serverInfo.publicPort = b.GetPort();

                    Log::Out() << "Public server info: " << serverInfo.publicIP << ":" << serverInfo.publicPort << "\n";
                    if ( metaServer )
                    { 
                        metaServer->Force();
                    }
                }
            }
            else if ( !portForwarding )
            {
                // write packet
                AutoTagWriter writer(C_METASERVER_MATCH, "ad", C_DC_TAG_TYPE );

                // and send it over the regular socket to the two clients
                writer.SendTo( serverSocket, a.target, PLEBIAN_ENCRYPTION );

                // or rather, only to one. Dunno what the other connection info is for.
                // writer.SendTo( serverSocket, b.target );
            }
        }
    }
private:
    std::string type, clas; //!< the packet type and class

    Client a, b;    //!< two client data, I suppose for two different kind of NATs.
};

//! flag indicating whether the metaserver is reachable
static bool queriesAnswered = false;

// processes metaserver data coming in from the
// given source
bool MetaServer::Process( TagReader & reader, sockaddr_in const & addr )
{
    try
    {
        if ( addr.sin_port == metaserver.sin_port && addr.sin_addr.s_addr == metaserver.sin_addr.s_addr )
        {
            MetaServerQueryList * query = metaServerQueries;
            while ( query )
            {
                // prepare next query
                MetaServerQueryList * next = query->Next();
                
                TagReaderCopy readerCopy( reader );
                
                // prepare query for parsing
                query->good = true;
                
                // parse
                query->Parse( readerCopy );
                
                // get rid of the query when it is done
                if ( query->answered )
                {
                    queriesAnswered = true;
                    delete query;
                }
                
                query = next;
            }
            
            // check for matchmaking packets
            TagParserMatchMaker().Parse( reader );

            return true;
        }
        else
        {
            TagReaderCopy copy( reader );

            try
            {
                if ( lanIP.Receive( copy, addr ) )
                {
                    reader.Obsolete();
                    return true;
                }
            }
            catch( ReadException & e )
            {
                // ignore read errors
                copy.Obsolete();
                throw;
            }
        }
    }
    catch( ReadException & e )
    {
        // ignore read errors
        Log::Err() << "Read error while processing metaserver message, ignoring it.\n";
        reader.Obsolete();

        // mark message as handled
        return true;
    }

    return false;
}

// send all queries
void MetaServer::SendQueries()
{
    Time current;
    current.GetTime();

    MetaServerQueryList * query = metaServerQueries;
    while ( query )
    {
/*
        {
            static Time first = current;
            if ( !queriesAnswered && (current - first).Seconds() > 60 )
            {
                Log::Err() << "Metaserver does not answer to queries, switching to demo mode. Check your network setup.\n";
                MetaServerUnavailable();
                demoLimits.demoMode = true;
                demoLimits.set = true; // go with the hardcoded limits
                demoLimits.ApplyLimits();
                first.AddSeconds( 3600 );
            }
        }
*/

        // resend every 2 seconds
        if ( ( current - query->lastSent ).Milliseconds() > 2000 )
        {
            if ( !reachable )
            {
                Init();
            }

            if ( reachable )
            {
                query->DoSend( metaserverSocket, metaserver );
                query->lastSent = current;
            }
        }

        query = query->Next();
    }
}

void MetaServer::AddQuery( MetaServerQueryList * query )
{
    if ( !reachable )
    {
        Init();
    }

    if ( query )
    {
        query->Insert(metaServerQueries);
    }
}

// send the metaserver some data
void MetaServer::Send( AutoTagWriter & writer, Socket & socket )
{
    writer.SendTo( socket, metaserver, METASERVER_ENCRYPTION );
}

// send the metaserver some data over the special socket
void MetaServer::Send( AutoTagWriter & writer )
{
    Send( writer, metaserverSocket );
}

// receive data from the metaserver socket
void MetaServer::Receive()
{
    AutoTagReader reader( metaserverSocket );

    if ( reader.Received() )
    {
        if ( !Process( reader, reader.Sender() ) )
            reader.Obsolete();
    }
}

bool MetaServer::Reachable() const
{
    return reachable;
}

bool MetaServer::Answers() const
{
    return queriesAnswered;
}

void QueryFactory::QueryDemoLimits()
{
    assert( metaServer );
    metaServer->AddQuery( new MetaServerQueryDemoLimits() );
}

void QueryFactory::QueryServerKey()
{
    assert( metaServer );
    metaServer->AddQuery( (new MetaServerQueryServerKey())->WorthBothering() );
}

void QueryFactory::QueryClientKey( Client * client )
{
    assert( metaServer );
    metaServer->AddQuery( (new MetaServerQueryClientKey( client ))->WorthBothering() );
}

LANIP::LANIP()
: token( rand() ), numTries( 0 ){}

//!< sends out a query
void LANIP::Send()
{
    {
        // don't send too often.
        Time time;
        time.GetTime();
        static Time nextTime = time;
        if ( ( time - nextTime ).Seconds() < 0 )
        {
            return;
        }
        nextTime = time;
        nextTime.AddSeconds(1);
    }

    // try to auto-determine LAN IP..
    if ( settings.serverLANIP.IsDefault() )
    {
        ++numTries;
        if ( numTries > 10 )
        {
            Log::Err() << "LAN IP could not be determined via broadcasts. This hints at a problem with your network setup you should fix, possibly a firewall blocking incoming connections. Using the less reliable gethostbyname(hostname) method.\n";
            const int NAMELEN = 100;
            char hostname[NAMELEN];
            gethostname( hostname, NAMELEN );
            Log::Err() << "Hostname = " << hostname << "\n";

            sockaddr_in addr;
            if( GetHostByName( hostname, addr ) )
            {
                settings.serverLANIP.Set( inet_ntoa(addr.sin_addr) );
            }
            else
            {
                Log::Err() << "Lookup failed. Using loopback.\n";
                settings.serverLANIP.Set( "127.0.0.1" );
            }

            return;
        }

        token = rand();

        // by broadcasting over the LAN to the port the server listens on. This should
        // be received, and the source address is our LAN ip.
        AutoTagWriter writer( "slan" );
        writer.WriteInt( "token", token );

        sockaddr_in broadcastaddr;
        broadcastaddr.sin_family = AF_INET;
        broadcastaddr.sin_addr.s_addr = INADDR_BROADCAST;
        broadcastaddr.sin_port = htons(settings.serverPort);

        writer.SendTo( serverSocket, broadcastaddr, PLEBIAN_ENCRYPTION );
    }
}

 //!< receives the return
bool LANIP::Receive( TagReader & reader, sockaddr_in const & addr )
{
    // check if this is a LAN ip determination packet
    if ( reader.Start() == "slan" )
    {
        if ( reader.ReadKey() == "token" )
        {
            if ( reader.ReadInt() == token )
            {
                // this is our LAN address!
                if( settings.serverLANIP.IsDefault() )
                {
                    settings.serverLANIP.Set( inet_ntoa(addr.sin_addr) );
                }
                
                if ( metaServer )
                {
                    metaServer->Force();
                }
            }

            reader.Obsolete();
            return true;
        }
    }
    reader.Obsolete();
    return false;
}

LANIP lanIP;

//! sets the IP address
class ServerIPActionSetting: public StringActionSetting
{
public:
    ServerIPActionSetting()
    : StringActionSetting( Setting::Network, "ServerIP" )
    {}

    //! activates the action with the specified value
    virtual void Activate( std::string const & val )
    {
        if ( val.size() > 2 )
        {
            serverSocket.SetIP( val );
            if ( metaServer )
            {
                metaServer->metaserverSocket.SetIP( val );
            }
            Log::Out() << "ServerIP = " << serverSocket.GetIP() << '\n';
            settings.serverLANIP.Set( val );
        }
        else
        {
            serverSocket.SetIPAny();
            if ( metaServer )
            {
                metaServer->metaserverSocket.SetIPAny();
            }
            Log::Out() << "ServerIP = ANY\n";
        }

        if ( serverSocket.Bound() )
        {
            serverSocket.Reset();
            if ( metaServer )
            {
                metaServer->metaserverSocket.Reset();
            }
        }
    }
};

static ServerIPActionSetting serverIP;

/*
class MetaServerQuery: public MetaServerQuery
{
    MetaServerQuery()
    {
    }
protected:
    virtual void OnChar( std::string const & key, int value )
    {
    }
    virtual void OnInt( std::string const & key, int value )
    {
    }
    virtual void OnString( std::string const & key, std::string const & value )
    {
    }
    virtual void OnFinish( TagReader & reader )
    {
    }
private:
    virtual void DoSend( Socket & socket, sockaddr_in target )
    {
    }
};
*/
