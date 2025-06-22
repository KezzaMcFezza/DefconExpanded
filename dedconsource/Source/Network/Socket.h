/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_Socket_INCLUDED
#define DEDCON_Socket_INCLUDED

#include <string>

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#define socklen_t   int
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#define SOCKET      int
#define closesocket close
#define ioctlsocket ioctl
#endif

#include "Lib/FakeEncryption.h"

//! wrapper for network sockets
class Socket
{
public:
    Socket();
    ~Socket();

    //! returns the raw socket
    SOCKET GetSocket() const
    {
        return socket_;
    }

    //! sets the port to listen on
    void SetPort( int port );

    //! returns the port to be listened on
    int GetPort() const;

    //! sets the IP to bind to
    void SetIP( std::string ip );

    //! discards the IP to bind to
    void SetIPAny();

    //! returns the IP
    std::string GetIP() const;

    //! binds to the port specified earlier with SetPort()
    bool Bind();

    //! is the socket bound?
    bool Bound() const { return bound_;}

    //! resets the socket
    void Reset();

    //! returns whether a reset is needed
    static bool NeedsReset();

    //! read from the socket
    int RecvFrom( sockaddr_in & source, void * buffer, size_t size );

    //! write to the socket
    int SendTo( sockaddr_in const & target, void const * buffer, size_t len, ENCRYPTION_VERSION encryption = NO_ENCRYPTION );

    //! make the socket able to broadcast
    void Broadcastable();

    //! opens the socket
    void Open();

    //! closes the socket
    void Close();

    sockaddr_in const & LocalAddr() const { return localAddr; }
private:

    SOCKET      socket_;
    bool        bound_;
    bool        opened_;
    sockaddr_in localAddr;
};

#endif // DEDCON_Socket_INCLUDED
