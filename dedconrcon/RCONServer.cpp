/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "RCONServer.h"
#include "Main\Log.h"
#include "GameSettings\Settings.h"
#include "Lib\FakeEncryption.h"
#include <sstream>
#include <iostream>
#include <string.h>
#include <algorithm>

// configuration settings
IntegerSetting rconEnabled( Setting::Admin, "RCONEnabled", 0 );
IntegerSetting rconPort( Setting::Admin, "RCONPort", 8800 ); // static port assignment, can be set manually inside the configfiles
// RCON port range
IntegerSetting rconStartPort( Setting::Admin, "RCONStartPort", 8800 ); // beginning
IntegerSetting rconEndPort( Setting::Admin, "RCONEndPort", 8850 );     // end
StringSetting rconPassword( Setting::Admin, "RCONPassword", "" );

// client implementation
RCONClient::RCONClient( struct sockaddr_in addr )
    : authenticated( false ), clientAddr( addr ), wantsGameEvents( false ), wantsServerLog( false )
{
    lastActivity.GetTime(); // get current time
}

RCONClient::~RCONClient()
{
}

// RCONServer implementation
RCONServer::RCONServer()
    : initialized( false ), settingsReader( NULL )
{
}

RCONServer::~RCONServer()
{
    Shutdown(); // describes itself
}

RCONServer & RCONServer::GetInstance()
{
    static RCONServer instance;
    return instance;
}

bool RCONServer::Initialize()
{
    if ( initialized )
    {
        return true;
    }

    // are we enabled?
    if ( rconEnabled.Get() <= 0 )
    {
        return false;
    }

    // warn the user of their risky behaviour
    if ( rconPassword.Get().empty() )
    {
        Log::Err() << "RCON enabled but no password set! This is a security risk.\n";
    }

    // try the specific port if set
    int specificPort = rconPort.Get();
    if ( specificPort > 0 )
    {
        rconSocket.Open();
        rconSocket.SetPort( specificPort );

        if ( rconSocket.Bind() )
        {
            initialized = true;
            Log::Out() << "RCON server listening on port " << specificPort << "\n";

            // Set up log streaming
            static RCONLogStreambuf gameEventBuf( this, "GAMEEVENT" );
            static RCONLogStreambuf serverLogBuf( this, "SERVERLOG" );
            static std::ostream gameEventStream( &gameEventBuf );
            static std::ostream serverLogStream( &serverLogBuf );

            // Attach to log system
            Log::Event().GetWrapper().SetSecondaryStream( &gameEventStream );
            Log::Out().GetWrapper().SetSecondaryStream( &serverLogStream );
            Log::Err().GetWrapper().SetSecondaryStream( &serverLogStream );

            return true;
        }

        rconSocket.Close();
        Log::Err() << "Failed to bind RCON server to specified port " << specificPort << "\n";
    }

    // if no port is set inside the configuration file, lets dynamically set a port
    for ( int port = rconStartPort.Get(); port <= rconEndPort.Get(); port++ )
    {
        rconSocket.Open();
        rconSocket.SetPort( port );

        if ( rconSocket.Bind() )
        {
            initialized = true;
            Log::Out() << "RCON server listening on port " << port << " (from range)\n";

            // Set up log streaming
            static RCONLogStreambuf gameEventBuf( this, "GAMEEVENT" );
            static RCONLogStreambuf serverLogBuf( this, "SERVERLOG" );
            static std::ostream gameEventStream( &gameEventBuf );
            static std::ostream serverLogStream( &serverLogBuf );

            // Attach to log system
            Log::Event().GetWrapper().SetSecondaryStream( &gameEventStream );
            Log::Out().GetWrapper().SetSecondaryStream( &serverLogStream );
            Log::Err().GetWrapper().SetSecondaryStream( &serverLogStream );

            return true;
        }

        // close socket and try next port
        rconSocket.Close();
    }

    // if the user has already a port in use by another program, or by another server...
    Log::Err() << "Failed to initialize RCON server: couldn't bind to any port\n";
    return false;
}

void RCONServer::Shutdown()
{
    if ( initialized )
    {
        rconSocket.Close();

        for ( std::map<std::string, RCONClient *>::iterator it = clients.begin();
              it != clients.end(); ++it )
        {
            delete it->second;
        }
        clients.clear();

        failedLoginAttempts.clear();

        initialized = false;
    }
}

void RCONServer::ProcessCommands()
{
    if ( !initialized )
    {
        return;
    }

    char buffer[4096];
    struct sockaddr_in fromAddr;
    int bytesRead = rconSocket.RecvFrom( fromAddr, buffer, sizeof( buffer ) - 1 );

    if ( bytesRead > 0 )
    {
        buffer[bytesRead] = '\0';

        ProcessCommand( buffer, fromAddr );
    }

    CleanupIdleClients();
}

