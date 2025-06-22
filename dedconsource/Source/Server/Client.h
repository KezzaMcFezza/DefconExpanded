/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_Client_INCLUDED
#define DEDCON_Client_INCLUDED

#include "Lib/Globals.h"
#include <deque>
#include <algorithm>

#include "Recordings/Sequence.h"
#include "GameSettings/Settings.h"
#include "Metaserver/MetaServer.h"
#include "Lib/UserInfo.h"

class FTPClient;

#ifdef COMPILE_DEDWINIA
unsigned int RandomSeed();
void SetRandomSeed( unsigned int seed );
#endif

//! while an object of this class exist, player state changes
//! are not sent over the network
class StateChangeNetMuter
{
public:
    StateChangeNetMuter();
    ~StateChangeNetMuter();
};

//! while an object of this class exist, player state changes
//! are not logged
class StateChangeLogMuter
{
public:
    StateChangeLogMuter();
    ~StateChangeLogMuter();
};


//! averager for ping samples
class PingAverager
{
private:
    float sum;         //!< the sum of the samples
    float sumSquared;  //!< the sum of the squares of the samples
    float weight;      //!< the sum of the weights
public:
    // start with 2 seconds of ping
    PingAverager():sum( 0 ), sumSquared( 10000 ), weight( .1 ){}

    //! returns the average ping
    float GetAverage() const { return sum / weight; }

    //! returns the variance (average fluctuation)
    float GetVariance() const;

    void Add( float sample, float weight = 1 ); //!< add a ping sample

    void Decay( float factor ); //!< reduces the significance of the data gathered so far by the given factor
};

//! data bit describing a sequence that was sent to the client, but not yet acknowledged.
struct SequenceAck
{
    int timesSent;       //!< the number of times this sequence has been sent already
    Time lastSent;       //!< the last time this 
    Sequence * sequence; //!< the sequence

    SequenceAck( Sequence * s )
    : timesSent(0), sequence( s )
    {}
};

class IgnoreList
{
public:
    //! the list of player IDs a player is ignored by
    std::vector< int > ignoredBy;
    
    //! check whether player is ignoring this
    bool IgnoredBy( int player ) const
    {
        return std::find( ignoredBy.begin(), ignoredBy.end(), player ) != ignoredBy.end();
    }

    //! adds a player
    void Add( int player )
    {
        if ( !IgnoredBy( player ) )
            ignoredBy.push_back( player );
    }

    //! removes a player
    void Remove( int player )
    {
        // find the player
        std::vector< int >::iterator found = std::find( ignoredBy.begin(), ignoredBy.end(), player );
        if( found != ignoredBy.end() )
        {
            // move the last player there instead
            *found = ignoredBy.back();
        
            // shrink
            ignoredBy.pop_back();
        }
    }
};

//! player data
class SuperPower
{
public:
    typedef ListBase< SuperPower >::iterator iterator;
    typedef ListBase< SuperPower >::const_iterator const_iterator;

    //! the various existence states
    enum State { NonEx, Ex, Spec, Game };
#if VANILLA
    enum { MaxSuperPowers = 6 };
#elif EIGHTPLAYER
    enum { MaxSuperPowers = 8 };
#elif TENPLAYER
    enum { MaxSuperPowers = 10 };
#endif

    //! reason given to the client why the connection was dropped from the server side
    enum KickReason {
#ifdef COMPILE_DEDCON
        Left = 0,
        Dropped = 1,             // connection loss
        ServerQuit = 2,
        InvalidKey = 3,
        DuplicateKey = 4,
        AuthenticationFailed = 5,
        WrongPassword = 6,
        ServerFull = 7,
        Kick = 8,
        Demo = 9,
#else
        Left = 0,
        Dropped = 1,             // connection loss (guessing)
        ServerQuit = 9,
        InvalidKey = 9,
        DuplicateKey = 5,
        AuthenticationFailed = 9,
        WrongPassword = 6,
        ServerFull = 3,
        Kick = 9, // this is correct
        Demo = 9,
#endif
        NoMessage = 100
    };

    //! returns the superpower playing as a certain team
    static SuperPower * GetTeam( int teamID );

    //! returns the first superpower
    static iterator GetFirstSuperPower();

    //! checks whether the given team ID is used
    static bool TeamIDTaken( int teamID );

    //! returns a free team ID
    static int GetFreeTeamID();

    //! call on all changes
    void OnChange();

