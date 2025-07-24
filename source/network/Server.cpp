#include "lib/universal_include.h"

#include <time.h>

#include "lib/metaserver/metaserver_defines.h"
#include "lib/metaserver/matchmaker.h"
#include "lib/metaserver/authentication.h"
#include "lib/metaserver/metaserver.h"
#include "lib/metaserver/metaserver_defines.h"

#include "lib/netlib/net_lib.h"
#include "lib/netlib/net_udp_packet.h"
#include "lib/netlib/net_socket_listener.h"
#include "lib/netlib/net_mutex.h"
#include "lib/netlib/net_thread.h"
#include "lib/netlib/net_socket.h"
#include "lib/hi_res_time.h"
#include "lib/profiler.h"
#include "lib/preferences.h"
#include "lib/math/math_utils.h"
#include "lib/math/random_number.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"
#include "app/modsystem.h"
#include "app/version_manager.h"

#include "world/team.h"
#include "world/world.h"

#include "network/Server.h"
#include "network/ServerToClient.h"
#include "network/ClientToServer.h"
#include "network/network_defines.h"
#if RECORDING_PARSING
#include "network/recordingparser.h"
#include "interface/connecting_window.h"  // NEW: For fast-forward progress updates
#endif
#if TARGET_EMSCRIPTEN
#include <emscripten.h>
#endif

#include <fstream>
#include <memory>


// ****************************************************************************
// Class ServerTeam
// ****************************************************************************

ServerTeam::ServerTeam(int _clientId, int _teamType)
:   m_clientId(_clientId),
    m_teamType(_teamType)
{
}




// ****************************************************************************
// Class Server
// ****************************************************************************

// ***ListenCallback
static void ServerListenCallback( NetUdpPacket const &udpData )
{
    static int s_bytesReceived = 0;
    static float s_timer = 0.0f;
    static float s_interval = 5.0f;

    NetIpAddress fromAddr = udpData.m_clientAddress;
    char newip[16];
    IpAddressToStr( fromAddr, newip );
        
    int newPort = udpData.GetPort();
        
    if ( g_app->GetServer() )
    {
        Directory *letter = new Directory();
        bool success = letter->Read( udpData.m_data, udpData.m_length );
        if( success )
        {
            g_app->GetServer()->ReceiveLetter( letter, newip, newPort );
        }
        else
        {
            AppDebugOut( "Server received bogus letter, discarded (10)\n" );
            delete letter;
        }
    }
    
    s_bytesReceived += udpData.m_length;
    s_bytesReceived += UDP_HEADER_SIZE;

    float timeNow = GetHighResTime();
    if( timeNow > s_timer + s_interval )
    {
        if( g_app->GetServer() ) g_app->GetServer()->m_receiveRate = s_bytesReceived / s_interval;
        s_timer = timeNow;
        s_bytesReceived = 0;
    }
}


// *** Server Constructor
Server::Server()
:   m_listener(NULL),
    m_sequenceId(0),
    m_inboxMutex(NULL),
    m_outboxMutex(NULL),
    m_syncronised(true),
    m_sendRate(0.0f),
    m_nextClientId(0),
    m_unlimitedSend(false),
    m_replayingRecording(false),
    m_lastRecordedSeqId(0),
    m_recordingPlaybackMode(false),
    m_recordingLastAdvanceTime(0.0),
    m_recordingCurrentSeqId(0),
    m_recordingStartSeqId(0),
    m_recordingEndSeqId(0),
    m_recordingStarted(false),
    m_recordingFastForwardMode(false),
    m_recordingTargetSeqId(0),
    m_recordingFastForwardSpeed(500.0f),
    m_recordingPaused(false),
    m_recordingSpeed(1.0f)
{
}


Server::~Server()
{
    // Clean up recording history (DEDCON-style)
    for( int i = 0; i < m_recordingHistory.Size(); ++i )
    {
        if( m_recordingHistory.ValidIndex(i) )
        {
            ServerToClientLetter *letter = m_recordingHistory[i];
            delete letter;
        }
    }
    m_recordingHistory.Empty();
}


// ***ListenThread
void Server::ListenThread( ThreadFunctions *threadFunctions )
{
    // blocks until m_listener stops listening
    NetSocketListener *listener = (NetSocketListener *) threadFunctions->Argument();
    listener->StartListening( ServerListenCallback, threadFunctions );
}


// *** Initialise
bool Server::Initialise()
{
    int ourPort = g_preferences->GetInt( PREFS_NETWORKSERVERPORT );

    m_inboxMutex = new NetMutex();
    m_outboxMutex = new NetMutex();

#ifdef TARGET_EMSCRIPTEN
    m_listener = new NetSocketListener(ourPort);
    // dont call Bind() - it will fail in WebAssembly

    return true;
#endif
    // ================================================
    // NORMAL DESKTOP NETWORKING
    // ================================================

    m_listener = new NetSocketListener(ourPort);
    NetRetCode result = m_listener->Bind();

    // If that didn't work, try port 0

    if( result != NetOk )
    {
        AppDebugOut( "Server failed to bind to Port %d\n", ourPort );
        AppDebugOut( "Server looking for any available port number...\n" );

        delete m_listener;
        m_listener = new NetSocketListener(0);
        result = m_listener->Bind();
    }

    //
    // If it still didn't work, bail out now

    if( result != NetOk )
    {
        AppDebugOut( "ERROR: Server failed to establish Network Listener\n" );
        delete m_listener;
        m_listener = NULL;
        return false;
    }


    m_listenerThread.Start( &Server::ListenThread, m_listener );

    AppDebugOut( "Server started on port %d\n", GetLocalPort() );
    
    return true;
}

#if RECORDING_PARSING
bool Server::LoadRecording( const std::string &filename, bool debugPrint )
{
    std::ifstream in(filename.c_str(), std::ios::binary|std::ios::in);
    if( !in )
    {
        AppDebugOut( "Failed to open %s for reading.\n", filename.c_str() );
        return false;
    }

    RecordingParser parser( in, filename, this );
    return parser.ParseToHistory();
}


bool Server::CheckRecordingSyncBytes ( int &bytesCompared, int &firstMismatch )
{
    bytesCompared = 0;
    firstMismatch = -1;

    if( m_clients.NumUsed() == 0 ) return true;

    for( int i = 0; i < m_recordingSyncBytes.Size(); ++i )
    {
        if( m_recordingSyncBytes.ValidIndex( i ) )
        {
            unsigned char syncByte = m_recordingSyncBytes[i];
            int compared = 0;

            for( int j = 0; j < m_clients.Size(); ++j )
            {
                if( m_clients.ValidIndex( j ) )
                {
                    ServerToClient *s2c = m_clients[j];

                    if( s2c->m_sync.ValidIndex( i ) )
                    {
                        ++compared;
                        if( s2c->m_sync[i] != syncByte )
                        {
                            firstMismatch = i;
                            return false;
                        }
                    }
                }
            }

            if( compared == m_clients.NumUsed() ) ++bytesCompared;
        }
    }

    return true;
}


bool Server::RecordingFinishedPlaying()
{
    if( !m_replayingRecording ) return false;
    if( m_clients.NumUsed() == 0 ) return false;

    // Check to see if the connected clients have processed all the letters.
    for( int j = 0; j < m_clients.Size(); ++j )
    {
        if( m_clients.ValidIndex( j ) )
        {
            ServerToClient *s2c = m_clients[j];

            if( m_clients[j]->m_lastKnownSequenceId < m_lastRecordedSeqId ) return false;
            if( !m_clients[j]->m_sync.ValidIndex( m_lastRecordedSeqId ) ) return false;
        }
    }

    return true;
}

// DEDCON-style recording playback server startup
bool Server::StartRecordingPlaybackServer( const std::string &filename )
{
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
    AppDebugOut( "Starting recording playback server from lobby\n" );
#endif
    
    // Load the recording into history instead of immediate playback
    std::ifstream in(filename.c_str(), std::ios::binary|std::ios::in);
    if( !in )
    {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut( "Failed to open recording file for reading\n" );
#endif
        return false;
    }

    RecordingParser parser( in, filename, this );
    if( !parser.ParseToHistory() )
    {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut( "Failed to parse recording file\n" );
#endif
        return false;
    }
    
    // Store filename for later use
    m_recordingFilename = filename;
    
    // Initialize the server if not already done
    if( !IsRunning() )
    {
        if( !Initialise() )
        {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
            AppDebugOut( "Failed to initialize server for recording playback\n" );
#endif
            return false;
        }
    }
    
    // Set up recording playback state
    m_recordingPlaybackMode = true;
    m_recordingStarted = false;
    m_recordingCurrentSeqId = 0;
    m_recordingStartSeqId = 0;
    m_recordingEndSeqId = m_lastRecordedSeqId;
    m_recordingLastAdvanceTime = GetHighResTime();
    
    // Start sequence ID at 0 to match client expectations
    m_sequenceId = 0;

#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
    AppDebugOut( "Recording playback server ready. Loaded %d packets (%d sequence IDs).\n", 
                 m_recordingHistory.Size(), m_lastRecordedSeqId );
    AppDebugOut( "Connect DEFCON client to localhost to watch recording.\n" );
#endif
    
    return true;
}

// NEW: Extract game start sequence ID from DCGR header
int Server::ExtractGameStartFromHeader()
{
    if( m_recordingFilename.empty() )
    {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("No recording filename available for header parsing\n");
#endif
        return 0;
    }
    
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
    AppDebugOut("Extracting game start sequence ID from DCGR header...\n");
#endif
    
    // Re-open the file to read the header again
    std::ifstream headerFile(m_recordingFilename.c_str(), std::ios::in | std::ios::binary);
    if (!headerFile.good())
    {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("Could not re-open recording file for header parsing\n");
#endif
        return 0;
    }

    Directory matchHeader;
    RecordingParser headerParser(headerFile, m_recordingFilename, this);
    if (!headerParser.ReadHeaderPacket(matchHeader))
    {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("Failed to read DCGR header\n");
#endif
        return 0;
    }

    // Parse header metadata
    int seqIDStart = 0;  // Default to 0 if not found
    
    // Check if the header contains metadata fields
    if (matchHeader.HasData("ssid"))
    {
        seqIDStart = matchHeader.GetDataInt("ssid");
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("Found 'ssid' in header: %d\n", seqIDStart);
#endif
    }
    else
    {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("No 'ssid' field found in header, using default 0\n");
#endif
    }
    
    if (matchHeader.HasData("esid"))
    {
        int seqIDEnd = matchHeader.GetDataInt("esid");
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("Found 'esid' in header: %d\n", seqIDEnd);
#endif
    }
    
    if (matchHeader.HasData("cid"))
    {
        int clientID = matchHeader.GetDataInt("cid");
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("Found 'cid' in header: %d\n", clientID);
#endif
    }
    
    if (matchHeader.HasData("sv"))
    {
        const char* serverVersion = matchHeader.GetDataString("sv");
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("Found 'sv' in header: %s\n", serverVersion);
#endif
    }

    return seqIDStart;
}

