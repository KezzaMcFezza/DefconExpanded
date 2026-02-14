#include "lib/universal_include.h"

#include "lib/netlib/net_lib.h"
#include "lib/netlib/net_udp_packet.h"
#include "lib/netlib/net_socket_listener.h"
#include "lib/netlib/net_mutex.h"
#include "lib/metaserver/matchmaker.h"
#include "lib/metaserver/metaserver_defines.h"
#include "lib/metaserver/authentication.h"
#include "lib/metaserver/metaserver.h"
#include "lib/tosser/directory.h"
#include "lib/hi_res_time.h"
#include "lib/gucci/input.h"
#include "lib/debug/debug_utils.h"
#include "lib/preferences.h"
#include "lib/string_utils.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"
#include "app/version_manager.h"

#include "network/Server.h"
#include "network/ClientToServer.h"
#include "network/network_defines.h"

#include "world/world.h"
#include "world/team.h"
#include "world/fleet.h"

// ***ListenCallback
static void ClientListenCallback(const NetUdpPacket &udpData)
{
    static int s_bytesReceived = 0;
    static float s_timer = 0.0f;
    static float s_interval = 5.0f;

    NetIpAddress fromAddr = udpData.m_clientAddress;
    char newip[16];
    IpAddressToStr( fromAddr, newip );

    Directory *letter = new Directory();
    bool success = letter->Read(udpData.m_data, udpData.m_length);

    if( success )
    {
        g_app->GetClientToServer()->ReceiveLetter( letter );
    }
    else
    {
        AppDebugOut( "Client received bogus data, discarded (1)\n" );
        delete letter;
    }

    s_bytesReceived += udpData.m_length;
    s_bytesReceived += UDP_HEADER_SIZE;

    float timeNow = GetHighResTime();
    if( timeNow > s_timer + s_interval )
    {
        g_app->GetClientToServer()->m_receiveRate = s_bytesReceived / s_interval;
        s_timer = timeNow;
        s_bytesReceived = 0;
    }
}


// ***ListenThread
void ClientToServer::ListenThread(ThreadFunctions *threadFunctions)
{   
    NetSocketListener *listener = (NetSocketListener *) threadFunctions->Argument();
    listener->StartListening( ClientListenCallback, threadFunctions );
}


ClientToServer::ClientToServer()
:   m_sendSocket(NULL),
    m_retryTimer(0.0f),
    m_clientId(-1),
    m_sendRate(0.0f),
    m_receiveRate(0.0f),
    m_serverIp(NULL),
    m_serverPort(-1),
    m_nextChatMessageId(0),
    m_chatResetTimer(0.0f),
    m_resynchronising(-1.0f),
    m_synchronising(false),
    m_connectionAttempts(0),
    m_listener(NULL)
{
    m_lastValidSequenceIdFromServer = -1;
    m_serverSequenceId = -1;
    m_gameStartSeqId = -1;
    m_connectionState = StateDisconnected;

    m_inboxMutex = new NetMutex();
    m_outboxMutex = new NetMutex();

    m_password[0] = '\x0';
    strcpy( m_serverVersion, "1.0" );
}


ClientToServer::~ClientToServer()
{
    if( m_listener ) m_listener->StopListening( &m_listenerThread );
    delete m_listener;
    m_listener = NULL;

    m_history.EmptyAndDelete();
    m_recordingSyncBytes.Empty();
    m_gameStartSeqId = -1;

    //
    // Tidy any pending fleet placement buffers

    for( int i = 0; i < m_pendingFleetPlacements.Size(); ++i )
    {
        PendingFleetPlacement *pending = m_pendingFleetPlacements[i];
        if( pending )
        {
            for( int j = 0; j < pending->m_members.Size(); ++j )
            {
                delete pending->m_members[j];
            }
        }
        delete pending;
    }
    m_pendingFleetPlacements.Empty();
}


int ClientToServer::GetLocalPort()
{
    if( !m_listener ) return -1;

#ifdef TARGET_EMSCRIPTEN

    //
    // Return fake local port since we don't actually bind

    return g_preferences->GetInt( PREFS_NETWORKCLIENTPORT );

#else

    //
    // Return the actual local port

    return m_listener->GetPort();

#endif
}


void ClientToServer::OpenConnections()
{
    if( m_listener ) m_listener->StopListening( &m_listenerThread );
    delete m_listener;
    m_listener = NULL;

#ifdef TARGET_EMSCRIPTEN

    //
    // For WebAssembly, create a fake client listener that doesn't use networking
    // This allows connection to local servers and recording playback
    
#ifdef EMSCRIPTEN_NETWORK_TESTBED
    AppDebugOut("Starting local client\n");
#endif
    
    int port = g_preferences->GetInt( PREFS_NETWORKCLIENTPORT );
    m_listener = new NetSocketListener( port );

    //
    // Don't call Bind() as it will fail in WebAssembly
    
#ifdef EMSCRIPTEN_NETWORK_TESTBED
    AppDebugOut("Local client listening on fake port %d\n", port);
#endif
    
    return;
#else

    //
    // Try the requested port number

    int port = g_preferences->GetInt( PREFS_NETWORKCLIENTPORT );
    m_listener = new NetSocketListener( port );
    NetRetCode result = m_listener->Bind();

    
    //
    // If that failed, try port 0

    if( result != NetOk && port != 0 )
    {
        AppDebugOut( "Client failed to bind to port %d\n", port );
        AppDebugOut( "Client looking for any available port number...\n" );
        delete m_listener;
        m_listener = new NetSocketListener(0);
        result = m_listener->Bind();
    }


    //
    // If it still failed, bail out
    // (we're in trouble)

    if( result != NetOk )
    {
        AppDebugOut( "ERROR : Client failed to bind to open Listener\n" );
        delete m_listener;
        m_listener = NULL;
        return;
    }


    //
    // All ok

    m_listenerThread.Start( &ClientToServer::ListenThread, m_listener );
    AppDebugOut( "Client listening on port %d\n", GetLocalPort() );
#endif
}



// *** AdvanceSender
void ClientToServer::AdvanceSender()
{
    static int s_bytesSent = 0;
    static float s_timer = 0;
    static float s_interval = 5.0f;
 
    g_app->GetClientToServer()->m_outboxMutex->Lock();
    
    while (g_app->GetClientToServer()->m_outbox.Size())
    {
        Directory *letter = g_app->GetClientToServer()->m_outbox[0];
        AppAssert(letter);
        
        letter->CreateData( NET_DEFCON_LASTSEQID, m_lastValidSequenceIdFromServer );

        if( m_connectionState > StateDisconnected )
        {
            int letterSize = 0;
            char *byteStream = letter->Write(letterSize);
            NetSocketSession *socket = g_app->GetClientToServer()->m_sendSocket;
            int writtenData = 0;
            NetRetCode result = socket->WriteData( byteStream, letterSize, &writtenData );
            
            if( result != NetOk )           AppDebugOut("CLIENT write data bad result %d\n", (int) result );
            if( writtenData != 0 &&
                writtenData < letterSize )  AppDebugOut("CLIENT wrote less data than requested\n" );
            
            s_bytesSent += letterSize;
            s_bytesSent += UDP_HEADER_SIZE;
            
            delete letter;
            delete[] byteStream;
        }
        else
        {
            delete letter;
        }
        g_app->GetClientToServer()->m_outbox.RemoveData(0);
    }

    g_app->GetClientToServer()->m_outboxMutex->Unlock();

    
    float timeNow = GetHighResTime();
    if( timeNow > s_timer + s_interval )
    {
        m_sendRate = s_bytesSent / s_interval;
        s_bytesSent = 0;
        s_timer = timeNow;
    }
}


