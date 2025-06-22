/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "UserInfo.h"

#include <map>
#include <set>
#include <stdexcept>
#include <iostream>
#include <sstream>

#include "GameSettings/Settings.h"
#include "Main/Log.h"
#include "Server/IPRange.h"

#include <stdlib.h>
#include <string.h>

// the default user info
static UserInfo defaultUserInfo;

// user infos for players with fixed KeyIDs
static std::map< int, UserInfo > keyIDUsers;

// user infos for players with fixed IPs
static std::map< std::string, UserInfo > ipUsers;

// user infos for players from IP ranges
#ifdef COMPILE_DEDCON
static const int rangeTypes = 6;
#else
static const int rangeTypes = 4;
#endif
static IPSet rangeUsers[ rangeTypes ];

// range flags
enum RangeFlags
{
    Win = 1,     //!< Windows Demo users with an IP in the range are banned
    Lin = 2,     //!< Linux Demo users with an IP in the range are banned
    Mac = 4,     //!< Mac Demo users with an IP in the range are banned
    All = 8,     //!< all versions, even those with unique keys
#ifdef COMPILE_DEDCON
    Steam = 16,  //!< Steam users with an IP in the range are banned
    SteamCompatible = 32, //!< versions compatible with steam that log in with a full key (which may be from steam)
#endif
    Full = -1
};

ClientInfo::ClientInfo(): ratingScore(0), silenced(Default){}
UserInfo::UserInfo(): banned(Default){}

//! returns a user info for a specific person, based on its IP address, keyID, and version
UserInfo UserInfo::Get( in_addr ip, int keyID, std::string const & version, std::string const & platform )
{
    UserInfo ret;
    
    std::string ip_as_string = inet_ntoa( ip );

#ifdef COMPILE_DEDCON    
    bool demo = ( keyID < 0 );
    
    // detect various versions
    int type = 0;
    bool mac = strstr( version.c_str(), "mac" ) || strstr( platform.c_str(), "Mac" );
    if ( strstr( version.c_str(), "steam" ) )
    {   
        // steam
        type = Steam;
        if ( demo )
            type |= Win; 
    }
    else if ( demo )
    {
        // demo for Windows
        type = Win;
        if ( strstr( version.c_str(), "linux" )  || platform == "Linux" )
        {   
            // linux
            type = Lin;
        }   
        else if ( mac )
        {   
            // and mac
            type = Mac;
        }
    }
    else
    {
        // set all flag
        type |= All;
    
        if ( !mac )
        {
            // set steam compatibility flag
            type = SteamCompatible | type;
        }
    }
#else
    bool demo = ( keyID < 0 );
    
    // detect various versions
    int type = 0;
    if ( demo )
    {
        if ( strstr( version.c_str(), "steam" ) || strstr( version.c_str(), "retail.iv" ) || platform == "PC" )
        {   
            // Windows
            type |= Win; 
        }
        else if ( strstr( version.c_str(), "linux" ) || platform == "Linux" )
        {   
            // linux
            type |= Lin;
        }   
        else if ( strstr( version.c_str(), "mac" )  || strstr( platform.c_str(), "Mac" ) )
        {   
            // and mac
            type |= Mac;
        }
        else
        {
            // default to windows
            type |= Win; 
        }
    }
    else
    {
        // set all flag
        type |= All;
    }
#endif
   
    // merge info from corresponding ranges
    for ( int t = rangeTypes-1; t >= 0; --t )
    {
        if ( ( type & ( 1 << t ) ) )
        {
            rangeUsers[t].Get( ip, ret );
        }
    }
    
    // find IP
    try
    {
        ret.Merge( ipUsers[ ip_as_string ] );
    }
    catch ( std::out_of_range & e )
    {
    }

    // find key
    try
    {
        if ( keyID >= 0 )
        {
            ret.Merge( keyIDUsers[ keyID ] );
        }
    }
    catch ( std::out_of_range & e )
    {
    }

    return ret;
}