// cleanup clients that do not disconnect correctly
void RCONServer::CleanupIdleClients()
{
    Time currentTime;
    currentTime.GetTime();

    // set timeout to 5 minutes
    const int CLIENT_TIMEOUT_SECONDS = 300;

    std::map<std::string, RCONClient *>::iterator it = clients.begin();
    while ( it != clients.end() )
    {
        RCONClient * client = it->second;

        // check if client has been idle for too long
        Time idleTime = currentTime - client->lastActivity;
        if ( idleTime.Seconds() > CLIENT_TIMEOUT_SECONDS )
        {
            Log::Out() << "RCON client timed out: " << it->first << "\n";
            delete client;
            it = clients.erase( it );
        }
        else
        {
            ++it;
        }
    }
}

std::string RCONServer::IPToString( const struct sockaddr_in & addr )
{
    char ipStr[INET_ADDRSTRLEN];
    inet_ntop( AF_INET, &( addr.sin_addr ), ipStr, INET_ADDRSTRLEN );

    std::stringstream ss;
    ss << ipStr << ":" << ntohs( addr.sin_port );
    return ss.str();
}

bool RCONServer::IsIPLockedOut( const struct sockaddr_in & addr )
{
    std::string ipStr = IPToString( addr );

    // check if the IP has too many failed attempts
    std::map<std::string, int>::iterator it = failedLoginAttempts.find( ipStr );
    if ( it != failedLoginAttempts.end() && it->second >= MAX_FAILED_ATTEMPTS )
    {
        return true;
    }

    return false;
}

void RCONServer::RecordFailedLogin( const struct sockaddr_in & addr )
{
    std::string ipStr = IPToString( addr );

    // increment the counter
    failedLoginAttempts[ipStr]++;

    int attempts = failedLoginAttempts[ipStr];
    Log::Err() << "Failed RCON authentication attempt from " << ipStr
               << " (" << attempts << "/" << MAX_FAILED_ATTEMPTS << ")\n";

    // lockout the user from signing in again until the dedcon client restarts
    if ( attempts >= MAX_FAILED_ATTEMPTS )
    {
        Log::Err() << "RCON access from " << ipStr << " locked until server restart due to too many failed attempts\n";
    }
}

RCONClient * RCONServer::FindClient( const struct sockaddr_in & addr )
{
    std::string ipStr = IPToString( addr );

    std::map<std::string, RCONClient *>::iterator it = clients.find( ipStr );
    if ( it != clients.end() )
    {
        it->second->lastActivity.GetTime();
        return it->second;
    }

    return NULL;
}

RCONClient * RCONServer::CreateClient( const struct sockaddr_in & addr )
{
    std::string ipStr = IPToString( addr );

    RCONClient * client = new RCONClient( addr );
    clients[ipStr] = client;

    Log::Out() << "New RCON connection from " << ipStr << "\n";

    return client;
}

void RCONServer::ProcessCommand( const std::string & command, const struct sockaddr_in & clientAddr )
{
    std::string ipStr = IPToString( clientAddr );

    // check if the user is locked out
    if ( IsIPLockedOut( clientAddr ) )
    {
        // create a temporary client just for the response
        RCONClient tempClient( clientAddr );
        SendResponse( &tempClient, 403, "Too many failed authentication attempts. Access locked until server restart." ); // might remove the access locked message for extra security in the future
        return;
    }

    RCONClient * client = FindClient( clientAddr );
    if ( !client )
    {
        client = CreateClient( clientAddr );
    }

    if ( command.substr( 0, 5 ) == "AUTH " )
    {
        std::string password = command.substr( 5 );

        // make sure the password is correct
        if ( password == rconPassword.Get() )
        {
            client->SetAuthenticated( true );
            SendResponse( client, 200, "Authentication successful" );
            Log::Out() << "RCON authentication successful from " << ipStr << "\n";

            // clear the failed attempts
            failedLoginAttempts[ipStr] = 0;
        }
        else
        {
            SendResponse( client, 401, "Authentication failed: Invalid password" );

            // log the failed attempt
            RecordFailedLogin( clientAddr );
        }
    }
    else if ( command == "STARTGAMELOG" )
    {
        if ( !client->IsAuthenticated() )
        {
            SendResponse( client, 401, "Authentication required" );
            return;
        }

        client->wantsGameEvents = true;
        SendResponse( client, 200, "Game event logging started" );
        Log::Out() << "RCON game event logging started for " << ipStr << "\n";
    }
    else if ( command == "STOPGAMELOG" )
    {
        client->wantsGameEvents = false;
        SendResponse( client, 200, "Game event logging stopped" );
    }
    else if ( command == "STARTSERVERLOG" )
    {
        if ( !client->IsAuthenticated() )
        {
            SendResponse( client, 401, "Authentication required" );
            return;
        }

        client->wantsServerLog = true;
        SendResponse( client, 200, "Server logging started" );
        Log::Out() << "RCON server logging started for " << ipStr << "\n";
    }
    else if ( command == "STOPSERVERLOG" )
    {
        client->wantsServerLog = false;
        SendResponse( client, 200, "Server logging stopped" );
    }
    else if ( command.substr( 0, 5 ) == "EXEC " )
    {
        // execute a command, such as /set hostname or /set maxteams
        if ( !client->IsAuthenticated() )
        {
            SendResponse( client, 401, "Authentication required" );
            return;
        }

        // parse the command
        std::string serverCommand = command.substr( 5 );
        // log it
        Log::Out() << "RCON command from " << ipStr << ": " << serverCommand << "\n";
        // execute
        std::string result = ExecuteCommand( serverCommand );

        SendResponse( client, 200, result );
    }
    else
    {
        SendResponse( client, 400, "Unknown command" );
    }
}

