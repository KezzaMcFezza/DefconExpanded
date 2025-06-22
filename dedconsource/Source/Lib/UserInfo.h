/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_UserInfo_INCLUDED
#define DEDCON_UserInfo_INCLUDED

#include <string>
#include <vector>

#include "Network/Socket.h"

//! part of UserInfo to be directly copied into the client
struct ClientInfo
{
    // flag with default value 
    enum FlagWithDefault
    {
        Off = 0,
        On  = 1,
        Default = 2
    };

    // reverses a flag
    static void ReverseFlag( FlagWithDefault & flag );

    ClientInfo();

    float ratingScore;                          //!< the player's score on the rating table
    FlagWithDefault silenced;                   //!< is this user silenced?
    std::string forceName;                      //!< if non-empty, forces the user to this name.
};

//! Persistent information (bans, messages) about users based on KeyID or IP
class UserInfo: public ClientInfo
{
public:
    UserInfo();

    //! returns a user info for a specific person, based on its IP address, keyID, and version
    static UserInfo Get( in_addr ip, int keyID, std::string const & version, std::string const & platform );

    //! merges a user info with previous infos after a string token (representing a KeyID, single IP or IP range)
    static void Merge( std::string const & id, UserInfo info );

    //! merge two user infos
    void Merge( UserInfo const & other );

    //! reverses the info
    void Reverse();

    std::vector< std::string > publicMessages;  //!< public messages for this user
    std::vector< std::string > privateMessages; //!< private messages for this user
    FlagWithDefault banned;                     //!< is this user banned?
};

#endif // DEDCON_UserInfo_INCLUDED
