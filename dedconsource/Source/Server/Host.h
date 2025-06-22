/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_Host_INCLUDED
#define DEDCON_Host_INCLUDED

#include "Client.h"

//! a special superpower that represents the host
class Host: public SuperPower
{
public:
    Host();
    ~Host();

    //! sets the name of the host
    void SetName( std::string const & name );

    //! set default name
    void SetDefaultName();

    //! says something to everyone
    void Say( std::string const & line );

    //! says something to a selected crowd
    void Say( std::string const & line, std::vector< int > const & whitelist, std::vector< int > const & blacklist );

    //! says something to a selected crowd
    void Say( std::string const & line, std::vector< int > const & whitelist );

    //! says something to a single player
    void Say( std::string const & line, Client const & client );

    //! says something to a single player or logs it to the console (if client == 0 )
    void Say( std::string const & line, Client const * client );

    //! call to put the next chat into a reserved sequence
    void SayHere( Sequence * sequence );

    //! returns the singleton host
    static Host & GetHost();

    // set this client as the argument to Say() if you want a message broadcasted. Warning: this is not a real client.
    // static Client * GetBroadcastClient();
private:
    std::string name;
    bool nameSet;
    Sequence * reserved;
};


#endif // DEDCON_Host_INCLUDED