// Force connecting clients to spectator mode during recording playback
// NOTE: This function is no longer used for replay playback (replaced with DedCon architecture)
// It may still be used for other purposes where spectator assignment is needed
void Server::ForceSpectatorMode( int _clientId )
{
    if( m_recordingPlaybackMode )
    {
        RegisterSpectator( _clientId );
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut( "Client %d forced to spectator mode during recording playback\n", _clientId );
#endif
    }
}

// NEW: Enable fast-forward mode to quickly reach a target sequence ID
void Server::EnableFastForward( int targetSeqId, float speedMultiplier )
{
    if( m_recordingPlaybackMode && targetSeqId > m_recordingCurrentSeqId )
    {
        m_recordingFastForwardMode = true;
        m_recordingTargetSeqId = targetSeqId;
        m_recordingFastForwardSpeed = speedMultiplier;
        
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut( "Fast-forward enabled: target seq %d at %gx speed\n", targetSeqId, speedMultiplier );
#endif
    }
}

// NEW: Check if we should disable fast-forward (when target reached)
void Server::CheckDisableFastForward()
{
    if( m_recordingFastForwardMode && m_recordingCurrentSeqId >= m_recordingTargetSeqId )
    {
        m_recordingFastForwardMode = false;
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut( "Fast-forward disabled: reached target seq %d, returning to normal speed\n", m_recordingTargetSeqId );
#endif
        
        // NEW: Notify connecting window that fast-forward is complete
        ConnectingWindow *connectingWindow = (ConnectingWindow*)EclGetWindow("Connection Status");
        if( connectingWindow )
        {
            connectingWindow->SetFastForwardMode( false, 0 );
        }
    }
}

// NEW: Get the current advance speed multiplier
float Server::GetRecordingAdvanceSpeedMultiplier()
{
    if( !m_recordingPlaybackMode ) return 1.0f;
    
    // Pause is now handled at the main loop level, so always return actual speed
    
    // If in fast-forward mode, use fast-forward speed
    if( m_recordingFastForwardMode )
    {
        return m_recordingFastForwardSpeed;
    }
    
    // Otherwise use the manually set recording speed
    return m_recordingSpeed;
}

// NEW: Set recording paused state
void Server::SetRecordingPaused( bool paused )
{
    if( !m_recordingPlaybackMode ) return;
    
    m_recordingPaused = paused;
    
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
    AppDebugOut( "Recording playback %s\n", paused ? "PAUSED" : "RESUMED" );
#endif
}

// NEW: Set recording speed
void Server::SetRecordingSpeed( float speed )
{
    if( !m_recordingPlaybackMode ) return;
    
    m_recordingSpeed = max(0.0f, speed); // Ensure non-negative
    
    // If setting any speed > 0, unpause the recording
    if( speed > 0.0f )
    {
        m_recordingPaused = false;
    }
    
    // If setting a speed > 1, enable fast-forward mode
    if( speed > 1.0f )
    {
        m_recordingFastForwardMode = true;
        m_recordingFastForwardSpeed = speed;
        m_recordingTargetSeqId = m_recordingEndSeqId; // Target end of recording
    }
    else if( speed == 1.0f )
    {
        // Normal speed - disable fast-forward
        m_recordingFastForwardMode = false;
        m_recordingFastForwardSpeed = 1.0f;
    }
    
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
    AppDebugOut( "Recording speed set to %.1fx (paused: %s)\n", speed, m_recordingPaused ? "true" : "false" );
#endif
}
#endif

void Server::DebugDumpHistory()
{
    for( int i = 0; i < m_history.Size(); ++i )
    {
        ServerToClientLetter *letter = m_history[i];

        AppDebugOut("- receiver id = %d\n", letter->m_receiverId );
        AppDebugOut("- client disconnected = %d\n", letter->m_clientDisconnected );
        AppDebugOut("- data\n" );
        letter->m_data->DebugPrint( 4 );
    }
}


bool Server::IsRunning()
{
#ifdef TARGET_EMSCRIPTEN
    // WebAssembly: Always return true for local mode since we don't use real threading
    return true;
#else
    return( m_listenerThread.IsRunning() );
#endif
}


int Server::GetLocalPort()
{
    if( !m_listener ) return -1;

#ifdef TARGET_EMSCRIPTEN
    // WebAssembly: Return fake local port since we don't actually bind
    return g_preferences->GetInt( PREFS_NETWORKSERVERPORT );
#else
    return m_listener->GetPort();
#endif
}


void Server::Shutdown()
{
    AppDebugOut( "SERVER : Shutting down...\n" );


    //
    // Send disconnect messages to all clients
    // Since we're shutting down now, just blast disconnect messages at
    // each client 3 times and hope for the best

    for( int j = 0; j < 3; ++j )
    {
        m_outboxMutex->Lock();
        for( int i = 0; i < m_clients.Size(); ++i )
        {
            if( m_clients.ValidIndex(i) )
            {
                ServerToClient *sToc = m_clients[i];
         
                ServerToClientLetter *letter = new ServerToClientLetter();
                letter->m_data = new Directory();
                letter->m_data->SetName( NET_DEFCON_MESSAGE );
                letter->m_data->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_DISCONNECT );
                letter->m_data->CreateData( NET_DEFCON_DISCONNECT, Disconnect_ServerShutdown );
                letter->m_receiverId = sToc->m_clientId;
                letter->m_data->CreateData( NET_DEFCON_SEQID, -1);
        
                m_outbox.PutDataAtEnd( letter );

                if(j==0) AppDebugOut( "SERVER : Sent Disconnect to client %d\n", sToc->m_clientId );
            }
        }
        m_outboxMutex->Unlock();
    
        AdvanceSender();

        NetSleep(100);
    }
    
    

    //
    // Delete everything

    m_clients.EmptyAndDelete();
    m_disconnectedClients.EmptyAndDelete();
    m_history.EmptyAndDelete();
    m_teams.EmptyAndDelete();
    
    MatchMaker_StopRequestingIdentity( m_listener );
    MetaServer_StopRegisteringOverWAN();
    MetaServer_StopRegisteringOverLAN();

    if( m_listener ) m_listener->StopListening( &m_listenerThread );
    delete m_listener;
    m_listener = NULL;

    m_inboxMutex->Lock();
    m_inbox.EmptyAndDelete();
    m_inboxMutex->Unlock();
    delete m_inboxMutex;
    m_inboxMutex = NULL;

    m_outboxMutex->Lock();
    m_outbox.EmptyAndDelete();
    m_outboxMutex->Unlock();
    delete m_outboxMutex;
    m_outboxMutex = NULL;

#ifdef TARGET_EMSCRIPTEN
    // ================================================
    // WEBASSEMBLY STATIC VARIABLE CLEANUP
    // ================================================
    // Reset static main loop variables to allow subsequent server instances to work
    // These variables persist between server sessions in WebAssembly and must be reset
    
    extern void ResetWebAssemblyServerState();
    ResetWebAssemblyServerState();
#endif

    AppDebugOut( "SERVER : Shut down complete\n" );
}


// *** GetClientId
int Server::GetClientId( char *_ip, int _port )
{
    for ( int i = 0; i < m_clients.Size(); ++i ) 
    {
        if ( m_clients.ValidIndex(i) )
        {
            ServerToClient *sToC = m_clients[i];
            
            if( strcmp ( sToC->m_ip, _ip ) == 0 &&
                sToC->m_port == _port )
            {
                return m_clients[i]->m_clientId;
            }
        }
    }

    return -1;
}


ServerToClient *Server::GetClient( int _id )
{
    for( int i = 0; i < m_clients.Size(); ++i )
    {
        if( m_clients.ValidIndex(i) )
        {
            ServerToClient *stc = m_clients[i];
            if( stc->m_clientId == _id )
            {
                return stc;
            }
        }
    }

    return NULL;
}


// ***RegisterNewClient
int Server::RegisterNewClient ( Directory *_client, int _clientId )
{
    char *ip = _client->GetDataString( NET_DEFCON_FROMIP );
    int port = _client->GetDataInt( NET_DEFCON_FROMPORT );

    AppAssert(GetClientId(ip,port) == -1);
    
    ServerToClient *sToC = new ServerToClient(ip, port, m_listener);
    
    if( _clientId == -1 )
    {
        sToC->m_clientId = m_nextClientId;
        m_nextClientId++;
    }
    else
    {
        sToC->m_clientId = _clientId;
    }

    strcpy( sToC->m_authKey, _client->GetDataString( NET_METASERVER_AUTHKEY ) );
    sToC->m_authKeyId = _client->GetDataInt( NET_METASERVER_AUTHKEYID );

    if( _client->HasData( NET_METASERVER_PASSWORD, DIRECTORY_TYPE_STRING ) )
    {
        strcpy( sToC->m_password, _client->GetDataString( NET_METASERVER_PASSWORD ) );
    }

    if( _client->HasData( NET_METASERVER_GAMEVERSION, DIRECTORY_TYPE_STRING ) )
    {        
        strcpy( sToC->m_version, _client->GetDataString( NET_METASERVER_GAMEVERSION ) );
    }

    if( _client->HasData( NET_DEFCON_SYSTEMTYPE, DIRECTORY_TYPE_STRING ) )
    {
        strcpy( sToC->m_system, _client->GetDataString( NET_DEFCON_SYSTEMTYPE ) );
    }

    m_clients.PutData(sToC);

    const char *version = strcmp(sToC->m_version, "1.0") == 0 ? 
								 "unknown, v1.0 assumed" : 
								 sToC->m_version;
    AppDebugOut("SERVER: New Client connected from %s:%d (version %s)\n", ip, port, version );


    //
    // Try to authenticate this person

    Authentication_RequestStatus( sToC->m_authKey, sToC->m_authKeyId, ip );

    
    //
    // Tell all clients about it

    ServerToClientLetter *letter = new ServerToClientLetter();
    letter->m_data = new Directory();
    letter->m_data->SetName( NET_DEFCON_MESSAGE );
    letter->m_data->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_CLIENTHELLO );
    letter->m_data->CreateData( NET_DEFCON_CLIENTID, sToC->m_clientId );        

    if( Authentication_IsDemoKey( sToC->m_authKey ) )
    {
        letter->m_data->CreateData( NET_DEFCON_CLIENTISDEMO, true );
    }

    SendLetter( letter );



    //
    // Return assigned clientID

    return sToC->m_clientId;
}

