/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_MetaServer_INCLUDED
#define DEDCON_MetaServer_INCLUDED

#include "Lib/Misc.h"
#include "Network/Socket.h"
#include "Recordings/TagParser.h"
#include "Lib/List.h"
#include "Lib/Clock.h"

//! metaserver queries. DoSend() is called regularily, and when an answer
//! arrives, the TagParser's Parse() function is called on it. If the answer
//! was for this query, it is supposed to call WasForMe().
class MetaServerQuery: public TagParserLax
{
    friend class MetaServer;
public:
    MetaServerQuery();
protected:
    void WasForMe(); //!< call when a query answer was for you.

    //! called for nested tags
    virtual void OnNested( TagReader & reader );

    bool good;         //!< set to true before the answer is parsed. Do with it what you want.
private:
    virtual void DoSend( Socket & socket, sockaddr_in const & target ) = 0; //!< really sends the query

    bool answered;     //!< was the query answered?
    Time lastSent;     //!< the last time the query was sent
};

typedef List<MetaServerQuery> MetaServerQueryList;

class QueryFactory
{
public:
    static void QueryDemoLimits();                            //!< query the metaserver for demo limits
    static void QueryServerKey();                            //!< query the validity of the server key
    static void QueryClientKey( Client * client );            //!< query the validity of a client key
};

// metaserver handling
class MetaServer
{
    friend class ServerIPActionSetting;
public:
    MetaServer();
    ~MetaServer();

    //! initalizes the connection
    void Init();

    //! processes metaserver data coming in from the given source
    bool Process( TagReader & reader, sockaddr_in const & addr );

    //! send all queries to the server, call this repeatedly to compensate packet loss
    void SendQueries();

    //! adds a query to the query list
    void AddQuery( MetaServerQueryList * query );
    
    //! send the metaserver some data
    void Send( AutoTagWriter & writer, Socket & socket );

    //! send the metaserver some data over the special socket created for that purpose only
    void Send( AutoTagWriter & writer );

    //! receive data from the metaserver socket
    void Receive();

    //! check whether the metaserver is reachable
    bool Reachable() const;

    //! check whether the metaserver answers to calls
    bool Answers() const;

    int ServerTTL() const{ return serverTTL; }
private:
    ListBase<MetaServerQuery> metaServerQueries;   //!< the pending queries for the metaserver

    sockaddr_in metaserver;                        //!< the metaserver's address
    bool        reachable;                         //!< is the metaserver reachable?
    int         serverTTL;                         //!< the time a server info survives without getting updated

protected:
    Socket      metaserverSocket;                  //!< the socket used for metaserver communication
};

class InternetAdvertiser;
extern InternetAdvertiser * metaServer;

//! object that looks for the LAN IP
class LANIP
{
public:
    LANIP();
    
    void Send(); //!< sends out a query
    bool Receive( TagReader & reader, sockaddr_in const & addr ); //!< receives the return

    unsigned int token; //!< random token

    unsigned int numTries; //!< number of tries so far
};

extern LANIP lanIP;

#endif // DEDCON_MetaServer_INCLUDED
