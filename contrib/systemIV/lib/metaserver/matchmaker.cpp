#include "lib/universal_include.h"
#include "lib/tosser/darray.h"
#include "lib/tosser/directory.h"
#include "lib/netlib/net_lib.h"
#include "lib/debug_utils.h"
#include "lib/hi_res_time.h"
#include "lib/string_utils.h"

#include "matchmaker.h"
#include "metaserver.h"
#include "metaserver_defines.h"


// ============================================================================
// Static data for MatchMaker

static char     *s_matchMakerIp = NULL;
static int      s_matchMakerPort = -1;

static NetMutex s_uniqueRequestMutex;
static int      s_uniqueRequestid = 0;


struct MatchMakerListener
{
public:
    MatchMakerListener()
        :   m_listener(NULL),
            m_ip(NULL),
            m_port(-1),
            m_uniqueId(-1),
            m_identified(false),
            m_shutDown(false)
    {
    }

    ~MatchMakerListener()
    {
        delete[] m_ip;
    }

    NetSocketListener   *m_listener;
    char                *m_ip;
    int                 m_uniqueId;
    int                 m_port;
    bool                m_identified;
    bool                m_shutDown;
};

static DArray<MatchMakerListener *> s_listeners;
static NetMutex                     s_listenersMutex;


// ============================================================================


void MatchMaker_LocateService( char *_matchMakerIp, int _port )
{
    if( s_matchMakerIp )
    {
        delete[] s_matchMakerIp;
        s_matchMakerIp = NULL;
    }

    if( _matchMakerIp ) 
    {
        s_matchMakerIp = newStr(_matchMakerIp);
        s_matchMakerPort = _port;
    }
}

static int GetListenerIndex( NetSocketListener *_listener, NetMutex *lock )
{
   int index = -1;

    for( int i = 0; i < s_listeners.Size(); ++i )
    {
        if( s_listeners.ValidIndex(i) )
        {
            MatchMakerListener *listener = s_listeners[i];
            if( listener->m_listener == _listener )
            {
                index = i;
            }
        }
    }

    return index;
}


static MatchMakerListener *GetListener( NetSocketListener *_listener, NetMutex *lock )
{
    int listenerIndex = GetListenerIndex(_listener, lock);
    if( listenerIndex == -1 ) return NULL;
    
    return s_listeners[listenerIndex];
}


static NetCallBackRetType RequestIdentityThread(void *ignored)
{
#ifdef WAN_PLAY_ENABLED
    NetSocketListener *listener = (NetSocketListener *) ignored;
    
    s_listenersMutex.Lock();
    int listenerIndex = GetListenerIndex(listener, &s_listenersMutex);
    AppAssert( listenerIndex > -1 );
    
    MatchMakerListener *_listener = s_listeners[listenerIndex];
    AppAssert(_listener);
    s_listenersMutex.Unlock();


    //
    // Generate a uniqueID for this request
    // So we can tell the replies apart

    s_uniqueRequestMutex.Lock();
    int uniqueId = s_uniqueRequestid;
    ++s_uniqueRequestid;
    s_uniqueRequestMutex.Unlock();

    _listener->m_uniqueId = uniqueId;


    //
    // Build our request and convert to a byte stream
    // (only need to do this once and keep re-sending)

    Directory request;
    request.SetName( NET_MATCHMAKER_MESSAGE );
    request.CreateData( NET_METASERVER_COMMAND, NET_MATCHMAKER_REQUEST_IDENTITY );
    request.CreateData( NET_MATCHMAKER_UNIQUEID, uniqueId );
    

    //
    // Open a connection to the MatchMaker service
    // Start sending requests for our ID every few seconds
    // to ensure the connection stays open in the NAT 

    NetSocketSession socket( *listener, s_matchMakerIp, s_matchMakerPort );


    while( true )
    {
        //
        // Stop if We've been asked to 

        if( _listener->m_shutDown )
        {
            break;
        }

        //
        // Update the request with the latest auth data
        
        Directory *clientProps = MetaServer_GetClientProperties();
        request.CreateData( NET_METASERVER_GAMENAME,    clientProps->GetDataString(NET_METASERVER_GAMENAME) );
        request.CreateData( NET_METASERVER_GAMEVERSION, clientProps->GetDataString(NET_METASERVER_GAMEVERSION) );
        request.CreateData( NET_METASERVER_AUTHKEY,     clientProps->GetDataString(NET_METASERVER_AUTHKEY) );
        delete clientProps;

        int requestSize = 0;
        char *requestByteStream = request.Write(requestSize);


        //
        // Send the request

        int numBytesWritten = 0;
        NetRetCode result = socket.WriteData( requestByteStream, requestSize, &numBytesWritten );
        delete [] requestByteStream;

        if( result != NetOk || numBytesWritten != requestSize )
        {
            AppDebugOut( "MatchMaker encountered error sending data\n" );
            break;
        }

        NetSleep( PERIOD_MATCHMAKER_REQUESTID );
    }


    //
    // Shut down the request

    delete _listener;

#endif

    return 0;
}


