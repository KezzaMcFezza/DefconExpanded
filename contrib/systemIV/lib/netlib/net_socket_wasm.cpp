#include "systemiv.h"
#include <cstring>

#ifdef TARGET_EMSCRIPTEN

#include "net_lib.h"
#include "net_socket.h"

//
// WASM is single threaded, so conditions are no ops

NetSocket::NetSocket()
{
	m_sockfd = -1;
	m_port = 0;
	m_connected = false;
	m_timeout = 10000;
	m_polltime = 0;
	strcpy( m_hostname, "localhost" );
}


NetSocket::~NetSocket()
{
}


NetRetCode NetSocket::Connect()
{
	return NetFailed;
}

NetRetCode NetSocket::Connect( const char *host, unsigned short port )
{
	strncpy( m_hostname, host, MAX_HOSTNAME_LEN - 1 );
	m_hostname[MAX_HOSTNAME_LEN - 1] = '\0';
	m_port = port;
	return NetFailed;
}


void NetSocket::Close()
{
	m_connected = false;
}


NetRetCode NetSocket::WriteData( void *_buf, int _bufLen, int *_numActualBytes )
{
	if ( _numActualBytes )
		*_numActualBytes = 0;
	return NetOk;
}


NetIpAddress NetSocket::GetRemoteAddress()
{
	NetIpAddress addr;
	addr.sin_family = 0;
	addr.sin_port = 0;
	addr.sin_addr = 0x7F000001; // 127.0.0.1
	return addr;
}


NetIpAddress NetSocket::GetLocalAddress()
{
	NetIpAddress addr;
	addr.sin_family = 0;
	addr.sin_port = 0;
	addr.sin_addr = 0x7F000001; // 127.0.0.1
	return addr;
}


NetRetCode NetSocket::CheckTimeout( unsigned int *timeout, unsigned int *timedout, int haveAllData )
{
	*timedout = 1;
	return NetTimedout;
}

#endif // TARGET_EMSCRIPTEN