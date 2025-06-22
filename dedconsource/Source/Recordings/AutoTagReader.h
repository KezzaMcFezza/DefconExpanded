/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_AutoTagReader_INCLUDED
#define DEDCON_AutoTagReader_INCLUDED

#include "Lib/Misc.h"
#include "TagReader.h"
#include "Network/Socket.h"

//! helper class: reads data from socket into buffer
class AutoTagReaderBuffer
{
public:
    enum { BUFSIZE = 2048 };

    AutoTagReaderBuffer( Socket & socket ); //!< reads data from socket into buffer

    sockaddr_in const & Sender() const;    //!< returns the sender of the data

    void SendTo( Socket & socket, sockaddr_in const & target, ENCRYPTION_VERSION encryption ) const; //!< sends the data to some other socket

    bool Received() const; //!< returns whether data was received
protected:
    sockaddr_in sender;  //!< the address of the sender
    Socket &    socket;  //!< the socket to read from
    int         len;     //!< the length of the received data

    unsigned char buffer[BUFSIZE];
};

//! reads data from the network
class AutoTagReader: public AutoTagReaderBuffer, public TagReader
{
public:
    AutoTagReader( Socket & socket ); //!< reads data from socket, ready for parsing
    ~AutoTagReader();
    virtual void Dump();              //!< dump the contents
};

#endif // DEDCON_AutoTagReader_INCLUDED