std::string RCONServer::ExecuteCommand( const std::string & command )
{
    SettingsReader & settingsReader = SettingsReader::GetSettingsReader();

    std::stringstream output;

    if ( command == "quit" )
    {
        // handle the quit command
        output << "Server restarting...";
        settingsReader.AddLine( "RCON", "QUIT" );
    }
    else if ( command.substr( 0, 4 ) == "/set" )
    {
        // extract the GameSetting from the command
        if ( command.length() > 5 )
        {
            std::string settingCommand = command.substr( 5 ); // skip /set

            settingsReader.AddLine( "RCON", settingCommand );
            output << "Setting updated: " << settingCommand;
        }
        else
        {
            output << "Invalid /set command format. Use /set <name> <value>";
        }
    }
    else
    {
        settingsReader.AddLine( "RCON", command );
        output << "Command executed: " << command;
    }

    return output.str();
}

void RCONServer::SendResponse( RCONClient * client, int statusCode, const std::string & message )
{
    std::stringstream response;
    response << "STATUS " << statusCode << "\nMESSAGE " << message << "\n";

    std::string responseStr = response.str();

#if DEBUG
    Log::Out() << "RCON response to " << IPToString( client->clientAddr )
               << " - STATUS: " << statusCode << " MSG: " << message << "\n";
#endif

    rconSocket.SendTo( client->clientAddr, responseStr.c_str(), responseStr.length(), NO_ENCRYPTION );
}

void RCONServer::SendLogMessage( RCONClient * client, const std::string & logType, const std::string & message )
{
    std::stringstream response;
    response << "LOG " << logType << " " << message;

    std::string responseStr = response.str();

    rconSocket.SendTo( client->clientAddr, responseStr.c_str(), responseStr.length(), NO_ENCRYPTION );
}

void RCONServer::BroadcastGameEvent( const std::string & message )
{
    for ( std::map<std::string, RCONClient *>::iterator it = clients.begin();
          it != clients.end(); ++it )
    {
        RCONClient * client = it->second;
        if ( client->IsAuthenticated() && client->wantsGameEvents )
        {
            SendLogMessage( client, "GAMEEVENT", message );
        }
    }
}

void RCONServer::BroadcastServerLog( const std::string & message )
{
    for ( std::map<std::string, RCONClient *>::iterator it = clients.begin();
          it != clients.end(); ++it )
    {
        RCONClient * client = it->second;
        if ( client->IsAuthenticated() && client->wantsServerLog )
        {
            SendLogMessage( client, "SERVERLOG", message );
        }
    }
}

RCONServer::RCONServer( const RCONServer & )
{
}

RCONServer & RCONServer::operator=( const RCONServer & )
{
    return *this;
}

// RCONLogStreambuf implementation
RCONLogStreambuf::RCONLogStreambuf( RCONServer * server, const std::string & logType )
    : server( server ), logType( logType )
{
}

int RCONLogStreambuf::overflow( int c )
{
    if ( c != EOF )
    {
        buffer += static_cast<char>( c );
        if ( c == '\n' )
        {
            if ( logType == "GAMEEVENT" )
            {
                server->BroadcastGameEvent( buffer );
            }
            else if ( logType == "SERVERLOG" )
            {
                server->BroadcastServerLog( buffer );
            }
            buffer.clear();
        }
    }
    return c;
}

std::streamsize RCONLogStreambuf::xsputn( const char * s, std::streamsize n )
{
    for ( std::streamsize i = 0; i < n; ++i )
    {
        overflow( s[i] );
    }
    return n;
}