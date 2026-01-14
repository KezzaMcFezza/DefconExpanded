#include "systemiv.h"

#ifdef TARGET_EMSCRIPTEN

#include "net_lib.h"
#include "net_socket_session.h"
#include "net_socket_listener.h"

//
// WASM is single threaded, so conditions are no ops

NetSocketSession::NetSocketSession( NetSocketListener &_l, NetIpAddress _address )
{
	m_sockfd = _l.GetBoundSocketHandle();
	m_to = _address;
}


NetSocketSession::NetSocketSession( NetSocketListener &_l, const char *_host, unsigned _port )
{
	m_sockfd = _l.GetBoundSocketHandle();
	m_to.sin_family = 0;
	m_to.sin_port = _port;
	m_to.sin_addr = 0x7F000001; // 127.0.0.1
}


NetRetCode NetSocketSession::WriteData( void *_buf, int _bufLen, int *_numActualBytes )
{
	if ( _numActualBytes )
		*_numActualBytes = 0;
	return NetOk;
}

#endif // TARGET_EMSCRIPTEN