// *** Advance
void ClientToServer::Advance()
{
    //
    // Set our basic Client properties

    char authKey[256];
    Authentication_GetKey( authKey );
    Directory clientProperties;
    clientProperties.CreateData( NET_METASERVER_AUTHKEY, authKey );
    clientProperties.CreateData( NET_METASERVER_GAMENAME, APP_NAME );
    clientProperties.CreateData( NET_METASERVER_GAMEVERSION, APP_VERSION );
    MetaServer_SetClientProperties( &clientProperties );


    //
    // If we are disconnected burn all messages in the inbox

    if( m_connectionState == StateDisconnected )
    {
        m_inboxMutex->Lock();
        m_inbox.EmptyAndDelete();
        m_inboxMutex->Unlock();
    }


    //
    // Auto-retry to connect to the server until successful

    if( m_connectionState == StateConnecting )
    {
        float timeNow = GetHighResTime();
        if( timeNow > m_retryTimer )
        {
            //
            // Attempt to hole punch

            char ourIp[256];
            int ourPort = -1;
            bool identityKnown = GetIdentity( ourIp, &ourPort );

            if( identityKnown )
            {
                char authKey[256];
                Authentication_GetKey( authKey );

                AppAssert( m_serverIp );
                Directory ourDetails;
                
                ourDetails.CreateData( NET_METASERVER_GAMENAME, APP_NAME );
                ourDetails.CreateData( NET_METASERVER_GAMEVERSION, APP_VERSION );
                ourDetails.CreateData( NET_METASERVER_AUTHKEY, authKey );
                ourDetails.CreateData( NET_METASERVER_IP, ourIp );
                ourDetails.CreateData( NET_METASERVER_PORT, ourPort );

                MatchMaker_RequestConnection( m_serverIp, m_serverPort, ourDetails );
            }


            //
            // Attempt to directly connect

            Directory *letter = new Directory();
            letter->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_CLIENT_JOIN );
            letter->CreateData( NET_METASERVER_GAMEVERSION, APP_VERSION );
            letter->CreateData( NET_DEFCON_SYSTEMTYPE, APP_SYSTEM );

            char authKey[256];
            Authentication_GetKey(authKey);
            bool isDemoKey = Authentication_IsDemoKey(authKey);

            if( g_app->GetServer() || isDemoKey )
            {
                letter->CreateData( NET_METASERVER_AUTHKEY, authKey );
            }
            else
            {
                char authKeyHash[256];
                Authentication_GetKeyHash( authKeyHash );
                int keyId = Authentication_GetKeyId(authKey);

                letter->CreateData( NET_METASERVER_AUTHKEY, authKeyHash );
                letter->CreateData( NET_METASERVER_AUTHKEYID, keyId );
            }            
            
            if( strlen(m_password) > 0 )
            {
                letter->CreateData( NET_METASERVER_PASSWORD, m_password );
            }

            SendLetter( letter );
            m_retryTimer = timeNow + 1.0f;
            m_connectionAttempts++;
        }
    }

    
    //
    // Send our desired team name 

    if( m_connectionState == StateConnected )
    {
        float timeNow = GetHighResTime();
        if( timeNow > m_retryTimer )
        {
            m_retryTimer = timeNow + 1.0f;

            if( GetEstimatedLatency() < 10.0f )
            {
                const char *constRequestedTeamName = g_preferences->GetString( "PlayerName" );
				if( g_languageTable->DoesPhraseExist( "gameoption_PlayerName" ) && 
					strcmp( constRequestedTeamName, g_languageTable->LookupBasePhrase( "gameoption_PlayerName" ) ) == 0 )
				{
					if( g_languageTable->DoesTranslationExist( "gameoption_PlayerName" ) )
					{
						constRequestedTeamName = LANGUAGEPHRASE("gameoption_PlayerName");
					}
				}
				char *requestedTeamName = newStr( constRequestedTeamName );
                SafetyString( requestedTeamName );
                ReplaceExtendedCharacters( requestedTeamName );

                int specIndex = g_app->GetWorld()->IsSpectating( m_clientId );

                if( specIndex != -1 )
                {
                    Spectator *spec = g_app->GetWorld()->m_spectators[specIndex];
                    if( strcmp(spec->m_name, requestedTeamName) != 0 )
                    {
                        RequestSetTeamName( m_clientId, requestedTeamName, true );            
                    }
                }
                else
                {
                    Team *team = g_app->GetWorld()->GetMyTeam();
                    if( team &&
                        team->m_type == Team::TypeLocalPlayer &&
                        strcmp(team->m_name, requestedTeamName) != 0 )
                    {
                        RequestSetTeamName( team->m_teamId, requestedTeamName, false );            
                    }
                }

				delete [] requestedTeamName;
            }
        }
    }



    //
    // Re-send chat messages

    if( m_connectionState == StateConnected )
    {
        if( m_chatSendQueue.Size() && GetHighResTime() > m_chatResetTimer )
        {
            ChatMessage *msg = m_chatSendQueue[0];
            SendChatMessage( msg->m_fromTeamId, 
                             msg->m_channel, 
                             msg->m_message.c_str(),
                             msg->m_messageId, 
                             msg->m_spectator );
            m_chatResetTimer = GetHighResTime() + 0.5f;
        }
    }
        

    //
    // Send resynchronise requests if it hasn't happened yet

    if( m_resynchronising > 0.0f )
    {
        float timeNow = GetHighResTime();
        if( timeNow > m_resynchronising )
        {
            AppDebugOut( "Client requesting Resynchronisation...\n" );

            Directory *letter = new Directory();
            letter->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_RESYNCHRONISE );
            SendLetter( letter );

            m_resynchronising = timeNow + 1.0f;
        }
    }


    //
    // Advance the sender

	AdvanceSender();
}


void ClientToServer::SendChatMessageReliably( unsigned char teamId, unsigned char channel, const char *msg, bool spectator )
{
    int messageId = m_nextChatMessageId;
    ++m_nextChatMessageId;


    //
    // Put the message into our send queue

    ChatMessage *message = new ChatMessage();
    message->m_fromTeamId = teamId;
    message->m_channel = channel;
    message->m_messageId = messageId;
    message->m_spectator = spectator;
    message->m_message.assign(msg);
    m_chatSendQueue.PutDataAtEnd( message );

    m_chatResetTimer = GetHighResTime() + 0.5f;
    

    //
    // Send it right now

    g_app->GetClientToServer()->SendChatMessage( teamId, channel, msg, messageId, spectator );
}


ClientToServer::PendingFleetPlacement *ClientToServer::GetPendingFleetPlacement( unsigned char teamId, int fleetId, bool createIfMissing )
{
    //
    // Look for existing buffer for this team/fleet pair

    for( int i = 0; i < m_pendingFleetPlacements.Size(); ++i )
    {
        PendingFleetPlacement *pending = m_pendingFleetPlacements[i];
        if( pending->m_teamId == teamId &&
            pending->m_fleetId == fleetId )
        {
            return pending;
        }
    }

    if( !createIfMissing )
    {
        return NULL;
    }

    //
    // Create a new empty buffer

    PendingFleetPlacement *pending = new PendingFleetPlacement();
    pending->m_teamId = teamId;
    pending->m_fleetId = fleetId;
    m_pendingFleetPlacements.PutData( pending );
    return pending;
}


void ClientToServer::FlushPendingFleetPlacement( unsigned char teamId )
{
    //
    // Destroy the first pending placement buffer for this team
    // Assume placements are queued in the same order as RequestFleet calls

    for( int i = 0; i < m_pendingFleetPlacements.Size(); ++i )
    {
        PendingFleetPlacement *pending = m_pendingFleetPlacements[i];
        if( pending->m_teamId == teamId )
        {
            for( int j = 0; j < pending->m_members.Size(); ++j )
            {
                PendingFleetMember *member = pending->m_members[j];
                RequestPlacement( teamId, member->m_unitType, member->m_longitude, member->m_latitude, pending->m_fleetId );
                delete member;
            }
            pending->m_members.Empty();

            m_pendingFleetPlacements.RemoveData(i);
            delete pending;
            return;
        }
    }
}


void ClientToServer::QueueFleetPlacement( unsigned char teamId, int fleetId, unsigned char unitType, Fixed longitude, Fixed latitude )
{
    //
    // Store placement until we see the matching RequestFleet broadcast

    PendingFleetPlacement *pending = GetPendingFleetPlacement( teamId, fleetId, true );
    PendingFleetMember *member = new PendingFleetMember();
    member->m_unitType = unitType;
    member->m_longitude = longitude;
    member->m_latitude = latitude;
    pending->m_members.PutData( member );
}


void ClientToServer::ReceiveChatMessage( unsigned char teamId, int messageId )
{
    //
    // Remove the chat message from the send queue

    for( int i = 0; i < m_chatSendQueue.Size(); ++i )
    {
        ChatMessage *thisChat = m_chatSendQueue[i];
        if( thisChat->m_fromTeamId == teamId &&
            thisChat->m_messageId == messageId )
        {
            m_chatSendQueue.RemoveData(i);
            delete thisChat;
            --i;
        }
    }
}


int ClientToServer::GetNextLetterSeqID()
{
    int result = -1;

    m_inboxMutex->Lock();
    if( m_inbox.Size() > 0 )
    {
        result = m_inbox[0]->GetDataInt( NET_DEFCON_SEQID );
    }    
    m_inboxMutex->Unlock();

    return result;
}


