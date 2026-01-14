#include "systemiv.h"

#include <string.h> // For memcpy

#include "net_udp_packet.h"

NetUdpPacket::NetUdpPacket()
	: m_length( 0 )
{
	memset( m_data, 0, MAX_PACKET_SIZE * sizeof( char ) );
	memset( &m_clientAddress, 0, sizeof( m_clientAddress ) );
}

void NetUdpPacket::IpAddressToStr( char *_ipAddress ) const
{
	::IpAddressToStr( m_clientAddress, _ipAddress );
}

int NetUdpPacket::GetPort() const
{
	return ntohs( m_clientAddress.sin_port );
}