static bool MatchMaker_IsRequestingIdentity( NetSocketListener *_listener, NetMutex *lock )
{
    MatchMakerListener *listener = GetListener(_listener, lock);

    if( !listener ) return false;
    if( listener->m_shutDown ) return false;
    return true;
}


bool MatchMaker_IsRequestingIdentity( NetSocketListener *_listener )
{
    s_listenersMutex.Lock();
    bool result = MatchMaker_IsRequestingIdentity( _listener, &s_listenersMutex );
    s_listenersMutex.Unlock();

    return result;
}


void MatchMaker_StartRequestingIdentity( NetSocketListener *_listener )
{
#ifdef TARGET_EMSCRIPTEN
    // ================================================
    // WEBASSEMBLY LOCAL MATCHMAKER BYPASS
    // ================================================
    // For WebAssembly, we don't need real matchmaker functionality
    // since we're running in local mode. Set a fake matchmaker IP
    // to satisfy the assertion and then return immediately.
    
#ifdef EMSCRIPTEN_NETWORK_TESTBED
    AppDebugOut("WebAssembly: Bypassing matchmaker (local mode)\\n");
#endif
    
    if( !s_matchMakerIp )
    {
        // Set a fake matchmaker IP to satisfy the assertion
        s_matchMakerIp = newStr("127.0.0.1");
        s_matchMakerPort = 4000;
#ifdef EMSCRIPTEN_NETWORK_TESTBED
        AppDebugOut("WebAssembly: Set fake matchmaker IP: %s:%d\\n", s_matchMakerIp, s_matchMakerPort);
#endif
    }
    
    // Don't actually start requesting identity - return immediately
    return;
#else
    AppAssert( s_matchMakerIp );

    s_listenersMutex.Lock();
    if( _listener &&
        !MatchMaker_IsRequestingIdentity(_listener, &s_listenersMutex) )
    {
        AppDebugOut( "Started requesting public IP:port for socket %d\\n", _listener->GetBoundSocketHandle() );

        MatchMakerListener *listener = new MatchMakerListener();
        listener->m_listener = _listener;

        int index = s_listeners.PutData(listener);

        NetStartThread( RequestIdentityThread, _listener );
    }
    s_listenersMutex.Unlock();
#endif
}


static void MatchMaker_StopRequestingIdentity( NetSocketListener *_listener, NetMutex *lock )
{
    int index = GetListenerIndex(_listener, lock);
    if( !s_listeners.ValidIndex( index ) ) return;

    AppDebugOut( "Stopped requesting public IP:port for socket %d\n", _listener->GetBoundSocketHandle() );

    MatchMakerListener *listener = s_listeners[index];
    listener->m_shutDown = true;
    s_listeners.RemoveData( index );
}


void MatchMaker_StopRequestingIdentity( NetSocketListener *_listener )
{
    s_listenersMutex.Lock();
    MatchMaker_StopRequestingIdentity( _listener, &s_listenersMutex );
    s_listenersMutex.Unlock();
}


static bool MatchMaker_IsIdentityKnown( NetSocketListener *_listener, NetMutex *lock )
{
    MatchMakerListener *listener = GetListener(_listener, lock);

    if( !listener ) return false;

    return( listener->m_identified );    
}


bool MatchMaker_IsIdentityKnown( NetSocketListener *_listener )
{
    s_listenersMutex.Lock();
    bool result = MatchMaker_IsIdentityKnown( _listener, &s_listenersMutex );
    s_listenersMutex.Unlock();
    return result;
}


static bool MatchMaker_GetIdentity( NetSocketListener *_listener, char *_ip, int *_port, NetMutex *lock )
{
    MatchMakerListener *listener = GetListener(_listener, lock);

    if( !listener ) return false;

    if( !listener->m_identified ) return false;

    strcpy( _ip, listener->m_ip );
    *_port = listener->m_port;

    return true;
}


bool MatchMaker_GetIdentity( NetSocketListener *_listener, char *_ip, int *_port )
{
    s_listenersMutex.Lock();
    bool result = MatchMaker_GetIdentity( _listener, _ip, _port, &s_listenersMutex );
    s_listenersMutex.Unlock();
    return result;
}