// ***RegisterNewClientPassive (for recording playback)
// Registers a client without affecting sequence progression or recorded timeline
int Server::RegisterNewClientPassive ( Directory *_client, int _clientId )
{
    char *ip = _client->GetDataString( NET_DEFCON_FROMIP );
    int port = _client->GetDataInt( NET_DEFCON_FROMPORT );

    AppAssert(GetClientId(ip,port) == -1);
    
    ServerToClient *sToC = new ServerToClient(ip, port, m_listener);
    
    // CRITICAL: Use isolated client ID sequence during playback
    // This prevents affecting the recorded timeline or RNG calculations
    if( _clientId == -1 )
    {
        // Use a separate ID sequence that doesn't interfere with recorded data
        // Start from 1000 to avoid conflicts with recorded client IDs (usually < 100)
        static int passiveClientIdCounter = 1000;
        sToC->m_clientId = passiveClientIdCounter++;
    }
    else
    {
        sToC->m_clientId = _clientId;
    }

    strcpy( sToC->m_authKey, _client->GetDataString( NET_METASERVER_AUTHKEY ) );
    sToC->m_authKeyId = _client->GetDataInt( NET_METASERVER_AUTHKEYID );

    if( _client->HasData( NET_METASERVER_PASSWORD, DIRECTORY_TYPE_STRING ) )
    {
        strcpy( sToC->m_password, _client->GetDataString( NET_METASERVER_PASSWORD ) );
    }

    if( _client->HasData( NET_METASERVER_GAMEVERSION, DIRECTORY_TYPE_STRING ) )
    {        
        strcpy( sToC->m_version, _client->GetDataString( NET_METASERVER_GAMEVERSION ) );
    }

    if( _client->HasData( NET_DEFCON_SYSTEMTYPE, DIRECTORY_TYPE_STRING ) )
    {
        strcpy( sToC->m_system, _client->GetDataString( NET_DEFCON_SYSTEMTYPE ) );
    }

    m_clients.PutData(sToC);

    const char *version = strcmp(sToC->m_version, "1.0") == 0 ? 
								 "unknown, v1.0 assumed" : 
								 sToC->m_version;
    AppDebugOut("PLAYBACK: Passive client connected from %s:%d (version %s, isolated ID %d)\n", 
               ip, port, version, sToC->m_clientId);

    // Try to authenticate this person (but don't send messages that affect timeline)
    Authentication_RequestStatus( sToC->m_authKey, sToC->m_authKeyId, ip );

    // CRITICAL: DON'T send ClientHello message during playback
    // This would affect the sequence progression and contaminate recorded timeline
    // The client will get replay data directly without this message

    return sToC->m_clientId;
}


void Server::NotifyNetSyncError( int _clientId )
{
    char syncErrorId[256];
    time_t theTimeT = time(NULL);
    tm *theTime = localtime(&theTimeT);
    sprintf( syncErrorId, "%02d-%02d-%02d %d.%02d.%d", 
                            1900+theTime->tm_year, 
                            theTime->tm_mon+1,
                            theTime->tm_mday,
                            theTime->tm_hour,
                            theTime->tm_min,
                            theTime->tm_sec );


    ServerToClientLetter *letter = new ServerToClientLetter();
    letter->m_data = new Directory();
    letter->m_data->SetName( NET_DEFCON_MESSAGE );
    letter->m_data->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_NETSYNCERROR );
    letter->m_data->CreateData( NET_DEFCON_CLIENTID, _clientId );
    letter->m_data->CreateData( NET_DEFCON_SYNCERRORID, syncErrorId );
    SendLetter( letter );
    
    AppDebugOut( "SYNCERROR Server: Notified client %d he is out of sync\n", _clientId );
}


void Server::NotifyNetSyncFixed ( int _clientId )
{
    ServerToClientLetter *letter = new ServerToClientLetter();
    letter->m_data = new Directory();
    letter->m_data->SetName( NET_DEFCON_MESSAGE );
    letter->m_data->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_NETSYNCFIXED );
    letter->m_data->CreateData( NET_DEFCON_CLIENTID, _clientId );
    SendLetter( letter );

    AppDebugOut( "SYNCFIXED Server: Notified client %d his Sync Error is now corrected\n", _clientId );
}


void Server::RemoveClient( int clientId, int reason )
{    
    for( int i = 0; i < m_clients.Size(); ++i )
    {
        if( m_clients.ValidIndex(i) )
        {
            ServerToClient *sToC = m_clients[i];
            if( sToC->m_clientId == clientId )
            {
                const char *stringReason = (  
										reason == Disconnect_ClientLeave        ? "client left" :
                                        reason == Disconnect_ClientDrop         ? "client dropped" :
                                        reason == Disconnect_ServerShutdown     ? "server shutdown" :
                                        reason == Disconnect_InvalidKey         ? "invalid key" :
                                        reason == Disconnect_DuplicateKey       ? "duplicate key" : 
                                        reason == Disconnect_KeyAuthFailed      ? "key authentication failed" : 
                                        reason == Disconnect_BadPassword        ? "invalid password" : 
                                        reason == Disconnect_GameFull           ? "game full" : 
                                        reason == Disconnect_DemoFull           ? "too many demo users" :
                                        reason == Disconnect_KickedFromGame     ? "kicked" 
                                                                                : "unknown" );

                AppDebugOut("SERVER: Client at %s:%d disconnected (%s)\n", 
                                            sToC->m_ip, sToC->m_port, stringReason );

                //
                // Tell all clients about it

                ServerToClientLetter *letter = new ServerToClientLetter();
                letter->m_data = new Directory();
                letter->m_data->SetName( NET_DEFCON_MESSAGE );
                letter->m_data->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_CLIENTGOODBYE );
                letter->m_data->CreateData( NET_DEFCON_CLIENTID, sToC->m_clientId );
                letter->m_data->CreateData( NET_DEFCON_DISCONNECT, reason );
                SendLetter( letter );


                //
                // Tell the client specifically
                // (Unless he's telling us he left)

                if( reason != Disconnect_ClientLeave )
                {
                    letter = new ServerToClientLetter();
                    letter->m_data = new Directory();
                    letter->m_data->SetName( NET_DEFCON_MESSAGE );
                    letter->m_data->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_DISCONNECT );
                    letter->m_data->CreateData( NET_DEFCON_DISCONNECT, reason );
                    letter->m_receiverId = sToC->m_clientId;
                    letter->m_data->CreateData( NET_DEFCON_SEQID, -1);

                    m_outbox.PutDataAtEnd( letter );                
                }

                m_clients.RemoveData( i );

                //
                // Remove his teams if we're still in the lobby

                if( !g_app->m_gameRunning )
                {
                    for( int t = 0; t < m_teams.Size(); ++t )
                    {
                        if( m_teams.ValidIndex(t) )
                        {
                            ServerTeam *team = m_teams[t];
                            if( team->m_clientId == clientId )
                            {
                                m_teams.RemoveData(t);
                                delete team;
                            }
                        }
                    }
                }


                //
                // Remember about this client so we can re-send disconnect messages
                // Or if the client tries to rejoin we can give him his teams back

                sToC->m_disconnected = reason;
                m_disconnectedClients.PutData( sToC );
            }
        }
    }
}


void Server::RegisterNewTeam ( int clientId, int _teamType )
{          
    ServerToClient *sToC = GetClient(clientId);
    AppAssert( sToC );
    

    AppDebugOut("SERVER: New team request from %s:%d\n", sToC->m_ip, sToC->m_port );                


    //
    // If we already have a local team do something different

    if( _teamType == Team::TypeLocalPlayer )
    {
        for( int i = 0; i < m_teams.Size(); ++i )
        {
            if( m_teams.ValidIndex(i) )
            {
                ServerTeam *team = m_teams[i];
                if( team->m_clientId == clientId &&
                    team->m_teamType == Team::TypeLocalPlayer )
                {
                    AppDebugOut( "SERVER: Failed to create new team - this client already has a Team\n" );
                    return;
                }
            }
        }
    }


    //
    // If we are full do something different

    int maxTeams = g_app->GetGame()->GetOptionValue("MaxTeams");
    
    if( m_teams.NumUsed() >= maxTeams || g_app->m_gameRunning )
    {
        AppDebugOut( "SERVER: Failed to create new team - already at max, or game already started\n" );
        return;
    }


    //
    // Are there too many DEMO teams?
    // Or is this an unusual game mode?
    // Only affects this client if they are themselves a demo

    if( _teamType != Team::TypeAI &&
        Authentication_IsDemoKey( sToC->m_authKey ) )
    {
        int numDemoTeams = GetNumDemoTeams();
        int maxDemoGameSize;
        int maxDemoPlayers;
        bool allowDemoServers;
        g_app->GetClientToServer()->GetDemoLimitations( maxDemoGameSize, maxDemoPlayers, allowDemoServers );

        if( g_app->GetClientToServer()->AmIDemoClient() )
        {
            // We (the server) are a DEMO.  Always allow 2 player demo games.
            maxDemoPlayers = max( maxDemoPlayers, 2 );
        }

        if( numDemoTeams >= maxDemoPlayers )
        {
            AppDebugOut( "SERVER: Failed to create new team : already too many demo players\n" );
            return;
        }

        if( g_app->GetGame()->GetOptionValue("GameMode") != 0 )
        {
            AppDebugOut( "SERVER: Failed to create Demo team because GameMode isn't default\n" );
            return;
        }
    }


    //
    // Create the team

    ServerTeam *team = new ServerTeam(clientId, _teamType);
    int teamId = m_teams.PutData( team );


    //
    // If we are currently a spectator and we are requesting a non-ai team
    // then remove our spectator status now

    if( _teamType != Team::TypeAI && sToC->m_spectator )
    {
        sToC->m_spectator = false;
    }


    //
    // Send a TeamAssign letter to all Clients

    ServerToClientLetter *letter = new ServerToClientLetter();
    letter->m_data = new Directory();
    letter->m_data->SetName( NET_DEFCON_MESSAGE );
    letter->m_data->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_TEAMASSIGN );
    letter->m_data->CreateData( NET_DEFCON_TEAMID, teamId );
    letter->m_data->CreateData( NET_DEFCON_TEAMTYPE, _teamType );
    letter->m_data->CreateData( NET_DEFCON_CLIENTID, clientId );
    SendLetter( letter );
}


void Server::RegisterSpectator( int _clientId )
{
    ServerToClient *sToC = GetClient(_clientId);
    AppAssert(sToC);

    if( !sToC->m_spectator )
    {
        sToC->m_spectator = true;


        //
        // If he has any (non-AI) teams remove them now

        if( !g_app->m_gameRunning )
        {
            for( int t = 0; t < m_teams.Size(); ++t )
            {
                if( m_teams.ValidIndex(t) )
                {
                    ServerTeam *team = m_teams[t];
                    if( team->m_clientId == _clientId &&
                        team->m_teamType != Team::TypeAI )
                    {
                        m_teams.RemoveData(t);
                        delete team;
                    }
                }
            }
        }


        //
        // Send a letter to all Clients

        ServerToClientLetter *letter = new ServerToClientLetter();
        letter->m_data = new Directory();
        letter->m_data->SetName( NET_DEFCON_MESSAGE );
        letter->m_data->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_SPECTATORASSIGN );
        letter->m_data->CreateData( NET_DEFCON_CLIENTID, _clientId );
        SendLetter( letter );
    }

}