//! merges a user info after a string token (representing a KeyID, single IP or IP range)
void UserInfo::Merge( std::string const & id, UserInfo info )
{
    std::istringstream s( id );

    if ( strstr( id.c_str(), "." ) == 0 )
    {
        // it must be a keyID
        int keyID = 0;
        s >> keyID;
        if ( s.fail() )
        {
            Log::Out() << "Expected a KeyID (or alternatively an IP or IP range, but it didn't look like either) which is a number. Got " << id << " instead.\n";
            return;
        }
        
        keyIDUsers[ keyID ].Merge( info );
        
        return;
    }

    if ( strstr( id.c_str(), "/" ) == 0 && strstr( id.c_str(), ":" ) == 0 )
    {
        // it must be an IP
        ipUsers[id].Merge( info );
        return;
    }
    
    // else, it can only be an IP range.
    // read IP (up to the colon or slash)
    char c = ' ';
    std::ostringstream ip;
    while ( c != ':' && c != '/' && !s.eof() )
    {
        c = s.get();
        if ( c != ':' && c != '/' )
            ip << c;
    }
    
    // read significant bits
    int bits = 0;
    s >> bits;

    if ( s.fail() )
    {
        Log::Err() << "IP Ranges are expects an IP range in the form IP/bits where IP is the basic IP of the range and bits is the number of bits to use for comparison. Example: 192.168.0.0/24 means all IPs in the 192.158.0.x subnet.\n";
        return;
    }
        
    // read flags
    int flags = 0;
    while ( !s.eof() && s.good() )
    {
        c = s.get();
        
        // ignore eof
        if ( s.eof() )
        {
            break;
        }
        
        // ignore whitespace
        if (isspace(c))
        {
            continue;
        }
        
        switch (tolower(c))
        {
        case 'n':
            // flags = 0;
            info.Reverse();
            break;
        case 'w':
            flags |= Win;
            break;
        case 'l':
            flags |= Lin;
            break;
        case 'm':
            flags |= Mac;
            break;
#ifdef COMPILE_DEDCON
        case 's':
            flags |= Steam;
            break;
        case 'c':
            flags |= SteamCompatible;
            break;
#endif
        case 'a':
            flags |= All;
            break;
        default:
            Log::Err() << "Illegal version flag " << c << ". legal are: w for Windows, l for Linux, m for Mac, "
#ifdef COMPILE_DEDCON
            "s for Steam, c for Compatible with Steam, "
#endif
            "a for all, n for None.\n";
            return;
        }
    }

    // no flags means all flags
    if ( flags == 0 )
    {
#ifdef COMPILE_DEDCON
        flags = Win | Lin | Mac | Steam;
#else
        flags = Win | Lin | Mac;
#endif
    }

    // ban them all
    IPRange range;
    range.addr.s_addr = inet_addr( ip.str().c_str() );
    range.bits= bits;

    // merge info with corresponding ranges
    for ( int t = 0; t < rangeTypes; ++t )
    {
        if ( ( flags & ( 1 << t ) ) || flags == 0 )
        {
            rangeUsers[t].Merge( range, info );
        }
    }
}

//! merge two user infos
void UserInfo::Merge( UserInfo const & other )
{
    if ( other.ratingScore != 0 )
    {
        ratingScore = other.ratingScore;
    }
    
    publicMessages.insert( publicMessages.end(), other.publicMessages.begin(), other.publicMessages.end() );
    privateMessages.insert( privateMessages.end(), other.privateMessages.begin(), other.privateMessages.end() );

    if ( other.banned != Default )
    {
        banned = other.banned;
    }

    if ( other.silenced != Default )
    {
        silenced = other.silenced;
    }

    if ( other.forceName.size() > 0 )
    {
        forceName = other.forceName;
    }
}

void ClientInfo::ReverseFlag( FlagWithDefault & flag )
{
    if ( flag == On )
    {
        flag = Off;
    }
    else if ( flag == Off )
    {
        flag = On;
    }
    
}

//! reverses the info
void UserInfo::Reverse()
{
    ReverseFlag( silenced );
    ReverseFlag( banned );
}

static void ClearAll()
{
    keyIDUsers.clear();
    ipUsers.clear();
    for ( int i = rangeTypes-1; i > 0; --i )
    {
        rangeUsers[i].Clear();
    }
}

//! clears all user info
class AllClearActionSetting: public ActionSetting
{
public:
    AllClearActionSetting()
    : ActionSetting( Setting::Grief, "ClearAllUserInfo" )
    {}

    //! activates the action with the specified value
    virtual void DoRead( std::istream & source )
    {
        Log::Out() << "Erasing all bans and messages.\n";
        ClearAll();
   }

    virtual void Revert()
    {
        ClearAll();
    }
};
static AllClearActionSetting allClear;

//! messages
class MessageActionSetting: public ActionSetting
{
public:
    MessageActionSetting( char const * name, bool pub_ )
    : ActionSetting( Setting::Player, name ), pub( pub_ )
    {}

protected:
    //! activates the action with the specified value
    virtual void DoRead( std::istream & source )
    {
        std::ws( source );
        std::string id;
        std::string message;
        
        source >> id;
        if ( source.good() )
        {
            std::ws( source );
        }
        if ( source.good() )
        {
            std::getline( source, message );

            UserInfo userInfo;
            if ( pub )
            {
                userInfo.publicMessages.push_back( message );
            }
            else
            {
                userInfo.privateMessages.push_back( message );
            }

            UserInfo::Merge( id, userInfo );
        }
        else
        {
            Log::Out() << "Usage: [Public]Message <keyID/IP(range)> <message>\n";
        }
    }

    bool pub;
};

static MessageActionSetting keyMessagePublic( "PublicMessage", true );
static MessageActionSetting keyMessagePublicIP( "PublicMessageIP", true );
static MessageActionSetting keyMessagePublicKeyID( "PublicMessageKeyID", true );

static MessageActionSetting keyMessagePrivate( "Message", false );
static MessageActionSetting keyMessagePrivateIP( "MessageIP", false );
static MessageActionSetting keyMessagePrivateKeyID( "MessageKeyID", false );

// Banning