Directory *ClientToServer::GetNextLetter()
{
    m_inboxMutex->Lock();
    Directory *letter = NULL;
    
    if( m_inbox.Size() > 0 )
    {
        letter = m_inbox[0];
        int seqId = letter->GetDataInt( NET_DEFCON_SEQID );
        
        if( seqId == g_lastProcessedSequenceId+1 ||
            seqId == -1 )
        {
            //
            // Server stores one canonical entry per sequence, compressed empty runs
            // numEmptyUpdates must be expanded so playback matches server format.
            // Only the first letter of a run expands. Continuation letters must not 
            // append again.

            bool buildingHistory = !( g_app->GetServer() && g_app->GetServer()->IsRecordingPlaybackMode() );
            if( seqId >= 0 && buildingHistory )
            {
                if( letter->m_subDirectories.Size() == 0 &&
                    letter->HasData( NET_DEFCON_NUMEMPTYUPDATES, DIRECTORY_TYPE_INT ) )
                {
                    int numEmpty = letter->GetDataInt( NET_DEFCON_NUMEMPTYUPDATES );
                    if( numEmpty > 0 )
                    {
                        int lastSeqInHistory = -1;
                        if( m_history.Size() > 0 )
                        {
                            Directory *lastLetter = m_history.GetData( m_history.Size() - 1 );
                            if( lastLetter && lastLetter->HasData( NET_DEFCON_SEQID, DIRECTORY_TYPE_INT ) )
                            {
                                lastSeqInHistory = lastLetter->GetDataInt( NET_DEFCON_SEQID );
                            }
                        }
                        for( int k = 0; k < numEmpty; ++k )
                        {
                            int entrySeqId = seqId + k;
                            if( entrySeqId > lastSeqInHistory )
                            {
                                Directory *canonical = new Directory();
                                canonical->SetName( NET_DEFCON_MESSAGE );
                                canonical->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_UPDATE );
                                canonical->CreateData( NET_DEFCON_SEQID, entrySeqId );

                                m_history.PutDataAtEnd( canonical );
                            }
                        }
                    }
                }
                else
                {
                    //
                    // LASTSEQID "l" is added at send time, we never store it so 
                    // clienttoserver .dcrec matches the right format for playback.

                    Directory *copy = new Directory( *letter );
                    copy->RemoveData( NET_DEFCON_LASTSEQID );

                    //
                    // If we already have an entry for this seqId, merge
                    // subdirectories into it so we keep one entry per seqId like the server.

                    if( m_history.Size() > 0 )
                    {
                        Directory *lastEntry = m_history.GetData( m_history.Size() - 1 );
                        if( lastEntry && lastEntry->HasData( NET_DEFCON_SEQID, DIRECTORY_TYPE_INT ) &&
                            lastEntry->GetDataInt( NET_DEFCON_SEQID ) == seqId )
                        {
                            for( int j = 0; j < copy->m_subDirectories.Size(); ++j )
                            {
                                if( copy->m_subDirectories.ValidIndex( j ) && copy->m_subDirectories[j] )
                                {
                                    lastEntry->AddDirectory( new Directory( *copy->m_subDirectories[j] ) );
                                }
                            }

                            delete copy;
                            copy = NULL;
                        }
                    }
                    if( copy )
                    {
                        m_history.PutDataAtEnd( copy );
                    }
                }
            }

            m_inbox.RemoveData(0);
        }
        else if( seqId < g_lastProcessedSequenceId+1 )
        {
            // This is a duplicate old letter, throw it away
            m_inbox.RemoveData(0);
            letter = NULL;
        }
        else
        {
            // We're not ready to read this letter yet
            letter = NULL;
        }
    }

    m_inboxMutex->Unlock();

    return letter;
}


int ClientToServer::GetRecordingHistorySize() const
{
    return m_history.Size();
}


Directory *ClientToServer::GetRecordingHistoryLetter( int index ) const
{
    if( index < 0 || index >= m_history.Size() )
    {
        return NULL;
    }
    return m_history.GetData( index );
}


void ClientToServer::SetGameStartSeqId( int seqId )
{
    m_gameStartSeqId = seqId;
}

int ClientToServer::GetGameStartSeqId()
{   
    return m_gameStartSeqId;
}

#ifdef TARGET_EMSCRIPTEN
void ClientToServer::RouteServerMessageToClient( Directory *serverMessage )
{

    //
    // Server advance messages need to reach the client
    // so that g_lastServerAdvance gets updated properly
    
    if( !serverMessage || m_connectionState < StateHandshaking )
    {
        delete serverMessage;
        return;
    }
    
    serverMessage->SetName( NET_DEFCON_MESSAGE );
    serverMessage->CreateData( NET_DEFCON_FROMIP, "127.0.0.1" );
    serverMessage->CreateData( NET_DEFCON_FROMPORT, 5010 ); 
    
    ReceiveLetter( serverMessage );
    
#ifdef EMSCRIPTEN_NETWORK_TESTBED
    if( serverMessage->HasData(NET_DEFCON_SEQID) )
    {
        int seqId = serverMessage->GetDataInt(NET_DEFCON_SEQID);
        if( seqId >= 0 )
        {
            AppDebugOut("Successfully routed server message (seqId=%d) to client\n", seqId);
        }
    }
#endif
}
#endif


int ClientToServer::GetEstimatedServerSeqId()
{    
    float timeFactor = g_app->m_gameRunning ? 
                            SERVER_ADVANCE_PERIOD.DoubleValue() : 
                            SERVER_ADVANCE_PERIOD.DoubleValue() * 5.0f;

    float timePassed = GetHighResTime() - g_startTime;

    int estimatedSequenceId = timePassed / timeFactor;
    
    estimatedSequenceId = max( estimatedSequenceId, m_serverSequenceId );
    
    return estimatedSequenceId;
}


float ClientToServer::GetEstimatedLatency()
{
    int estimatedServerSeqId = GetEstimatedServerSeqId();

    float seqIdDifference = estimatedServerSeqId - g_lastProcessedSequenceId;

    seqIdDifference = max( seqIdDifference, 0.0f );

    float timeFactor = g_app->m_gameRunning ? 
                            SERVER_ADVANCE_PERIOD.DoubleValue() : 
                            SERVER_ADVANCE_PERIOD.DoubleValue() * 5.0f;


    float estimatedLatency = seqIdDifference * timeFactor;

    return estimatedLatency;
}


