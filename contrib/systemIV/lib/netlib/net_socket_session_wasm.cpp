#include "lib/universal_include.h"

#ifdef TARGET_EMSCRIPTEN

#include "net_lib.h"
#include "net_socket_session.h"
#include "net_socket_listener.h"

// WebAssembly NetSocketSession stubs

NetSocketSession::NetSocketSession(NetSocketListener &_l, NetIpAddress _address)
{
    m_sockfd = _l.GetBoundSocketHandle();
    m_to = _address;
}

NetSocketSession::NetSocketSession(NetSocketListener &_l, const char *_host, unsigned _port)
{
    m_sockfd = _l.GetBoundSocketHandle();
    // Initialize m_to to default values
    m_to.sin_family = 0;
    m_to.sin_port = _port;
    m_to.sin_addr = 0x7F000001; // 127.0.0.1 for localhost
}

NetRetCode NetSocketSession::WriteData(void *_buf, int _bufLen, int *_numActualBytes)
{
    // No-op in WebAssembly
    if (_numActualBytes) *_numActualBytes = 0;
    return NetOk;
}

#endif // TARGET_EMSCRIPTEN 