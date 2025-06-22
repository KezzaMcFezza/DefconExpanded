#include "lib/universal_include.h"
#include <cstring>

#ifdef TARGET_EMSCRIPTEN

#include "net_lib.h"
#include "net_udp_packet.h"

// WebAssembly NetUdpPacket stubs

NetUdpPacket::NetUdpPacket()
{
    m_length = 0;
    // Initialize client address to default values
    m_clientAddress.sin_family = 0;
    m_clientAddress.sin_port = 0;
    m_clientAddress.sin_addr = 0;
    // m_data is an array, so it doesn't need to be initialized to NULL
    memset(m_data, 0, MAX_PACKET_SIZE);
}

void NetUdpPacket::IpAddressToStr(char *_ipAddress) const
{
    // Return localhost for WebAssembly
    strcpy(_ipAddress, "127.0.0.1");
}

int NetUdpPacket::GetPort() const
{
    // Return 0 for WebAssembly
    return 0;
}

#endif // TARGET_EMSCRIPTEN 