void ClientToServer::ReceiveLetter( Directory *_letter )
{
    //
    // Take ownership of the letter.

    std::unique_ptr<Directory> letter( _letter );

    //
    // Simulate network packet loss
    
#ifdef _DEBUG
    if( g_keys[KEY_F] )
    {    
        return;
    }
#endif


    //
    // Is this part of the MatchMaker service?

    if( strcmp( letter->m_name.c_str(), NET_MATCHMAKER_MESSAGE ) == 0 )
    {
        MatchMaker_ReceiveMessage( m_listener, *letter );
        return;
    }


    //
    // If we are disconnected, chuck this message in the bin

    if( m_connectionState == StateDisconnected )
    {
        return;
    }


    //
    // Ensure the message looks valid

    if( strcmp( letter->m_name.c_str(), NET_DEFCON_MESSAGE ) != 0 )
    {
        AppDebugOut( "Client received bogus data, discarded (2)\n" );
        return;
    }
    

    //
    // If there is a previous update included in this letter,
    // take it out and deal with it now

    Directory *prevUpdate = letter->GetDirectory( NET_DEFCON_PREVUPDATE );
    if( prevUpdate )
    {
        letter->RemoveDirectory( NET_DEFCON_PREVUPDATE );
        prevUpdate->SetName( NET_DEFCON_MESSAGE );
        ReceiveLetter( prevUpdate );
    }


    //
    // Record how far behind the server we are

    if( letter->HasData(NET_DEFCON_LASTSEQID, DIRECTORY_TYPE_INT) )
    {
        int serverSeqId = letter->GetDataInt( NET_DEFCON_LASTSEQID );
        m_serverSequenceId = max( m_serverSequenceId, serverSeqId );
    }

    
    //
    // If this letter is just for us put it to the front of the queue
        
    if( !letter->HasData( NET_DEFCON_SEQID, DIRECTORY_TYPE_INT ) )
    {
        AppDebugOut( "Client received bogus data, discarded (3)\n" );
        return;
    }
    

    int seqId = letter->GetDataInt( NET_DEFCON_SEQID );
    if( seqId == -1 )
    {
        m_inboxMutex->Lock();
        m_inbox.PutDataAtStart( letter.release() );
        m_inboxMutex->Unlock();
        return;
    }


    //
    // Check for duplicates
    // Throw away stuff if we're not supposed to be connected
    
    if( seqId <= m_lastValidSequenceIdFromServer ||
        m_connectionState == StateDisconnected )
    {
        return;
    }


    //
    // Work out our start time

    double newStartTime = GetHighResTime() - (float)seqId * SERVER_ADVANCE_PERIOD.DoubleValue();
    if( !g_app->m_gameRunning ) newStartTime = GetHighResTime() - (float)seqId * SERVER_ADVANCE_PERIOD.DoubleValue() * 5.0f;
    if( newStartTime < g_startTime ||
        newStartTime > g_startTime + 0.1f ) 
    {
        g_startTime = newStartTime;     
    }


    //
    // Do a sorted insert of the letter into the inbox

    m_inboxMutex->Lock();

    bool inserted = false;
    for( int i = m_inbox.Size()-1; i >= 0; --i )
    {
        Directory *thisLetter = m_inbox[i];
        int thisSeqId = thisLetter->GetDataInt(NET_DEFCON_SEQID);
        if( seqId > thisSeqId )
        {
            m_inbox.PutDataAtIndex( letter.release(), i+1 );
            inserted = true;
            break;
        }
        else if( seqId == thisSeqId )
        {
            // Throw this letter away, it's a duplicate
            inserted = true;
            break;
        }
    }
    if( !inserted )
    {
        m_inbox.PutDataAtStart( letter.release() );
    }


    //
    // Recalculate our last Known Sequence Id

    for( int i = 0; i < m_inbox.Size(); ++i )
    {
        Directory *thisLetter = m_inbox[i];
        int thisSeqId = thisLetter->GetDataInt( NET_DEFCON_SEQID );
        if( thisSeqId > m_lastValidSequenceIdFromServer+1 )
        {
            break;
        }
        m_lastValidSequenceIdFromServer = max( m_lastValidSequenceIdFromServer, thisSeqId );

        if( thisLetter->m_subDirectories.Size() == 0 &&
            thisLetter->HasData( NET_DEFCON_NUMEMPTYUPDATES, DIRECTORY_TYPE_INT ) )
        {
            int numEmptyUpdates = thisLetter->GetDataInt( NET_DEFCON_NUMEMPTYUPDATES );
            AppAssert( numEmptyUpdates != -1 );
            m_lastValidSequenceIdFromServer = max( m_lastValidSequenceIdFromServer, 
                                                   thisSeqId + numEmptyUpdates - 1 );
        }
    }

    m_inboxMutex->Unlock();
}

void ClientToServer::SendLetter( Directory *letter )
{
#ifdef TARGET_EMSCRIPTEN

    //
    // In WebAssembly local mode, route client messages directly to the server's inbox
    // instead of trying to send them over the network
    
    if( m_connectionState >= StateHandshaking && g_app->GetServer() )
    {
        letter->CreateData( NET_DEFCON_FROMIP, "127.0.0.1" );
        letter->CreateData( NET_DEFCON_FROMPORT, 5011 ); 
        letter->SetName( NET_DEFCON_MESSAGE );
        g_app->GetServer()->ReceiveLetter( letter, "127.0.0.1", 5011 );
        
#ifdef EMSCRIPTEN_NETWORK_TESTBED
        AppDebugOut("CLIENT: Routed client message (%s) directly to local server\n", 
                   letter->GetDataString(NET_DEFCON_COMMAND));
#endif
        return;  
    }
    
#endif

    if( letter )
    {
        AppAssert( letter->HasData(NET_DEFCON_COMMAND,DIRECTORY_TYPE_STRING) );

        letter->SetName( NET_DEFCON_MESSAGE );

        m_outboxMutex->Lock();
        m_outbox.PutDataAtEnd( letter );
        m_outboxMutex->Unlock();
    }
}


bool ClientToServer::IsSequenceIdInQueue( int _seqId )
{
    bool found = false;

    m_inboxMutex->Lock();
    for( int i = 0; i < m_inbox.Size(); ++i )
    {
        Directory *thisLetter = m_inbox[i];
        int thisSeqId = thisLetter->GetDataInt( NET_DEFCON_SEQID );
        if( thisSeqId == _seqId )
        {
            found = true;
            break;
        }

        if( thisLetter->m_subDirectories.Size() == 0 &&
            thisLetter->HasData( NET_DEFCON_NUMEMPTYUPDATES, DIRECTORY_TYPE_INT ) )
        {
            int numEmptyUpdates = thisLetter->GetDataInt( NET_DEFCON_NUMEMPTYUPDATES );
            if( _seqId >= thisSeqId &&
                _seqId < thisSeqId + numEmptyUpdates )
            {
                found = true;
                break;
            }
        }
    }
    m_inboxMutex->Unlock();

    return found;
}


int ClientToServer::CountPacketLoss()
{
    int packetLoss = 0;
    int lastSeqId = g_lastProcessedSequenceId;

    m_inboxMutex->Lock();
    for( int i = 0; i < m_inbox.Size(); ++i )
    {
        Directory *thisLetter = m_inbox[i];
        int thisSeqId = thisLetter->GetDataInt( NET_DEFCON_SEQID );

        if( thisSeqId > lastSeqId + 1 )
        {
            ++packetLoss;
        }

        lastSeqId = thisSeqId;
    }
    m_inboxMutex->Unlock();

    return packetLoss;
}


bool ClientToServer::ClientJoin( char *ip, int _serverPort )
{
    AppDebugOut( "CLIENT : Attempting connection to Server at %s:%d...\n", ip, _serverPort );
    
    if( !m_listener )
    {
        AppDebugOut( "CLIENT: Can't join because Listener isn't open\n" );
        return false;
    }

    //
    // Clear inbox so no stale letters from a previous session are processed

    m_inboxMutex->Lock();
    m_inbox.EmptyAndDelete();
    m_inboxMutex->Unlock();

#ifdef TARGET_EMSCRIPTEN

    //
    // For WebAssembly, fake the entire client-server handshake locally
    // following the exact same message sequence as real DEFCON
    
#ifdef EMSCRIPTEN_NETWORK_TESTBED
    AppDebugOut("CLIENT: bridging local client-server connection\n");
#endif
    
    m_serverIp = newStr(ip);
    m_serverPort = _serverPort;
    m_lastValidSequenceIdFromServer = -1;
    m_serverSequenceId = -1;
    m_connectionAttempts = 0;
    m_connectionState = StateConnecting;  
    m_clientId = -1; 
    
    //
    // Simulate the client connection message

    Directory *clientIdMsg = new Directory();
    clientIdMsg->SetName(NET_DEFCON_MESSAGE);
    clientIdMsg->CreateData(NET_DEFCON_COMMAND, NET_DEFCON_CLIENTID);
    clientIdMsg->CreateData(NET_DEFCON_CLIENTID, 1);           
    clientIdMsg->CreateData(NET_DEFCON_SEQID, -1);             
    clientIdMsg->CreateData(NET_DEFCON_VERSION, APP_VERSION);  
    
    m_inboxMutex->Lock();
    m_inbox.PutDataAtStart(clientIdMsg); 
    m_inboxMutex->Unlock();
    
#ifdef EMSCRIPTEN_NETWORK_TESTBED
    AppDebugOut("CLIENT: Queued bridged NET_DEFCON_CLIENTID message\n");
#endif
    
    //
    // Simulate the client hello message
    
    Directory *clientHelloMsg = new Directory();
    clientHelloMsg->SetName(NET_DEFCON_MESSAGE);
    clientHelloMsg->CreateData(NET_DEFCON_COMMAND, NET_DEFCON_CLIENTHELLO);
    clientHelloMsg->CreateData(NET_DEFCON_CLIENTID, 1);
    clientHelloMsg->CreateData(NET_DEFCON_SEQID, -1);  
    
    m_inboxMutex->Lock();
    m_inbox.PutDataAtEnd(clientHelloMsg); 
    m_inboxMutex->Unlock();
    
#ifdef EMSCRIPTEN_NETWORK_TESTBED
    AppDebugOut("CLIENT: Queued bridged NET_DEFCON_CLIENTHELLO message\n");
    
    AppDebugOut("CLIENT: Local connection established!\n");
#endif
    
    return true;
#else

    m_serverIp = newStr(ip);
    m_serverPort = _serverPort;
    m_lastValidSequenceIdFromServer = -1;
    m_serverSequenceId = -1;
    m_connectionAttempts = 0;

    m_sendSocket = new NetSocketSession( *m_listener, ip, _serverPort );	
    m_connectionState = StateConnecting;
    m_retryTimer = 0.0f;

    return true;
#endif
}