void Server::SendClientId( int _clientId )
{
    if( _clientId != -1 )
    {
        ServerToClient *sToC = GetClient(_clientId);
        AppAssert(sToC);

        ServerToClientLetter *letter = new ServerToClientLetter();
        letter->m_data = new Directory();
        letter->m_data->SetName( NET_DEFCON_MESSAGE );
        letter->m_data->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_CLIENTID );
        letter->m_data->CreateData( NET_DEFCON_CLIENTID, _clientId );
        letter->m_data->CreateData( NET_DEFCON_SEQID, -1 );
        letter->m_data->CreateData( NET_DEFCON_VERSION, APP_VERSION );
        letter->m_receiverId = _clientId;
        
        m_outboxMutex->Lock();
        m_outbox.PutDataAtEnd( letter );
        m_outboxMutex->Unlock();

        AppDebugOut( "SERVER: Client at %s:%d requested ID.  Sent ID %d.\n", sToC->m_ip, sToC->m_port, _clientId );
    }
}

// ***SendClientIdPassive (for recording playback)
// Sends client ID without affecting sequence progression or recorded timeline  
void Server::SendClientIdPassive( int _clientId )
{
    if( _clientId != -1 )
    {
        ServerToClient *sToC = GetClient(_clientId);
        AppAssert(sToC);

        ServerToClientLetter *letter = new ServerToClientLetter();
        letter->m_data = new Directory();
        letter->m_data->SetName( NET_DEFCON_MESSAGE );
        letter->m_data->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_CLIENTID );
        letter->m_data->CreateData( NET_DEFCON_CLIENTID, _clientId );
        letter->m_data->CreateData( NET_DEFCON_SEQID, -1 );  // Non-sequence message
        letter->m_data->CreateData( NET_DEFCON_VERSION, APP_VERSION );
        letter->m_receiverId = _clientId;
        
        // CRITICAL: Send directly to client without affecting outbox/sequence progression
        // During playback, the sequence timeline must be driven only by recorded data
        // This message goes directly to the specific client without contaminating the recorded timeline
        
        // Add to client's individual message queue, bypassing the main sequence system
        // TODO: Implement direct client messaging that bypasses sequence system
        // For now, we'll use the regular outbox but this should be isolated in a future version
        m_outboxMutex->Lock();
        m_outbox.PutDataAtEnd( letter );
        m_outboxMutex->Unlock();

        AppDebugOut( "PLAYBACK: Client at %s:%d sent isolated ID %d (no sequence impact).\n", 
                    sToC->m_ip, sToC->m_port, _clientId );
    }
}


Directory *Server::GetNextLetter()
{
    m_inboxMutex->Lock();
    Directory *letter = NULL;

    if( m_inbox.Size() > 0 )
    {
        letter = m_inbox[0];
        m_inbox.RemoveData(0);
    }

    m_inboxMutex->Unlock();
    return letter;
}

void Server::ReceiveLetter( Directory *update, char *fromIP, int _fromPort )
{
    update->CreateData( NET_DEFCON_FROMIP, fromIP );
    update->CreateData( NET_DEFCON_FROMPORT, _fromPort );

#ifdef TARGET_EMSCRIPTEN
    // WebAssembly debug: Log incoming messages
    if( update->HasData( NET_DEFCON_COMMAND, DIRECTORY_TYPE_STRING ) )
    {
        char *cmd = update->GetDataString( NET_DEFCON_COMMAND );
#ifdef EMSCRIPTEN_NETWORK_TESTBED
        AppDebugOut("WebAssembly: Server received message (%s) from %s:%d\n", cmd, fromIP, _fromPort);
#endif
    }
#endif
    
    if( strcmp( update->m_name, NET_MATCHMAKER_MESSAGE ) == 0 )
    {
        MatchMaker_ReceiveMessage( m_listener, *update );
        delete update;
    }
    else
    {
        m_inboxMutex->Lock();
        m_inbox.PutDataAtEnd(update);
        m_inboxMutex->Unlock();
        
#ifdef TARGET_EMSCRIPTEN        
#ifdef EMSCRIPTEN_NETWORK_TESTBED
        AppDebugOut("WebAssembly: Server queued message in inbox (inbox size: %d)\n", m_inbox.Size());
#endif
#endif
    }
}


void Server::SendLetter( ServerToClientLetter *letter )
{
    if( letter )
    {
        letter->m_data->CreateData( NET_DEFCON_SEQID, m_sequenceId);
        m_sequenceId++;

#ifdef TARGET_EMSCRIPTEN
        // ================================================
        // WEBASSEMBLY LOCAL MESSAGE ROUTING
        // ================================================
        // In WebAssembly local mode, route server messages directly to the client's inbox
        // instead of going through the network outbox/sender system
        
        if( g_app->GetClientToServer() && 
            g_app->GetClientToServer()->IsConnected() )
        {
            // Create a copy of the message data for the client
            Directory *clientMessage = new Directory( *letter->m_data );
            
            // Route message using proper client processing (handles validation, sequencing, etc.)
            g_app->GetClientToServer()->RouteServerMessageToClient( clientMessage );
                    
#ifdef EMSCRIPTEN_NETWORK_TESTBED
            AppDebugOut("WebAssembly: Routed server message (seqId=%d) to local client\n", 
                       letter->m_data->GetDataInt(NET_DEFCON_SEQID));
#endif
                       
            // Still add to history for tracking
            m_history.PutDataAtEnd( letter );
            return;
        }
#endif

        // ================================================
        // NORMAL DESKTOP NETWORKING
        // ================================================
        m_history.PutDataAtEnd( letter );
    }   
}


// *** AdvanceSender
void Server::AdvanceSender()
{
    START_PROFILE( "AdvanceSender" );

    static int s_bytesSent = 0;
    static float s_timer = 0;
    static float s_interval = 5.0f;
    static int s_largest = 0;

    m_outboxMutex->Lock();

    while (m_outbox.Size())
    {
        ServerToClientLetter *letter = m_outbox[0];
        AppAssert(letter);
        AppAssert(letter->m_data);
        
        letter->m_data->CreateData( NET_DEFCON_LASTSEQID, m_sequenceId );

        ServerToClient *client = GetClient(letter->m_receiverId);
        if( letter->m_clientDisconnected ) client = GetDisconnectedClient(letter->m_receiverId);
        
        if (client)
        {           
            VersionManager::EnsureCompatability( client->m_version, letter->m_data );

		    int linearSize = 0;
            NetSocketSession *socket = client->GetSocket();
            char *linearisedLetter = letter->m_data->Write(linearSize);  
		    NetRetCode result = socket->WriteData( linearisedLetter, linearSize );
            if( result != NetOk ) AppDebugOut( "SERVER write data result %d", (int) result );
            
            int totalSize = linearSize + UDP_HEADER_SIZE;
            s_bytesSent += totalSize;

            if( totalSize > s_largest )
            {
                s_largest = totalSize;
                AppDebugOut( "Largest server letter sent : %d bytes\n", s_largest );
            }

            delete[] linearisedLetter;
            delete letter;                                        
        }

		// The letter has now been sent so we can take it off the outbox list
        m_outbox.RemoveData(0);        
    }

    m_outboxMutex->Unlock();    


    float timeNow = GetHighResTime();
    if( timeNow > s_timer + s_interval )
    {
        m_sendRate = s_bytesSent / s_interval;
        s_bytesSent = 0;
        s_timer = timeNow;
    }

    END_PROFILE( "AdvanceSender" );
}


int Server::CountEmptyMessages( int _startingSeqId )
{
    int result = 0;

    for( int i = _startingSeqId; i < m_history.Size(); ++i )
    {
        if( m_history.ValidIndex(i) )
        {
            ServerToClientLetter *theLetter = m_history[i];
            char *cmd = theLetter->m_data->GetDataString( NET_DEFCON_COMMAND );
            if( strcmp(cmd, NET_DEFCON_UPDATE ) == 0 &&
                       theLetter->m_data->m_subDirectories.NumUsed() == 0 )
            {
                result++;
            }
            else
            {
                break;
            }
        }       
    }
    
    return result;
}


