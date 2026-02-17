#include "systemiv.h"

#ifdef TARGET_EMSCRIPTEN

#include "net_lib.h"
#include "net_socket_listener.h"
#include "net_udp_packet.h"

//
// WASM is single threaded, so conditions are no ops

#ifdef OFFLINE_MODE
//
// In process loopback so client and server can talk without real UDP.

#define NETLOCAL_MAX_LISTENERS  4
#define NETLOCAL_MAX_PACKETS   256

struct NetLocalListener {
	unsigned short port;
	NetCallBack callback;
	int inUse;
};

struct NetLocalPacket {
	unsigned short fromPort;
	NetIpAddress toAddr;
	char *data;
	int len;
	int inUse;
};

static struct NetLocalListener s_listeners[NETLOCAL_MAX_LISTENERS];
static struct NetLocalPacket s_packets[NETLOCAL_MAX_PACKETS];
static int s_listenersInit;

static void netlocal_init( void )
{
	if ( s_listenersInit ) return;
	s_listenersInit = 1;
	for ( int i = 0; i < NETLOCAL_MAX_LISTENERS; i++ ) s_listeners[i].inUse = 0;
	for ( int i = 0; i < NETLOCAL_MAX_PACKETS; i++ )  s_packets[i].inUse = 0;
}

void NetLocal_RegisterListener( unsigned short port, NetCallBack callback )
{
	netlocal_init();
	for ( int i = 0; i < NETLOCAL_MAX_LISTENERS; i++ ) {
		if ( !s_listeners[i].inUse ) {
			s_listeners[i].port = port;
			s_listeners[i].callback = callback;
			s_listeners[i].inUse = 1;
			return;
		}
	}
}

void NetLocal_UnregisterListener( unsigned short port )
{
	for ( int i = 0; i < NETLOCAL_MAX_LISTENERS; i++ ) {
		if ( s_listeners[i].inUse && s_listeners[i].port == port ) {
			s_listeners[i].inUse = 0;
			return;
		}
	}
}

void NetLocal_QueuePacket( unsigned short fromPort, const NetIpAddress *toAddr, const void *data, int len )
{
	if ( len <= 0 || len > MAX_PACKET_SIZE ) return;
	netlocal_init();
	for ( int i = 0; i < NETLOCAL_MAX_PACKETS; i++ ) {
		if ( !s_packets[i].inUse ) {
			s_packets[i].fromPort = fromPort;
			s_packets[i].toAddr = *toAddr;
			s_packets[i].data = (char *)malloc( (size_t)len );
			if ( !s_packets[i].data ) return;
			memcpy( s_packets[i].data, data, (size_t)len );
			s_packets[i].len = len;
			s_packets[i].inUse = 1;
			return;
		}
	}
}
#endif // OFFLINE_MODE

NetSocketListener::NetSocketListener( unsigned short _port )
{
	m_port = _port;
	m_sockfd = -1;
	m_binding = false;
	m_listening = false;
	m_thread = NULL;
}


NetSocketListener::~NetSocketListener()
{
#ifdef OFFLINE_MODE
	NetLocal_UnregisterListener( (unsigned short)m_port );
#endif
}


NetRetCode NetSocketListener::StartListening( NetCallBack _callback, ThreadFunctions *_threadFunctions )
{
#ifdef OFFLINE_MODE
	NetLocal_RegisterListener( (unsigned short)m_port, _callback );
	return NetOk;
#else
	return NetFailed;
#endif
}


void NetSocketListener::StopListening( ThreadController *_threadToStop )
{
#ifdef OFFLINE_MODE
	NetLocal_UnregisterListener( (unsigned short)m_port );
#endif
}


NetSocketHandle NetSocketListener::GetBoundSocketHandle()
{
	return m_sockfd;
}


NetRetCode NetSocketListener::Bind()
{
#ifdef OFFLINE_MODE
	return NetOk;
#else
	return NetFailed;
#endif
}


NetRetCode NetSocketListener::EnableBroadcast()
{
	return NetOk;
}


int NetSocketListener::GetPort() const
{
	return m_port;
}


NetIpAddress NetSocketListener::GetListenAddress() const
{
	NetIpAddress addr;
	addr.sin_family = 0;
	addr.sin_port = m_port;
	addr.sin_addr = 0x7F000001; // 127.0.0.1
	return addr;
}


void NetSocketListener::PumpLocalPackets()
{
#ifdef OFFLINE_MODE
	if ( !s_listenersInit ) return;
	for ( int i = 0; i < NETLOCAL_MAX_PACKETS; i++ ) {
		if ( !s_packets[i].inUse ) continue;
		unsigned short toPort = (unsigned short)s_packets[i].toAddr.sin_port;
		NetCallBack callback = NULL;
		for ( int j = 0; j < NETLOCAL_MAX_LISTENERS; j++ ) {
			if ( s_listeners[j].inUse && s_listeners[j].port == toPort ) {
				callback = s_listeners[j].callback;
				break;
			}
		}
		if ( callback ) {
			NetUdpPacket packet;
			packet.m_length = s_packets[i].len;
			packet.m_clientAddress.sin_family = 0;
			packet.m_clientAddress.sin_port = s_packets[i].fromPort;
			packet.m_clientAddress.sin_addr = 0x7F000001;
			if ( s_packets[i].len > 0 && s_packets[i].len <= MAX_PACKET_SIZE )
				memcpy( packet.m_data, s_packets[i].data, (size_t)s_packets[i].len );
			callback( packet );
		}
		free( s_packets[i].data );
		s_packets[i].data = NULL;
		s_packets[i].inUse = 0;
	}
#endif
}

#endif // TARGET_EMSCRIPTEN