void ClientToServer::ClientLeave( bool _notifyServer )
{
    if( _notifyServer )
    {
        AppDebugOut( "CLIENT : Sending disconnect...\n" );

        Directory *letter = new Directory();
        letter->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_CLIENT_LEAVE );
        SendLetter( letter );

        AdvanceSender();
    }
    
    NetSleep(100);

    //
    // Mark disconnected immediately so ReceiveLetter() drops any further packets

    m_connectionState = StateDisconnected;

    delete m_sendSocket;
    m_sendSocket = NULL;

    delete[] m_serverIp;
    m_serverIp = NULL;
    m_serverPort = -1;

    m_chatSendQueue.EmptyAndDelete();
    m_nextChatMessageId = 0;
    m_chatResetTimer = 0.0f;
    
    m_clientId = -1;
    m_lastValidSequenceIdFromServer = -1;
    m_serverSequenceId = -1;
    
    m_outOfSyncClients.Empty();
    m_demoClients.Empty();

    m_inboxMutex->Lock();
    m_inbox.EmptyAndDelete();
    m_inboxMutex->Unlock();

    m_outboxMutex->Lock();
    m_outbox.EmptyAndDelete();
    m_outboxMutex->Unlock();

    m_history.EmptyAndDelete();
    m_recordingSyncBytes.Empty();
    m_gameStartSeqId = -1;

    g_lastProcessedSequenceId = -2;

    AppDebugOut( "CLIENT : Disconnected\n" );
}

void ClientToServer::RequestStartGame( unsigned char teamId, unsigned char randSeed )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_START_GAME );
    letter->CreateData( NET_DEFCON_TEAMID,      teamId );
    letter->CreateData( NET_DEFCON_RANDSEED,    randSeed );
    
    SendLetter( letter );
}

bool ClientToServer::IsConnected()
{    
    return( m_connectionState == StateHandshaking ||
            m_connectionState == StateConnected );
}

void ClientToServer::RequestTeam( int teamType )
{
    Directory *letter = new Directory();
    
    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_REQUEST_TEAM );
    letter->CreateData( NET_DEFCON_TEAMTYPE,    teamType );
    
    SendLetter( letter );
}

void ClientToServer::RequestSpectate()
{
    Directory *letter = new Directory();
    
    letter->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_REQUEST_SPECTATE );
    
    SendLetter( letter );
}

void ClientToServer::RequestRemoveAI( unsigned char teamId )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_REMOVEAI );
    letter->CreateData( NET_DEFCON_TEAMID, teamId );

    SendLetter( letter );
}

void ClientToServer::RequestAlliance( unsigned char teamId, unsigned char allianceId )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_REQUEST_ALLIANCE );
    letter->CreateData( NET_DEFCON_TEAMID,      teamId );
    letter->CreateData( NET_DEFCON_ALLIANCEID,  allianceId );

    SendLetter( letter );
}


void ClientToServer::RequestStateChange ( int objId, unsigned char state )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_OBJSTATECHANGE );
    letter->CreateData( NET_DEFCON_OBJECTID,    objId );
    letter->CreateData( NET_DEFCON_STATE,       state );

    SendLetter( letter );
}

void ClientToServer::RequestAction( int objId, int targetObjectId, Fixed longitude, Fixed latitude )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_OBJACTION );
    letter->CreateData( NET_DEFCON_OBJECTID,        objId );
    letter->CreateData( NET_DEFCON_TARGETOBJECTID,  targetObjectId );
    letter->CreateData( NET_DEFCON_LONGITUDE,       longitude );
    letter->CreateData( NET_DEFCON_LATTITUDE,       latitude );

    SendLetter( letter );
}


void ClientToServer::RequestPlacement( unsigned char teamId, unsigned char unitType, Fixed longitude, Fixed latitude, unsigned char fleetId )
{
    Directory *letter = new Directory();
    
    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_OBJPLACEMENT );
    letter->CreateData( NET_DEFCON_TEAMID,      teamId );
    letter->CreateData( NET_DEFCON_UNITTYPE,    unitType );
    letter->CreateData( NET_DEFCON_LONGITUDE,   longitude );
    letter->CreateData( NET_DEFCON_LATTITUDE,   latitude );
    letter->CreateData( NET_DEFCON_FLEETID,     fleetId );

    SendLetter( letter );
}


void ClientToServer::RequestSpecialAction( int objId, int targetObjectId, unsigned char specialActionType )
{
    Directory *letter = new Directory();
    
    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_OBJSPECIALACTION );
    letter->CreateData( NET_DEFCON_OBJECTID,        objId );
    letter->CreateData( NET_DEFCON_TARGETOBJECTID,  targetObjectId );
    letter->CreateData( NET_DEFCON_ACTIONTYPE,      specialActionType );

    SendLetter( letter );
}


void ClientToServer::RequestSetWaypoint( int objId, Fixed longitude, Fixed latitude )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_OBJSETWAYPOINT );
    letter->CreateData( NET_DEFCON_OBJECTID,    objId );
    letter->CreateData( NET_DEFCON_LONGITUDE,   longitude );
    letter->CreateData( NET_DEFCON_LATTITUDE,   latitude );

    SendLetter( letter );
}


void ClientToServer::RequestClearActionQueue( int objId )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_OBJCLEARACTIONQUEUE );
    letter->CreateData( NET_DEFCON_OBJECTID,    objId );

    SendLetter( letter );
}

void ClientToServer::RequestClearLastAction( int objId )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_OBJCLEARLASTACTION );
    letter->CreateData( NET_DEFCON_OBJECTID,    objId );

    SendLetter( letter );
}

void ClientToServer::RequestOptionChange( unsigned char optionId, int newValue )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_CHANGEOPTION );
    letter->CreateData( NET_DEFCON_OPTIONID,        optionId );
    letter->CreateData( NET_DEFCON_OPTIONVALUE,     newValue );

    SendLetter( letter );
}


void ClientToServer::RequestOptionChange( unsigned char optionId, char *newValue )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_CHANGEOPTION );
    letter->CreateData( NET_DEFCON_OPTIONID,        optionId );
    letter->CreateData( NET_DEFCON_OPTIONVALUE,     newValue );
    
    SendLetter( letter );
}


void ClientToServer::RequestGameSpeed( unsigned char teamId, unsigned char gameSpeed )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_SETGAMESPEED );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_OPTIONVALUE,     gameSpeed );

    SendLetter( letter );
}


void ClientToServer::RequestTerritory( unsigned char teamId, unsigned char territoryId, int addOrRemove )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_REQUEST_TERRITORY );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_TERRITORYID,     territoryId );
    letter->CreateData( NET_DEFCON_OPTIONVALUE,     addOrRemove );

    SendLetter( letter );
}

void ClientToServer::RequestFleet( unsigned char teamId )
{
    //
    // Create fleet immediately
    
    Team *team = g_app->GetWorld()->GetTeam( teamId );
    if( team )
    {
        team->CreateFleet();
    }
    
    //
    // Send network message to keep all clients in sync
    
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_REQUEST_FLEET );
    letter->CreateData( NET_DEFCON_TEAMID,      teamId );

    SendLetter( letter );
}

void ClientToServer::RequestFleetMovement( unsigned char teamId, int fleetId, Fixed longitude, Fixed latitude )
{    
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_FLEETMOVE );
    letter->CreateData( NET_DEFCON_TEAMID,      teamId );
    letter->CreateData( NET_DEFCON_FLEETID,     fleetId );
    letter->CreateData( NET_DEFCON_LONGITUDE,   longitude );
    letter->CreateData( NET_DEFCON_LATTITUDE,   latitude );

    SendLetter( letter );
}


void ClientToServer::RequestSetTeamName( unsigned char teamId, char *teamName, bool spectator )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,     NET_DEFCON_SETTEAMNAME );
    letter->CreateData( NET_DEFCON_TEAMID,      teamId );
    letter->CreateData( NET_DEFCON_OPTIONVALUE, teamName );

    if( spectator )
    {
        letter->CreateData( NET_DEFCON_SPECTATOR, 1 );
    }

    SendLetter(letter);
}

void ClientToServer::RequestCeaseFire( unsigned char teamId, unsigned char targetTeam )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_CEASEFIRE );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_TARGETTEAMID,    targetTeam );

    SendLetter( letter );
}

void ClientToServer::RequestShareRadar( unsigned char teamId, unsigned char targetTeam )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_SHARERADAR );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_TARGETTEAMID,    targetTeam );

    SendLetter( letter );
}

void ClientToServer::RequestAggressionChange( unsigned char teamId, unsigned char value )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_AGGRESSIONCHANGE );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_OPTIONVALUE,     value );

    SendLetter( letter );
}

