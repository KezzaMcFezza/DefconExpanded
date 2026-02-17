#include "systemiv.h"

#ifdef TARGET_EMSCRIPTEN

#include "net_lib.h"
#include "net_udp_packet.h"

//
// WASM is single threaded, so conditions are no ops

NetUdpPacket::NetUdpPacket()
{
	m_length = 0;
	m_clientAddress.sin_family = 0;
	m_clientAddress.sin_port = 0;
	m_clientAddress.sin_addr = 0;
	memset( m_data, 0, MAX_PACKET_SIZE );
}

void NetUdpPacket::IpAddressToStr( char *_ipAddress ) const
{
	//
	// Use actual address when set, otherwise localhost

	if ( m_clientAddress.sin_addr != 0 )
	{
		::IpAddressToStr( m_clientAddress, _ipAddress );
	}
	else
	{
		strcpy( _ipAddress, "127.0.0.1" );
	}
}

int NetUdpPacket::GetPort() const
{
	return (int)m_clientAddress.sin_port;
}

#endif // TARGET_EMSCRIPTEN