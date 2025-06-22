/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_SyncValue_INCLUDED
#define DEDCON_SyncValue_INCLUDED

#include "Lib/Misc.h"
#include <map>

//! determines the correct game state checksum by comparing the opinions of the clients
class SyncValue
{
public:
    //! single checksum
    struct Single
    {
        unsigned char val;      //!< the checksum value
        bool set:1;             //!< whether it was set

        Single(): val(0), set(false){}
        Single( unsigned char v ): val(v), set(true){}
    };

    typedef std::map< Client *, Single > Map; //!< maps from clients to checksums

    //! set the checksum sent from one particular client
    void FromClient( Client * client, Single checksum )
    {
        map[ client ] = checksum;
    }
    
    //! see if enough votes have been cast to determine the true checksum
    bool Majority( unsigned char & checksum, int seqID, bool force = false ) const;
private:
    Map map; //!< checksums from all clients
};

#endif // DEDCON_SyncValue_INCLUDED