void ClientToServer::SendChatMessage( unsigned char teamId, unsigned char channel, const char *msg, int messageId, bool spectator )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_CHATMESSAGE );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_CHATCHANNEL,     channel );
    letter->CreateData( NET_DEFCON_CHATMSG,         msg );
    letter->CreateData( NET_DEFCON_CHATMSGID,       messageId );

    if( spectator )
    {
        letter->CreateData( NET_DEFCON_SPECTATOR, 1 );
    }

    SendLetter(letter);
}


void ClientToServer::RequestWhiteBoard( unsigned char teamId, unsigned char action, int pointId, Fixed longitude, Fixed latitude, Fixed longitude2, Fixed latitude2 )
{
    if( VersionManager::DoesSupportWhiteBoard( m_serverVersion ) )
	{
		Directory *letter = new Directory();

		letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_WHITEBOARD );
		letter->CreateData( NET_DEFCON_TEAMID,          teamId );
		letter->CreateData( NET_DEFCON_ACTIONTYPE,      action );
		letter->CreateData( NET_DEFCON_OBJECTID,        pointId );
		letter->CreateData( NET_DEFCON_LONGITUDE,       longitude );
		letter->CreateData( NET_DEFCON_LATTITUDE,       latitude );
		letter->CreateData( NET_DEFCON_LONGITUDE2,      longitude2 );
		letter->CreateData( NET_DEFCON_LATTITUDE2,      latitude2 );

		SendLetter( letter );
	}
}


void ClientToServer::SendTeamScore( unsigned char teamId, int teamScore )
{
	if( VersionManager::DoesSupportSendTeamScore( m_serverVersion ) )
	{
		Directory *letter = new Directory();

		letter->CreateData( NET_DEFCON_COMMAND,  NET_DEFCON_TEAM_SCORE );
		letter->CreateData( NET_DEFCON_TEAMID,   teamId );
		letter->CreateData( NET_DEFCON_SCORE,    teamScore );

		SendLetter( letter );
	}
}


void ClientToServer::BeginVote( unsigned char teamId, unsigned char voteType, unsigned char voteData )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_BEGINVOTE );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_VOTETYPE,        voteType );
    letter->CreateData( NET_DEFCON_VOTEDATA,        voteData );

    SendLetter( letter );
}

void ClientToServer::CastVote( unsigned char teamId, unsigned char voteId, unsigned char myVote )
{
    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,         NET_DEFCON_CASTVOTE );
    letter->CreateData( NET_DEFCON_TEAMID,          teamId );
    letter->CreateData( NET_DEFCON_VOTETYPE,        voteId );
    letter->CreateData( NET_DEFCON_VOTEDATA,        myVote );

    SendLetter( letter );
}


void ClientToServer::SendSyncronisation( int _lastProcessedId, unsigned char _sync )
{
    if( !( g_app->GetServer() && g_app->GetServer()->IsRecordingPlaybackMode() ) )
    {
        m_recordingSyncBytes.PutData( _sync, _lastProcessedId );
    }

    Directory *letter = new Directory();

    letter->CreateData( NET_DEFCON_COMMAND,             NET_DEFCON_SYNCHRONISE );
    letter->CreateData( NET_DEFCON_LASTPROCESSEDSEQID,  _lastProcessedId );
    letter->CreateData( NET_DEFCON_SYNCVALUE,           _sync );

    SendLetter( letter );
}


void ClientToServer::SetSyncState( int _clientId, bool _synchronised )
{
    if( !_synchronised && IsSynchronised(_clientId) )
    {
        m_outOfSyncClients.PutData( _clientId );
    }

    if( _synchronised )
    {
        for( int i = 0; i < m_outOfSyncClients.Size(); ++i )
        {
            if( m_outOfSyncClients[i] == _clientId )
            {
                m_outOfSyncClients.RemoveData(i);
                --i;
            }
        }
    }
}


bool ClientToServer::IsSynchronised( int _clientId )
{
    for( int i = 0; i < m_outOfSyncClients.Size(); ++i )
    {
        if( m_outOfSyncClients[i] == _clientId )
        {
            return false;
        }
    }

    return true;
}


void ClientToServer::SetClientDemo( int _clientId )
{
    if( !IsClientDemo(_clientId) )
    {
        m_demoClients.PutData( _clientId );
    }
}


bool ClientToServer::IsClientDemo( int _clientId )
{
    for( int i = 0; i < m_demoClients.Size(); ++i )
    {
        if( m_demoClients[i] == _clientId )
        {
            return true;
        }
    }

    return false;
}


bool ClientToServer::AmIDemoClient()
{
#if defined(TARGET_EMSCRIPTEN) || defined(SYNC_PRACTICE)
    return false;
#else
    if( IsConnected() )
    {
        return IsClientDemo( m_clientId );
    }
    else
    {
        char authKey[256];
        Authentication_GetKey( authKey );
        return Authentication_IsDemoKey( authKey );
    }
#endif
}


void ClientToServer::GetDemoLimitations( int &_maxGameSize, int &_maxDemoPlayers, bool &_allowDemoServers )
{
    Directory *demoLims = MetaServer_RequestData( NET_METASERVER_DATA_DEMOLIMITS );

    if( demoLims &&
        demoLims->HasData( NET_METASERVER_MAXDEMOGAMESIZE, DIRECTORY_TYPE_INT ) &&
        demoLims->HasData( NET_METASERVER_MAXDEMOPLAYERS, DIRECTORY_TYPE_INT ) &&
        demoLims->HasData( NET_METASERVER_ALLOWDEMOSERVERS, DIRECTORY_TYPE_BOOL) )
    {
        // The MetaServer gave us these numbers
        _maxGameSize = demoLims->GetDataInt( NET_METASERVER_MAXDEMOGAMESIZE );
        _maxDemoPlayers = demoLims->GetDataInt( NET_METASERVER_MAXDEMOPLAYERS );
        _allowDemoServers = demoLims->GetDataBool( NET_METASERVER_ALLOWDEMOSERVERS );
    }
    else
    {
        // We don't have a clear answer from the MetaServer
        // So go with sensible defaults
        _maxGameSize = 2;
        _maxDemoPlayers = 2;
        _allowDemoServers = true;
    }    

    delete demoLims;
}


void ClientToServer::GetPurchasePrice( char *_prices )
{
    Directory *prices = MetaServer_RequestData( NET_METASERVER_DATA_DEMOPRICES );

    if( _prices )
    {
        strcpy( _prices, "�10     $15     �12" );

		if( prices &&
            prices->HasData( NET_METASERVER_DATA_DEMOPRICES, DIRECTORY_TYPE_STRING ) )
        {
            DirectoryData *priceData = prices->GetData( NET_METASERVER_DATA_DEMOPRICES );
            if( priceData && priceData->IsString() )
            {
                strcpy( _prices, priceData->m_string.c_str() );
            }
        }
    }

    delete prices;
}


void ClientToServer::GetPurchaseUrl( char *_purchaseUrl )
{
    Directory *purchaseUrl = MetaServer_RequestData( NET_METASERVER_DATA_BUYURL );

    if( _purchaseUrl )
    {
        strcpy( _purchaseUrl, "https://www.introversion.co.uk/defcon/purchase/" );

        if( purchaseUrl &&
            purchaseUrl->HasData( NET_METASERVER_DATA_BUYURL, DIRECTORY_TYPE_STRING ) )
        {
            DirectoryData *urlData = purchaseUrl->GetData( NET_METASERVER_DATA_BUYURL );
            if( urlData && urlData->IsString() )
            {
                strcpy( _purchaseUrl, urlData->m_string.c_str() );
            }
        }
    }

    delete purchaseUrl;
}