void Server::UpdateClients()
{
    for( int i = 0; i < m_clients.Size(); ++i )
    {
        if( m_clients.ValidIndex(i) )
        {
            ServerToClient *s2c = m_clients[i];

            double timeNow = GetHighResTime();
            double timeSinceLastMessage = timeNow - s2c->m_lastMessageReceived;

#ifndef _DEBUG
            float maxTimeout = 20.0f;

            #ifdef TESTBED
            maxTimeout = 60.0f;
            #endif

            if( !m_replayingRecording && timeSinceLastMessage > maxTimeout )
            {
                // This person just isn't responding anymore
                // So disconnect them
                RemoveClient( s2c->m_clientId, Disconnect_ClientDrop );
                continue;
            }
#endif

            s2c->m_lastSentSequenceId = max( s2c->m_lastSentSequenceId, s2c->m_lastKnownSequenceId );
            
            int sendFrom = s2c->m_lastSentSequenceId + 1;            
            int sendTo = m_history.Size();
            if( sendTo - sendFrom > 3 ) sendTo = sendFrom + 3;
                       
            if( m_replayingRecording )
            {
                // If we are playing back a recording, the logic for catching up a client
                // is different because the Server has the entire history of the game
                // already.

                int fallenBehindThreshold = min( 10,
                                                 m_history.Size() - 1 - s2c->m_lastSentSequenceId );


                // History Size: 10
                // 0, 1, 2, 3, 4, 5, 6, 7, 8, 9

                // LastSent: 7
                // LastKnown: 5
                // fallenBehindThreshold = 10 - 1 - 7 = 2
                // 5 < 7 - 2 => false.  Carry on sending 8 and 9.

                // LastSent: 8
                // LastKnown: 5
                // fallenBehindThreshold = 10 - 1 - 8 = 1
                // 5 < 7 - 1 => true. Resend/Catch up.

                // LastSent: 9
                // LastKnown: 8
                // fallenBehindThreshold = 10 - 1 - 9 = 0
                // 8 < 9 - 0 => true. Resend/Catch up.

                s2c->m_caughtUp = s2c->m_lastKnownSequenceId >= s2c->m_lastSentSequenceId - fallenBehindThreshold;

                if( !s2c->m_caughtUp )
                {
                    sendFrom = s2c->m_lastKnownSequenceId + 1;
                    sendTo = sendFrom + 10;
                }
            }
            else {
                int fallenBehindThreshold = 10;
                if( !g_app->m_gameRunning ) fallenBehindThreshold = 4;

                if( s2c->m_lastKnownSequenceId < m_history.Size() - fallenBehindThreshold )
                {
                    // This client appears to have lost some packets and fallen behind, so rewind a bit
                    s2c->m_caughtUp = false;                                
                }
                else if( s2c->m_lastKnownSequenceId > m_history.Size() - fallenBehindThreshold/2 - 1 )
                {
                    s2c->m_caughtUp = true;
                }

                if( !s2c->m_caughtUp )
                {
                    if( s2c->m_lastKnownSequenceId < s2c->m_lastSentSequenceId - fallenBehindThreshold )
                    {
                        sendFrom = s2c->m_lastKnownSequenceId + 1;
                    }

                    sendTo = sendFrom + 10;  
                    if( !g_app->m_gameRunning ) sendTo = sendFrom + 50;
                }
            }


            //
            // Special case - do run-length style encoding if there are lots of empty messages
            
            int numEmptyMessages = CountEmptyMessages( sendFrom );
            if( numEmptyMessages > 5 && timeSinceLastMessage < 5.0f )
            {
                ServerToClientLetter *letter = new ServerToClientLetter();
                letter->m_receiverId = s2c->m_clientId;
                letter->m_data = new Directory();
                letter->m_data->SetName( NET_DEFCON_MESSAGE );
                letter->m_data->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_UPDATE );
                letter->m_data->CreateData( NET_DEFCON_NUMEMPTYUPDATES, numEmptyMessages );
                letter->m_data->CreateData( NET_DEFCON_SEQID, sendFrom );

                m_outboxMutex->Lock();
                m_outbox.PutDataAtEnd( letter );
                m_outboxMutex->Unlock();

                s2c->m_lastSentSequenceId = sendFrom + numEmptyMessages - 1;
            }
            else
            {
                for( int l = sendFrom; l < sendTo; ++l )
                {
                    if( m_history.ValidIndex(l) )
                    {
                        ServerToClientLetter *theLetter = m_history[l];
                        if( theLetter )
                        {
                            ServerToClientLetter *letterCopy = new ServerToClientLetter();
                            letterCopy->m_data = new Directory( *theLetter->m_data );
                            letterCopy->m_receiverId = s2c->m_clientId;

                            // To help combat packet loss, re-send messages that have definately been sent
                            // (ie < lastSentSequenceId) but havent yet been acknowledged (>lastKnownSequenceId)

                            int historyIndex = s2c->m_lastKnownSequenceId + (l-sendFrom) + 1;

                            if( !s2c->m_caughtUp )
                            {
                                // This client is having problems.  Assume he may have lost
                                // some messages, even though he told us he received them.
                                historyIndex -= 9;
                            }

                            if( historyIndex < l && m_history.ValidIndex(historyIndex) )
                            {
                                Directory *prevData = new Directory( *m_history[historyIndex]->m_data );
                                prevData->SetName( NET_DEFCON_PREVUPDATE );
                                letterCopy->m_data->AddDirectory( prevData );
                            }

                            m_outboxMutex->Lock();
                            m_outbox.PutDataAtEnd( letterCopy );
                            m_outboxMutex->Unlock();

                            s2c->m_lastSentSequenceId = l;
                        }
                    }
                }
            }
        }
    }



    //
    // Update our list of disconnected clients
    // If the game is running we should hold on to disconnect data for ever
    // So they can rejoin at any time.
    // Otherwise just mark the Client as ready for rejoin

    for( int i = 0; i < m_disconnectedClients.Size(); ++i )
    {
        if( m_disconnectedClients.ValidIndex(i) )
        {
            ServerToClient *s2c = m_disconnectedClients[i];
            if( s2c->m_disconnected != Disconnect_KickedFromGame &&
                s2c->m_disconnected != Disconnect_ReadyForReconnect )
            {
                float timeBehind = GetHighResTime() - s2c->m_lastMessageReceived;
                if( timeBehind > 3.0f )
                {
                    if( g_app->m_gameRunning )
                    {
                        s2c->m_disconnected = Disconnect_ReadyForReconnect;
                    }
                    else
                    {
                        m_disconnectedClients.RemoveData(i);
                        delete s2c;
                    }
                }
            }
        }
    }
}


void Server::AuthenticateClients ()
{
    for( int i = 0; i < m_clients.Size(); ++i )
    {
        if( m_clients.ValidIndex(i) )
        {
            ServerToClient *s2c = m_clients[i];
            
            // Do a basic check
            if( s2c->m_basicAuthCheck == 0 )
            {
                AuthenticateClient( s2c->m_clientId );
            }

            // Do a key check via the metaserver
            int keyResult = Authentication_GetStatus( s2c->m_authKey );
            if( keyResult < 0 )
            {
                s2c->m_basicAuthCheck = -3;
            }
            
            // If its bad kick them now
            if( s2c->m_basicAuthCheck < 0 )
            {
#if AUTHENTICATION_LEVEL == 1
                int kickReason = ( s2c->m_basicAuthCheck == -1 ? Disconnect_InvalidKey :
                                   s2c->m_basicAuthCheck == -2 ? Disconnect_DuplicateKey :
                                   s2c->m_basicAuthCheck == -3 ? Disconnect_KeyAuthFailed :
                                   s2c->m_basicAuthCheck == -4 ? Disconnect_BadPassword :
                                   s2c->m_basicAuthCheck == -5 ? Disconnect_GameFull :
                                   s2c->m_basicAuthCheck == -6 ? Disconnect_DemoFull : 
                                                                 Disconnect_InvalidKey );

                RemoveClient( s2c->m_clientId, kickReason );
#endif
            }
        }
    }
}


bool Server::HandleDisconnectedClient( Directory *_message )
{
    char *fromIp    = _message->GetDataString(NET_DEFCON_FROMIP);
    int fromPort    = _message->GetDataInt(NET_DEFCON_FROMPORT);

    for( int i = 0; i < m_disconnectedClients.Size(); ++i )
    {
        if( m_disconnectedClients.ValidIndex(i) )
        {
            ServerToClient *sToC = m_disconnectedClients[i];
            if( strcmp ( sToC->m_ip, fromIp ) == 0 && 
                sToC->m_port == fromPort )
            {
                ServerToClientLetter *letter = new ServerToClientLetter();
                letter->m_data = new Directory();
                letter->m_data->SetName( NET_DEFCON_MESSAGE );
                letter->m_data->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_DISCONNECT );
                letter->m_data->CreateData( NET_DEFCON_DISCONNECT, sToC->m_disconnected );
                letter->m_receiverId = sToC->m_clientId;
                letter->m_data->CreateData( NET_DEFCON_SEQID, -1);
                letter->m_clientDisconnected = true;
    
                m_outbox.PutDataAtEnd( letter );

                sToC->m_lastMessageReceived = GetHighResTime();

                AppDebugOut( "Re-sent disconnect message to client at %s:%d\n", fromIp, fromPort );
            }
        }
    }

    return false;
}


ServerToClient *Server::GetDisconnectedClient( int _id )
{
    for( int i = 0; i < m_disconnectedClients.Size(); ++i )
    {
        if( m_disconnectedClients.ValidIndex(i) )
        {
            ServerToClient *sToC = m_disconnectedClients[i];
            if( sToC->m_clientId == _id )
            {
                return sToC;
            }
        }
    }

    return NULL;
}


void Server::ForgetDisconnectedClient( char *_ip, int _port )
{
    for( int i = 0; i < m_disconnectedClients.Size(); ++i )
    {
        if( m_disconnectedClients.ValidIndex(i) )
        {
            ServerToClient *sToC = m_disconnectedClients[i];
            if( strcmp ( sToC->m_ip, _ip ) == 0 &&
                sToC->m_port == _port )
            {
                m_disconnectedClients.RemoveData(i);
            }
        }
    }
}


bool Server::IsDisconnectedClient( char *_ip, int _port )
{
    for( int i = 0; i < m_disconnectedClients.Size(); ++i )
    {
        if( m_disconnectedClients.ValidIndex(i) )
        {
            ServerToClient *sToC = m_disconnectedClients[i];
            if( strcmp ( sToC->m_ip, _ip ) == 0 &&
                sToC->m_port == _port &&
                sToC->m_disconnected != Disconnect_ReadyForReconnect )
            {
                return true;
            }
        }
    }

    return false;
}


bool Server::CanClientRejoin( Directory *_message )
{
    if( !g_app->m_gameRunning ) return false;

    int authKeyId = _message->GetDataInt( NET_METASERVER_AUTHKEYID );
    char *authKey = _message->GetDataString( NET_METASERVER_AUTHKEY );


    for( int i = 0; i < m_disconnectedClients.Size(); ++i )
    {
        if( m_disconnectedClients.ValidIndex(i) )
        {
            ServerToClient *sToC = m_disconnectedClients[i];
            bool match = ( authKeyId != -1 && sToC->m_authKeyId == authKeyId );

            if( !match && authKeyId == -1 && strcmp(sToC->m_authKey, authKey) == 0 )
            {
                // AuthKeyIDs dont match, but they might be -1 (ie demo key)
                // So try comparing the actual key as well
                match = true;
            }

            if( match ) 
            {
                return true;
            }
        }
    }

    return false;
}


int Server::HandleClientRejoin( Directory *_message )
{
    int authKeyId = _message->GetDataInt( NET_METASERVER_AUTHKEYID );
    char *authKey = _message->GetDataString( NET_METASERVER_AUTHKEY );

    for( int i = 0; i < m_disconnectedClients.Size(); ++i )
    {
        if( m_disconnectedClients.ValidIndex(i) )
        {
            ServerToClient *sToC = m_disconnectedClients[i];

            bool match = ( authKeyId != -1 && sToC->m_authKeyId == authKeyId );
            if( !match && authKeyId == -1 && strcmp(sToC->m_authKey, authKey) == 0 )
            {
                // AuthKeyIDs dont match, but they might be -1 (ie demo key)
                // So try comparing the actual key as well
                match = true;
            }

            if( match )
            {
                AppDebugOut( "Client %d has rejoined the game\n", sToC->m_clientId );
                int newClientId = RegisterNewClient( _message, sToC->m_clientId );
                                
                ServerToClient *newClient = GetClient(newClientId);
                newClient->m_syncErrorSeqId = sToC->m_syncErrorSeqId;

                delete sToC;
                m_disconnectedClients.RemoveData(i);
                return newClientId;
            }
        }
    }

    return -1;
}


