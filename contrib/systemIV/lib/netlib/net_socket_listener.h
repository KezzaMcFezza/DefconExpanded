// ****************************************************************************
// A UDP socket listener implementation. This class blocks until data is
// received, so you probably want to put it in its own thread.
// ****************************************************************************

#ifndef INCLUDED_NET_SOCKET_LISTENER_H
#define INCLUDED_NET_SOCKET_LISTENER_H


#include "net_lib.h"


class NetUdpPacket;
class ThreadController;
class ThreadFunctions;

typedef void (*NetCallBack)(const NetUdpPacket &data);

class NetSocketListener
{
public:
	NetSocketListener(unsigned short port);
	~NetSocketListener();

    // Start listening. If threadFunctions is not NULL, will also terminate listening loop
    // if thread is requested to stop.
    NetRetCode	StartListening(NetCallBack fnptr, ThreadFunctions *threadFunctions = NULL );
	
    // Asynchronously stops the listener after next accept call. If threadtoStop is not NULL,
    // will call threadToStop->Stop() to terminate listening thread.
    void		StopListening( ThreadController *threadToStop = NULL );

	// Called by NetSocketSession to get the socket
	NetSocketHandle		GetBoundSocketHandle();

	NetRetCode	Bind();
	
    NetRetCode  EnableBroadcast();

	int			GetPort() const;

	NetIpAddress	GetListenAddress() const;
	
protected:
	NetSocketHandle 	m_sockfd;
	bool				m_binding;
	bool				m_listening;
	unsigned short	 	m_port;
    ThreadFunctions    *m_thread;
};

#endif