void ClientToServer::ProcessServerUpdates( Directory *letter )
{
    if( strcmp(letter->m_name.c_str(), NET_DEFCON_MESSAGE) != 0 ||
        !letter->HasData(NET_DEFCON_COMMAND, DIRECTORY_TYPE_STRING) )
    {
        AppDebugOut( "ClientToServer received bogus message, discarded (8)\n" );
        return;
    }

    char *cmd = letter->GetDataString(NET_DEFCON_COMMAND);    
    if(strcmp(cmd, NET_DEFCON_UPDATE) != 0 )
    {
        AppDebugOut( "ClientToServer received bogus message, discarded (9)\n" );
        return;
    }
    
    
    //
    // Special case - this may be an empty update informing us
    // of a stack of upcoming empty updates

    if( letter->m_subDirectories.Size() == 0 &&
        letter->HasData( NET_DEFCON_NUMEMPTYUPDATES, DIRECTORY_TYPE_INT ) )
    {
        int numEmptyUpdates = letter->GetDataInt( NET_DEFCON_NUMEMPTYUPDATES );
        AppAssert( numEmptyUpdates != -1 );
        
        int seqId = letter->GetDataInt( NET_DEFCON_SEQID );
        
        if( numEmptyUpdates > 1 )
        {            
            Directory *letterCopy = new Directory( *letter );
            letterCopy->CreateData( NET_DEFCON_NUMEMPTYUPDATES, numEmptyUpdates-1 );
            letterCopy->CreateData( NET_DEFCON_SEQID, seqId+1 );
            
            m_inboxMutex->Lock();
            m_inbox.PutDataAtStart( letterCopy );
            m_inboxMutex->Unlock();
        }

        return;
    }
    

    //
    // Our updates are stored in the sub-directories
    
    for( int i = 0; i < letter->m_subDirectories.Size(); ++i )
    {
        if( letter->m_subDirectories.ValidIndex(i) )
        {
            Directory *update = letter->m_subDirectories[i];
            AppAssert( strcmp(update->m_name.c_str(), NET_DEFCON_MESSAGE) == 0 );
            AppAssert( update->HasData( NET_DEFCON_COMMAND, DIRECTORY_TYPE_STRING ) );
    
            char *cmd = update->GetDataString( NET_DEFCON_COMMAND );

            if( strcmp( cmd, NET_DEFCON_START_GAME ) == 0 )
            {
                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                Team *team = g_app->GetWorld()->GetTeam( teamId );
                if( team )
                {
                    team->m_readyToStart = !team->m_readyToStart;
                    team->m_randSeed = update->GetDataUChar(NET_DEFCON_RANDSEED);
                }

				g_app->SaveGameName();
            }
            else if( strcmp( cmd, NET_DEFCON_OBJPLACEMENT ) == 0 )
            {
                g_app->GetWorld()->ObjectPlacement( update->GetDataUChar(NET_DEFCON_TEAMID), 
                                                    update->GetDataUChar(NET_DEFCON_UNITTYPE),
                                                    update->GetDataFixed(NET_DEFCON_LONGITUDE),
                                                    update->GetDataFixed(NET_DEFCON_LATTITUDE),
                                                    update->GetDataUChar(NET_DEFCON_FLEETID) );
            }
            else if( strcmp( cmd, NET_DEFCON_OBJSTATECHANGE ) == 0 )
            {
                g_app->GetWorld()->ObjectStateChange( update->GetDataInt(NET_DEFCON_OBJECTID), 
                                                      update->GetDataUChar(NET_DEFCON_STATE) );

            }
            else if( strcmp( cmd, NET_DEFCON_OBJACTION ) == 0 )
            {
                g_app->GetWorld()->ObjectAction( update->GetDataInt(NET_DEFCON_OBJECTID), 
                                                 update->GetDataInt(NET_DEFCON_TARGETOBJECTID), 
                                                 update->GetDataFixed(NET_DEFCON_LONGITUDE),
                                                 update->GetDataFixed(NET_DEFCON_LATTITUDE), 
                                                 true );
            }
            else if( strcmp( cmd, NET_DEFCON_OBJSPECIALACTION ) == 0 )
            {
                g_app->GetWorld()->ObjectSpecialAction( update->GetDataInt(NET_DEFCON_OBJECTID),
                                                        update->GetDataInt(NET_DEFCON_TARGETOBJECTID),
                                                        update->GetDataUChar(NET_DEFCON_ACTIONTYPE) );
            }
            else if( strcmp( cmd, NET_DEFCON_OBJCLEARACTIONQUEUE ) == 0 )
            {
                g_app->GetWorld()->ObjectClearActionQueue( update->GetDataInt(NET_DEFCON_OBJECTID) );
            }
            else if( strcmp( cmd, NET_DEFCON_OBJCLEARLASTACTION ) == 0 )
            {
                g_app->GetWorld()->ObjectClearLastAction( update->GetDataInt(NET_DEFCON_OBJECTID) );
            }
            else if( strcmp( cmd, NET_DEFCON_OBJSETWAYPOINT ) == 0 )
            {
                g_app->GetWorld()->ObjectSetWaypoint( update->GetDataInt(NET_DEFCON_OBJECTID), 
                                                      update->GetDataFixed(NET_DEFCON_LONGITUDE), 
                                                      update->GetDataFixed(NET_DEFCON_LATTITUDE) );
            }                
            else if( strcmp( cmd, NET_DEFCON_REQUEST_TERRITORY ) == 0 )
            {
                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                g_app->GetWorld()->AssignTerritory( update->GetDataUChar(NET_DEFCON_TERRITORYID), 
                                                    update->GetDataUChar(NET_DEFCON_TEAMID),
                                                    update->GetDataInt(NET_DEFCON_OPTIONVALUE) );
                Team *team = g_app->GetWorld()->GetTeam( teamId );
                if( team )
                {
                    team->m_readyToStart = false;
                }
            }
            else if( strcmp( cmd, NET_DEFCON_REQUEST_ALLIANCE ) == 0 )
            {
                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                g_app->GetWorld()->RequestAlliance( teamId,
                                                    update->GetDataUChar(NET_DEFCON_ALLIANCEID) );
                Team *team = g_app->GetWorld()->GetTeam( teamId );
                if( team )
                {
                    team->m_readyToStart = false;
                }
            }
            else if( strcmp( cmd, NET_DEFCON_CHANGEOPTION ) == 0 )
            {
                int optionIndex = (int)update->GetDataUChar( NET_DEFCON_OPTIONID );
                GameOption *option = g_app->GetGame()->m_options[optionIndex];
                AppAssert(option);
        
                if( update->HasData( NET_DEFCON_OPTIONVALUE, DIRECTORY_TYPE_INT ) )
                {
                    option->m_currentValue = update->GetDataInt(NET_DEFCON_OPTIONVALUE);
                }
                else if( update->HasData( NET_DEFCON_OPTIONVALUE, DIRECTORY_TYPE_STRING ) )
                {
                    strcpy( option->m_currentString, update->GetDataString(NET_DEFCON_OPTIONVALUE) );
                }
        
                if( strcmp(option->m_name, "GameMode" ) == 0 )
                {
                    g_app->GetGame()->SetGameMode( option->m_currentValue );
                }

                for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
                {
                    g_app->GetWorld()->m_teams[i]->m_readyToStart = false;
                }
            }
            else if( strcmp( cmd, NET_DEFCON_SETTEAMNAME ) == 0 )
            {
                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                char *teamName = update->GetDataString(NET_DEFCON_OPTIONVALUE);
				char defaultName[256];

				// If player attempts to set invalid name, use a default instead
				if( !Team::IsValidName( teamName ) )
				{
					if( update->HasData( NET_DEFCON_SPECTATOR ) )
					{
						strcpy( defaultName, LANGUAGEPHRASE("dialog_c2s_default_name_spectator") );
					}
					else
					{
						strcpy( defaultName, LANGUAGEPHRASE("dialog_c2s_default_name_player") );
						LPREPLACEINTEGERFLAG( 'T', teamId, defaultName );
					}

					teamName = defaultName;
				}

                if( update->HasData(NET_DEFCON_SPECTATOR) )
                {
                    // Spectator renaming himself
                    Spectator *spec = g_app->GetWorld()->GetSpectator(teamId);
                    
                    if( spec && strcmp( spec->m_name, teamName ) != 0 )
                    {
                        char msg[256];
                        strcpy( msg, LANGUAGEPHRASE("dialog_c2s_team_renamed") );
						LPREPLACESTRINGFLAG( 'T', spec->m_name, msg );
						LPREPLACESTRINGFLAG( 'N', teamName, msg );
                        g_app->GetWorld()->AddChatMessage( -1, CHATCHANNEL_PUBLIC, msg, -1, false );

                        strcpy( spec->m_name, teamName );        

                        bool isRecordingPlayback = (g_app->GetServer() && g_app->GetServer()->IsRecordingPlaybackMode());
                        if( teamId == m_clientId && !isRecordingPlayback )
                        {
                            if( strcmp( teamName, LANGUAGEPHRASE("gameoption_PlayerName") ) == 0 )
                            {
                                g_preferences->SetString( "PlayerName", g_languageTable->LookupBasePhrase( "gameoption_PlayerName" ) );
                            }
                            else
                            {
                                g_preferences->SetString( "PlayerName", teamName );
                            }
                            g_preferences->Save();
                        }
                    }
                }
                else
                {
                    // Team renaming itself
                    Team *team = g_app->GetWorld()->GetTeam( teamId );
                    
                    if( team &&
                        strcmp( team->m_name, teamName ) != 0 )
                    {
                        char msg[256];
                        if( team->m_nameSet )
                        {
                            strcpy( msg, LANGUAGEPHRASE("dialog_c2s_team_renamed") );
							LPREPLACESTRINGFLAG( 'T', team->m_name, msg );
							LPREPLACESTRINGFLAG( 'N', teamName, msg );
                        }
                        else
                        {
                            strcpy( msg, LANGUAGEPHRASE("dialog_c2s_team_joined_game") );
							LPREPLACESTRINGFLAG( 'T', teamName, msg );
                            team->m_nameSet = true;
                        }
                        g_app->GetWorld()->AddChatMessage( team->m_teamId, CHATCHANNEL_PUBLIC, msg, -1, false );

                        team->SetTeamName( teamName );

                        bool isRecordingPlayback = (g_app->GetServer() && g_app->GetServer()->IsRecordingPlaybackMode());
                        if( teamId == g_app->GetWorld()->m_myTeamId && !isRecordingPlayback )
                        {
                            if( strcmp( teamName, LANGUAGEPHRASE("gameoption_PlayerName") ) == 0 )
                            {
                                g_preferences->SetString( "PlayerName", g_languageTable->LookupBasePhrase( "gameoption_PlayerName" ) );
                            }
                            else
                            {
                                g_preferences->SetString( "PlayerName", teamName );
                            }
                            g_preferences->Save();
                        }
                    }
                }
            }
            else if( strcmp( cmd, NET_DEFCON_SETGAMESPEED ) == 0 )
            {
                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                Team *team = g_app->GetWorld()->GetTeam( teamId );                
                if( team )
                {
                    team->m_desiredGameSpeed = update->GetDataUChar(NET_DEFCON_OPTIONVALUE);
                }
            }
            else if( strcmp( cmd, NET_DEFCON_REQUEST_FLEET ) == 0 )
            {
                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                Team *team = g_app->GetWorld()->GetTeam( teamId );
                AppAssert(team);
                
                //
                // Only skip creation if we actually OWN this team (not just viewing it).
                // In player perspective mode m_myTeamId changes to the viewed team,
                // so we must verify ownership via team type and client ID.
                
                bool isRecordingPlayback = (g_app->GetServer() && g_app->GetServer()->IsRecordingPlaybackMode());
                bool isOurOwnTeam = !isRecordingPlayback && 
                                    (team->m_type == Team::TypeLocalPlayer && 
                                     team->m_clientId == m_clientId);
                
                bool needsCreation = true;
                if( isOurOwnTeam && 
                    team->m_fleets.Size() > 0 &&
                    !m_synchronising )
                {
                    //
                    // This is an ACK for our own RequestFleet call
                    // Flush any queued placements before we bail out

                    FlushPendingFleetPlacement( teamId );

                    Fleet *lastFleet = NULL;
                    if( team->m_fleets.Size() > 0 )
                    {
                        lastFleet = team->m_fleets[ team->m_fleets.Size() - 1 ];
                    }

                    if( lastFleet && !lastFleet->m_active )
                    {
                        // This is our team and last fleet is a template, we created it locally
                        needsCreation = false;
                    }
                }
                
                if( needsCreation )
                {
                    team->CreateFleet();
                }
            }
            else if( strcmp( cmd, NET_DEFCON_FLEETMOVE ) == 0 )
            {
                g_app->GetWorld()->FleetSetWaypoint( update->GetDataUChar(NET_DEFCON_TEAMID), 
                                                     update->GetDataInt(NET_DEFCON_FLEETID),
                                                     update->GetDataFixed(NET_DEFCON_LONGITUDE),
                                                     update->GetDataFixed(NET_DEFCON_LATTITUDE) );
            }
            else if( strcmp( cmd, NET_DEFCON_CHATMESSAGE ) == 0 )
            {
                bool spectator = update->HasData(NET_DEFCON_SPECTATOR, DIRECTORY_TYPE_INT );

                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                int messageId = update->GetDataInt(NET_DEFCON_CHATMSGID);

                ReceiveChatMessage( teamId, messageId );
                g_app->GetWorld()->AddChatMessage( teamId, 
                                                   update->GetDataUChar(NET_DEFCON_CHATCHANNEL), 
                                                   update->GetDataString(NET_DEFCON_CHATMSG),
                                                   messageId,
                                                   spectator );
            }
            else if( strcmp( cmd, NET_DEFCON_BEGINVOTE ) == 0 )
            {
                g_app->GetWorld()->m_votingSystem.RegisterNewVote( update->GetDataUChar(NET_DEFCON_TEAMID),
                                                                   update->GetDataUChar(NET_DEFCON_VOTETYPE),
                                                                   update->GetDataUChar(NET_DEFCON_VOTEDATA) );
            }
            else if( strcmp( cmd, NET_DEFCON_CASTVOTE ) == 0 )
            {
                g_app->GetWorld()->m_votingSystem.CastVote( update->GetDataUChar(NET_DEFCON_TEAMID),
                                                            update->GetDataUChar(NET_DEFCON_VOTETYPE),
                                                            update->GetDataUChar(NET_DEFCON_VOTEDATA) );
            }
            else if( strcmp( cmd, NET_DEFCON_CEASEFIRE ) == 0 )
            {
                g_app->GetWorld()->TeamCeaseFire( update->GetDataUChar(NET_DEFCON_TEAMID), 
                                                  update->GetDataUChar(NET_DEFCON_TARGETTEAMID) );
            }
            else if( strcmp( cmd, NET_DEFCON_SHARERADAR ) == 0 )
            {
                g_app->GetWorld()->TeamShareRadar( update->GetDataUChar(NET_DEFCON_TEAMID),
                                                   update->GetDataUChar(NET_DEFCON_TARGETTEAMID) );
            }
            else if( strcmp( cmd, NET_DEFCON_AGGRESSIONCHANGE ) == 0 )
            {
                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                Team *team = g_app->GetWorld()->GetTeam( teamId );
                AppAssert(team);
                team->m_aggression = update->GetDataUChar(NET_DEFCON_OPTIONVALUE);               
            }
            else if( strcmp( cmd, NET_DEFCON_REMOVEAI ) == 0 )
            {
                int teamId = update->GetDataUChar(NET_DEFCON_TEAMID);
                g_app->GetWorld()->RemoveAITeam(teamId);
            }
			else if( strcmp( cmd, NET_DEFCON_WHITEBOARD ) == 0 )
			{
				g_app->GetWorld()->WhiteBoardAction( update->GetDataUChar(NET_DEFCON_TEAMID),
                                                     update->GetDataUChar(NET_DEFCON_ACTIONTYPE),
                                                     update->GetDataInt(NET_DEFCON_OBJECTID),
                                                     update->GetDataFixed(NET_DEFCON_LONGITUDE),
                                                     update->GetDataFixed(NET_DEFCON_LATTITUDE),
                                                     update->GetDataFixed(NET_DEFCON_LONGITUDE2),
                                                     update->GetDataFixed(NET_DEFCON_LATTITUDE2) );
			}
			else if( strcmp( cmd, NET_DEFCON_TEAM_SCORE ) == 0 )
			{
				//do nothing
			}
			else
            {
                AppDebugOut( "ClientToServer received bogus message, discarded (11)\n" );
            }
        }
    }
}