void MatchMaker_RequestConnection( char *_targetIp, int _targetPort, const Directory &_myDetails )
{
#ifdef WAN_PLAY_ENABLED
    AppDebugOut( "Requesting connection to %s:%d via matchmaker\n", _targetIp, _targetPort );

    Directory request( _myDetails );
    request.SetName     ( NET_MATCHMAKER_MESSAGE );
    request.CreateData  ( NET_METASERVER_COMMAND, NET_MATCHMAKER_REQUEST_CONNECT );
    request.CreateData  ( NET_MATCHMAKER_TARGETIP, _targetIp );
    request.CreateData  ( NET_MATCHMAKER_TARGETPORT, _targetPort );

    AppAssert( s_matchMakerIp );

    NetSocket socket;   
    NetRetCode result = socket.Connect( s_matchMakerIp, s_matchMakerPort );
    AppAssert( result == NetOk );

    MetaServer_SendDirectory( &request, &socket );
#endif
}


bool MatchMaker_ReceiveMessage( NetSocketListener *_listener, const Directory &_message )
{
    AppAssert( strcmp(_message.m_name, NET_MATCHMAKER_MESSAGE) == 0 );
    AppAssert( _message.HasData( NET_METASERVER_COMMAND, DIRECTORY_TYPE_STRING ) );

    char *cmd = _message.GetDataString( NET_METASERVER_COMMAND );

    if( strcmp( cmd, NET_MATCHMAKER_IDENTIFY ) == 0 )
    {
        //
        // This message contains the external IP and port of one of our connections

        if( !_message.HasData( NET_MATCHMAKER_UNIQUEID, DIRECTORY_TYPE_INT ) ||
            !_message.HasData( NET_MATCHMAKER_YOURIP, DIRECTORY_TYPE_STRING ) ||
            !_message.HasData( NET_MATCHMAKER_YOURPORT, DIRECTORY_TYPE_INT ) )
        {
            AppDebugOut( "MatchMaker : Received badly formed identity message, discarded\n" );
        }
        else
        {
            int uniqueId = _message.GetDataInt( NET_MATCHMAKER_UNIQUEID );
            
            s_listenersMutex.Lock();
            for( int i = 0; i < s_listeners.Size(); ++i )
            {
                if( s_listeners.ValidIndex(i) )
                {
                    MatchMakerListener *listener = s_listeners[i];
                    if( listener->m_uniqueId == uniqueId )
                    {
                        if( !listener->m_identified )
                        {                        
                            listener->m_ip = newStr( _message.GetDataString(NET_MATCHMAKER_YOURIP) );
                            listener->m_port = _message.GetDataInt(NET_MATCHMAKER_YOURPORT);
                            listener->m_identified = true;
                            AppDebugOut( "Socket %d identified as public IP %s:%d\n", 
                                            listener->m_listener->GetBoundSocketHandle(), listener->m_ip, listener->m_port );
                        }
                        break;
                    }
                }
            }
            s_listenersMutex.Unlock();
        }
    }
    else if( strcmp( cmd, NET_MATCHMAKER_REQUEST_CONNECT ) == 0 )
    {
        //
        // This is a request from a client for the server to set up a UDP hole punch
        
        if( !_message.HasData( NET_METASERVER_IP, DIRECTORY_TYPE_STRING ) ||
            !_message.HasData( NET_METASERVER_PORT, DIRECTORY_TYPE_INT ) )
        {
            AppDebugOut( "MatchMaker : Received badly formed connection request, discarded\n" );
        }
        else
        {
            char *ip = _message.GetDataString( NET_METASERVER_IP );
            int port = _message.GetDataInt( NET_METASERVER_PORT );

            AppDebugOut( "MatchMaker : SERVER Received request to allow client to join from %s:%d\n", ip, port );

            Directory dir;
            dir.SetName( NET_MATCHMAKER_MESSAGE );
            dir.CreateData( NET_METASERVER_COMMAND, NET_MATCHMAKER_RECEIVED_CONNECT );

            NetSocketSession session( *_listener, ip, port );            
            MetaServer_SendDirectory( &dir, &session );
        }
    }
    else if( strcmp( cmd, NET_MATCHMAKER_RECEIVED_CONNECT ) == 0 )
    {
        AppDebugOut( "MatchMaker : CLIENT received confirmation from Server that Hole Punch is set up\n" );
    }
    else
    {
        AppAbort( "unrecognised matchmaker message" );
    }

    return true;
}