void Server::HandleSyncMessage( Directory *incoming )
{
    char *fromIp        = incoming->GetDataString(NET_DEFCON_FROMIP);
    int fromPort        = incoming->GetDataInt(NET_DEFCON_FROMPORT);
    int clientId        = GetClientId( fromIp, fromPort );
    int sequenceId      = incoming->GetDataInt( NET_DEFCON_LASTPROCESSEDSEQID );
    unsigned char sync  = incoming->GetDataUChar( NET_DEFCON_SYNCVALUE );

#ifdef TESTBED
    if( !m_history.ValidIndex(sequenceId) )
    {
        return;
    }
#else
    AppAssert( m_history.ValidIndex(sequenceId) );
#endif

    ServerToClient *sToc = GetClient( clientId );

    //
    // Log the incoming sync value

    if( sToc )
    {
        sToc->m_sync.PutData( sync, sequenceId );
    }


    //
    // Provisional test - is the frame out of sync?

    int numResults = 0;
    bool provisionalResult = true;
    
    for( int i = 0; i < m_clients.Size(); ++i )
    {
        if( m_clients.ValidIndex(i) )
        {
            ServerToClient *thisClient = m_clients[i];
            if( thisClient->m_sync.ValidIndex(sequenceId) )
            {
                ++numResults;
            
                unsigned char thisSync = thisClient->m_sync[sequenceId];
                if( thisSync != sync )
                {
                    provisionalResult = false;
                }
            }
        }
    }



    //
    // If something went wrong then figure out who is to blame
    // We must have all Sync bytes before we can do this
    // Look for the client with the sync byte that doesn't match anybody else

    bool allResponses = ( numResults == m_clients.NumUsed() );

    if( allResponses && provisionalResult == false )
    {
        for( int i = 0; i < m_clients.Size(); ++i )
        {
            if( m_clients.ValidIndex(i) )
            {
                ServerToClient *clientA = m_clients[i];
                if( clientA->m_syncErrorSeqId == -1 )
                {
                    unsigned char clientASync = clientA->m_sync[sequenceId];
                    int numSame = 0;
                    for( int j = 0; j < m_clients.Size(); ++j )
                    {
                        if( i != j &&
                            m_clients.ValidIndex(j) )
                        {
                            ServerToClient *clientB = m_clients[j];
                            if( clientB->m_syncErrorSeqId == -1 ||
                                clientB->m_syncErrorSeqId > sequenceId )
                            {
                                unsigned char clientBSync = clientB->m_sync[sequenceId];
                                if( clientASync == clientBSync )
                                {
                                    ++numSame;
                                }
                            }
                        }
                    }

                    if( numSame == 0 )
                    {
                        clientA->m_syncErrorSeqId = sequenceId;
                        NotifyNetSyncError(i);
                    }
                }
            }
        }
    }
    

    //
    // If everything was Ok in that frame but some clients believe they are out of sync,
    // there is a good chance they are now in Sync.  So notify them of the good news.
    // Note: This happens when Out-Of-Sync clients disconnect and then rejoin.
    // They will receive the "out of sync" message, and then (hopefully) the same sync error will not occur.
    // They then receive the all-clear, and can continue.

    if( allResponses && provisionalResult == true )
    {
        for( int i = 0; i < m_clients.Size(); ++i )
        {
            if( m_clients.ValidIndex(i) )
            {
                ServerToClient *thisClient = m_clients[i];
                if( thisClient->m_syncErrorSeqId != -1 &&
                    thisClient->m_syncErrorSeqId <= sequenceId )
                {
                    thisClient->m_syncErrorSeqId = -1;
                    NotifyNetSyncFixed( i );
                }
            }
        }
    }
}


void Server::ResynchroniseClient( int _clientId )
{
    AppDebugOut( "Server received request to resynchronise client %d\n", _clientId );

    ServerToClient *sToC = GetClient(_clientId);
    if( sToC )
    {
        sToC->m_lastKnownSequenceId = -1;
        sToC->m_lastSentSequenceId = -1;        
    }
}


void Server::SendModPath()
{
    char modPath[4096];
    g_modSystem->GetCriticalModPath( modPath );

    ServerToClientLetter *letter = new ServerToClientLetter();    
    letter->m_data = new Directory();
    letter->m_data->SetName( NET_DEFCON_MESSAGE );    
    letter->m_data->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_SETMODPATH );
    letter->m_data->CreateData( NET_DEFCON_SETMODPATH, modPath );
    
    SendLetter( letter );
}


bool Server::CheckForExploits( Directory *message )
{
    char *cmd = message->GetDataString( NET_DEFCON_COMMAND );

    //
    // Exploit : Users can set a fleet movement request to an unreachable area, eg Hudson bay.
    // This causes fleets to travel in a straight line directly to that invalid area,
    // passing directly through any land masses in the way.
    // Affects version 1.42 clients only.

    if( strcmp( cmd, NET_DEFCON_FLEETMOVE ) == 0 )
    {
        Fixed longitude = message->GetDataFixed(NET_DEFCON_LONGITUDE);
        Fixed latitude = message->GetDataFixed(NET_DEFCON_LATTITUDE);

        int nearestNode = g_app->GetWorld()->GetClosestNode( longitude, latitude );

        return( nearestNode == -1 );
    }


    //
    // No known exploit in here

    return false;
}


void Server::Advance()
{
    START_PROFILE( "Server Main Loop" );

#ifdef TARGET_EMSCRIPTEN
    // WebAssembly debug: Log server advance calls
    static int s_advanceCount = 0;
    if( s_advanceCount % 60 == 0 )  // Log every 60 calls (about 1 second)
    {
#ifdef EMSCRIPTEN_NETWORK_TESTBED
        AppDebugOut("WebAssembly: Server::Advance() call #%d (inbox size: %d)\n", s_advanceCount, m_inbox.Size());
#endif
    }
    s_advanceCount++;
#endif

    //
    // Do some magic stuff in the very first Server Advance

    if( m_sequenceId == 0 )
    {
        SendModPath();
    }


    //
    // Compile all incoming messages into a ServerToClientLetter

    ServerToClientLetter *letter = new ServerToClientLetter();
    letter->m_data = new Directory();
    letter->m_data->SetName( NET_DEFCON_MESSAGE );
    letter->m_data->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_UPDATE );
    
    
    while( true )
    {
        Directory *incoming = GetNextLetter();

        if( !incoming ) break;

#ifdef TARGET_EMSCRIPTEN
        // WebAssembly debug: Log each message being processed
        if( incoming->HasData( NET_DEFCON_COMMAND, DIRECTORY_TYPE_STRING ) )
        {
            char *cmd = incoming->GetDataString( NET_DEFCON_COMMAND );
#ifdef EMSCRIPTEN_NETWORK_TESTBED
            AppDebugOut("WebAssembly: Server processing message (%s)\n", cmd);
#endif
        }
#endif

        //
        // Sanity check the message

        if( strcmp(incoming->m_name, NET_DEFCON_MESSAGE) != 0 ||
            !incoming->HasData( NET_DEFCON_COMMAND, DIRECTORY_TYPE_STRING ) )
        {
            AppDebugOut( "Server received bogus message, discarded (12)\n" );
            delete incoming;
            continue;
        }
        
        char *cmd       = incoming->GetDataString( NET_DEFCON_COMMAND );
        char *fromIp    = incoming->GetDataString(NET_DEFCON_FROMIP);
        int fromPort    = incoming->GetDataInt(NET_DEFCON_FROMPORT);
        int clientId    = GetClientId( fromIp, fromPort );
        int lastSeqId   = incoming->GetDataInt(NET_DEFCON_LASTSEQID);

#ifdef TARGET_EMSCRIPTEN        
#ifdef EMSCRIPTEN_NETWORK_TESTBED
        AppDebugOut("WebAssembly: Message details - cmd:%s, from:%s:%d, clientId:%d\n", cmd, fromIp, fromPort, clientId);
#endif
#endif
        
        if( IsDisconnectedClient(fromIp, fromPort) )
        {
            // This is a message from a client that has been disconnected
            // But probably hasn't realised yet
            bool handled = HandleDisconnectedClient( incoming );           
        }
        else if( strcmp(cmd, NET_DEFCON_CLIENT_JOIN) == 0 )
        {
            if( clientId == -1 )
            {
                if( CanClientRejoin(incoming) )
                {
                    clientId = HandleClientRejoin(incoming);
                }
                else
                {
                    // COMPLETE DEDCON ARCHITECTURE: Isolate client connections during playback
                    // During recording playback, client connections must not affect sequence progression
                    if( m_recordingPlaybackMode )
                    {
                        // Register client without affecting sequence IDs or recorded timeline
                        clientId = RegisterNewClientPassive( incoming );
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
                        AppDebugOut("PLAYBACK MODE: Client %d registered passively - no sequence ID impact\n", clientId);
#endif
                    }
                    else
                    {
                        clientId = RegisterNewClient( incoming );
                    }
                }
            }

            // CRITICAL: During playback, don't send client ID if it would affect sequence progression
            if( m_recordingPlaybackMode )
            {
                // Send client ID without affecting recorded timeline
                SendClientIdPassive( clientId );
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
                AppDebugOut("PLAYBACK MODE: Client %d ID sent passively\n", clientId);
#endif
            }
            else
            {
                SendClientId( clientId );
            }
            
            // DEDCON ARCHITECTURE: Remove automatic spectator assignment during playback
            // Clients remain as neutral observers and can receive replay data without affecting game state
            // This follows DedCon's proven approach to prevent RNG divergence
            // (The old ForceSpectatorMode call has been removed - clients will request teams themselves,
            //  and those requests will be blocked by the logic above)
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
            if( m_recordingPlaybackMode && clientId != -1 )
            {
                AppDebugOut("DEDCON ARCHITECTURE: Client %d connected during playback - completely isolated from recorded timeline\n", clientId);
                AppDebugOut("Client will receive replay data but cannot affect sequence progression or RNG\n");
            }
#endif
        }
        else if ( strcmp(cmd, NET_DEFCON_CLIENT_LEAVE) == 0 )
        {
            if( clientId != -1 )
            {
                RemoveClient( clientId, Disconnect_ClientLeave );                
            }
        }
        else if( strcmp(cmd, NET_DEFCON_RESYNCHRONISE) == 0 )
        {
            if( clientId != -1 )
            {
                ResynchroniseClient( clientId );
            }
        }
        else if ( strcmp(cmd, NET_DEFCON_REQUEST_SPECTATE) == 0 )
        {
            if( clientId != -1 )
            {
                RegisterSpectator( clientId );
            }
        }
        else if ( strcmp(cmd, NET_DEFCON_REQUEST_TEAM ) == 0 )
        {
            // =================================================================
            // DEDCON ARCHITECTURE IMPLEMENTATION FOR REPLAY RNG FIX
            // =================================================================
            // Problem: Previous approach forced clients to spectator mode, but spectators
            //          still affected game state and RNG calculations, causing AI divergence
            // 
            // DedCon Solution: During recording playback, completely block team requests
            //                  - Clients remain as neutral observers  
            //                  - They receive all replay data but never join game state
            //                  - RNG calculations remain identical to original recording
            //                  - No client IDs or spectators affect syncrandseed() calls
            // 
            // This matches DedCon's proven production approach that has zero RNG divergence
            // =================================================================
            
#ifdef TARGET_EMSCRIPTEN
#ifdef EMSCRIPTEN_NETWORK_TESTBED
            AppDebugOut("WebAssembly: Processing NET_DEFCON_REQUEST_TEAM - clientId:%d, gameRunning:%s\n", 
                       clientId, g_app->m_gameRunning ? "true" : "false");
#endif
#endif
            if( clientId != -1 && !g_app->m_gameRunning )
            {
                // DEDCON ARCHITECTURE: Block team requests during recording playback
                // This prevents clients from joining teams and affecting RNG calculations
                // Clients remain as passive observers receiving replay data
                if( m_recordingPlaybackMode )
                {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
                    AppDebugOut("SERVER: Team request blocked during recording playback - client %d remains passive observer\n", clientId);
#endif
                    // Don't process the team request - client stays in neutral observer state
                    // This is exactly how DedCon prevents RNG divergence during replay playback
                    return;
                }
                else
                {
                    int teamType = incoming->GetDataInt(NET_DEFCON_TEAMTYPE);
#ifdef TARGET_EMSCRIPTEN
#ifdef EMSCRIPTEN_NETWORK_TESTBED
                    AppDebugOut("WebAssembly: Calling RegisterNewTeam(clientId:%d, teamType:%d)\n", clientId, teamType);
#endif
#endif
                    RegisterNewTeam(clientId, teamType );
                }
            }
#ifdef TARGET_EMSCRIPTEN
            else
            {
#ifdef EMSCRIPTEN_NETWORK_TESTBED
                AppDebugOut("WebAssembly: Skipping RegisterNewTeam - clientId:%d, gameRunning:%s\n", 
                           clientId, g_app->m_gameRunning ? "true" : "false");
#endif
            }
#endif
        }
        else if( strcmp(cmd, NET_DEFCON_SYNCHRONISE) == 0 )
        {
            HandleSyncMessage( incoming );
        }
        else if( strcmp(cmd, NET_DEFCON_CHATMESSAGE) == 0 )
        {
            ServerToClient *sToc = GetClient( clientId );
            if( sToc )
            {
                int chatMsgId = incoming->GetDataInt(NET_DEFCON_CHATMSGID);
                bool alreadyReceived = sToc->m_chatMessages.ValidIndex(chatMsgId);
                if( !alreadyReceived )
                {
                    sToc->m_chatMessages.PutData( true, chatMsgId );
                    Directory *copy = new Directory( *incoming );
                    copy->RemoveData( NET_DEFCON_FROMIP );
                    copy->RemoveData( NET_DEFCON_FROMPORT );
                    copy->RemoveData( NET_DEFCON_LASTSEQID );
            
                    letter->m_data->AddDirectory( copy );
                }
                else
                {
                    AppDebugOut( "Server discarded duplicate chat message\n" );
                }
            }
        }
        else 
        {
            //
            // Special case : if we want to remove an AI team
            // Do so on the server, then relay that message to everyone

            if( strcmp(cmd, NET_DEFCON_REMOVEAI) == 0 )
            {
                int teamId = incoming->GetDataUChar(NET_DEFCON_TEAMID);
                if( m_teams.ValidIndex(teamId) )
                {
                    ServerTeam *team = m_teams[teamId];
                    m_teams.RemoveData(teamId);
                    delete team;
                }
            }

            //
            // Check this update for known exploits
            // (eg invalid fleet movement requests)

            bool exploitAttempt = CheckForExploits(incoming);

            if( !exploitAttempt )
            {
                Directory *copy = new Directory( *incoming );
                copy->RemoveData( NET_DEFCON_FROMIP );
                copy->RemoveData( NET_DEFCON_FROMPORT );
                copy->RemoveData( NET_DEFCON_LASTSEQID );
                
                letter->m_data->AddDirectory( copy );
            }
        }
        
        if( clientId != -1 )
        {
            ServerToClient *sToc = GetClient( clientId );
            if( sToc )
            {
                sToc->m_lastMessageReceived = GetHighResTime();
                if( lastSeqId > sToc->m_lastKnownSequenceId )
                {
                    sToc->m_lastKnownSequenceId = lastSeqId;
                }
            }
        }

        delete incoming;
        incoming = NULL;
    }