void ClientToServer::Resynchronise()
{
    m_lastValidSequenceIdFromServer = -1;
    m_serverSequenceId = -1;
    //m_clientId = -1;

    m_resynchronising = GetHighResTime();

    m_history.EmptyAndDelete();
    m_recordingSyncBytes.Empty();
    m_gameStartSeqId = -1;

    //
    // Drop any queued placements
    // Trust the server to resend world state

    while( m_pendingFleetPlacements.Size() > 0 )
    {
        PendingFleetPlacement *pending = m_pendingFleetPlacements[0];
        for( int i = 0; i < pending->m_members.Size(); ++i )
        {
            delete pending->m_members[i];
        }
        m_pendingFleetPlacements.RemoveData(0);
        delete pending;
    }
}


void ClientToServer::StartIdentifying()
{
    MatchMaker_StartRequestingIdentity( m_listener );
}


void ClientToServer::StopIdentifying()
{
    MatchMaker_StopRequestingIdentity( m_listener );
}


bool ClientToServer::GetIdentity( char *_ip, int *_port )
{
    return MatchMaker_GetIdentity( m_listener, _ip, _port );
}


bool ClientToServer::GetServerDetails( char *_ip, int *_port )
{
    if( !m_serverIp )
    {
        return false;
    }

    strcpy( _ip, m_serverIp );
    *_port = m_serverPort;
    return true;
}


