#include "systemiv.h"
#include <cstring>

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
	strcpy( _ipAddress, "127.0.0.1" );
}

int NetUdpPacket::GetPort() const
{
	return 0;
}

#endif // TARGET_EMSCRIPTEN