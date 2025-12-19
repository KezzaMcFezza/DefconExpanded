#include "systemiv.h"

#ifdef TARGET_EMSCRIPTEN

#include "net_lib.h"
#include "net_socket_listener.h"
#include "net_udp_packet.h"

// WebAssembly NetSocketListener stubs

NetSocketListener::NetSocketListener(unsigned short _port)
{
    m_port = _port;
    m_sockfd = -1;
    m_binding = false;
    m_listening = false;
    m_thread = NULL;
}

NetSocketListener::~NetSocketListener()
{
}

NetRetCode NetSocketListener::StartListening(NetCallBack _callback, ThreadFunctions *_threadFunctions)
{
    // Cannot start listening in WebAssembly
    return NetFailed;
}

void NetSocketListener::StopListening(ThreadController *_threadToStop)
{
    // No-op
}

NetSocketHandle NetSocketListener::GetBoundSocketHandle()
{
    return m_sockfd;
}

NetRetCode NetSocketListener::Bind()
{
    // Cannot bind in WebAssembly
    return NetFailed;
}

NetRetCode NetSocketListener::EnableBroadcast()
{
    // No-op
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

#endif // TARGET_EMSCRIPTEN 