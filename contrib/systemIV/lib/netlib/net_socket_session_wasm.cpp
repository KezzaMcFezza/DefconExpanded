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
	m_localPort = (unsigned short)_l.GetPort();
}


NetSocketSession::NetSocketSession( NetSocketListener &_l, const char *_host, unsigned _port )
{
	m_sockfd = _l.GetBoundSocketHandle();
	m_to.sin_family = 0;
	m_to.sin_port = (uint16_t)_port;
	m_to.sin_addr = 0x7F000001; // 127.0.0.1
	m_localPort = (unsigned short)_l.GetPort();
}


NetRetCode NetSocketSession::WriteData( void *_buf, int _bufLen, int *_numActualBytes )
{
#ifdef OFFLINE_MODE
	if ( _buf && _bufLen > 0 ) {
		NetLocal_QueuePacket( m_localPort, &m_to, _buf, _bufLen );
		if ( _numActualBytes )
			*_numActualBytes = _bufLen;
		return NetOk;
	}
#endif
	if ( _numActualBytes )
		*_numActualBytes = 0;
	return NetOk;
}

#endif // TARGET_EMSCRIPTEN