    //! switch to an existing state
    void Exist();
    
    //! vanish into nothingness
    void Vanish( KickReason reason = Left );

    //! spectate
    void Spectate();
      
    //! play
    void Play( int teamID, bool reconnect = false );

    //! play. Return value: success (failure means no team ID was free)
    bool Play( bool reconnect = false );

    //! set the identification number
    void SetID( int id )
    {
        ID = id;
    }

    //! picks an arbitrary ID ( increasing from 1 on )
    void PickID();

    //! sends a readyness change signal
    void SendToggleReady();

    //! toggles internal readyness state
    void ToggleReady( bool manual = false );

    //! sends a ready state toggle command for all clients that are marked as ready. The flag 
    //! controls whether the internal readyness state should be changed. Setting it to false
    //! could be useful if you want to ready the players again later.
    static void UnreadyAll( bool changeInternalState = true );

    //! sends a ready state toggle command for all clients that are marked as ready.
    static void UnreadyAll( SuperPower & cause );

    //! resets the manual readyness state of everyone
    static void ResetManualReadyness();

    //! returns the seqID of the last change
    int LastChange() const
    {
        return lastChange;
    }

    //! returns the identification number
    int GetID() const
    {
        return ID;
    }

    //! returns the ID of the team
    int GetTeamID() const
    {
        return teamID;
    }

    //! returns the ID of the alliance the player is in
    int GetAllianceID() const
    {
        return allianceID;
    }

    //! sets the ID of the alliance the player is in
    void SetAllianceID( int allianceID_, char const * reason = nullptr );

    //! automatically determines the alliance
    void SetAllianceID( char const * reason = nullptr );

    //! determines whether this superpower is in the game
    bool IsPlaying() const
    {
        return teamID >= 0 && state == Game;
    }

    //! returns the current existence state
    State GetState() const
    {
        return state;
    }

    //! returns whether the superpower is a demo player
    bool GetDemo() const
    {
        return demo;
    }

    //! determines whether the superpower is a demo player
    void SetDemo( bool d )
    {
        demo = d;
    }
 
    //! returns the rating score
    float GetRating() const
    {
        return info.ratingScore;
    }

    //! sets the rating score
    void SetRating( float s )
    {
        info.ratingScore = s;
    }

    //! sets the client info from the database
    void SetClientInfo( ClientInfo & info_ )
    {
        info = info_;
    }

    bool IsSilenced() const
    {
        return info.silenced == ClientInfo::On;
    }

    void Silence()
    {
        info.silenced = ClientInfo::On;
    }

    std::string const & ForceName() const
    {
        return info.forceName;
    }

    //! returns the score
    float GetScore() const
    {
        return score;
    }

    //! returns the score
    float GetScoreSigned() const
    {
        return scoreSigned;
    }

    //! sets the score
    void SetScore( float s );

    //! checks whether the score was et
    bool ScoreSet() const
    {
        return scoreSet;
    }

    //! sets the score
    void SetScoreSigned( float s );

    //! signs the score
    void SignScore();

    //! fills an array with superpowers sorted by score
    static void GetSortedByScore( std::vector< SuperPower * > & powers );

    //! returns whether the superpower is a human
    bool GetHuman() const
    {
        return human;
    }

    //! determines whether the superpower is a human
    void SetHuman( bool h )
    {
        human = h;
    }

    //! returns the random seed
    unsigned char GetRandomSeed() const
    {
        return randomSeed;
    }

    //! sets the random seed
    void SetRandomSeed( unsigned char seed )
    {
        randomSeed = seed;
    }

    //! get the raw name
    std::string const & GetName() const
    {
        return name;
    }

    //! get the name for printing
    std::string GetPrintName() const;

    void SetAndSendName( std::string const & name );
    void SetAndSendColor( int color );

#ifdef COMPILE_DEDCON
    //! toggles territory possession (internal state only)
    void OnlyToggleTerritory( int territory );

    //! sends territory toggle message 
    void SendToggleTerritory( int territory );

    //! toggles and sends territory possession
    void ToggleTerritory( int territory );

    int GetTerritory() const
    {
        return territory;
    }
#endif

    //! sets the name
    void SetName( std::string const & name_ )
    {
        name = name_;
        nameSet = true;
    }

    bool NameSet() const
    {
        return nameSet;
    }

    int Color() const
    {
        return allianceID;
    }

