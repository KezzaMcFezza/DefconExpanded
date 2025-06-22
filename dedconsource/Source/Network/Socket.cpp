/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "Socket.h"

#include "Main/Log.h"
#include <errno.h>
#include "GameSettings/Settings.h"

#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>

#undef EADDRINUSE
#undef EWOULDBLOCK
#undef ECONNREFUSED
#undef ENOTSOCK
#undef ECONNRESET
#undef ENETDOWN
#undef EISCONN
#undef ENETRESET
#undef EOPNOTSUPP
#undef EAFNOSUPPORT
#undef EADDRNOTAVAIL
#undef EMSGSIZE
#undef ETIMEDOUT
#undef ENETUNREACH
#endif

#ifdef WIN32
    #include <winsock2.h>
    #include <windows.h>
#else
    #include <netinet/in.h>
    #include <netinet/ip.h>
    #include <sys/ioctl.h>
    #include <sys/time.h>
    #include <sys/param.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #include <stdlib.h>
#endif

Socket::Socket(): socket_(0), bound_( false ), opened_( false )
{
    localAddr.sin_port=0;
    localAddr.sin_family=AF_INET;
    localAddr.sin_addr.s_addr=INADDR_ANY;
}

Socket::~Socket()
{
    if ( opened_ )
        Close();
}

void Socket::SetPort( int port )
{
    localAddr.sin_port=htons((unsigned short) port );
}

int Socket::GetPort() const
{
    return ntohs(localAddr.sin_port);
}

//! sets the IP to bind to
void Socket::SetIP( std::string ip )
{
    localAddr.sin_addr.s_addr=inet_addr(ip.c_str());
}

//! sets the IP to bind to
void Socket::SetIPAny()
{
    localAddr.sin_addr.s_addr=INADDR_ANY;
}

//! returns the IP
std::string Socket::GetIP() const
{
    return inet_ntoa(localAddr.sin_addr);
}

bool Socket::Bind()
{
    if ( bind(socket_, (const struct sockaddr *) &localAddr, sizeof(struct sockaddr_in)) == 0 )
    {
        bound_ = true;
    }
    else
    {
        bound_ = false;
    }

    return bound_;
}

void Socket::Reset()
{
    if ( socket_ > 0 )
    {
        if ( Log::GetLevel() >= 1 )
        {
            // only report every once in a while
            static int count = 0;
            ++count;
            int report = count;
            while ( ( report & 1 ) == 0 && report > 0 )
            {
                report >>= 1;
                // Log::Err() << report << "\n";
            }
            if ( report <= 5 )
            {
                if ( count == 1 )
                {
                    Log::Err() << "Resetting socket. This is required from time to time, for example when a client disconnects in a particularly violent manner without informing the server. Don't be too alarmed, this is not a bug unless it has other visible consequences, like unrelated clients dropping.\n";
                }
                else
                {
                    Log::Err() << "Resetting socket (" << count << ").\n";
                }
            }
        }

        if ( opened_ )
        {
            Close();
            Open();
            if ( bound_ )
                Bind();
        }
    }
}

bool Socket::NeedsReset()
{
#ifdef WIN32
#define EADDRINUSE WSAEADDRINUSE
#define EWOULDBLOCK WSAEWOULDBLOCK
#define ECONNREFUSED WSAECONNREFUSED
#define ENOTSOCK WSAENOTSOCK
#define ECONNRESET WSAECONNRESET
#define ECONNREFUSED WSAECONNREFUSED
#define ENETDOWN WSAENETDOWN
#ifndef EINTR
#define EINTR WSAEINTR
#endif
#ifndef EINVAL
#define EINVAL WSAEINVAL
#endif
#define EISCONN WSAEISCONN
#define ENETRESET WSAENETRESET
#define EOPNOTSUPP WSAEOPNOTSUPP
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#define ESHUTDOWN WSAESHUTDOWN
#define EMSGSIZE WSAEMSGSIZE
#define ETIMEDOUT WSAETIMEDOUT
#define ENETUNREACH WSAENETUNREACH
#ifndef EINVAL
#define EAGAIN WSAEAGAIN
#endif

    int error = WSAGetLastError();
#else
    int error = errno;
#endif

    switch ( error )
    {
    case EADDRINUSE:
        break;
    case ENOTSOCK:
    case ECONNRESET:
        return true;
        break;
    case ECONNREFUSED:
    case EWOULDBLOCK:
    case ENETUNREACH:
        break;
    case ENETDOWN:
        break;
    case EFAULT:
        return true;
        break;
    case EINTR:
        break;
    case EINVAL:
        break;
    case EISCONN:
        break;
    case ENETRESET:
        break;
    case EOPNOTSUPP:
    case EAFNOSUPPORT:
    case EADDRNOTAVAIL:
        return true;
        break;
    case ESHUTDOWN:
        break;
#ifndef WIN32
    case EMSGSIZE:
        break;
#endif
    case ETIMEDOUT:
        break;
    default:
#ifdef DEBUG
        // Log::Err() << "Unknown network error " << error << "\n";
#endif
        break;
    }

    return false;
}

//! read
int Socket::RecvFrom( sockaddr_in & source, void * buffer, size_t size )
{
    char * b = (char *)buffer;

    // prepare receiver address
    memset(&source,0,sizeof(struct sockaddr_in));

    // and receive
    socklen_t fromaddrlen = sizeof(source);
    int received = recvfrom(socket_, b, size, 0, (struct sockaddr *) &(source),  &fromaddrlen);
    if ( received < 0 && NeedsReset() )
    {
        Reset();
        received = recvfrom(socket_, b, size, 0, (struct sockaddr *) &(source), &fromaddrlen);
    }

    return received;
}

//! write
int Socket::SendTo( sockaddr_in const & target, void const * buffer, size_t len, ENCRYPTION_VERSION encryption )
{
    assert( encryption == NO_ENCRYPTION );

    char const * b = (char const *)buffer;
    int sent = sendto( socket_, b, len, 0, (struct sockaddr *) &target, sizeof(sockaddr_in));

    if ( sent < 0 && NeedsReset() )
    {
        Reset();
        sent = sendto( socket_, b, len, 0, (struct sockaddr *) &target, sizeof(sockaddr_in));
    }
    
    return sent;
}

void Socket::Broadcastable()
{
    int i = 1;
    setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, (char *)&i, sizeof(i));
}

void Socket::Open()
{
    opened_ = true;

    // create socket
    unsigned long nonzero=1;
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);

#ifndef WIN32
    // set realtime priority
    char tos = IPTOS_LOWDELAY;

    // int ret = 
    setsockopt( socket_, IPPROTO_IP, IP_TOS, &tos, sizeof(char) );

    // remove this error reporting some time later, the success is not critical
#ifdef DEBUG_NEVER
    if ( ret != 0 )
    {
        static bool warn=true;
        if ( warn )
            Log::Err() << "Setting TOS to LOWDELAY failed.\n";
        warn=false;
    }
#endif
#endif    

    // unblock
    ioctlsocket ( socket_, FIONBIO, &nonzero);
}

void Socket::Close()
{
    closesocket( socket_ );
    socket_ = 0;
}