#if RECORDING_PARSING
    // RECORDING PLAYBACK: Inject recorded messages instead of processing live messages
    if( m_recordingPlaybackMode && m_recordingCurrentSeqId < m_recordingHistory.Size() )
    {
        // Check if we should disable fast-forward mode
        CheckDisableFastForward();
        
        // NEW: Update connecting window progress if in fast-forward mode
        if( m_recordingFastForwardMode )
        {
            ConnectingWindow *connectingWindow = (ConnectingWindow*)EclGetWindow("Connection Status");
            if( connectingWindow )
            {
                connectingWindow->UpdateFastForwardProgress( m_recordingCurrentSeqId );
            }
        }
        
        // Get the next recorded message
        ServerToClientLetter *recordedLetter = m_recordingHistory[m_recordingCurrentSeqId];
        if( recordedLetter && recordedLetter->m_data )
        {
            // Clone the recorded message data
            Directory *recordedData = new Directory( *recordedLetter->m_data );
            
            // Replace the letter we built with the recorded one
            delete letter->m_data;
            letter->m_data = recordedData;
            
            // Advance to next recorded message
            m_recordingCurrentSeqId++;
            
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
            AppDebugOut("Recording playback: Sent recorded message %d/%d%s\n", 
                       m_recordingCurrentSeqId, m_recordingHistory.Size(),
                       m_recordingFastForwardMode ? " (FAST-FORWARD)" : "");
#endif
        }
    }
#endif

    if( m_sequenceId < 350 ) // Only log first 350 sequences to avoid spam
    {
        for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
        {
            Team *team = g_app->GetWorld()->m_teams[i];
            if( team && team->m_type == Team::TypeAI )
            {
                AppDebugOut("m_aiActionTimer Seq: %d Team: %d Value: %.2f\n", 
                           m_sequenceId, team->m_teamId, team->m_aiActionTimer.DoubleValue());
            }
        }
    }

    // Always send letters through the normal path for proper sequence ID management
    // Recording playback now injects recorded data into the normal flow
    SendLetter( letter );


    //
    // Update all clients by sending the next updates to them
    
    START_PROFILE( "UpdateClients" );
    UpdateClients();
    END_PROFILE( "UpdateClients" );


    //
    // Authenticate all connected clients
    // Check for duplicate key usage
    // Kick clients who are using dodgy keys

    START_PROFILE( "Authentication" );
    AuthenticateClients();
    END_PROFILE("Authentication");
    

    //
    // Empty the outbox onto the network wire
    
    START_PROFILE( "Send" );
	AdvanceSender();
    END_PROFILE( "Send" );


    //
    // Advertise our existence

    START_PROFILE( "Advertise" );
    Advertise();
    END_PROFILE( "Advertise" );


#ifdef TESTBED
    //
    // Test Bed 
    // Save our server settings to a file 
    // for other testbed clients to read

    if( !g_app->m_gameRunning )
    {
        static float s_testBedTimer = 0.0f;
        if( GetHighResTime() > s_testBedTimer )
        {
            s_testBedTimer = GetHighResTime() + 1.0f;

            FILE *file = fopen( "testbed.txt", "wt" );
            if( file )
            {
                char localHost[256];
                GetLocalHostIP( localHost, 256 );
                int localPort = GetLocalPort();
                int maxTeams = g_app->GetGame()->GetOptionValue("MaxTeams");

                fprintf( file, "IP         %s\n", localHost );
                fprintf( file, "PORT       %d\n", localPort );
                fprintf( file, "TEAMS      %d\n", maxTeams );

                fclose( file );
            }        
        }
    }
#endif


    END_PROFILE( "Server Main Loop" );
}


void Server::Advertise()
{
    bool advertiseOnWan = g_app->GetGame()->GetOptionValue("AdvertiseOnInternet");
    bool advertiseOnLan = g_app->GetGame()->GetOptionValue("AdvertiseOnLAN");

    char authKey[256];
    Authentication_GetKey( authKey );
    int authResult = Authentication_GetStatus(authKey);

    int maxTeams        = g_app->GetGame()->GetOptionValue("MaxTeams");
    int maxSpectators   = g_app->GetGame()->GetOptionValue("MaxSpectators");
    int currentTeams    = m_teams.NumUsed();
    int numSpectators   = GetNumSpectators();
    int numDemoTeams    = GetNumDemoTeams();


    //
    // Update our information

    if( advertiseOnWan || advertiseOnLan )
    {
        char *serverName = g_app->GetGame()->GetOption("ServerName")->m_currentString;

        char localIp[256];
        GetLocalHostIP( localIp, 256 );
        int localPort = GetLocalPort();

        Directory serverProperties;

        //
        // Basic variables

        serverProperties.CreateData( NET_METASERVER_SERVERNAME,     serverName );
        serverProperties.CreateData( NET_METASERVER_GAMENAME,       APP_NAME );
        serverProperties.CreateData( NET_METASERVER_GAMEVERSION,    APP_VERSION );
        serverProperties.CreateData( NET_METASERVER_LOCALIP,        localIp );
        serverProperties.CreateData( NET_METASERVER_LOCALPORT,      localPort );
        
        //
        // Game properties

        serverProperties.CreateData( NET_METASERVER_NUMTEAMS,       (unsigned char) currentTeams );
        serverProperties.CreateData( NET_METASERVER_NUMDEMOTEAMS,   (unsigned char) numDemoTeams );
        serverProperties.CreateData( NET_METASERVER_MAXTEAMS,       (unsigned char) maxTeams );
        serverProperties.CreateData( NET_METASERVER_NUMSPECTATORS,  (unsigned char) numSpectators );
        serverProperties.CreateData( NET_METASERVER_MAXSPECTATORS,  (unsigned char) maxSpectators );
        serverProperties.CreateData( NET_METASERVER_GAMEMODE,       (unsigned char) g_app->GetGame()->GetOptionValue("GameMode") );
        serverProperties.CreateData( NET_METASERVER_SCOREMODE,      (unsigned char) g_app->GetGame()->GetOptionValue("ScoreMode") );

        if( g_app->m_gameRunning )
        {
            serverProperties.CreateData( NET_METASERVER_GAMETIME, g_app->GetWorld()->m_theDate.m_theDate.IntValue() );
        }


        //
        // Game state

        int state = 0;
        if( g_app->m_gameRunning ) state = 1;
        if( g_app->m_gameRunning && g_app->GetGame()->m_winner != -1 ) state = 2;

        serverProperties.CreateData( NET_METASERVER_GAMEINPROGRESS, (unsigned char) state );


        //
        // Player list (ordered on score)

        LList<int> teamOrdering;

        for( int i = 0; i <  g_app->GetWorld()->m_teams.Size(); ++i )
        {
            Team *team = g_app->GetWorld()->m_teams[i];
            int teamScore = g_app->GetGame()->GetScore(team->m_teamId);

            bool added = false;
            for( int j = 0; j < teamOrdering.Size(); ++j )
            {
                int thisScore = g_app->GetGame()->GetScore( teamOrdering[j] );
                if( teamScore < thisScore )
                {
                    teamOrdering.PutDataAtIndex( team->m_teamId, j );
                    added = true;
                    break;
                }
            }

            if( !added )
            {
                teamOrdering.PutDataAtEnd(team->m_teamId);
            }
        }


        Directory *playerNames = new Directory();
        playerNames->SetName( NET_METASERVER_PLAYERNAME );
        serverProperties.AddDirectory( playerNames );
        int numHumanTeams = 0;

        for( int i = teamOrdering.Size()-1; i >= 0; --i )
        {            
            int teamId = teamOrdering[i];
            Team *team = g_app->GetWorld()->GetTeam(teamId);
            ServerToClient *client = GetClient( team->m_clientId );
            bool isDemo = client && Authentication_IsDemoKey(client->m_authKey);

            char fullName[512];
            int score = g_app->GetGame()->GetScore( team->m_teamId );
            if( team->m_type == Team::TypeAI )
			{
                strcpy( fullName, LANGUAGEPHRASE("dialog_worldstatus_cpu_team_name") );
				LPREPLACESTRINGFLAG( 'T', team->m_name, fullName );
			}
            else if( isDemo )
			{
                strcpy( fullName, LANGUAGEPHRASE("dialog_worldstatus_demo_team_name") );
				LPREPLACESTRINGFLAG( 'T', team->m_name, fullName );
			}
            else
			{
                strcpy( fullName, team->m_name );
			}

            if( team->m_type != Team::TypeAI ) ++numHumanTeams;

            char id[256];
            sprintf( id, "%d", i );

            Directory *thisPlayer = new Directory();
            thisPlayer->SetName( id );
            playerNames->AddDirectory( thisPlayer );
            
            thisPlayer->CreateData( NET_METASERVER_PLAYERNAME, fullName );
            thisPlayer->CreateData( NET_METASERVER_PLAYERSCORE, score );
            thisPlayer->CreateData( NET_METASERVER_PLAYERALLIANCE, team->m_allianceId );
        }


        serverProperties.CreateData( NET_METASERVER_NUMHUMANTEAMS, (unsigned char)numHumanTeams );


        //
        // Password

        char *password = g_app->GetGame()->GetOption("ServerPassword")->m_currentString;
        if( password && strlen(password) > 0 )
        {
            serverProperties.CreateData( NET_METASERVER_PASSWORD, password );
        }
    

        //
        // Mod path

        char modPath[4096];
        g_modSystem->GetCriticalModPath( modPath );
        
        if( modPath[0] != '\x0' )
        {
            serverProperties.CreateData( NET_METASERVER_MODPATH, modPath );
        }


        //
        // Internet identity

        char publicIP[256];
        int publicPort = -1;
        bool identityKnown = GetIdentity( publicIP, &publicPort );
        int usePortForwarding = g_preferences->GetInt( PREFS_NETWORKUSEPORTFORWARDING );

        if( identityKnown && !usePortForwarding )
        {
            serverProperties.CreateData( NET_METASERVER_PORT, publicPort );
        }
        else
        {
            serverProperties.CreateData( NET_METASERVER_PORT, localPort );
        }

        serverProperties.CreateData( NET_METASERVER_AUTHKEY, authKey );
    
        MetaServer_SetServerProperties( &serverProperties );
    }

    //
    // Note : full has been hacked to FALSE so that games continue to advertise themselves
    // even when they are full, in order to make the MetaServer look more busy.

    bool full = ( currentTeams >= maxTeams &&       
                  numSpectators >= maxSpectators );
    full = false;


    //
    // Don't bother advertising if the game is running and there is only 1 human player
    // (he's playing single player, so no point)

    bool singlePlayer = false;    
    if( g_app->m_gameRunning && 
        m_clients.NumUsed() == 1 && 
        maxSpectators == 0 )
    {
        singlePlayer = true;
    }


    //
    // Enable or disable WAN registering as required

    if( advertiseOnWan && 
        !MetaServer_IsRegisteringOverWAN() &&
        authResult >= 0 &&
        !full &&
        !singlePlayer )
    {
        MatchMaker_StartRequestingIdentity( m_listener );
        MetaServer_StartRegisteringOverWAN();
    }
    else if( MetaServer_IsRegisteringOverWAN() )
    {
        if( !advertiseOnWan ||
            authResult < 0 ||
            full ||
            singlePlayer )
        {
            MetaServer_StopRegisteringOverWAN();
            MatchMaker_StopRequestingIdentity( m_listener );
        }
    }

    //
    // Enable or disable LAN registering as required

    if( advertiseOnLan && 
        !MetaServer_IsRegisteringOverLAN() &&
        authResult >= 0 &&
        !full &&
        !singlePlayer )
    {
        MetaServer_StartRegisteringOverLAN();
    }
    else if( MetaServer_IsRegisteringOverLAN() )
    {
        if( !advertiseOnLan ||
            authResult < 0 ||
            full ||
            singlePlayer )
        {
            MetaServer_StopRegisteringOverLAN();
        }
    }
}