    void SetColor( int c )
    {
        allianceID = c;
    }

    bool IsReady() const
    {
        return ready;
    }

    bool IsReadyManual() const
    {
        return readyManual;
    }

    Time const & ReadySince() const
    {
        return readySince;
    }

    static int currentID; // the ID the next superpower is going to get
protected:
    SuperPower();
    virtual ~SuperPower();
private:
    State state;   //!< the current state
    int ID;        //!< the superpower ID (or client ID; even spectators get one)
    int teamID;    //!< the ingame ID (only players get these)
    int allianceID;//!< the ID of the alliance
    int territory; //!< the territory in single territory mode

    std::string name; //!< the superpower's name
    bool nameSet;     //!< has the name been set?

    float score;        //!< the score
    float scoreSigned;  //!< the score as last signed by the most players
    bool  scoreSet;     //!< flag indicating whether the score was set

    ClientInfo info;    //!< info from persistent user info database

    bool demo;     //!< demo mode?
    bool human;    //!< human or AI?

    unsigned char randomSeed; //!< the random seed value

    bool ready;    //!< ready to play?
    Time readySince; //!< when whas the last readyness change?
    bool readyManual; //!< the last manually executed readyness change

    int lastChange; //!< seqID of the last change
};

class SuperPowerList: public List< SuperPower >
{
public:
    SuperPowerList();
};

//! prevents certain annoying events from happening too often
class SpamProtection
{
public:
    SpamProtection();

    //! checks whether something annoying should be allowed
    bool Allow( float decay, float maxPerHour );
private:
    PingAverager averager; //!< averages the intervals between annoying things
    Time last;             //!< the last annoying thing
};

//! a client. Each connecting computer is represented by exactly one of those.
class Client
{
    friend class FTP;

    // the FTP part of the client
    FTPClient * ftpClient;
public:
    typedef SuperPower::KickReason KickReason;

    // warning modes for slowdown budget running out
    enum SlowBudgetWarning
    {
        SlowBudgetWarning_Gone,
        SlowBudgetWarning_10Seconds,
        SlowBudgetWarning_30Seconds,
        SlowBudgetWarning_1Minute,
        SlowBudgetWarning_2Minutes,
        SlowBudgetWarning_None
    };

    PingAverager pingAverager; //!< for averaging the ping
    PingAverager packetLoss;   //! for measuring packet loss
    
    SpamProtection chatSpam, nameChangeSpam; //!< spam protection from chat and name changes
    bool spammed;                //!< whether this client has been accused of spamming

    int chatFilterWarnings;      //!< number of chat filter warnings received
    
    int lastChatID;              //!< last chat ID

    Socket sock;               //!< only used in proxy mode: the socket used for communication with the server
    struct sockaddr_in saddr;  //!< the address of the client
    Time lastAck;              //!< the last time data was received
    Time lastActivity;         //!< the last time a client was actually active
    int  quitTime;             //!< the quit time in game ticks
    bool outOfSync;            //!< whether the client is considered out of sync currently
    int outOfSyncCount;        //!< how much the client is considered out of sync currently (for hysteresis)

    int lastSentSeqID;         //!< the largest SeqID sent to the client so far
    
    SuperPowerList id;         //!< the superpower represented by this client
    IgnoreList ignoredBy;      //!< list of players that ignore this one
    std::vector< Client const * > kickVotes; //!< list of other players who would like to see this client kicked
    bool rankWish;             //!< whether this client wishes a ranked game

    int bandwidthControl;         //!< subtract to it when sending stuff, add when bandwidth gets available
    int bandwidthControlThisFrame;  //!< bandwidth control for this frame
    int writeAhead;                 //!< the current number of packets that writing is ahead of compared to acks received
    bool finishedSending;      //!< optimization flag for sending code

    bool navalBlacklist;       //!< should naval blacklists be applied?
    bool noPlay;               //!< is this client allowed to play?
    bool forceReady;           //!< is this client forced to be ready to play?

    Time joinTime;             //!< the time the player joined the server
    Time readyWarnTime;        //!< the time the player was warned about readying up
    bool readyWarn;            //!< whether the player was warned to ready up

    float slowBudget;          //!< the budged (in real time seconds) to slow the game down.
    SlowBudgetWarning slowBudgetWarning; //!< the warning mode

    bool scoresSigned;         //!< whether the player has singed the scores
    int  scoresLastSeen;       //!< the last revision of the scores the player has seen