//! bans someone by IP
class BanActionSetting: public StringActionSetting
{
public:
    BanActionSetting( char const * name, UserInfo::FlagWithDefault ban_ )
    : StringActionSetting( Setting::Grief, name ), ban( ban_ )
    {}

    //! activates the action with the specified value
    virtual void Activate( std::string const & val )
    {
        if ( val.size() > 0 )
        {
            UserInfo info;
            info.banned = ban;
            UserInfo::Merge( val, info  );

            Log::Out() << val << ( (ban == UserInfo::On) ? " banned.\n" : " unbanned.\n" );
        }
        else
        {
            Log::Err() << "(Un)Ban expects an IP address, a keyID or an IP range as argument.";
        }
    }

    UserInfo::FlagWithDefault ban;
};

static BanActionSetting ban( "Ban", UserInfo::On );
static BanActionSetting ipBan( "BanIP", UserInfo::On );
static BanActionSetting keyBan( "BanKeyID", UserInfo::On );
static BanActionSetting rangeBan( "BanIPRange", UserInfo::On );

static BanActionSetting unBan( "UnBan", UserInfo::Off );
static BanActionSetting ipUnBan( "UnBanIP", UserInfo::Off  );
static BanActionSetting keyUnBan( "UnBanKeyID", UserInfo::Off  );
static BanActionSetting rangeUnBan( "UnBanIPRange", UserInfo::Off  );

//! silences someone by IP
class SilenceActionSetting: public StringActionSetting
{
public:
    SilenceActionSetting( char const * name, UserInfo::FlagWithDefault silence_ )
    : StringActionSetting( Setting::Grief, name ), silence( silence_ )
    {}

    //! activates the action with the specified value
    virtual void Activate( std::string const & val )
    {
        if ( val.size() > 0 )
        {
            UserInfo info;
            info.silenced = silence;
            UserInfo::Merge( val, info  );

            Log::Out() << val << ( (silence == UserInfo::On) ? " silencened.\n" : " unsilencened.\n" );
        }
        else
        {
            Log::Err() << "(Un)Silence expects an IP address, a keyID or an IP range as argument.";
        }
    }

    UserInfo::FlagWithDefault silence;
};

static SilenceActionSetting silence( "Silence", UserInfo::On );
static SilenceActionSetting ipSilence( "SilenceIP", UserInfo::On );
static SilenceActionSetting keySilence( "SilenceKeyID", UserInfo::On );
static SilenceActionSetting rangeSilence( "SilenceIPRange", UserInfo::On );

static SilenceActionSetting unSilence( "UnSilence", UserInfo::Off );
static SilenceActionSetting ipUnSilence( "UnSilenceIP", UserInfo::Off  );
static SilenceActionSetting keyUnSilence( "UnSilenceKeyID", UserInfo::Off  );
static SilenceActionSetting rangeUnSilence( "UnSilenceIPRange", UserInfo::Off  );

//! sets someone's ranking
class RankActionSetting: public ActionSetting
{
public:
    RankActionSetting( char const * name )
    : ActionSetting( Setting::Player, name )
    {}

    //! activates the action with the specified value
    virtual void DoRead( std::istream & source )
    {
        std::ws( source );
        std::string id;
        float ratingScore;
        
        source >> id;
        if ( source.good() )
        {
            std::ws( source );
        }
        if ( source.good() )
        {
            source >> ratingScore;
        }
        if ( !source.fail() )
        {
            UserInfo userInfo;
            userInfo.ratingScore = ratingScore;
            UserInfo::Merge( id, userInfo );
        }
        else
        {
            Log::Err() << "Usage: Rank <IP/KeyID/IP Range> <rating score>\n";
        }
    }
};
static RankActionSetting rank( "Rank" );
static RankActionSetting rating( "Rating" );
static RankActionSetting rankID( "RankKeyID" );
static RankActionSetting ratingID( "RatingKeyID" );
static RankActionSetting rankIP( "RankIP" );
static RankActionSetting ratingIP( "RatingIP" );


//! sets someone's ranking
class ForceNameActionSetting: public ActionSetting
{
public:
    ForceNameActionSetting( char const * name )
    : ActionSetting( Setting::Player, name )
    {}

    //! activates the action with the specified value
    virtual void DoRead( std::istream & source )
    {
        std::ws( source );
        std::string id;
        std::string forceName;
        
        source >> id;
        if ( source.good() )
        {
            std::ws( source );
        }
        if ( source.good() )
        {
            getline( source, forceName );
        }
        if ( !source.fail() && forceName.size() > 0 )
        {
            UserInfo userInfo;
            userInfo.forceName = forceName;
            UserInfo::Merge( id, userInfo );
        }
        else
        {
            Log::Err() << "Usage: ForceName <IP/KeyID/IP Range> <Name>, where <Name> = NONE undoes a previous ForceName command.\n";
        }
    }
};
static ForceNameActionSetting forceName( "ForceName" );
static ForceNameActionSetting forceNameID( "ForceNameKeyID" );
static ForceNameActionSetting forceNameIP( "ForceNameIP" );
static ForceNameActionSetting forceNameIPRange( "ForceNameIPRange" );