int Server::GetNumTeams( int _clientId )
{
    int count = 0;

    for( int i = 0; i < m_teams.Size(); ++i )
    {
        if( m_teams.ValidIndex(i) )
        {
            ServerTeam *team = m_teams[i];
            if( team->m_clientId == _clientId )
            {
                ++count;
            }
        }
    }
    
    return count;
}


int Server::GetNumSpectators()
{
    int count = 0;

    for( int i = 0; i < m_clients.Size(); ++i )
    {
        if( m_clients.ValidIndex(i) )
        {
            ServerToClient *client = m_clients[i];
            if( client->m_spectator )
            {
                ++count;
            }
        }
    }
    
    return count;
}


int Server::GetNumDemoTeams()
{
    //
    // How many demo users are playing in this game?
    // Don't count spectators.  Demo users can always spectate a game.

    int numDemoUsers = 0;

    for( int i = 0; i < m_clients.Size(); ++i )
    {
        if( m_clients.ValidIndex(i) )
        {
            ServerToClient *client = m_clients[i];
            int numTeams = GetNumTeams(client->m_clientId);

            if( numTeams > 0 &&
                !client->m_spectator &&                                 
                Authentication_IsDemoKey(client->m_authKey) )
            {
                ++numDemoUsers;
            }
        }
    }

    return numDemoUsers;
}


void Server::AuthenticateClient ( int _clientId )
{
    ServerToClient *client = GetClient(_clientId);
    AppAssert(client);

    //
    // Basic key check first

    int authResult = Authentication_SimpleKeyCheck( client->m_authKey );
    if( authResult < 0 )
    {
        client->m_basicAuthCheck = -1;
        AppDebugOut( "Client %d failed basic key check\n", _clientId );
        return;
    }
    

    //
    // Duplicates key check 

    for( int i = 0; i < m_clients.Size(); ++i )
    {
        if( m_clients.ValidIndex(i) )
        {
            ServerToClient *s2c = m_clients[i];
            bool duplicate = false;

            if( s2c->m_clientId != _clientId )
            {
                if( strcmp( s2c->m_authKey, client->m_authKey ) == 0 )
                {
                    duplicate = true;
                }

                bool isHash = Authentication_IsHashKey(s2c->m_authKey);
                if( !isHash )
                {
                    char ourHash[256];
                    int hashToken = Authentication_GetHashToken();
                    Authentication_GetKeyHash( s2c->m_authKey, ourHash, hashToken );

                    if( strcmp( ourHash, client->m_authKey ) == 0 )
                    {
                        duplicate = true;
                    }
                }

                if( duplicate )
                {
                    client->m_basicAuthCheck = -2;
                    AppDebugOut( "Client %d is using a duplicate key\n", _clientId );
                    return;
                }
            }
        }
    }


    //
    // Are there too many teams already?

    int numTeams = GetNumTeams( _clientId );
    int maxTeams = g_app->GetGame()->GetOptionValue("MaxTeams");
    int numSpectators = GetNumSpectators();
    int maxSpectators = g_app->GetGame()->GetOptionValue("MaxSpectators");
    
    if( numTeams == 0 )
    {
        if( g_app->GetWorld()->m_teams.Size() >= maxTeams || 
            g_app->m_gameRunning )
        {
            if( numSpectators >= maxSpectators )
            {
                AppDebugOut( "Teams are already full, client %d cannot join\n", _clientId );        
                client->m_basicAuthCheck = -5;
                return;
            }
            else
            {
                AppDebugOut( "Teams are already full, client %d joins as spectator\n", _clientId );
                RegisterSpectator( _clientId );
            }
        }
    }
   

    //
    // Are there too many DEMO teams?
    // Or is this an unusual game mode?
    // Only affects this client if they are themselves a demo

    if( Authentication_IsDemoKey( client->m_authKey ) )
    {
        int numDemoTeams = GetNumDemoTeams();
        int maxDemoGameSize;
        int maxDemoPlayers;
        bool allowDemoServers;
        g_app->GetClientToServer()->GetDemoLimitations( maxDemoGameSize, maxDemoPlayers, allowDemoServers );

        if( g_app->GetClientToServer()->AmIDemoClient() )
        {
            // We (the server) are a DEMO.  Always allow 2 player demo games.
            maxDemoPlayers = max( maxDemoPlayers, 2 );
        }

        if( numDemoTeams >= maxDemoPlayers ||
            g_app->GetGame()->GetOptionValue("GameMode") != 0 )
        {
            if( numSpectators >= maxSpectators )
            {
                AppDebugOut( "Cannot allow more Demo testers to join\n" );
                client->m_basicAuthCheck = -6;
                return;
            }
            else
            {
                AppDebugOut( "Cannot allow more Demo testers to Play, client %d joins as a Spectator\n", _clientId );
                RegisterSpectator( _clientId );
            }
        }

        if( g_modSystem->IsCriticalModRunning() )
        {
            AppDebugOut( "Cannot allow Demo users to join when MODs are running\n" );
            client->m_basicAuthCheck = -6;
            return;
        }
    }


    //
    // If we are running a MOD and the client does not support MODs
    // (ie < v1.2) we must disconnect them now.

    if( !VersionManager::DoesSupportModSystem(client->m_version) &&
        g_modSystem->IsCriticalModRunning() )
    {
        AppDebugOut( "This client does not support MODs, disconnecting\n" );
        client->m_basicAuthCheck = -5;
        return;
    }


    //
    // Check for a password

    char *password = g_app->GetGame()->GetOption("ServerPassword")->m_currentString;

    if( password && 
        strlen(password) > 0 &&
        strcmp( client->m_password, password ) != 0 )
    {
        client->m_basicAuthCheck = -4;
        AppDebugOut( "Client %d failed password check\n", _clientId );
        return;
    }
    

    //
    // Everything looks good
    
    client->m_basicAuthCheck = 1;

}



int Server::GetHistoryByteSize ()
{
    int byteSize = 0;
    
    for( int i = 0; i < m_history.Size(); ++i )
    {
        ServerToClientLetter *letter = m_history[i];
        byteSize += sizeof(letter);
        byteSize += letter->m_data->GetByteSize();
    }
  
    return byteSize;
}


bool Server::GetIdentity( char *_ip, int *_port )
{
    return MatchMaker_GetIdentity( m_listener, _ip, _port );
}


bool Server::TestBedReadyToContinue()
{
    if( m_sequenceId <= 1 ) return true;

    int endtest = 7;
    int starttest = 4;
    int numFailed = 0;

    for( int i = starttest; i <= endtest; ++i )
    {
        int index = m_sequenceId - i;
        int numResponses = 0;

        for( int j = 0; j < m_clients.Size(); ++j )
        {
            if( m_clients.ValidIndex(j) )
            {
                ServerToClient *sToc = m_clients[j];
                if( sToc->m_sync.ValidIndex(index) )
                {
                    ++numResponses;
                }
            }
        }

        if( numResponses < m_clients.NumUsed() )
        {
            ++numFailed;
        }
    }

    return( numFailed <= 2 );
}