    std::string lastSetName;   //!< the name the player tried to set last

    bool admin;                //!< is this client administrator?
    std::string adminName;     //!< the name of the player when he logged in as admin
    int adminLevel;            //!< the admin level. Lower numeric values are better.
    int loginAttemptsLeft;     //!< number of login attempts the client has left.

    bool inGame;               //!< has this client made it into the game?

    int speed;                 //!< the client's speed selection

    int numberOfKicks;         //!< how often has this player been kicked already?
    int numberOfSyncs;         //!< how often has this player tried to resync?
    int numberOfAborts;        //!< how often has this player aborted the countdown?
    int numberOfLeaves;        //!< how often has this player left the active players to spectate?

    int joinedTick;            //!< the seqID the client joined at
    int desyncTick;            //!< first seqID with error

    std::string key;           //!< authentication key
    int keyID;                 //!< the key's ID
    std::string version;       //!< version the client is running
    std::string platform;      //!< platform the client is using

    Sequence * loginAck;       //!< the sequence containing the special "login accepted" tag of this client

#ifdef COMPILE_DEDWINIA
    bool directControl;         //!< flag indicating whether a unit is currently selected that may receive direct control
#endif

    int  achievements;          //!< viral achievement flags

    int GetAchievements() const
    {
        return achievements;
    }

    void SetAchievements( int flags );

    //! signals this client is fully connected now
    void Connect();

    //! checks whether this client is fully connected
    bool IsConnected() const { return connected; }

    //! queue of not yet acknowledged/not yet sent sequences for this client
    std::deque< SequenceAck > notYetAcknowledged;

    //! queue of received, but not yet processed, sequences
    std::deque< SequenceAck > notYetProcessed;

    //! sends a login ack packet to this client and stores it
    void LogIn( Sequence * ack );

    //! drops the connection to this client, giving it a reason
    void Quit( KickReason reason );

    //! sends the client the "you're out of sync" message
    void OutOfSync( bool outOfSync, int seqID );

    //! sets the client's address
    void SetAddr( sockaddr_in const & fromaddr );

    //! sets a new speed
    void SetSpeed( int factor );
    
    //! returns the client name
    std::string const & GetName() const { return id.GetName(); }

    //! returns the client ID
    int GetID() const { return id.GetID(); }

    //! returns the client Team ID
    int GetTeamID() const { return id.GetTeamID(); }

    //! returns whether the client is playing
    bool IsPlaying() const { return id.IsPlaying(); }

    //! checks to run on setting changes
    static void OnSettingChange();

    //! check whether the game is ready to start
    static void CheckStart();

    //! checks for illegal commands
    static void CheckOwnership( int teamID, int objectId );

    //! counts the number of players
    static void Count( int & total, int & players, int & spectators );

    //! calculate slowdown budget usage. Returns current speed.
    static int TimeBudget( float dt );

    //! resets the manual readyness state of everyone
    static void ResetManualReadyness();

    static int CountClients()
    {
        int total, players, spectators;
        Count( total, players, spectators );
        return total;
    }


    static int CountSpectators()
    {
        int total, players, spectators;
        Count( total, players, spectators );
        return spectators;
    }

    static int CountPlayers()
    {
        int total, players, spectators;
        Count( total, players, spectators );
        return players;
    }

    Client( Socket & serverSocket_ );
    virtual ~Client();

    typedef ListBase<Client>::const_iterator const_iterator;
    typedef ListBase<Client>::iterator       iterator;

    //! returns the list of all clients
    static ListBase<Client> & GetClients();
    static iterator           GetFirstClient();

    //! notifies the client system that the server's key ID is now known
    static void OnServerKeyID();
    
    void Silence();

    //! checks whether the client is on the LAN
    bool IsOnLAN() const;

    void ReadFrom( TagReader & reader ); //!< read a tag that was sent by this client
    void SendTo();                       //!< send outstanding data to this client
    static void SendToAll();             //!< send outstanding data to all clients
    static void DeleteAll();             //!< delete all clients
private:
    bool connected;            //!< is this client fully connected?
    Socket & serverSocket;     //!< the server socket to use
};

typedef List<Client>                     ClientList;

std::ostream & operator << ( std::ostream & s, SuperPower const & id );
std::ostream & operator << ( std::ostream & s, Client const & client );

extern IntegerSetting packetOverhead;

#endif // DEDCON_Client_INCLUDED
