/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "Main/config.h"

#include "Client.h"

#include "Lib/Codes.h"

#include "Recordings/GameHistory.h"
#include "GameSettings/GameSettings.h"
#include "Recordings/AutoTagWriter.h"
#include "Metaserver/Advertiser.h"
#include "Recordings/TagReader.h"
#include "Lib/Globals.h"
#include "Codes/defcon.h"
#include "Network/Exception.h"
#include "Recordings/DumpBuff.h"
#include "GameSettings/Rotator.h"
#include "Lib/Unicode.h"
#include "GameSettings/ChatFilter.h"
#include "Host.h"
#include "CPU.h"
#include "Main/Log.h"
#include <sstream>
#include <map>
#include <set>
#include <iomanip>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "Network/FTP.h"

static IntegerSetting minRating( Setting::ExtendedGameOption, "MinRating", -0x80000000 );
static IntegerSetting maxRating( Setting::ExtendedGameOption, "MaxRating",  0x7fffffff );

// anoymization
static BoolSetting anonymize( Setting::ExtendedGameOption, "Anonymise", 0 );

// checks whether a given player is allowed to play. Returns NULL if he can,
// a reason why he can't otherwise.
static char const * PlayVeto( SuperPower const & power )
{
    if ( !minRating.IsDefault() && minRating.Get() > power.GetRating() )
    {
        return "rating is too low";
    }
    if ( !maxRating.IsDefault() && maxRating.Get() < power.GetRating() )
    {
        return "rating is too high";
    }

    return NULL;
}

#ifndef COMPILE_DEDWINIA
// minimal admin level required to play
static IntegerSetting playLevel( Setting::Admin, "AdminLevelPlay", 1000 );
#endif

// checks whether a given player is allowed to play. Returns NULL if he can,
// a reason why he can't otherwise.
static char const * PlayVeto( Client const * player )
{
    if( !player )
    {
        return "you do not exist";
    }

#ifndef COMPILE_DEDWINIA
    int adminLevel = player->admin ? player->adminLevel : 1000;
    if( adminLevel > playLevel )
    {
        if( !player->admin )
        {
            return "you need to log in to play";
        }
        else
        {
            return "your admin level is not high enough";
        }
    }
#endif

    return PlayVeto(player->id);
}

static unsigned int & AccessRandomSeed()
{
    static unsigned int r = rand();
    return r;
}

unsigned int RandomSeed()
{
    // return random();
    return AccessRandomSeed();
}

void SetRandomSeed( unsigned int seed )
{
    AccessRandomSeed() = seed;
}

// the best signers of the score so far
static std::vector< Client * > bestSigners;

// the list of deleted clients (yeah, we keep them around the whole game)
static ListBase<Client>  deletedClients;

static void UnsignAllScores();

// writes a key ID to a stream
class KeyIDWriter
{
public:
    KeyIDWriter( int id ): id( id ){}
    int id;
};

std::ostream & operator << ( std::ostream & s, KeyIDWriter const & w )
{
    if ( w.id >= 0 )
    {
        s << w.id;
    }
    else if ( w.id == -2 )
    {
        s << "DEMO";
    }
    else
    {
        s << "UNKNOWN";
    }

    return s;
}

// compares two keys
static bool KeysEqual( int keyIDA, std::string const & keyA, int keyIDB, std::string const & keyB )
{
    // demo keys are never equal to real keys
    if ( keyIDA < 0 && keyIDB >= 0 )
        return false;
    if ( keyIDA >= 0 && keyIDB < 0 )
        return false;

    // no key comparisons without working metaserver
    if ( metaServer && metaServer->Reachable() )
    {
        // demo keys are compared over their string
        if ( keyIDA < 0 && keyIDB < 0 )
            return keyA == keyB;

        // otherwise, compare the IDs
        return keyIDA == keyIDB;
    }

    // fallback: not equal.
    return false;
}

// print a quoted name
static void PrintQuoted( std::ostream & s, std::string const & name )
{
    for ( unsigned int i = 0; i < name.size(); ++i )
    {
        char c= name[i];

        switch(c)
        {
        case '(':
        case ')':
        case '\\':
        case '\"':
        case '\'':
            s << '\\' << c;
            break;
        default:
            s << c;
        }
    }
}

std::ostream & operator << ( std::ostream & s, SuperPower const & id )
{
    if ( id.GetID() < 0 )
    {
        s << "unknown client";
    }
    else
    {
        s << "client " << id.GetID();
        std::string name = id.GetPrintName();

        if ( name.size() > 0 )
        {
            s << " (";
            PrintQuoted( s, name );
            s << ")";
        }
    }

    return s;
}

std::ostream & operator << ( std::ostream & s, Client const & client )
{
    return s << client.id;
}

static IntegerSetting saveKeyID( Setting::Log, "SaveKeyID", 1 );
static IntegerSetting saveIP( Setting::Log, "SaveIP", 0 );

// writes a client with full info
class ClientWriter
{
public:
    ClientWriter( Client & c ): client( c ){}
    Client const & client;
};

std::ostream & operator << ( std::ostream & s, ClientWriter const & w )
{
    s << w.client;
    if( saveKeyID )
    {
        s << ", keyID = " << KeyIDWriter( w.client.keyID );
    }
    if( saveIP )
    {
        s << ", IP = " << inet_ntoa(w.client.saddr.sin_addr);
    }

    return s;
}

FStreamProxy GetGriefLog( Client const & griefer, char const * grief, bool eventLog = true )
{
    if ( eventLog )
    {
        Log::Event() << "CLIENT_GRIEF " << griefer.id.GetID() << " "
                     << grief << "\n";
    }

    return Log::GriefLog().GetStream()
        << "keyID = " << KeyIDWriter( griefer.keyID )
        << ", IP = " << inet_ntoa( griefer.saddr.sin_addr)
        << ", griefer = " << griefer
        << ", grief = " << grief;
}

void PingAverager::Add( float sample, float w )
{
    sum *= .99;
    sumSquared *= .99;
    weight *= .99;

    sumSquared += sample * sample * w;
    sum += sample * w;
    weight += w;
}

void PingAverager::Decay( float factor )
{
    sum *= factor;
    sumSquared *= factor;
    weight *= factor;
}

//! returns the variance (average fluctuation)
float PingAverager::GetVariance() const
{
    float average = GetAverage();
    float variance = sumSquared/weight - average * average;

    if ( variance > 0 )
        return sqrt( variance );
    else
        return 0;
}

// static bool teamIDTaken[ SuperPower::MaxSuperPowers ] =
// { false, false, false, false, false, false };

//! checks whether the given team ID is used
bool SuperPower::TeamIDTaken( int teamID )
{
    assert( 0 <= teamID && teamID < MaxSuperPowers );

    SuperPower * owner = GetTeam( teamID );

    return owner && owner->state == Game;
    // return teamIDTaken[ id ];
}

//! returns a free team ID
int SuperPower::GetFreeTeamID()
{
    for ( int i = 0; i < MaxSuperPowers; ++i )
    // for ( int i = settings.maxTeams.Get()-1; i >= 0; --i )
    {
        if ( !TeamIDTaken(i) )
        {
            return i;
        }
    }

    return -1;
}

static ListBase< SuperPower > & GetSuperPowers()
{
    static ListBase< SuperPower > superPowers(false);
    return superPowers;
}

//! returns the superpower playing as a certain team
SuperPower * SuperPower::GetTeam( int teamID )
{
    SuperPower * candidate = 0;
    for ( iterator i = GetFirstSuperPower(); i; ++i )
    {
        if ( i->GetTeamID() == teamID )
        {
            if ( !candidate || (int) i->state > (int) candidate->state )
            {
                candidate = &(*i);
            }
        }
    }

    return candidate;
}

SuperPower::iterator SuperPower::GetFirstSuperPower()
{
    return GetSuperPowers().begin();
}

SuperPowerList::SuperPowerList()
{
    Insert( GetSuperPowers() );
}

SuperPower::SuperPower()
: state( NonEx ), ID(-1), teamID( -1 ), allianceID( -2 ), territory( -1 ), nameSet(false), score(0), scoreSigned(0), scoreSet( false ), demo( false ), human( true ), ready( false )
{
    /* chris writes:
The mystery byte is a Random Seed value.  This value is randomly generated
by each client (using a call to time(NULL)) and then sent to the Server.  As
you already know, the server relays those random seeds back to every client.
When the game begins each client adds up all the random seeds of all the
teams, to produce the Game Random Seed value.  This value is used to
initialise the deterministic random number generator on each computer.

The reason for this is that you need deterministic random number generation
for the system to work, but if its totally deterministic then it will be the
same every time.  Eg in a 3 player game, each team would be randomly
assigned the same team in each game.  So this method is used to introduce
some random starting element.

So this means you can set the mystery byte to whatever you want, but each
client MUST receive the same random seed from each other client.  Eg if 3
clients are connected, and client 1 gets random bytes 10,56 and 200, then
client 2 must receive exactly the same random bytes in those ready packets.
*/

    static unsigned char mysteryByteOffset=rand()/( RAND_MAX >> 8 );
    randomSeed = static_cast< unsigned char >( gameHistory.GetTailSeqID() + mysteryByteOffset );

    readySince.GetTime();
    OnChange();
    readyManual = false;
}

SuperPower::~SuperPower()
{
    Vanish();
}


//! call on all changes
void SuperPower::OnChange()
{
    lastChange = gameHistory.GetTailSeqID();
}

// switch to an existing state
void SuperPower::Exist()
{
    if ( ID < 0 )
    {
        PickID();
    }

    if ( state == NonEx )
    {
        state = Ex;

#ifdef COMPILE_DEDWINIA
        if ( gameHistory.Playback() )
        {
            return;
        }
#endif

        // write creation message
        AutoTagWriter writer( C_TAG, C_PLAYER_JOIN );
        writer.WriteInt( C_PLAYER_ID, ID );

        // demo tag
        if ( demo )
        {
//#ifdef COMPILE_DEDCON
            writer.WriteBool( C_PLAYER_DEMO, true );
//#else
            // writer.WriteInt( C_PLAYER_DEMO, 1 );
//#endif
        }

#ifdef COMPILE_DEDWINIA
        Advertiser::WriteRandom( writer );
#endif

        gameHistory.AppendServer( writer );

        OnChange();
    }
}

// list of superpowers that are supposed to leave their current alliance
// static std::deque< Client * > toLeaveAlliance;

// call when a player leaves
static void OnPlayerLeave()
{
    // count the active players before the event
    int players = 0;
    for ( SuperPower::const_iterator id = SuperPower::GetFirstSuperPower(); id; ++id )
    {
        if ( id->IsPlaying() )
        {
            players++;
        }
    }

    // if the server was full, advertise it again
    if ( players == settings.maxTeams )
    {
        Advertiser::Force();
    }
}

// leave the unstoppable state, if need be, start the game
static void DontStopUnstoppable()
{
    if ( serverInfo.unstoppable )
    {
        // force a game start before we do anything
        // hard to control narrow situation
        while ( !serverInfo.started )
        {
            gameHistory.Bump();
        }
    }
}

class StateChangeNetMuter;
static StateChangeNetMuter * netMuter = 0;
static bool NetSilentStateChanges()
{
    return netMuter;
}

StateChangeNetMuter::StateChangeNetMuter(){ netMuter = this; }
StateChangeNetMuter::~StateChangeNetMuter(){ netMuter = 0; }

class StateChangeLogMuter;
static StateChangeLogMuter * logMuter = 0;
static bool LogSilentStateChanges()
{
    return logMuter;
}

StateChangeLogMuter::StateChangeLogMuter(){ logMuter = this; }
StateChangeLogMuter::~StateChangeLogMuter(){ logMuter = 0; }

// send an arbitrary color
static void SendColor( SuperPower const & power, int color )
{
#ifdef COMPILE_DEDCON
    // don't send illegal colors
    if ( color < 0 )
    {
        return;
    }
#endif

    gameHistory.RequestBump();
    AutoTagWriter writer( C_TAG, C_TEAM_COLOR_CHANGE );
    
    writer.
#ifdef COMPILE_DEDWINIA
    WriteInt
#endif
#ifdef COMPILE_DEDCON
    WriteChar
#endif
    ( C_TEAM_COLOR, color );

    writer.WriteChar( C_PLAYER_TEAM_ID, power.GetTeamID() );
    gameHistory.AppendClient( writer );
}

// send the correct color
static void SendColor( SuperPower const & power )
{
    SendColor( power, power.Color() );
}

// vanish into nothingness
void SuperPower::Vanish( KickReason reason )
{
    // avoid narrow countdown aborts
    DontStopUnstoppable();

    int oldTeamID = teamID;

    territory = -1;

#ifdef COMPILE_DEDWINIA
    if ( !nameSet )
    {
        SetName( "Vanished" );
    }

    // send a fake color
    // SendColor( *this, -1 );
#endif

    if ( state == Game )
    {
        serverInfo.everyoneReadySince = gameHistory.GetTailSeqID();

        if ( !LogSilentStateChanges() )
        {
            if ( serverInfo.started )
            {
                Log::Event() << "TEAM_ABANDON " << teamID << "\n";
            }
            else
            {   
                OnPlayerLeave();

                Log::Event() << "TEAM_LEAVE " << teamID << "\n";

                SuperPower::UnreadyAll();
                teamID = -1;
                SetAllianceID( -1 );
            }
        }
    }

    SuperPower::UnreadyAll( false );

    if ( state != NonEx )
    {
        state = NonEx;

        assert( ID >= 0 );

        if ( !NetSilentStateChanges() )
        {
            if ( human )
            {
                // write destruction message
                AutoTagWriter writer(C_TAG, C_PLAYER_REMOVED );
                writer.WriteInt( C_PLAYER_ID, ID );
                writer.WriteInt( C_LOGOUT_REASON, reason );
                gameHistory.AppendServer( writer );
            }
            else
            {
                // AIs get a completely different message, strange
                AutoTagWriter writer( C_TAG, "cx" );
                writer.WriteChar( C_PLAYER_TEAM_ID, oldTeamID );
                gameHistory.AppendClient( writer );
            }
        }

        OnChange();
    }

    SuperPower::UnreadyAll( false );
}

// spectate
void SuperPower::Spectate()
{
    // avoid narrow countdown aborts
    DontStopUnstoppable();

    ready = false;

    if ( state == Game )
    {
        OnPlayerLeave();

        Log::Event() << "TEAM_LEAVE " << teamID << "\n";

        serverInfo.everyoneReadySince = gameHistory.GetTailSeqID();

        territory = -1;

#ifdef COMPILE_DEDWINIA
        // just disappear into the shadows, but play for team evil
        {
            StateChangeLogMuter muter;

            Vanish();
            if ( !serverInfo.started )
            {
                Play(4);
                Vanish();
            }
        }
#endif        

        // teamIDTaken[ teamID ] = false;

        if ( !serverInfo.started )
        {
            SuperPower::UnreadyAll( true );
        }
    }

    // CPU players can't be spectators
    if ( !human )
    {
        Vanish();
    }
    else if ( state != Spec )
    {
        if ( !serverInfo.started )
        {
            SuperPower::UnreadyAll( false );
        }

        Exist();

        state = Spec;

        teamID = -1;
        SetAllianceID( -1 );

#ifdef COMPILE_DEDCON
        if ( !NetSilentStateChanges() )
        {
            AutoTagWriter writer(C_TAG, C_PLAYER_SPECTATE);
            writer.WriteInt( C_PLAYER_ID, ID );
            gameHistory.AppendServer( writer );
        }
#endif

        if ( !serverInfo.started )
        {
            SuperPower::UnreadyAll( false );
        }

        OnChange();
    }
}

// play
bool SuperPower::Play( bool reconnect )
{
    if ( state != Game )
    {
        int teamID = GetFreeTeamID();
        if ( teamID < 0 )
            return false;

        Play( teamID, reconnect );
    }

    return true;
}

#ifdef COMPILE_DEDCON
#define IS_HUMAN 1
#define IS_CPU   3
#endif

#ifdef COMPILE_DEDWINIA
#define IS_HUMAN 0
#define IS_CPU   2
#endif

// play
void SuperPower::Play( int teamID_, bool reconnect )
{
    if ( state != Game )
    {
        Exist();

        state = Game;

        // assert( teamIDTaken[ teamID ] == false );

        // teamIDTaken[ teamID ] = true;

        // on reconnection, this message causes a clone to appear, we don't want that.
        if ( !reconnect )
        {
            if ( !LogSilentStateChanges() )
            {
                Log::Event() << "TEAM_ENTER " << teamID_ << " " << ID << "\n";
            }

            if ( !NetSilentStateChanges() )
            {
                AutoTagWriter writer( C_TAG, C_PLAYER_TEAM_ENTER );
                writer.WriteInt( C_PLAYER_TEAM_ID, teamID_ );
                writer.WriteInt( C_PLAYER_HUMAN, human ? IS_HUMAN : IS_CPU );
                writer.WriteInt( C_PLAYER_ID, ID );

                if ( demo )
                {
                    writer.WriteBool( C_PLAYER_DEMO, true );
                }

                gameHistory.AppendServer( writer );
            }

            teamID = teamID_;

            SetAllianceID();
        }
        else
        {
            if ( !LogSilentStateChanges() )
            {
                Log::Event() << "TEAM_RECONNECT " << teamID_ << " " << ID << "\n";
            }

            teamID = teamID_;

        }

        OnChange();
    }
}

int SuperPower::currentID = SuperPower::MaxSuperPowers;
// int SuperPower::currentID = 0;

void SuperPower::PickID()
{
    // give each Superpower a new ID
    SetID( currentID++ );
}

void SuperPower::SendToggleReady()
{
    AutoTagWriter writer( C_TAG, C_PLAYER_READY );
    writer.WriteChar(C_PLAYER_TEAM_ID, GetTeamID() );

#ifdef COMPILE_DEDCON
    writer.WriteChar(C_RANDOM_SEED, randomSeed);
#endif

    gameHistory.AppendClient( writer );
}

void SuperPower::ToggleReady( bool manual )
{
    ready = !ready;

    serverInfo.everyoneReadySince = gameHistory.GetTailSeqID();

    if ( ready && !readyManual )
    {
        readySince.GetTime();
    }

    if ( manual )
    {
        readyManual = ready;
    }

    Log::Event() << "TEAM_READY " << GetTeamID()
                 << " " << ready << "\n";
}

void SuperPower::UnreadyAll( bool changeInternalState )
{
    serverInfo.everyoneReady = false;

    // don't do anything if the countdown is unstoppable
    if ( serverInfo.unstoppable )
        return;

    bool needBump = false;

    for ( ListBase< SuperPower >::iterator c = GetSuperPowers().begin(); c; ++c )
    {
        if ( c->ready )
        {
            c->SendToggleReady();

            needBump = true;

            if ( changeInternalState )
                c->ready = false;
        }
    }

    if ( Log::GetLevel() >= 1 && changeInternalState && needBump )
    {
        Log::Out() << "All players unreadied.\n";
    }

    serverInfo.everyoneReadySince = gameHistory.GetTailSeqID();

    if ( needBump )
        gameHistory.RequestBump();
}

void SuperPower::UnreadyAll( SuperPower & cause )
{
    // the player causing the unreadyness shall be treated as if he unreadied manually
    if ( cause.IsReady() )
        cause.ToggleReady( true );

    // the others shall be marked as automatic unreadyings
    UnreadyAll();
}

void SuperPower::ResetManualReadyness()
{
    // make readyManual be equal to ready, and reset last readyness time
    for ( ListBase< SuperPower >::iterator c = GetSuperPowers().begin(); c; ++c )
    {
        c->readyManual = c->ready;
        c->readySince.GetTime();
    }
}

void SuperPower::SetAllianceID( int allianceID_, char const * reason )
{
    allianceID = allianceID_;
#ifndef COMPILE_DEDWINIA
    if ( allianceID >= 0 )
    {
        if ( !reason )
            reason = "SWITCH";

        Log::Event() << "TEAM_ALLIANCE " << teamID << " " << allianceID << " " << reason << "\n";
    }
#endif
}

void SuperPower::SetAllianceID( char const * reason )
{
    SetAllianceID( -1 );

    // find free alliance
    const int count = MaxSuperPowers;
#if VANILLA
    bool allianceUsed[count] = { false, false, false, false, false, false };
#elif EIGHTPLAYER
    bool allianceUsed[count] = { false, false, false, false, false, false, false, false };
#elif TENPLAYER
    bool allianceUsed[count] = { false, false, false, false, false, false, false, false, false, false };
#endif

    for ( SuperPower::iterator i = GetSuperPowers().begin(); i; ++i )
    {
        int alliance = i->GetAllianceID();
        if ( alliance >= 0 )
        {
            allianceUsed[alliance] = true;
        }
    }

    // set the first available free alliance
    for ( int alliance = 0; alliance < count; ++alliance )
    {
        if ( !allianceUsed[alliance] )
        {
            SetAllianceID( alliance, reason );
            return;
        }
    }
}

//! get the name for printing
std::string SuperPower::GetPrintName() const
{
    if ( human )
    {
        if ( demo )
        {
            return "[DEMO]" + name;
        }
        else
        {
            return name;
        }
    }
    else
    {
        return "[CPU]" + name;
    }
}

void SuperPower::SetAndSendName( std::string const & name )
{
    gameHistory.RequestBump();
    AutoTagWriter writer(C_TAG, C_PLAYER_RENAME );
    
    if ( !NetSilentStateChanges() )
    {
        if ( IsPlaying() )
        {
            writer.WriteChar(C_PLAYER_TEAM_ID,GetTeamID());
        }
        else
        {
#ifdef COMPILE_DEDCON
            writer.WriteChar(C_PLAYER_TEAM_ID,GetID());
            writer.WriteInt(C_PLAYER_SPECTATOR,1);
#else
            writer.WriteChar(C_PLAYER_TEAM_ID,GetID());
            writer.WriteInt(C_PLAYER_SPECTATOR,1);
        // writer.WriteChar(C_PLAYER_ID,GetID());
#endif
        }
        writer.WriteFlexString(C_PLAYER_NAME,name);
        gameHistory.AppendClient( writer );
    }

    SetName( name );

    if ( !LogSilentStateChanges() )
    {
        if ( GetHuman() )
        {
            Log::Event() << "CLIENT_NAME " << GetID() << " "
                         << name << "\n";
        }
        else
        {
            Log::Event() << "CPU_NAME " << GetID()
                         << " " << GetName() << "\n";
        }
    }
}

//! the current score revision
static int scoreRevision = 1;

//! sets the score
void SuperPower::SetScore( float s )
{
    ++scoreRevision;
    score = s;
    scoreSet = true;
}

//! signs the score
void SuperPower::SignScore()
{
    scoreSigned = score;
}

//! sets the score
void SuperPower::SetScoreSigned( float s )
{
    scoreSigned = score;
}

struct SortByScore
{
     bool operator()(SuperPower const * a, SuperPower const * b)
     {
         if ( bestSigners.size() > 0 )
         {
	     if ( a->GetScoreSigned() > b->GetScoreSigned() )
                 return true;
             if ( a->GetScoreSigned() < b->GetScoreSigned() )
                 return false;
         }
         else
         {
	     if ( a->GetScore() > b->GetScore() )
                 return true;
             if ( a->GetScore() < b->GetScore() )
                 return false;
         }

         return a->GetRating() > b->GetRating();
     }
};
// fills an array with superpowers sorted by score
void SuperPower::GetSortedByScore( std::vector< SuperPower * > & powers )
{
    // fill the array
    powers.clear();
#if VANILLA  
    powers.reserve( 6 );
    for ( int i = 0; i < 6; ++i )
#elif EIGHTPLAYER 
        powers.reserve(8);
    for (int i = 0; i < 8; ++i)
#elif TENPLAYER  
        powers.reserve(10);
    for (int i = 0; i < 10; ++i)
#endif
    {
        SuperPower * p = SuperPower::GetTeam( i );
        if ( p )
        {
            powers.push_back( p );
        }
    }

    // sort
    std::sort( powers.begin(), powers.end(), SortByScore() );
}












void Client::ResetManualReadyness()
{
    SuperPower::ResetManualReadyness();

    // warn everyone again
    for ( Client::iterator client = Client::GetFirstClient(); client; ++client )
    {
        client->readyWarn = false;
    }
}

void Client::SetAchievements( int flags )
{
    if ( flags != achievements )
    {
        achievements = flags;
        Log::Event() << "CLIENT_ACHIEVEMENTS " << GetID() << ' ' << achievements << '\n';
    }
}

void Client::LogIn( Sequence * ack )
{
    loginAck = ack;
    notYetAcknowledged.push_back( SequenceAck( ack ) );
}

// the last game time the majority of players was online
static int lastPlayerMajority = -1;

void Client::Quit( KickReason reason )
{
    FTP::DeleteFTPClient( ftpClient );
    ftpClient = 0;

    {
        // count the number of active players.
        int players = CountPlayers();
        if ( players * 2 >= serverInfo.totalPlayers )
        {
            // at least half the players are online now, remember that!
            lastPlayerMajority = gameHistory.GameTime();
        }
    }

    if ( reason == SuperPower::Kick )
        numberOfKicks++;

    // compose the login deny message
    if ( reason != SuperPower::NoMessage && reason != SuperPower::Dropped )
    {
        AutoTagWriter writer(C_TAG, C_LOGOUT_ACCEPTED);
        writer.WriteInt( C_LOGOUT_ACCEPTED, reason );

        writer.WriteInt( C_SEQID_CURRENT, 0xffffffff );

        // write our version
        writer.WriteString( C_METASERVER_SERVER_VERSION, settings.serverVersion );

        // send it to the client in three copies
        if ( serverInfo.dedicated )
        {
            writer.SendTo( serverSocket, saddr, PLEBIAN_ENCRYPTION );
            writer.SendTo( serverSocket, saddr, PLEBIAN_ENCRYPTION );
            writer.SendTo( serverSocket, saddr, PLEBIAN_ENCRYPTION );
        }
    }

    // and mark the client as not logged in
    delete loginAck;
    loginAck = 0;

    if ( id.GetID() >= 0 )
    {
        Log::Event() << "CLIENT_QUIT " << id.GetID() << " " << int(reason) << "\n";
    }

    // insert into the list of old clients
    ClientList * client = dynamic_cast< ClientList * >( this );
    if ( client )
    {
        // pretend he went back in sync before he left
        if ( client->outOfSync )
        {
            client->outOfSyncCount = 0;
            client->OutOfSync( false, gameHistory.GetTailSeqID() );
        }
        client->id.Vanish( reason );
        client->InsertSafely( deletedClients );

        // mark client as a dropped player who got replaced by a CPU and should
        // defect soon
        // if ( client->GetTeamID() >= 0 )
        // toLeaveAlliance.push_back( this );
    }

    if ( serverInfo.unstoppable )
    {
        // mark as replaced by AI
        id.SetHuman( false );
    }
}

void Client::OutOfSync( bool outOf, int seqID )
{
    // check whether this client is active
    for ( Client::const_iterator c = deletedClients.begin(); c; ++c )
    {
        if ( &(*c) == this )
            return;
    }

    // save first desynced seqID and restore it later
    if ( !outOf )
    {
        desyncTick = -1;
    }
    else if ( desyncTick < 0 )
    {
        desyncTick = seqID;
    }
    else
    {
        seqID = desyncTick;
    }

    static const int max = 10;
    outOfSyncCount += outOf ? 1 : -1;

    bool change = false;
    if ( outOfSync && outOfSyncCount <= 0 )
    {
        if ( Log::GetLevel() >= 2 )
        {
            Log::Out() << *this << " is back in sync.\n";
        }

        Log::Event() << "CLIENT_BACK_IN_SYNC " << id.GetID() << "\n";

        change = true;
        outOfSync = false;
    }
    if ( !outOfSync && outOfSyncCount >= max )
    {
        if ( Log::GetLevel() >= 2 )
        {
            Log::Out() << *this << " went out of sync.\n";
        }

#ifdef COMPILE_DEDWINIA
        // kick spectators
        if ( ( !serverInfo.started || id.GetTeamID() < 0 ) && !gameHistory.Playback() )
        {
            Quit( SuperPower::ServerFull );
            return;
        }
#endif

        Log::Event() << "CLIENT_OUT_OF_SYNC " << id.GetID() << "\n";

        change = true;
        outOfSync = true;
    }

    // only inform about changes
    if ( change )
    {
        // sanity check seqID, it can't be larger than the last processed ID
        SequenceAck * lastAck = NULL;
        if( notYetProcessed.size() > 0 )
        {
            lastAck = &notYetProcessed.front();
        }
        else if( notYetAcknowledged.size() > 0 )
        {
            lastAck = &notYetAcknowledged.front();
        }
        int lastSeqID = gameHistory.GetTailSeqID() - 1;
        if ( lastAck && lastAck->sequence )
        {
            lastSeqID = lastAck->sequence->SeqID() - 1;
        }

        if ( seqID > lastSeqID )
        {
            seqID = lastSeqID;
        }

        gameHistory.Bump();

        if ( outOfSync )
        {
            Host::GetHost().Say( "You have fallen out of sync with the other players.", *this );
        }

        // store the out of sync message in the game history
        AutoTagWriter writer( C_TAG, outOfSync ? C_PLAYER_SYNCERROR : C_PLAYER_SYNCERROR_RESOLVED );
        writer.WriteInt( C_PLAYER_ID, id.GetID() );
        std::stringstream stamp;
        stamp << Time::GetCurrentTimeString("%Y-%m-%d %H.%M.%S");
#ifdef COMPILE_DEDWINIA
        stamp << " " << seqID;
#endif
        writer.WriteString( C_PLAYER_SYNCERROR_STAMP, stamp.str() ); // format: "2007-06-01 0.48.18 seqID" (seqID only for multiwinia)
#ifdef COMPILE_DEDWINIA
        writer.WriteInt( C_PLAYER_SYNCERROR_SEQID, seqID );

        {
            static bool reported = false;
            if ( !reported )
            {
                std:: stringstream say;
                say << "A sync error has occured. Please help Introversion fix it. In your Multiwinia "
                "profile directory  (or the steam directory if you're using that version), "
                "you will find a text file with a name starting with " <<
                Time::GetCurrentTimeString("syncerror-%Y-%m-%d--%H.%M.%S-client") <<
                ". It contains precise information about the error. Please make a bug report "
                "on the Introversion forums and post the file there. Also note the server name: " <<
                settings.serverName.Get() << ".";
                Host::GetHost().Say( say.str() );
                reported = true;
            }
        }
#endif

        gameHistory.AppendServer( writer );
    }

    // clamp
    if ( outOfSyncCount < 0 )
        outOfSyncCount = 0;
    if ( outOfSyncCount > max )
        outOfSyncCount = max;
}

void Client::SetAddr( sockaddr_in const & fromaddr )
{
    memcpy(&(saddr), &fromaddr, sizeof(struct sockaddr_in));
}

void Client::SetSpeed( int factor )
{
    if ( id.IsPlaying() )
    {
        if ( Log::GetLevel() >= 2 )
            Log::Out() << *this << " set to a speed factor of " << factor << ".\n";

#ifdef COMPILE_DEDCON
        AutoTagWriter writer( C_TAG, C_SPEED );
        writer.WriteChar( C_PLAYER_TEAM_ID, id.GetTeamID() );
        writer.WriteChar( C_SPEED_FACTOR, factor );

        gameHistory.AppendClient( writer );
#endif

        speed = factor;
    }
}

//! time after which a client is considered idle
static IntegerSetting idleTime( Setting::Grief, "IdleTime", 300 );

//! time of inactivity after which the game is considered over
static IntegerSetting idleTimeGlobal( Setting::Grief, "IdleTimeGlobal", 7200 );

// the last global activity in real time and game time
static Time lastActivityGlobal;
int  lastActivityGlobalGameTime = -1;

static void Timestamp()
{
#ifndef COMPILE_DEDWINIA
    // clear score signaures
    UnsignAllScores();
    bestSigners.clear();
#endif

    // timestamp
    lastActivityGlobal.GetTime();
    lastActivityGlobalGameTime = gameHistory.GameTime();
}

#ifdef COMPILE_DEDCON
static IntegerSetting ghostProtection( Setting::Grief, "GhostProtection", 2, 2 ); // set to 1 to allow spectators and players from the same IP, but issue a warning. Set to 2 to auto-kick the spectators.

// checks whether a specific IP is already used by an active player
static bool IPUsedByPlayerAndSpectator( Client const & player, Client const & spectator )
{
    // same client is OK, as is same IP and same port.
    return &spectator != &player && player.saddr.sin_addr.s_addr == spectator.saddr.sin_addr.s_addr && player.saddr.sin_port != spectator.saddr.sin_port && ( player.id.IsPlaying() || player.inGame );
}

// checks whether a specific IP is already used by an active player
static Client const * IPUsedByPlayer( Client const & spectator )
{
    for ( Client::const_iterator c = Client::GetFirstClient(); c; ++c )
    {
        if ( IPUsedByPlayerAndSpectator( *c, spectator ) )
        {
            return &(*c);
        }
    }

    for ( Client::const_iterator c = deletedClients.begin(); c; ++c )
    {
        if ( IPUsedByPlayerAndSpectator( *c, spectator ) )
        {
            return &(*c);
        }
    }

    return NULL;
}

static void GhostWarning( Client const & player, Client const & spectator )
{
    std::stringstream s;
    s << "Warning: player " << player << " and spectator " << spectator << " are connected from the same IP, it is likely that the spectator is gathering intel for the player.";
    Host::GetHost().Say( s.str() );
}

static void GhostKick( Client const & player, Client & spectator, bool warn )
{
    if ( warn )
    {
        std::stringstream s;
        s << "Player " << player << " and spectator " << spectator << " are connected from the same IP, it is likely that the spectator is gathering intel for the player. Kicking the spectator.";
        Host::GetHost().Say( s.str() );
    }
    spectator.Quit( SuperPower::Kick );
}

// checks whether the specified spectator is a ghost, returns true if he was kicked
static bool CheckGhosting( Client & spectator, bool warnKick )
{
    // spectator is no spectator?
    if ( spectator.id.IsPlaying() )
    {
        return  false;
    }

    Client const * body = IPUsedByPlayer( spectator );
    if ( body )
    {
        switch( ghostProtection.Get() )
        {
        case 0:
            // do nothing
            break;
        case 1:
            // warn
            GhostWarning( *body, spectator );
            break;
        case 2:
            // kick
            GhostKick( *body, spectator, warnKick );
        return true;
            break;
        default:
            assert( 0 );
        }
    }

    return false;
}
#else
static bool CheckGhosting( Client & spectator, bool warnKick ){ return false; }
#endif

// does everything that needs to be done at game start
static void StartGame()
{
    // force advertising
    Advertiser::Force();

    bool ranked = false;
    for ( Client::iterator player = Client::GetFirstClient(); player; ++player )
    {
        if ( player->IsPlaying() )
        {
            ranked |= player->rankWish;
        }
    }
    Log::Event() << "RANKED " << ranked << "\n";

    if ( Log::GetLevel() >= 2 )
        Log::Out() << "Game started. Players:\n";

    // start the game
    gameClock.ticksPerSecond = 10;
    serverInfo.started = true;
    Timestamp();

    SettingBase::StartLogging();

    Log::Event() << "GAME_START\n";
    
    Rotator::Save();

    settings.SendSettings();

    // lock important settings
#ifdef COMPILE_DEDWINIA
    Map::Lock();
    settings.gameMode.Lock();
    settings.maxTeams.Lock();
    settings.coopMode.Lock();
#endif

    // mark players
    {
        for ( Client::iterator c = Client::GetFirstClient(); c; ++c )
        {
            if ( c->id.IsPlaying() )
            {
                c->inGame = true;

                Log::Out()                   << "Team " << c->id.GetTeamID() << " : " << ClientWriter( *c ) << "\n";
                Log::PlayerLog().GetStream() << "Team " << c->id.GetTeamID() << " : " << ClientWriter( *c ) << "\n";
            }
        }
    }

    // kick spectators that have the same IP as players
    {
        for ( Client::iterator c = Client::GetFirstClient(); c; ++c )
        {
            if ( !c->id.IsPlaying() )
            {
                CheckGhosting( *c, true );
            }
        }
    }

    // get random seed
#ifdef COMPILE_DEDCON
    std::vector< SuperPower * > powers;
    SuperPower::GetSortedByScore( powers );
    int seed = 0;
    for( std::vector< SuperPower * >::iterator i = powers.begin(); i != powers.end(); ++i )
    {
        seed = seed + (*i)->GetRandomSeed();
    }

    Log::Out() << "Random seed: " << seed << " (0x" << std::setbase(16) << seed << std::setbase(10) << ")\n";
#endif

/*
    // bump history a bit to avoid fake starts
    for(int i = 2; i > 0; --i)
    {
        gameHistory.Bump();
    }
*/
}

// the number of seconds a readyness signal needs to be active to
// count as a vote
static IntegerSetting readyVoteDelay( Setting::Grief, "ReadyVoteDelay", 120 );

#ifdef COMPILE_DEDCON
char const * readyVoteWarningString = "Please ready up or leave, the other players want to start.";
#endif
#ifdef COMPILE_DEDWINIA
char const * readyVoteWarningString = "Please ready up, the other players want to start.";
#endif

static StringSetting  readyVoteWarning( Setting::Grief, "ReadyVoteWarning", readyVoteWarningString );

// maximum number of players to remove
static IntegerSetting readyVoteMaxRemove( Setting::Grief, "ReadyVoteMaxRemove", 1 );

// force players into readyness state
static void ReadyVote()
{
    if ( serverInfo.unstoppable )
    {
        // nothing to do
        return;
    }

    Time current;
    current.GetTime();

#ifdef COMPILE_DEDCON
    // in Dedcon, we remove unready players
    bool removeUnready = true;
#else
    // in Dedwinia, we just make them ready
    bool removeUnready = false;
#endif

    // see if we have spectators that can fill in
    int numDemoSpectators = 0;
    int numSpectators = 0;

    // the number of players voting to start the game
    int numVote = 0;
    int numAboutToVote = 0;

    // the number of players refusing to play
    int numUnready = 0;
    int numDemoUnready = 0;
    int numDemoNotUnready = 0;

    // number of players who are really legally currently marked as ready
    int numReallyReady = 0;

    // the number of players that are just ready, but not yet in vote status
    int numReady = 0;

    for ( Client::iterator player = Client::GetFirstClient(); player; ++player )
    {
        if ( player->id.IsPlaying() )
        {
            // enforce readyness
            if ( player->forceReady && !player->id.IsReady() )
            {
                player->id.ToggleReady( true );
                player->id.SendToggleReady();
            }

            // count the various states
            if ( player->id.IsReady() )
            {
                numReallyReady++;
            }

            if ( player->id.IsReadyManual() )
            {
                if ( (current - player->id.ReadySince()).Seconds() > readyVoteDelay )
                {
                    numVote++;
                }
                else
                {
                    numReady++;
                    if ( (current - player->id.ReadySince()).Seconds() > readyVoteDelay/2 )
                    {
                        numAboutToVote++;
                    }
                }

                if ( player->id.GetDemo() )
                {
                    numDemoNotUnready++;
                }
            }
            else
            {
                if ( player->id.GetDemo() )
                {
                    numDemoUnready++;
                }
                else
                {
                    numUnready++;
                }
            }
        }
        else
        {
            if ( player->id.GetDemo() )
            {
                numDemoSpectators++;
            }
            else
            {
                numSpectators++;
            }
        }
    }

    // the total number of players
    int totalPlayers = numVote + numReady + numDemoUnready + numUnready;

    // mark readyness status
    serverInfo.everyoneReady = ( totalPlayers <= numReallyReady );

    // see if replacing the unready players with spectators could get the game going
    if ( removeUnready )
    {
        int replacementPlayers = numVote + numReady + numSpectators - numDemoNotUnready;
        int demoReplacementPlayers = numDemoNotUnready + numDemoSpectators;
        if ( demoReplacementPlayers > demoLimits.maxPlayers )
        {
            demoReplacementPlayers = demoLimits.maxPlayers;
        }
        replacementPlayers += demoReplacementPlayers;

        if ( replacementPlayers < settings.minTeams )
        {
            // not enough players to start after the exchange, forget it
            return;
        }
    }

    // if a majority of players is really ready, revoke the manual readyness state of the others
    if ( numReallyReady + readyVoteMaxRemove >= totalPlayers && numReallyReady > 0 )
    {
        for ( Client::iterator player = Client::GetFirstClient(); player; ++player )
        {
            if ( player->id.IsPlaying() )
            {
                if ( !player->id.IsReady() && player->id.IsReadyManual() )
                {
                    // toggle the player twice so he is no longer marked as manually ready
                    player->id.ToggleReady( true );
                    player->id.ToggleReady( true );
                }
            }
        }
    }

    // see if the voters and players who signal they are ready will have a
    // sufficient advantage
    static Time lastWarnTime = current;
    if ( ( current - lastWarnTime ).Seconds() > 5 )
    {
        bool warn = numVote + numAboutToVote + readyVoteMaxRemove >= totalPlayers && numVote + numAboutToVote > 0;
        if ( warn )
        {
            lastWarnTime = current;

            // warn the unready players about it
            for ( Client::iterator player = Client::GetFirstClient(); player; ++player )
            {
                if ( player->id.IsPlaying() && !player->readyWarn && ( current - player->joinTime ).Seconds() >= 2*readyVoteDelay/3 )
                {
                    if ( !player->id.IsReadyManual() )
                    {
                        player->readyWarn = true;
                        player->readyWarnTime = current;

                        std::ostringstream s;
                        s << player->id.GetName() << " : " << readyVoteWarning.Get();
                        Host::GetHost().Say( s.str(), *player );
                    }
                }
            }
        }

        // see if the voters have a sufficient advantage now
        if ( numVote + readyVoteMaxRemove >= totalPlayers && numDemoUnready + numUnready > 0 && numVote > 0 )
        {
            bool print = false;

            // remove unready players from the game
            for ( Client::iterator player = Client::GetFirstClient(); player; ++player )
            {
                if ( player->id.IsPlaying() && player->readyWarn && ( current - player->readyWarnTime ).Seconds() >= readyVoteDelay/3 )
                {
                    if ( !player->id.IsReadyManual() )
                    {
                        GetGriefLog( *player, "noReady" ) << "\n";

                        print = true;

                        (*player).forceReady = true;

                        if ( removeUnready )
                        {
                            Log::Out() << "Removing " << *player << " for not readying up.\n";
                            (*player).id.Spectate();
                        }
                        else
                        {
                            // force them ready
                            (*player).id.ToggleReady( true );
                            (*player).id.SendToggleReady();
                        }
                    }
                }
            }

            if ( print )
            {
                Client::ResetManualReadyness();

                if ( removeUnready )
                {
                    Host::GetHost().Say( "Removing players who refuse to ready up. They can rejoin, but will be forced to stay ready to play." );
                    SuperPower::UnreadyAll();
                }
                else
                {
                    Host::GetHost().Say( "Forcing players to ready up." );
                }
            }
        }
    }
}

#ifdef COMPILE_DEDWINIA
// number of regular colors. Evil/Futurewinians get two extra colors.
#define MAXCOLOR 8
// assault game mode
#define ASSAULT 3
#endif

#ifdef COMPILE_DEDCON
#if VANILLA
// number of regular colors.
#define MAXCOLOR 6
#endif
#ifdef NOTVANILLA
// number of regular colors.
#define MAXCOLOR 16
#endif
// assault game mode (invalid)
#define ASSAULT -1

static IntegerSetting teamSize( Setting::ExtendedGameOption, "AllianceSize", 0, 3 );
#endif

void SuperPower::SetAndSendColor( int color )
{
    bool changed = ( color != Color() );

    {
        SetColor( color );

        if ( IsPlaying() )
        {
            if ( !NetSilentStateChanges() )
            {

#ifdef COMPILE_DEDCON
                SuperPower::UnreadyAll( false );
#endif

                if ( color >= 0 )
                {
                    SendColor( *this, -1 );
                }

                SendColor( *this );

#ifdef COMPILE_DEDCON
                SuperPower::UnreadyAll( false );
#endif
            }

            if( changed && color >= 0 && !LogSilentStateChanges() )
            {
                Log::Event() << "TEAM_ALLIANCE " << GetTeamID() << " " << color << " SWITCH\n";
            }
       }
    }
}

#ifdef COMPILE_DEDCON
//! toggles territory possession (internal state only)
void SuperPower::OnlyToggleTerritory( int territory_ )
{
    // in single-territory mode, unselect the previous
    // territory first
    static bool multipleTerritories = false;

    if ( settings.territoriesPerTeam > 1 )
    {
        multipleTerritories = true;
    }

    if ( !multipleTerritories )
    {
        if( territory != territory_ )
        {
            // deselect old territory
            if ( territory >= 0 )
            {
                ToggleTerritory( territory );
            }

            territory = territory_;
        }
        else
        {
            // deselect current territoty
            territory = -1;
        }   
    }
    else
    {
        territory = -1;
    }

    Log::Event() << "TEAM_TERRITORY " << teamID << " " << territory_ << "\n";
}

//! sends territory toggle message 
void SuperPower::SendToggleTerritory( int territory )
{
    // send territory toggle message
    AutoTagWriter writer( C_TAG, C_TEAM_TERRITORY_CHANGE );
    writer.WriteChar( C_PLAYER_TEAM_ID, GetTeamID() );
    writer.WriteChar( C_TEAM_TERRITORY, territory );
    writer.WriteInt( C_PLAYER_NAME, 0 );
    gameHistory.AppendClient( writer );
}

//! toggles and sends territory possession
void SuperPower::ToggleTerritory( int territory )
{
    OnlyToggleTerritory( territory );
    SendToggleTerritory( territory );
}
#endif

class ColorUse
{
public:
    ColorUse()
    {
        for( int i = MAXCOLOR-1; i >= 0; --i )
        {
            playersPerColor[i] = 0;
        }

        colorNames = true;

        numTeams = 0;
        maxTeams = 2;

#ifdef COMPILE_DEDWINIA
        {
            // set maximal counts
            if ( settings.gameMode == ASSAULT )
            {
                for( int i = MAXCOLOR-1; i >= 2; --i )
                {
                    maxPlayersPerColor[i] = 0;
                }
                maxPlayersPerColor[1] = settings.numAttackers;
                maxPlayersPerColor[0] = settings.maxTeams - settings.numAttackers;
            }
            else if ( settings.coopMode )
            {
                for( int i = MAXCOLOR-1; i >= 0; --i )
                {
                    maxPlayersPerColor[i] = 2;
                }
            }
            else
            {
                maxTeams = settings.maxTeams;
                colorNames = false;
                for( int i = MAXCOLOR-1; i >= 0; --i )
                {
                    maxPlayersPerColor[i] = 1;
                }
            }
        }
#endif

#ifdef COMPILE_DEDCON
        {
            maxTeams = MAXCOLOR;
            int maxPlayersPerTeam = MAXCOLOR;

            if ( !teamSize.IsDefault() )
            {
                maxPlayersPerTeam = teamSize;
                maxTeams = 1 + (settings.maxTeams - 1 )/maxPlayersPerTeam;
            }

            for( int i = MAXCOLOR-1; i >= 0; --i )
            {
                maxPlayersPerColor[i] = maxPlayersPerTeam;
            }
        }
#endif
    }

    // count current color use, ignoring the given player
    void CountCurrent( SuperPower const * avoid = 0 )
    {
        // count
        for ( SuperPower::iterator player = SuperPower::GetFirstSuperPower(); player; ++player )
        {
            if ( !player->IsPlaying() || &(*player) == avoid )
            {
                continue;
            }
            
            int color = player->Color();
            if ( color >= 0 )
            {
                if ( !AddPlayer( color ) )
                {
                    player->SetAndSendColor( -1 );
                }
            }
        }
    }
    
    // return value: true if allowed
    bool AddPlayer( int color, bool avoid = false )
    {
        if ( playersPerColor[color] >= maxPlayersPerColor[color] )
        {
            // too many players of the same color. No.
            return false;
        }

        if ( !playersPerColor[color] )
        {
            if ( numTeams >= maxTeams )
            {
                // too many teams. Don't allow this.
                return false;
            }
            else
            {
                numTeams++;
            }
        }
        else if ( avoid && numTeams < maxTeams )
        {
            // avoid putting people into the same team
            return false;
        }

        playersPerColor[color]++;
        
        return true;
    }

    int MaxTeams() const { return maxTeams; }
    int NumTeams() const { return numTeams; }
    int PlayersPerColor( int index ){ return playersPerColor[index]; }
    int MaxPlayersPerColor( int index ){ return maxPlayersPerColor[index]; }

    bool colorNames;
private:
    int numTeams;
    int maxTeams;
    int playersPerColor[ MAXCOLOR ];
    int maxPlayersPerColor[ MAXCOLOR ];
};


// flag allowing/forbiding client color changes
static bool blockClientColorChanges = false;

static IntegerSetting humanTeam( Setting::ExtendedGameOption, "HumanTeam", -1, MAXCOLOR-1, -2 );

// color sanity checks, assign teams if randomize is set
static void CheckColors( bool randomize = false )
{
    if ( serverInfo.started )
    {
        // umm, definitely don't do this while a game is going on. Bad mojo.
        return;
    }

#ifdef COMPILE_DEDCON
    // don't mess with the teams until the very last moment
    if ( !randomize )
    {
        return;
    }
#endif

#ifdef COMPILE_DEDWINIA
    // reset stored colors if coop mode is activated
    static bool lastCoop = false;
    if ( !lastCoop && settings.coopMode.Get() )
    {
        // Log::Out() << "reseting colours\n";
        for ( SuperPower::iterator player = SuperPower::GetFirstSuperPower(); player; ++player )
        {
            player->SetAndSendColor( -1 );
        }
    }
    lastCoop = settings.coopMode.Get();
#endif

    // stats for colors
    ColorUse colorUse;

    // count
    colorUse.CountCurrent();

    // randomize (Ok, this does not yield a uniform randomization. Who cares?)
    // and it's not really efficient, but with 8 colors, that should be OK.
    if ( randomize )
    {
        for( int human = 1; human >= 0; --human )
        {
            std::vector< SuperPower * > players;
            for ( SuperPower::iterator player = SuperPower::GetFirstSuperPower(); player; ++player )
            {
                if ( player->GetHuman() != human )

                {
                    continue;
                }
                
                if ( !player->IsPlaying() )
                {
                    continue;
                }
                
                players.push_back( &(*player) );
            }

            // shuffle them
            for ( int i = players.size()-1; i >= 0; --i )
            {
                int swap = int((float(rand())/RAND_MAX) * players.size()) % players.size();
                if ( i != swap )
                {
                    std::swap( players[i], players[swap] );
                }
            }

            for ( std::vector< SuperPower * >::const_iterator i = players.begin(); i != players.end(); ++i )
            {
                SuperPower * player = *i;

                bool trySpecial = true; // try some special team selection?

                int tries = 1000;
                
                while ( player->Color() < 0 && --tries >= 0 )
                {
                    int maxcolor = MAXCOLOR;
                    
                    int color = humanTeam.Get();
                    if ( human && color >= 0 && trySpecial )
                    {
                        // human team exists and is not full. fine!
                        // but don't try again if it fails.
                        trySpecial = false;
                    }
                    else
                    {
                        // pick random team.
                        // only pick the first four colours if teams are still open
                        if ( colorUse.NumTeams() < colorUse.MaxTeams() )
                        {
                            maxcolor = 4;
                        }
                        
                        color = int((float(rand())/RAND_MAX) * maxcolor) % maxcolor;

                        // if desperate, fill from the bottom
                        if ( tries < 2 * maxcolor )
                        {
                            color = ( maxcolor - 1 - tries ) % maxcolor;
                        }
                    }

                    // is it allowed?
                    if ( !colorUse.AddPlayer( color, human && humanTeam.Get() == -2 ) )
                    {
                        continue;
                    }
                    // no objections, set!
                    player->SetAndSendColor( color );

#ifndef COMPILE_DEDCON                    
                    // set CPU player name
                    if ( !player->GetHuman() )
                    {
                        std::ostringstream s;
                        if ( colorUse.colorNames )
                        {
#ifdef COMPILE_DEDWINIA
                            char const * colors[] ={ "Green", "Red", "Yellow", "Blue", "Orange", "Purple", "Cyan", "Pink" };
#endif
#ifdef COMPILE_DEDCON
#if VANILLA
                            char const * colors[] ={ "Red", "Green", "Blue", "Yellow", "Orange", "Turq" };
#endif
#ifdef NOTVANILLA
                            char const* colors[] = { "Green", "Red", "Blue", "Yellow", "Cyan", "Pink", "Black", "Orange", "Olive", "Scarlet", "Indigo", "Gold", "Teal", "Purple", "White", "Brown" };
#endif
#endif
                            s << colors[color];
                            if ( colorUse.MaxPlayersPerColor(color) > 1 )
                            {
                                s << " " << colorUse.PlayersPerColor(color);
                                player->SetAndSendName( s.str() );
                            }
                            else
                            {
                                player->SetAndSendName( s.str() );
                            }
                        }
                        else
                        {
                            s << "Player " << player->GetTeamID()+1;
                            player->SetName( s.str() );
                        }
                    }
#endif
                }
            }
        }
    }
}

#ifdef COMPILE_DEDCON
static bool randomizationPossible = true;
#if VANILLA
char const * territoryValues[] = { "None", "NA", "SA", "EU", "RU", "AS", "AF", 0 };
char const * territoryAlternativeValues[] = { "Random", "NorthAmerica", "SouthAmerica", "Europe", "Russia", "Asia", "Africa", 0 };
#elif EIGHTPLAYER
char const* territoryValues[] = { "None", "NA", "SA", "EU", "RU", "WA", "AF", "AA", "EA", 0 };
char const* territoryAlternativeValues[] = { "Random", "NorthAmerica", "SouthAmerica", "Europe", "Russia", "West Asia", "Africa", "Australia", "East Asia", 0 };
#elif TENPLAYER
char const* territoryValues[] = { "None", "NA", "SA", "EU", "RU", "WA", "AF", "AA", "EA", "AN", "NAF", 0 };
char const* territoryAlternativeValues[] = { "Random", "NorthAmerica", "SouthAmerica", "Europe", "Russia", "West Asia", "Africa", "Australia", "East Asia", "Antartica", "North Africa", 0 };
#endif
class TerritorySetting: public EnumSetting
{
public:
    TerritorySetting( char const * name )
    : EnumSetting( Setting::ExtendedGameOption, name, territoryValues, -1, -1 )
    {
        SetAlternativeValues( territoryAlternativeValues );
    }
};
#if VANILLA
static TerritorySetting territory1( "Territory1" );
static TerritorySetting territory2( "Territory2" );
static TerritorySetting territory3( "Territory3" );
static TerritorySetting territory4( "Territory4" );
static TerritorySetting territory5( "Territory5" );
static TerritorySetting territory6( "Territory6" );
#elif EIGHTPLAYER
static TerritorySetting territory1("Territory1");
static TerritorySetting territory2("Territory2");
static TerritorySetting territory3("Territory3");
static TerritorySetting territory4("Territory4");
static TerritorySetting territory5("Territory5");
static TerritorySetting territory6("Territory6");
static TerritorySetting territory7("Territory7");
static TerritorySetting territory8("Territory8");
#elif TENPLAYER
static TerritorySetting territory1("Territory1");
static TerritorySetting territory2("Territory2");
static TerritorySetting territory3("Territory3");
static TerritorySetting territory4("Territory4");
static TerritorySetting territory5("Territory5");
static TerritorySetting territory6("Territory6");
static TerritorySetting territory7("Territory7");
static TerritorySetting territory8("Territory8");
static TerritorySetting territory9("Territory9");
static TerritorySetting territory10("Territory10");
#endif
#define UNSTOPPABLE 8

// randomize a single territory value
void RandomizeTerritory( int & territory, bool * occupied )
{
    do
    {
        territory = int( ( (float(rand())*SuperPower::MaxSuperPowers)/RAND_MAX) ) % SuperPower::MaxSuperPowers;
    }
    while( occupied[ territory ] );
    occupied[ territory ] = true;
}

// randomizes all territories
void RandomizeTerritories()
{
    // new random seed each time territories are randomized
    Time current;
    current.GetTime();
    srand(current.Seconds() + current.Microseconds() + (rand() % 10000));
    
    if ( !randomizationPossible )
    {
        if ( settings.randomTerritories.Get() )
        {
            Log::Err() << "Custom territory randomisation not possible; have RandomTerritories active from the beginning.\n";
        }
        return;
    }

    // fetch setting values
#if VANILLA
    int territories[ SuperPower::MaxSuperPowers ] = { territory1, territory2, territory3, territory4, territory5, territory6 };
#elif EIGHTPLAYER
    int territories[SuperPower::MaxSuperPowers] = { territory1, territory2, territory3, territory4, territory5, territory6, territory7, territory8 };
#elif TENPLAYER
    int territories[SuperPower::MaxSuperPowers] = { territory1, territory2, territory3, territory4, territory5, territory6, territory7, territory8, territory9, territory10 };
#endif
    // fill in missing bits, replacing "None" territory choice with something random
    {
#if VANILLA
        bool occupied[ SuperPower::MaxSuperPowers ] = { false, false, false, false, false, false };
#elif EIGHTPLAYER
        bool occupied[SuperPower::MaxSuperPowers] = { false, false, false, false, false, false, false, false };
#elif TENPLAYER
        bool occupied[SuperPower::MaxSuperPowers] = { false, false, false, false, false, false, false, false, false, false };
#endif
        // first round: mark explicit selections
        for( int i = 0; i < SuperPower::MaxSuperPowers; ++i )
        {
            int & territory = territories[ i ];
            if ( territory >= 0 && occupied[ territory ] )
            {
                Log::Err() << "Value of Territory" << i+1 << " (" << territory << ") already taken.\n";
                territory = -1;
            }
            if ( territory >= 0 )
            {
                occupied[ territory ] = true;
            }
        }

        // second round: make random picks
        for( int i = 0; i < SuperPower::MaxSuperPowers; ++i )
        {
            int & territory = territories[ i ];
            if ( territory < 0 )
            {
                RandomizeTerritory( territory, occupied );
            }
        }
    }

    // distribute territories, sorting players by team
    int teamOrder[ SuperPower::MaxSuperPowers ];
    {
#if VANILLA
        bool occupied[SuperPower::MaxSuperPowers] = { false, false, false, false, false, false };
#elif EIGHTPLAYER
        bool occupied[SuperPower::MaxSuperPowers] = { false, false, false, false, false, false, false, false };
#elif TENPLAYER
        bool occupied[SuperPower::MaxSuperPowers] = { false, false, false, false, false, false, false, false, false, false };
#endif

        for( int i = 0; i < SuperPower::MaxSuperPowers; ++i )
        {
            int & team = teamOrder[ i ];
            RandomizeTerritory( team, occupied );
        }
    }

    SuperPower::UnreadyAll( false );

    // iterate through randomized teams
    int currentTerritory = 0;
    for( int i = 0; i < SuperPower::MaxSuperPowers; ++i )
    {
        int team = teamOrder[ i ];

        std::deque< SuperPower * > teamMembers;

        // collect team members
        for ( SuperPower::iterator player = SuperPower::GetFirstSuperPower(); player; ++player )
        {
            if ( player->GetAllianceID() == team )
            {
                teamMembers.push_back( &*player );
            }
        }

        // randomize team members
        int teamMemberOrder[ SuperPower::MaxSuperPowers ];
        {
#if VANILLA
            bool occupied[SuperPower::MaxSuperPowers] = { false, false, false, false, false, false };
#elif EIGHTPLAYER
            bool occupied[SuperPower::MaxSuperPowers] = { false, false, false, false, false, false, false, false };
#elif TENPLAYER
            bool occupied[SuperPower::MaxSuperPowers] = { false, false, false, false, false, false, false, false, false, false };
#endif

            for( int i = 0; i < SuperPower::MaxSuperPowers; ++i )
            {
                int & teamMember = teamMemberOrder[ i ];
                RandomizeTerritory( teamMember, occupied );
            }
        }


        // distribute territories to clients in the determined random order
        for( int j = 0; j < SuperPower::MaxSuperPowers; ++j )
        {
            // ignore out of range team members
            if ( teamMemberOrder[j] >= (int) teamMembers.size() )
            {
                continue;
            }

            // pick random player
            SuperPower * player = teamMembers[teamMemberOrder[j]];

            // deselect previous territory
            if( settings.randomTerritories.Get() && player->GetTerritory() >= 0 )
            {
                player->ToggleTerritory( player->GetTerritory() );
            }
            
            // select new territories
            if ( player->GetTerritory() < 0 )
            {
                for( int j = settings.territoriesPerTeam.Get()-1; j >= 0; --j )
                {
                    assert( currentTerritory < SuperPower::MaxSuperPowers );
                    player->ToggleTerritory( territories[ currentTerritory++] );
                }
            }
        }
    }

    gameHistory.Bump();
    SuperPower::UnreadyAll( false );
    while( serverInfo.everyoneReadySince > gameHistory.GetTailSeqID() - UNSTOPPABLE )
    {
        gameHistory.Bump();
    }
}
#endif

#ifdef COMPILE_DEDWINIA
static IntegerSetting countdownLength( Setting::ExtendedGameOption, "CountdownLength", 3, 60, -1 );

// checks for holes in the team list and fixes them
void CheckTeamHoles()
{
    if ( LogSilentStateChanges() )
    {
        // we should not do this while the changes we make will be suppressed.
        return;
    }

    if ( serverInfo.started )
    {
        // umm, definitely don't do this while a game is going on. Bad mojo.
        return;
    }

    bool goon = true;
    while( goon )
    {
        int inGame = 0;
        int maxTeam = -1;

        for ( SuperPower::iterator player = SuperPower::GetFirstSuperPower(); player; ++player )
        {
            // ready to go?
            if ( player->IsPlaying() )
            {
                inGame++;
                
                if ( player->GetTeamID() > maxTeam )
                {
                    maxTeam = player->GetTeamID();
                }
            }
        }
        
        if ( maxTeam >= inGame && maxTeam >= 1 )
        {
            // bad. Holes in the team list. Fix them.
            for ( SuperPower::iterator player = SuperPower::GetFirstSuperPower(); player; ++player )
            {
                if ( player->IsPlaying() && player->GetTeamID() == maxTeam )
                {
                    // move the last player to a different slot
                    player->Vanish();
                    player->Play();

                    // resend and relog the color
                    SendColor( *player, -1 );
                    SendColor( *player );
                }
            }
        }
        else
        {
            goon = false;
        }
    }
}
#endif

void Client::OnSettingChange()
{
#ifdef COMPILE_DEDWINIA
    CheckTeamHoles();
    CheckColors();
#endif
}

//! signals this client is fully connected now
void Client::Connect()
{
    // force advertising
    if ( !connected )
    {
#ifdef COMPILE_DEDCON
        // duplicate IP (ghosting protection)?
        if ( serverInfo.unstoppable && ghostProtection == 1 && !id.IsPlaying() )
        {
            if ( CheckGhosting( *this, false ) )
                return;
        }
#endif
        
        Advertiser::Force();
        
        connected = true;
    }
}

static IntegerSetting weightWaitTeams( Setting::Flow, "WeightWaitTeams", 2, 4, 1 );

void Client::CheckStart()
{
    static bool recursion = false;
    if ( recursion )
        return;
    recursion = true;

#ifdef COMPILE_DEDCON
    bool ready = true;           // everyone ready to go?
    int  players = 0;            // anyone there?

    for ( SuperPower::iterator player = SuperPower::GetFirstSuperPower(); player; ++player )
    {
        // ready to speed up?
        if ( player->IsPlaying() )
        {
            if ( settings.maxTeams <= players && !serverInfo.unstoppable )
            {
                // too many players, kick one to spectator mode
                player->Spectate();
            }
            else
            {
                players++;
                if ( !player->IsReady() )
                    ready = false;
            }
        }
    }

    if ( players >= weightWaitTeams.Get() )
    {
        Rotator::Save();
    }

    // also check for game end condition
    if ( players == 0 )
    {
        serverInfo.everyoneReady = ready = false;
        // the game ends when all the players left
        if ( serverInfo.started  )
        {
            if ( !serverInfo.ended )
            {
                Log::Event() << "GAME_END\n";
            }
            serverInfo.ended = true;
        }
    }

    if ( gameClock.ticksPerSecond == 0 )
    {
        recursion = false;
        return;
    }

    // see if the progression starts the server
    if ( !serverInfo.started  )
    {
        // force players into readyness state
        if ( !serverInfo.unstoppable )
        {
            ReadyVote();
        }

        if ( settings.minTeams <= CountPlayers() )
            CPU::ReadyAll();

        // record number of players right now, assuming nobody dropped before placing stuff
        serverInfo.totalPlayers = players;

        serverInfo.everyoneReady = ready;

        if ( serverInfo.everyoneReady )
        {
            // check that all alliances are correct
            CheckColors( true );

            if ( serverInfo.everyoneReadySince <= gameHistory.GetTailSeqID() - 10 )
            {
                StartGame();
            }
            else if ( serverInfo.everyoneReadySince <= gameHistory.GetTailSeqID() - UNSTOPPABLE )
            {
                // mark the game as unstoppable (a limit of 8 should be enough, but let's add one for safety)
                Timestamp();
                {
                    static bool randomized = false;
                    if ( !randomized )
                    {
                        randomized = true;
                        RandomizeTerritories();
                    }
                }
                serverInfo.unstoppable = true;
            }
            else
            {
                if ( gameClock.ticksPerSecond < 2 )
                {
                    if ( Log::GetLevel() >= 2 )
                    {
                        Log::Out() << "Countdown started.\n";
                    }
                    CPU::OnStartCountdown();
                }

                // speed up when everyone is ready to make the countdown appear at normal pace
                Timestamp();
                gameClock.ticksPerSecond = 2;

            }
        }
        else
        {
            if ( gameClock.ticksPerSecond >= 2 )
            {
                if ( Log::GetLevel() >= 2 )
                    Log::Out() << "Countdown aborted.\n";
                CPU::OnAbortCountdown();
            }

            // slow down again to save seqIDs
            gameClock.ticksPerSecond = 1;
        }
    }
    else
    {
        // if no command was given for some time, the game has certainly ended, too
        Time current;
        current.GetTime();
        if ( ( current - lastActivityGlobal ).Seconds() > idleTimeGlobal )
        {
            if ( !serverInfo.ended )
            {
                Log::Event() << "GAME_END\n";
            }
            serverInfo.ended = true;
        }
    }
#if 0
    // need to understand alliance messages first

    // check whether a recently quit AI player needs to defect
    if ( toLeaveAlliance.size() > 0 )
    {
        Client * toDefect = toLeaveAlliance.front();

        // cancel defection if the player rejoined by now
        if ( toDefect->GetTeamID() < 0 )
        {
            toLeaveAlliance.pop_front();
        }
        else if ( toDefect->quitTime + 100 < gameHistory.GameTime() )
        {
            toLeaveAlliance.pop_front();

            // write alliance quit message
            AutoTagWriter writer( C_TAG, C_VOTE_REQUEST );
            writer.WriteChar( C_PLAYER_TEAM_ID, toDefect->id.GetTeamID() );
            writer.WriteChar( C_VOTE_REQUEST_TYPE, 3 );
            writer.WriteChar( C_VOTE_REQUEST_DATA, toDefect->id.GetTeamID() );
            gameHistory.AppendClient( writer );
        }
    }
#endif

#endif
#ifdef COMPILE_DEDWINIA
    if ( !serverInfo.started  )
    {
        int inGame = 0;
        int ready = 0;
        for ( Client::iterator player = Client::GetFirstClient(); player; ++player )
        {
            // ready to go?
            if ( player->id.IsPlaying() && player->id.NameSet() )
            {
                inGame++;
                if ( player->id.IsReady() )
                {
                    ready++;
                }
            }
        }

        bool allConnected = true;
        for ( Client::iterator player = Client::GetFirstClient(); player; ++player )
        {
            if ( !player->IsConnected() )
            {
                allConnected = false;
                break;
            }
        }

        if ( inGame >= weightWaitTeams.Get() )
        {
            Rotator::Save();
        }

        CheckTeamHoles();

        // determine whether enough people are ready
        bool allReady = ready >= settings.maxTeams || ready >= settings.minTeams;

        static bool lastAllReady = serverInfo.everyoneReady;
        static Time countdownStart;
        bool justStarted = false;

        if ( allReady && !lastAllReady )
        {
            countdownStart.GetTime();
            countdownStart.AddMicroseconds(999999);
        }
        else if ( lastAllReady && !allReady )
        {
            Host::GetHost().Say( "Countdown aborted." );
        }
        serverInfo.everyoneReady = lastAllReady = allReady;

        {
            int check = inGame + ( allReady ? 0x10 : 0 ) + ( allConnected ? 0x20 : 0 );
            static int lastCheck = -1;
            if ( lastCheck != check || settings.mapFile.IsDefault() )
            {
                lastCheck = check;

                int mapHash = 0;
                int maxTeams = 0;
                if ( !allReady )
                {
                    Map::Players( inGame + 1, false );
                    mapHash = Map::GetHash();
                    maxTeams = settings.maxTeams;
                }
                Map::Players( inGame, false );
                if ( !allReady )
                {
                    // advertise with a false hash (the one that will be activated when
                    // another player joins)
                    Map::SetHash( mapHash );
                    settings.maxTeams.Set( maxTeams );
                }
            }
        }

        if ( allReady )
        {
            Time now;
            now.GetTime();
        
            int countdown = ( countdownStart - now ).Seconds() + countdownLength;
            static int lastCountdown = -1;
            if ( countdown > 0 && ( justStarted || countdown != lastCountdown ) )
            {
                std::ostringstream s;
                if ( lastCountdown < countdown )
                {
                    s << "Loading map in ";
                }
                s << countdown << "...";
                Host::GetHost().Say( s.str() );
                gameHistory.Bump();
            }
            else if ( countdown <= 0 )
            {
                {
                    static bool warn = true;
                    if ( !allConnected )
                    {
                        if ( warn )
                        {
                            Host::GetHost().Say( "Waiting a bit for connecting players..." );
                            warn = false;
                        }
                    }
                    else
                    {
                        warn = true;
                    }
                }

                {
                    static bool warn = true;
                    if ( !FTP::Complete() && allConnected )
                    {
                        if ( warn )
                        {
                            Host::GetHost().Say( "Waiting a bit for file transfers to complete..." );
                            warn = false;
                        }
                    }
                    else
                    {
                        warn = true;
                    }
                }
            }

            lastCountdown = countdown;
            
            if ( countdown <= 0 && allConnected && FTP::Complete() )
            {
                Map::Players( inGame, true );

                // remove readyness force
                for ( Client::iterator client = Client::GetFirstClient(); client; ++client )
                {
                    client->forceReady = false;
                }

                // and unready everyone, a new ready up phase will follow
                SuperPower::UnreadyAll();

                // fill with CPU players
                while( inGame < settings.maxTeams )
                {
                    CPU::Add();
                    ++inGame;
                }
                
                // distribute random colors
                CheckColors( true );

                blockClientColorChanges = true;
                
                // remove host
                Host & host = Host::GetHost();
                if ( host.IsPlaying() )
                {
                    StateChangeLogMuter logMuter;
                    host.Vanish();
                }
                
                // start the game.
                StartGame();
                AutoTagWriter writer( C_TAG, C_GAME_START );
                gameHistory.AppendClient( writer );
                // advance to next sequence to avoid trouble.
                gameHistory.Bump();
            }
        }
    }
    else
    {
        // force players into readyness state
        ReadyVote();

        // don't allow countdown aborts
        if ( !serverInfo.unstoppable && serverInfo.everyoneReady && serverInfo.everyoneReadySince <= gameHistory.GetTailSeqID() - 27 )
        {
            serverInfo.unstoppable = true;
        }
    }
#endif
    recursion = false;
}

struct TeamId{
    int id;
    bool warned;
    TeamId(): id(-1), warned(false){}
};

//! checks for illegal commands
void Client::CheckOwnership( int teamID, int objectID )
{
    if( objectID < 0 || teamID < 0 )
    {
        return;
    }

    static std::map< int, TeamId > s_objectToTeam;

    TeamId & id = s_objectToTeam[objectID];
    if( id.id == -1 )
    {
        id.id = teamID;
    }
    else if( id.id != teamID && !id.warned )
    {
        id.warned = true;

        std::ostringstream s;
        SuperPower * previous = SuperPower::GetTeam( id.id );
        SuperPower * now = SuperPower::GetTeam( teamID );;
        
        s  << "Possible cheat: Object ID " << objectID << " used by both ";
        if( previous )
        {
            s << *previous;
        }
        else
        {
            s << "Team " << id.id;
        }
        s << " and ";
        if( now )
        {
            s << *now;
        }
        else
        {
            s << teamID;
        }
        s << ".";

        Host::GetHost().Say(s.str());
    }
}

//! counts the number of players
void Client::Count( int & total, int & players, int & spectators )
{
    total = players = spectators = 0;

    for ( Client::const_iterator client = Client::GetFirstClient(); client; ++client )
    {
        total++;
        if ( client->id.IsPlaying() )
        {
            players++;
        }
        else if ( client->id.GetState() == SuperPower::Spec )
        {
            spectators++;
        }
    }
}

#ifdef COMPILE_DEDCON
// slowdown budget settings:
// the budget you start with
static IntegerSetting slowBudgetStart( Setting::ExtendedGameOption, "SlowBudgetStart", 300 );
// the maximal budget that can accumulate
static IntegerSetting slowBudgetMax( Setting::ExtendedGameOption, "SlowBudgetMax", 600 );
// the refill rate (in percent)
static IntegerSetting slowBudgetRefill( Setting::ExtendedGameOption, "SlowBudgetRefill", 5 );
// the percentage of budget that, when used, gets redistributed to the other players
static IntegerSetting slowBudgetRedistributeUsed( Setting::ExtendedGameOption, "SlowBudgetRedistributeUsed", 100 );
// the percentage of budget that, when unused, gets redistributed.
static IntegerSetting slowBudgetRedistributeUnused( Setting::ExtendedGameOption, "SlowBudgetRedistributeUnused", 0, 100 );

// the number of points that is substracted from the slowdown budget
// in the various speed modes per 100 seconds spent in them
static IntegerSetting slowBudgetUsePause( Setting::ExtendedGameOption, "SlowBudgetUsePause", 0 );
static IntegerSetting slowBudgetUseRealtime( Setting::ExtendedGameOption, "SlowBudgetUseRealtime", 0 );
static IntegerSetting slowBudgetUse5x( Setting::ExtendedGameOption, "SlowBudgetUse5x", 0 );
static IntegerSetting slowBudgetUse10x( Setting::ExtendedGameOption, "SlowBudgetUse10x", 0 );

//! calculate slowdown budget usage
int Client::TimeBudget( float dt )
{
    if ( !serverInfo.started || serverInfo.ended )
        return 0;

    // count total players and players on the slowest speed
    int players = 0;
    int slowPokes = 0;
    int speed = 255;
    float slowBudgetUnused = 0; // the slowdown budget unused (filled from overflows)
    int notOverflowing = 0;     // number of players with non-overflowing budget
    for ( Client::iterator client = Client::GetFirstClient(); client; ++client )
    {
        if ( client->id.IsPlaying() )
        {
            players++;



            if ( client->speed < speed )
            {
                speed = client->speed;
                slowPokes = 1;
            }
            else if ( client->speed == speed )
            {
                slowPokes++;
            }

            // clamp
            if ( client->slowBudget >= slowBudgetMax )
            {
                slowBudgetUnused += client->slowBudget - slowBudgetMax;
                client->slowBudget = slowBudgetMax;
            }
            else
            {
                notOverflowing++;
            }
        }
    }

    if ( dt <= 0 )
        return speed;

    // determine the budget usage quotient
    float budgetUse = 0;
    switch( speed )
    {
    case 0:
        budgetUse = slowBudgetUsePause;
        break;
    case 1:
        budgetUse = slowBudgetUseRealtime;
        break;
    case 5:
        budgetUse = slowBudgetUse5x;
        break;
    case 10:
        budgetUse = slowBudgetUse10x;
        break;
    }
    budgetUse *= .01;

    // this is the amount of points subtracted from the budgets of the players
    // who have selected the slowest speed
    float cost = - slowBudgetRefill * .01;

    // this is what the others get
    float benefit = slowBudgetRefill * .01;

    // nothing to do
    if ( budgetUse == 0 && slowBudgetRefill == 0 && ( notOverflowing == 0 || slowBudgetUnused <= 0 ) )
        return speed;

    // redistribute budget, if appropriate
    if ( budgetUse > 0 && slowPokes != players && slowPokes != 0 )
    {
        cost += budgetUse/slowPokes;
        benefit += .01 * slowBudgetRedistributeUsed * budgetUse/( players - slowPokes );
    }

    // apply the costs and benefits
    for ( Client::iterator client = Client::GetFirstClient(); client; ++client )
    {
        if ( client->id.IsPlaying() )
        {
            // redistribute unused budget
            if ( client->slowBudget < slowBudgetMax )
            {
                client->slowBudget += slowBudgetUnused * slowBudgetRedistributeUnused *.01/notOverflowing;
            }

            if ( client->speed == speed )
            {
                // a slow player
                client->slowBudget -= cost * dt;

                if ( cost > 0 )
                {
                    // issue budget warning
                    SlowBudgetWarning warning = SlowBudgetWarning_None;
                    if ( client->slowBudget < 0 )
                    {
                        warning = SlowBudgetWarning_Gone;
                    }
                    else if ( client->slowBudget < 10 )
                    {
                        warning = SlowBudgetWarning_10Seconds;
                    }
                    else if ( client->slowBudget < 30 )
                    {
                        warning = SlowBudgetWarning_30Seconds;
                    }
                    else if ( client->slowBudget < 60 )
                    {
                        warning = SlowBudgetWarning_1Minute;
                    }
                    else if ( client->slowBudget < 120 )
                    {
                        warning = SlowBudgetWarning_2Minutes;
                    }
                    if ( warning != client->slowBudgetWarning )
                    {
                        switch( warning )
                        {
                        case SlowBudgetWarning_Gone:
                            Host::GetHost().Say( "Slowdown budget used up, speeding you up.", *client );
                            break;
                        case SlowBudgetWarning_10Seconds:
                            Host::GetHost().Say( "Less than ten seconds of slowdown budget left.", *client );
                            break;
                        case SlowBudgetWarning_30Seconds:
                            Host::GetHost().Say( "Less than thirty seconds of slowdown budget left.", *client );
                            break;
                        case SlowBudgetWarning_1Minute:
                            Host::GetHost().Say( "Less than one minute of slowdown budget left.", *client );
                            break;
                        case SlowBudgetWarning_2Minutes:
                            Host::GetHost().Say( "Less than two minutes of slowdown budget left.", *client );
                        break;
                        case SlowBudgetWarning_None:
                            break;
                        }
                        client->slowBudgetWarning = warning;
                    }

                    // if the budget is gone, bump up the client's speed.
                    if ( client->slowBudget < 0 )
                    {
                        if ( speed < 1 )
                        {
                            client->SetSpeed( 1 );
                        }
                        else if ( speed < 5 )
                        {
                            client->SetSpeed( 5 );
                        }
                        else if ( speed < 10 )
                        {
                            client->SetSpeed( 10 );
                        }
                        else if ( speed < 20 )
                        {
                            client->SetSpeed( 20 );
                        }
                    }
                }
            }
            else
            {
                // a fast player
                client->slowBudget += benefit * dt;
            }
        }
    }

    return speed;
}

static IntegerSetting dropCourtesy( Setting::ExtendedGameOption, "DropCourtesy", 0, 0x7fffffff, -1 );

static void PlayerInTrouble()
{
    if ( !dropCourtesy.Get() )
        return;

    // don't do this too often
    static int count = 0;
    if ( dropCourtesy.Get() > 0 && dropCourtesy.Get() <= count )
    {
        return;
    }
    ++count;

    // apply the costs and benefits
    bool set = false;
    for ( Client::iterator client = Client::GetFirstClient(); client; ++client )
    {
        if ( client->id.IsPlaying() )
        {
            set = true;
            client->SetSpeed( settings.MinSpeed() );
        }
    }

    if ( set )
    {
        Host::GetHost().Say( "A player has technical problems, slowing everyone down." );
    }
}

// call when a client is in trouble
static void ClientInTrouble( Client const & client )
{
    if ( client.inGame )
    {
        PlayerInTrouble();
    }
}
#else
static void ClientInTrouble( Client const & client ){}
#endif

SpamProtection::SpamProtection()
{
    averager.Add( 0, .1 );
    last.GetTime();
    last.AddSeconds(-600);
}

//! checks whether something annoying should be allowed
bool SpamProtection::Allow( float decay, float maxPerHour )
{
    // spam check
    if ( maxPerHour > 0 )
    {
        Time current;
        current.GetTime();

        // determine time since last activity
        int secondsSinceLast = ( current - last ).Seconds();
        if ( secondsSinceLast <= 0 )
            secondsSinceLast = 1;

        // add it to a copy of the statistics
        PingAverager averagerCopy = averager;
        averagerCopy.Add( 3600/float( secondsSinceLast ), secondsSinceLast );
        if ( averagerCopy.GetAverage() >= maxPerHour )
        {
            // do not allow, and drop new statistics
            return false;
        }

        // accept and update statistics
        averagerCopy.Decay(decay);
        averager = averagerCopy;
        last = current;
    }

    return true;
}

// how many times an admin login may be attempted
static IntegerSetting loginAttempts( Setting::Admin, "LoginAttempts", 5 );
static BoolSetting rankedDefault( Setting::Log, "RankedDefault", true );

Client::Client( Socket & serverSocket_)
  : bandwidthControl( 0 ), joinedTick(-1), desyncTick(-1), loginAck( NULL ), serverSocket( serverSocket_ )
{
#ifdef COMPILE_DEDWINIA
    achievements = -1;
    directControl = false;
#endif
    ftpClient = 0;

    chatFilterWarnings = 0;

    rankWish = rankedDefault.Get();

    quitTime = -1;
    lastChatID = -1;
    outOfSync = false;
    outOfSyncCount = 0;
    // lastSent.GetTime();

    lastActivity.GetTime();
    joinTime.GetTime();
    readyWarnTime = lastActivity;
    readyWarn = false;

    // be gentle with packet loss
    packetLoss.Add(0,10);

    // score
    scoresSigned = false;
    scoresLastSeen = 0;

    // start with high ping, but very low weight
    pingAverager.Add(1000,.01);

#ifdef COMPILE_DEDCON
    slowBudget = slowBudgetStart;
    slowBudgetWarning = SlowBudgetWarning_None;
#endif

    spammed = false;

    numberOfKicks = 0;
    numberOfSyncs = 0;
    numberOfAborts = 0;
    numberOfLeaves = 0;

    lastSentSeqID = -2;

    lastAck.GetTime();

    speed = 5;

    writeAhead = 0;
    noPlay = false;
    forceReady = false;

    navalBlacklist = true;

    admin = false;
    adminLevel = 1000;
    loginAttemptsLeft = loginAttempts;

    connected = false;
    inGame = false;



    keyID = -2; // demo user
}

Client::~Client()
{
    if ( loginAck )
    {
        Quit( SuperPower::Left );
    }

    FTP::DeleteFTPClient( ftpClient );
    ftpClient = 0;
}

static bool silentTeamSwitchingBlock = false;

class TeamSwitchException: public ReadException
{
public:
    TeamSwitchException( char const * message )
    : ReadException( silentTeamSwitchingBlock ? "" : message )
    {
    }
};

// base class for parsers that read client data
// tag parser that ignores everything
class TagParserClient: public TagParserLax
{
public:
    TagParserClient( Client * client_ ):teamID(-100), clientID(-100), spectator( false ), client( client_ ), poser( NULL )
    {
        harmless = false;
        seqID = gameHistory.GetTailSeqID();
    }

protected:
    // called when value-key pairs of the various types are encountered.
    virtual void OnChar( std::string const & key, int value )
    {
        if ( key == C_PLAYER_TEAM_ID )
        {
            teamID = value;
        }
        else if ( key == C_PLAYER_ID )
        {
            // read client ID
            clientID = value;
        }
    }

    virtual void OnInt( std::string const & key, int value )
    {
        if ( key == C_SEQID_LAST )
        {
            seqID = value;
        }
        else if ( key == C_PLAYER_SPECTATOR )
        {
            spectator = value;
        }
    }

    // called for nested tags
    virtual void OnNested( TagReader & reader )
    {
       throw ReadException( "Client data is not supposed to have nested tags." );
    }

    // call at the beginning of OnFinish of derived classes
    virtual void OnFinish( TagReader & reader )
    {
        // check for team switching
        if ( teamID >= 0 && !spectator && ( !client->id.IsPlaying() || client->id.GetTeamID() != teamID ) )
        {
            // store poser
            poser = client;

            // check false client IDs
            if ( clientID >= 0 && teamID < 0 && client->GetID() == clientID )
            {
                // everything is OK
                return;
            }

            // team switching. Find the client the command is pretending to come from.
            for ( Client::iterator c = Client::GetFirstClient(); c; ++c )
            {
                if ( c->id.IsPlaying() && c->id.GetTeamID() == teamID )
                {
                    // found the right one
                    client = &(*c);
                    break;
                }
            }

            // umm, OK, perhaps it would be a good idea to check whether team switching is legal
            if ( !settings.teamSwitching )
            {
                // undo the posing change
                client = poser;
                poser = 0;

                if ( serverInfo.dedicated && ( client->id.LastChange() + 10 < seqID || serverInfo.started ) )
                {
                    if ( client->id.IsPlaying() )
                    {
                        if ( teamID != client->id.GetTeamID() )
                        {
                            /*
                            if ( client->keyID == 623363 || client->keyID == 664042 )
                            {
                                Host::GetHost().Say( "Team switching exploit!", client );
                            }
                            */
                            throw TeamSwitchException("Team switching exploit attempted.");
                        }
                    }
                    else
                    {
                        if ( !harmless || !spectator )
                        {
                            if ( teamID != 255 )
                            {
                                throw TeamSwitchException("Spectator tries to influence the game.");
                            }
                            else
                            {
                                // packets with TeamID 255 come in a lot because of client errors, no need to alarm anyone.
                                throw ReadException();
                            }
                        }
                        if ( teamID != client->id.GetID() )
                        {
                            throw TeamSwitchException("Team switching exploit by an spectator detected.");
                        }
                    }
                }
            }
        }
    }

    bool harmless;   //!< set to true if the command is safe for spectator use
    int seqID;       //!< the seqID best fitting to the incoming command
    int teamID;      //!< the claimend team ID (for players) resp. client ID (for spectators)
    int clientID;    //!< the claimend client ID. Always.
    bool spectator;  //!< whether the client claims to be a spectator
    Client * client; //!< the the command should be effective for (usually the sender)
    Client * poser;  //!< the client really sending the command
};



// client ID to join as
static int joinAsID = -1;
// team ID to join as
static int joinAsTeam = -1;

//! allows to join as a specific player
class JoinAsActionSetting: public IntegerActionSetting
{
    //! finds out which client ID belonged to which teamID
    class TeamIDParser: public TagParserLax
    {
    public:
        TeamIDParser(): teamID(-1), clientID(-1){}

        //! called when the type of the tag is read (happens only if the reader was not started yet)
        virtual void OnType( std::string const & type )
        {
            // type must be C_TAG
            if ( type != C_TAG )
            {
                throw ReadException();
            }
        }

        virtual void OnString( std::string const & key, std::string const & value )
        {
            // class must be C_PLAYER_TEAM_ENTER
            if ( key == C_TAG_TYPE && value != C_PLAYER_TEAM_ENTER )
            {
                throw ReadException();
            }
        }

        virtual void OnInt( std::string const & key, int value )
        {
            if ( key == C_PLAYER_TEAM_ID )
            {
                // read team ID
                teamID = value;
            }
            else if ( key == C_PLAYER_ID )
            {
                // read client ID
                clientID = value;
            }
        }

        virtual void OnNested( TagReader & reader )
        {
            throw ReadException();
        }

        int teamID, clientID;
    };
public:
    JoinAsActionSetting()
    : IntegerActionSetting( Setting::Flow, "JoinAs" )
    {}

    //! activates the action with the specified value
    virtual void Activate( int val )
    {
        joinAsTeam = -1;
        joinAsID = -1;

        // scan the game history, we need to know which client ID got this team
        for ( GameHistory::iterator iter = gameHistory.begin(); iter != gameHistory.end(); ++iter )
        {
            Sequence const & s = *(*iter);
            if ( s.HasServerTag() )
            {
                try
                {
                    TeamIDParser parser;
                    TagReader reader( s.ServerTag() );
                    parser.Parse( reader );

                    // found matching team/client, save the client ID
#if VANILLA
                    if ( ( val < 6 && parser.teamID == val ) ||
                        ( val >= 6 && parser.clientID == val )
#elif EIGHTPLAYER
                    if ( ( val < 8 && parser.teamID == val ) ||
                        ( val >= 8 && parser.clientID == val )
#elif TENPLAYER
                    if ( ( val < 10 && parser.teamID == val ) ||
                        ( val >= 10 && parser.clientID == val )
#endif
                        )
                    {
                        joinAsID   = parser.clientID;
                        joinAsTeam = parser.teamID;
                    }

                }
                catch ( ReadException const & )
                {
                    // ignore read exceptions
                }
            }
        }

        if ( joinAsID == -1 )
        {
            Log::Err() << "No player with teamID/clientID " << val << " found.\n";

            return;
        }
        else
        {
            Log::Err() << "The next client to join will take over Team " << joinAsTeam << " with clientID " << joinAsID << " as it he was rejoining after a disconnect.\n";
        }
    }
};
static JoinAsActionSetting joinAs;

#ifdef COMPILE_DEDWINIA
//! activates merry multiwinians throughout the year
class XmasActionSetting: public ActionSetting
{
public:
    XmasActionSetting()
    : ActionSetting( Setting::ExtendedGameOption, "HoHoHo" )
    {}

    //! activates the action with the specified value
    virtual void DoRead( std::istream & )
    {
        if ( !serverInfo.started )
        {
            AutoTagWriter writer( C_TAG, C_XMAS_MODE );
            gameHistory.AppendClient( writer );
        }
        else
        {
            Log::Err() << "Sorry, can't do that while the game is running.\n";
        }
    }
};
static XmasActionSetting xmas;
#endif

// fast forwards the game history to a spot
static void FastForward( int seqID )
{
    gameHistory.GetTail( seqID );
    seqID = gameHistory.GetTailSeqID();
    gameClock.Set( seqID );
    gameHistory.SetEffectiveTailSeqID( seqID );

}

//! allows to join at a specific time
class JoinAtActionSetting: public IntegerActionSetting
{
public:
    JoinAtActionSetting()
    : IntegerActionSetting( Setting::Flow, "JoinAt" )
    {}

    //! activates the action with the specified value
    virtual void Activate( int val )
    {
        Log::Out() << "You will join the game at SeqID " << val << ", all player input after that time is discarded and players get replaced by CPUs.\n";
        FastForward( val );
        gameHistory.DiscardLoad();
    }
};
static JoinAtActionSetting joinAt;

// store all of the other sequences in the client's queue
static void Resync( Client * client, int start )
{
    client->writeAhead = 0;
    for ( GameHistory::iterator iter = gameHistory.begin(); iter != gameHistory.end(); ++iter )
    {
        if ( (*iter)->SeqID() > start )
        {
            client->notYetAcknowledged.push_back( SequenceAck( *iter ) );
        }
    }
}

// check wheter needle is in a list of semicolon separated strings
static bool IsInList( std::string const & haystack, std::string const & needle, char separator = ';' )
{
    std::string h = separator + haystack + separator;
    std::string n = separator + needle + separator;
    return strstr( h.c_str(), n.c_str() );
}

#ifdef COMPILE_DEDCON
#if VANILLA
#define MIN_VERSION "1.42"
#endif
#ifdef NOTVANILLA
#define MIN_VERSION "1.60.1.5"
#endif
#endif

#ifdef COMPILE_DEDWINIA
#define MIN_VERSION "1.0.5"
#endif

static IntegerSetting maxResyncAttempts( Setting::Network, "MaxResyncAttempts", 3 );             // number of resync attempts before sync is considered hopelessly lost
static IntegerSetting kicksToSpectator( Setting::Grief, "KicksToSpectator", 2 ); // number of kicks that prevent a player from rejoining and make him a spectator
static IntegerSetting kicksToBan( Setting::Grief, "KicksToBan", 3 );             // number of kicks that lead to a ban for this game
static StringSetting minVersion( Setting::ExtendedGameOption, "MinVersion", MIN_VERSION );         // the minimal compatible version
static StringSetting maxVersion( Setting::ExtendedGameOption, "MaxVersion", "" );             // the maximal compatible version

#ifdef COMPILE_DEDCON
static IntegerSetting stopForNewPlayers( Setting::ExtendedGameOption, "StopForNewPlayers", 3 ); // number of times the server will stop the game start countdown when a new potential player is connecting

static StringSetting navalBlacklistMinVersion( Setting::ExtendedGameOption, "NavalBlacklistMinVersion", "" );  // the minimal client version naval blacklists are applied to
static StringSetting navalBlacklistMaxVersion( Setting::ExtendedGameOption, "NavalBlacklistMaxVersion", "" );   // the maximal version naval blacklists are applied to
static StringSetting navalBlacklistExcludeVersions( Setting::ExtendedGameOption, "NavalBlacklistExcludeVersions", "" );   // semicolon separated list of version to exclude from naval blacklists
#endif

// no packets received for 20 seconds: bye-bye.
static IntegerSetting connectionTimeout( Setting::Network, "ConnectionTimeout", 30 ); // Number of seconds of no packets before a client is considered lost

// searches the superpowers in the game and finds the one with a lower ranking than the passed client
static SuperPower * CanReplacePlayer( SuperPower const & superPower )
{
    if ( settings.allStarTime.Get() == 0 || serverInfo.unstoppable )
        return 0;

    std::vector< SuperPower * > powers;
    SuperPower::GetSortedByScore( powers );

    // count demo players
    int demoPlayers = 0;
    for ( std::vector< SuperPower * >::const_iterator i = powers.begin(); i != powers.end(); ++i )
    {
        if ( (*i)->GetDemo() )
        {
            demoPlayers++;
        }
    }

    float minRating = superPower.GetRating();
    SuperPower * ret = 0;
    for ( std::vector< SuperPower * >::const_iterator i = powers.begin(); i != powers.end(); ++i )
    {
        // special coding required if the demo limits have been reached and the new player
        // is a demo player
        if ( superPower.GetDemo() && demoLimits.maxPlayers <= demoPlayers )
        {
            if ( !(*i)->GetDemo() )
            {
                // only demo players can be kicked out to make room for this player
                continue;
            }
        }

        // CPUs are always replaced
        if ( !(*i)->GetHuman() )
            return (*i);

        if ( (*i)->GetRating() < minRating )
        {
            minRating = (*i)->GetRating();
            ret = (*i);
        }
    }

    return ret;
}

// creates the message that a client has been silenced
std::string SilenceMessage( Client & silenced )
{
    std::ostringstream out;
    out << silenced << " has been silenced by the server administrator. If you want to hear what he is saying, say \"/unignoreid  " << silenced.GetID() << "\".";
    return out.str();
}

class TagParserClientLogin: public TagParserClient
{
public:
    TagParserClientLogin( Client * client )
        :TagParserClient( client ), playerKeyID( -2 ), needMap( false )
    {}

protected:
    // called when value-key pairs of the various types are encountered.
    virtual void OnString( std::string const & key, std::string const & value )
    {
        if ( key == C_METASERVER_VERSION )
            version = value;
        else if ( key == C_PLATFORM )
            platform = value;
        else if ( key == C_LOGIN_KEY )
            playerKey = value;
        else if ( key == C_METASERVER_PASSWORD )
            pass = value;
        else if ( key == C_TAG_RESEND )
        {
            // value = System Type
        }
    }

#ifndef COMPILE_DEDCON
    virtual void OnUnicodeString( std::string const & key, std::wstring const & value )
    {
        if ( key == C_LOGIN_PASSWORD )
        {
            pass = Unicode::Convert( value );
        }
    }
#endif

    virtual void OnInt( std::string const & key, int value )
    {
        TagParserClient::OnInt( key, value );

        if ( key == C_LOGIN_KEY_ID )
        {
            playerKeyID = value;
#ifdef METASERVER_OUTAGE
            playerKeyID = -1;
#endif
        }
    }

#ifdef COMPILE_DEDWINIA
    virtual void OnBool( std::string const & key, bool value )
    {
        TagParserClient::OnBool( key, value );

        if ( key == C_LOGIN_MAP_NEEDED )
        {
            needMap = true;
        }
    }
#endif

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        TagParserClient::OnFinish( reader );

        // if set, ClientInTrouble will be called at the end
        bool inTrouble = false;

        OStreamProxy out = Log::Out();

        if ( Log::GetLevel() >= 2 )
            out << "Login request from " << inet_ntoa(client->saddr.sin_addr) << ":" << ntohs(client->saddr.sin_port) << ", version=\"" << version << "\", keyID=" << KeyIDWriter(playerKeyID) << " : ";

        client->version = version;
        client->platform = platform;

        // clear this client already got an ID
        bool createNewID = true;
        bool checkIfFull = true;
        bool logEntry    = true;
        
        // ignore double logins
        if ( client->loginAck )
        {
            // if they come close after the initial login, that is
            if ( (int)client->notYetAcknowledged.size() > (gameHistory.GetTailSeqID()*7)/8 )
            {
                if ( Log::GetLevel() >= 2 )
                    out << "ignored, already logged in.\n";

                return;
            }

            if ( Log::GetLevel() >= 2 )
                out << "reconnect, ";

            // reconnections should not be denied if the server is full
            checkIfFull = false;

            // and shouldn't be logged.
            logEntry    = false;

            // otherwise, they're probaby coming from a client that got disconnected. Restart
            // the connection.
            delete client->loginAck;
            client->loginAck = 0;
            client->notYetAcknowledged.clear();
            client->notYetProcessed.clear();
            client->lastChatID = -1;

            inTrouble = true;

            createNewID = false;
        }
        else
        {
#ifndef COMPILE_DEDWINIA
            bool rejoin = false;
            int numberOfZombies = 0;

            // check whether this is one of the dead clients reconnecting
            for ( Client::iterator c = deletedClients.begin(); c; ++c )
            {
                // same key or two demo keys from the same IP
                if ( KeysEqual( c->keyID, c->key, playerKeyID, playerKey ) || ( c->keyID < 0 && playerKeyID < 0 && c->saddr.sin_addr.s_addr == client->saddr.sin_addr.s_addr ) )
                {
                    if ( c->numberOfKicks > client->numberOfKicks )
                        client->numberOfKicks = c->numberOfKicks;
                    if ( c->numberOfSyncs > client->numberOfSyncs )
                        client->numberOfSyncs = c->numberOfSyncs;
                    if ( c->numberOfAborts > client->numberOfAborts )
                        client->numberOfAborts = c->numberOfAborts;
                    if ( c->numberOfLeaves > client->numberOfLeaves )
                        client->numberOfLeaves = c->numberOfLeaves;

                    numberOfZombies++;

                    if ( client->numberOfKicks >= kicksToBan )
                    {
                        // ban
                        if ( Log::GetLevel() >= 2 )
                            out << "denied, banned.\n";
                        client->Quit( SuperPower::Kick );
                        return;
                    }

                    // check whether the player has tried to resync too often
                    if ( client->numberOfSyncs > maxResyncAttempts )
                    {
                        // kick. It's very cruel, but the player is
                        // pointlessly trying to resync and may get
                        // the others in trouble.
                        if ( Log::GetLevel() >= 2 )
                            out << "denied, hopelessly out of sync.\n";
                        client->Quit( SuperPower::Kick );
                        return;
                    }

                    // enforce readyness here, too
                    client->forceReady = c->forceReady;

                    // know what? force him to spectate.
                    client->noPlay = c->noPlay;

                    // transfer warning about readying up
                    client->readyWarn = c->readyWarn;
                    client->readyWarnTime = c->readyWarnTime;
                    client->joinTime = c->joinTime;

                    // transfer name
                    if ( c->id.NameSet() )
                    {
                        client->id.SetName( c->id.GetName() );
                    }

                    // transfer scores
                    if ( c->id.ScoreSet() )
                    {
                        client->id.SetScore( c->id.GetScore() );
                        client->id.SetScoreSigned( c->id.GetScoreSigned() );
                    }

                    // transfer signatures
                    {
                        client->scoresSigned = c->scoresSigned;
                        c->scoresSigned = false;
                        std::vector< Client * >::iterator found = std::find( bestSigners.begin(), bestSigners.end(), &(*c) );
                        if ( found != bestSigners.end() )
                        {
                            (*found) = &(*c);
                        }
                    }

                    // if the previous client was a player, take over its team and identity
                    if ( c->id.GetTeamID() >= 0 && client->numberOfKicks < kicksToSpectator && c->inGame )
                    {
                        rejoin = true;

                        if ( Log::GetLevel() >= 2 )
                            out << "reconnect, ";

                        createNewID = false;
                        client->id.SetID( c->id.GetID() );
                        int teamID = c->id.GetTeamID();
                        client->id.Play( teamID, true );
                        client->inGame = true;
                        if ( c->IsConnected() )
                            client->Connect();

                        // transfer speed budget
                        client->speed = c->speed;
                        client->slowBudget = c->slowBudget;

                        // mark the old client as not in game so it does not appear in the quitlog
                        c->inGame = false;

                        inTrouble = true;

                        client->id.SetAllianceID( c->id.GetAllianceID(), "REJOIN" );
                    }

                    // delete the old client's name so it doesn't appear in searches any more
                    c->id.SetName( "" );
                }
            }
            
            if( rejoin )
            {
                numberOfZombies /= 2;
            }
            if( numberOfZombies > 0 && numberOfZombies > 10 + gameHistory.GetTailSeqID()/100 )
            {
                if ( Log::GetLevel() >= 2 )
                    out << "denied, too many rejoins.\n";
                client->Quit( SuperPower::ServerFull );
                return;
            }
#endif
        }

        client->key = playerKey;
        if ( playerKeyID >= 0 || ( playerKey.size() < 4 || playerKey[0] != 'D' || playerKey[1] != 'E' || playerKey[2] != 'M' || playerKey[3] != 'O' ) )
        {
            client->keyID = playerKeyID;
        }
        else
        {
            client->id.SetDemo(true);
        }

        // check for duplicate keys
        int clientCount = 0;
        int spectatorCount = 0;
        {
            for ( Client::iterator c = Client::GetFirstClient(); c; ++c )
            {
                clientCount++;

                if ( !c->id.IsPlaying() )
                {
                    spectatorCount++;
                }

                if ( KeysEqual( c->keyID, c->key, playerKeyID, playerKey ) && &(*c) != client )
                {
                    // check whether the connection with the other client is about to break down anyway
                    Time current;
                    current.GetTime();
                    if ( (current - c->lastAck).Seconds() >= 2 )
                    {
                        // let the other client time out a little faster
                        c->lastAck.AddSeconds(-connectionTimeout/3);
                        if ( Log::GetLevel() >= 2 )
                            out << "duplicate key, but original client seems to be timing out.\n";
                        client->Quit( SuperPower::NoMessage );

                        return;
                    }

#ifndef XDEBUG
                    if ( Log::GetLevel() >= 2 )
                        out << "denied, duplicate key.\n";
                    client->Quit( SuperPower::DuplicateKey );
                    return;
#endif
                }
            }
        }

        // find various reasons to kick players

        // incompatible?
        {
#ifdef COMPILE_DEDCON
            // strip trailing "steam" et al from the client's version. Rule: only words ending in a digit are good words.
            std::istringstream stripper( version );
            std::ostringstream composer;
            bool space = false;
            while ( !stripper.eof() && stripper.good() )
            {
                std::ws(stripper);
                if ( !stripper.eof() )
                {
                    std::string component;
                    stripper >> component;
                    if ( component.size() > 0 && isdigit(component[component.size()-1]) )
                    {
                        if ( space )
                            composer << ' ';
                        space = true;
                        composer << component;
                    }
                }
            }

            // the stripped version
            std::string const & strippedVersion = composer.str();
#else
            // strip prefix "retail.iv", "steam" et al from the client's version.
            // Rule: strip all leading non-digits.
            std::istringstream stripper( version );
            std::ostringstream composer;
            bool good = false;
            while ( !stripper.eof() && stripper.good() )
            {
                std::ws(stripper);
                if ( !stripper.eof() )
                {
                    char c = stripper.get();
                    if ( isdigit(c) )
                    {
                        good = true;
                    }

                    if ( good )
                    {
                        composer.put( c );
                    }
                }
            }

            // the stripped version
            std::string const & strippedVersion = composer.str();
#endif

            // the limits
            std::string const & minVersion = ::minVersion;
            std::string const & maxVersion = ::maxVersion;

            if ( minVersion.size() && CompareVersions( minVersion, strippedVersion ) > 0 )
            {
                if ( Log::GetLevel() >= 2 )
                    out << "denied, incompatible version, too old.\n";
                client->Quit( SuperPower::Kick );
                return;
            }

            if ( maxVersion.size() && CompareVersions( maxVersion, strippedVersion ) < 0 )
            {
                if ( Log::GetLevel() >= 2 )
                    out << "denied, incompatible version, too new.\n";
                client->Quit( SuperPower::Kick );
                return;
            }

#ifdef COMPILE_DEDCON
            // same for the naval blacklist versions
            std::string const & navalBlacklistMinVersion = ::navalBlacklistMinVersion;
            std::string const & navalBlacklistMaxVersion = ::navalBlacklistMaxVersion;

            client->navalBlacklist = true;

            if ( navalBlacklistMinVersion.size() && CompareVersions( navalBlacklistMinVersion, strippedVersion ) > 0 )
            {
                client->navalBlacklist = false;
            }
            else if ( navalBlacklistMaxVersion.size() && CompareVersions( navalBlacklistMaxVersion, strippedVersion ) < 0 )
            {
                client->navalBlacklist = false;
            }
            else if ( IsInList( navalBlacklistExcludeVersions, version ) )
            {
                client->navalBlacklist = false;
            }
            else
            {
                out << "naval blacklists apply, ";
            }
#endif
        }

        // illegal version?
        {
            if ( IsInList( settings.forbiddenVersions, version ) )
            {
                if ( Log::GetLevel() >= 2 )
                    out << "denied, blacklisted version.\n";
                client->Quit( SuperPower::Kick );
                return;
            }
        }

        // no-play version?
        {
            if ( IsInList( settings.noPlayVersions, version ) )
            {
                if ( Log::GetLevel() >= 2 )
                    out << "no-play version, ";
                client->noPlay = true;
            }
        }

        // read rating score
        UserInfo userInfo = UserInfo::Get( client->saddr.sin_addr, client->keyID, version, platform );
        client->id.SetClientInfo( userInfo );

#ifdef COMPILE_DEDWINIA
        {
            char const * veto = PlayVeto( client );
            if ( veto )
            {
                if ( Log::GetLevel() >= 2 )
                    out << "denied, can't play, " << veto << ".\n";
                client->Quit( SuperPower::Kick );
            }
        }
#endif

        // server full? (the current client is already included in the counts, hence we compare with >
        if ( ( clientCount > settings.maxTeams + settings.maxSpectators || ( spectatorCount > settings.maxSpectators && serverInfo.started ) ) && checkIfFull && !CanReplacePlayer( client->id ) )
        {
            if ( Log::GetLevel() >= 2 )
                out << "denied, server is full.\n";
            client->Quit( SuperPower::ServerFull );
            return;
        }

        // password?
        if ( !settings.serverPassword.IsDefault() && pass != std::string(settings.serverPassword) )
        {
            if ( Log::GetLevel() >= 2 )
                out << "denied, wrong password.\n";
            client->Quit( SuperPower::WrongPassword );
            /*
            static int reason = 0;
            Log::Out() << "reason = " << reason << "\n";
            client->Quit( (SuperPower::KickReason) reason );
            reason++;
            */
            return;
        }

        // demo limits?
        if ( client->id.GetDemo() && ( demoLimits.demoRestricted 
#ifdef COMPILE_DEDWINIA
                                       || settings.demoRestricted > 0
#endif
                 )
)
        {
#ifdef COMPILE_DEDWINIA
            if ( client->IsOnLAN() )
            {
                if ( Log::GetLevel() >= 2 )
                    out << "LAN exception, ";
            }
            else
#endif
            {
                if ( Log::GetLevel() >= 2 )
                    out << "denied, demo restricted.\n";
                client->Quit( SuperPower::Demo );
                return;
            }
        }

#ifdef COMPILE_DEDCON
        // duplicate IP?
        if ( serverInfo.unstoppable && ghostProtection == 2 )
        {
            if ( CheckGhosting( *client, false ) )
            {
                if ( Log::GetLevel() >= 2 )
                    out << "denied, ghosting attempt.\n";

                return;
            }
        }
#endif

        // banned?
        if ( userInfo.banned == UserInfo::On )
        {
            out << "denied, banned.\n";
            client->Quit( client->id.GetDemo() ? SuperPower::Demo : SuperPower::Kick );
            return;
        }

        // pick a new client ID
        if ( createNewID )
        {
            // last check: if the ID is to be overridden, do it.
            // demo players can't use this feature, it would allow them to play
            // in arbitrary modes otherwise.
            // Steam users can't use this during playback,
            // or they would get the achievements of the player they
            // take over.
            if ( joinAsID != -1 && ( !client->id.GetDemo() ) 
                 && ( ( !strstr( client->version.c_str(), "STEAM" ) && !strstr( client->version.c_str(), "steam" ) ) || !gameHistory.Playback() )
                )
            {
                // go to the end of the recording
                while( !gameHistory.PlaybackEnded() )
                {
                    gameHistory.Bump();
                }
                FastForward( gameHistory.GetTailSeqID() );

                client->id.SetID( joinAsID );
                client->id.Play( joinAsTeam, true );
                client->inGame = true;
                client->Connect();
                joinAsID = -1;
                createNewID = false;
            }
            else
            {
                if ( joinAsID != -1 )
                {
                    if ( client->id.GetDemo() )
                    {
                        Log::Out() << "JoinAs rejected. You need a fully authenticated client, the demo won't do, ";
                    }
                    else
                    {
                        Log::Out() << "JoinAs with Steam version rejected to prevent achievement cheating. Try the retail version with the authkey of the Steam version copied over, ";
                    }
                }

                client->id.PickID();
            }
        }

        // protect the new client against silenced users
        {
            for ( Client::iterator i = Client::GetFirstClient(); i; ++i )
            {
                Client & silenced = *i;
                if( silenced.id.IsSilenced() && &silenced != client )
                {
                    Host::GetHost().Say( SilenceMessage( silenced ), client );
                    silenced.ignoredBy.Add( client->GetID() );
                }
            }
        }

        if ( Log::GetLevel() >= 2 )
            out << "accepted, ID = " << client->id.GetID() << ".\n";

        if ( logEntry )
        {
            Log::Event() << "CLIENT_NEW " << client->id.GetID() << " "
                         << KeyIDWriter( client->keyID )<< " "
                         << inet_ntoa(client->saddr.sin_addr) << " "
                         << client->version << "\n";
        }

        // query key validity
        assert( metaServer );
        QueryFactory::QueryClientKey( client );

        // compose the login ack message
        if (1)
        {
            AutoTagWriter writer(C_TAG, C_LOGIN_ACCEPTED );
            writer.WriteInt( C_PLAYER_ID, client->id.GetID() );

#ifdef COMPILE_DEDWINIA
            writer.WriteInt( C_LOGIN_RANDOM_SEED, RandomSeed() );
            writer.WriteInt( C_LOGIN_MAKE_SPECTATOR, 0 );
            bool fakeVersion = false;
#endif
            // write our version; lie about it if the client is > 1.51 and we're not.
#ifdef COMPILE_DEDCON
            bool fakeVersion = ( strcmp( client->version.c_str(), "1.52" ) >= 0 &&
                                 strcmp( settings.serverVersion.Get().c_str(), "1.52" ) < 0 );
#endif

            writer.WriteString( C_LOGIN_SERVER_VERSION, fakeVersion ? "1.52 beta1" : settings.serverVersion.Get() );

            client->notYetAcknowledged.clear();
            client->notYetProcessed.clear();

            // store it into the client's send queue
            // it will then be sent with the next send cycle.
            client->LogIn( new Sequence( writer, -1 ) );

#ifdef COMPILE_DEDWINIA
            Map::Players( 99, false );

            // initiate FTP transfer of map
            if ( needMap )
            {
                Map::SendMap( *client );
            }
#endif

            // store all of the other sequences in the client's queue, while we're at it.
            Resync( client, -1 );
        }

#ifdef COMPILE_DEDCON
        // possible new players should stop the game start countdown
        static int stoppedForNewPlayers = 0;
        if ( !serverInfo.unstoppable && Client::CountPlayers() < settings.maxTeams && !client->id.GetDemo() && ++stoppedForNewPlayers <= stopForNewPlayers )
        {
            bool log = serverInfo.everyoneReady;
            SuperPower::UnreadyAll();
            if ( log )
                Host::GetHost().Say( "A new potential player is connecting, stopping countdown." );
        }
#endif

        client->quitTime = -1;

        // generate team message
        if ( createNewID )
        {
            client->id.Spectate();
        }

        int minSpeed = settings.MinSpeed();
        if ( client->speed < minSpeed )
        {
            client->SetSpeed( minSpeed );
        }

        if ( inTrouble )
        {
            ClientInTrouble( *client );
        }
    }

private:
    std::string version;     // version of the candidate
    std::string playerKey;   // registration key of the candidate
    std::string platform;    // the platform
    int         playerKeyID; // key id
    std::string pass;        // password attempt
    bool        needMap;     // whether the map file is needed
};

// minimal connection quality
static IntegerSetting maxPing( Setting::ExtendedGameOption, "MaxPing", 1000 );
static IntegerSetting maxPacketLoss( Setting::ExtendedGameOption, "MaxPacketLoss", 30, 100 );

// checks if a player's ping is suitable for playing
static bool CheckPing( Client * client )
{
    // don't accept join requests from players with high ping
    if ( !serverInfo.started && maxPing > 0 && int(client->pingAverager.GetAverage()) > maxPing )
    {
        std::stringstream err;
        err << "Sorry, " ;
        if ( client->GetName().size() > 0 )
        {
            err << client->id.GetName() << ", ";
        }
        err << "you have a ping that is too high. The server limit is " << maxPing.Get() << " ms, and your ping is " << int(client->pingAverager.GetAverage()) << " ms. You can stay here and spectate, or try again later; ping values can fluctuate.";
        Host::GetHost().Say( err.str(), *client );
        return false;
    }

    // don't accept join requests from players with high packet loss
    if ( !serverInfo.started && maxPacketLoss > 0 && int(client->packetLoss.GetAverage() * 100) > maxPacketLoss )
    {
        std::stringstream err;
        err << "Sorry, " << client->id.GetName() << ", you have too much packet loss. The server limit is " << maxPacketLoss.Get() << " percent, and your packet loss is " << int(client->packetLoss.GetAverage()*100) << " percent. You can stay here and spectate.";
        Host::GetHost().Say( err.str(), *client );
        return false;
    }

    return true;
}

// the last time someone clicked ready
static int lastReadyTick = -60;

static BoolSetting optionalSync( Setting::Network, "SyncValueOptional", false );

class TagParserClientAck: public TagParserClient
{
public:
    TagParserClientAck( Client * client ):TagParserClient(client), processed(-2), acked(-2), syncValue(-2){}

protected:
    // called when value-key pairs of the various types are encountered.
    virtual void OnInt( std::string const & key, int value )
    {
        TagParserClient::OnInt( key, value );

        if ( key == C_SEQID_LASTCOMPLETE )
            processed = value;
        if ( key == C_SEQID_LAST )
            acked = value;
    }

    // called when value-key pairs of the various types are encountered.
    virtual void OnChar( std::string const & key, int value )
    {
        TagParserClient::OnChar( key, value );

        if ( key == C_SYNC_VALUE )
        {
            syncValue = value;
        }
    }

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        TagParserClient::OnFinish( reader );

        if ( acked < -1 || processed < -1 )
        {
            throw ReadException( "Unknown ack format" );
        }


        Time current;
        current.GetTime();

        // move the acked packages from the not-yet-acked to the not-yet-processed queue
        bool measurePing = true;
        while ( client->notYetAcknowledged.size() && client->notYetAcknowledged.front().sequence->SeqID() <= acked )
        {
            SequenceAck & ack = client->notYetAcknowledged.front();
            // Sequence * sequence = ack.sequence;

            // measure ping
            if ( measurePing && ack.sequence->SeqID() == acked )
            {
                client->lastAck = current;

                // only the ping of the first packet needs to be measured, the later packets may have been received much
                // earlier by the client, but kept from getting acked because there was a gap.
                measurePing = false;
                int ping = ( current - ack.lastSent ).Milliseconds();
                if ( ping > 0 )
                {
                    client->pingAverager.Add( ping );
                    client->pingAverager.Decay( .99 );
                }

                // no packet loss here
                client->packetLoss.Add( 0 );
            }

            client->notYetProcessed.push_back( client->notYetAcknowledged.front() );
            client->notYetAcknowledged.pop_front();

            --client->writeAhead;
        }

        if( syncValue < 0 && !optionalSync )
        {
            client->OutOfSync( true, gameHistory.GetTailSeqID() );
        }

        // store the syncValues for the processed packages and delete them
        while ( client->notYetProcessed.size() && client->notYetProcessed.front().sequence->SeqID() <= processed )
        {
            Sequence * sequence = client->notYetProcessed.front().sequence;

            // store syncValue
            if ( sequence->SeqID() == processed && syncValue >= 0 )
                sequence->AddSyncValue( client, syncValue );
            else
                sequence->AddNoSyncValue( client );

            client->notYetProcessed.pop_front();
        }

#ifdef DEBUG
        if ( Log::GetLevel() >= 4 )
            Log::Out() << "ack " << acked << ", processed = " << processed << ", ping = " << client->pingAverager.GetAverage() << '\n';
#endif
    }
private:
    int processed;  //!< the last processed package
    int acked;      //!< the last received package without gaps before it
    int syncValue;  //!< the syncValue of the simulation up to the processed package
};

class TagParserClientResync: public TagParserClient
{
public:
    TagParserClientResync( Client * client ):TagParserClient( client ), start(-1){}

protected:
    // called when value-key pairs of the various types are encountered.
    virtual void OnInt( std::string const & key, int value )
    {
        TagParserClient::OnInt( key, value );

        // read start point? Was always -1 when testing
        if ( key == C_SEQID_LAST )
            start = value;
    }

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        TagParserClient::OnFinish( reader );

        if ( static_cast<int>( client->notYetAcknowledged.size() ) < gameHistory.GetTailSeqID() / 4 )
        {
            ClientInTrouble( *client );

            if ( ++client->numberOfSyncs <= maxResyncAttempts )
            {
                // re-add all sequences with seqID > start to the queue.
                client->notYetAcknowledged.clear();
                Resync( client, start );

                if ( client->numberOfSyncs == maxResyncAttempts )
                {
                    std::ostringstream s;
                    Host::GetHost().Say( "You seem to be badly out of sync; usually, there is no hope for recovery. Sorry, the next time you have to click that button, the connection will have to be terminated.", client );
                }
            }
            else
            {
                // sorry, I don't see how resyncing again and again would
                // change this: you're out of sync. Face it.
                std::ostringstream s;
                s << *client << " seems to be hopelessly out of sync with no chance of recovery. There is no choice but quick termination. Sorry!";
                Host::GetHost().Say( s.str() );
                client->Quit( SuperPower::Kick );
            }
        }
    }
private:
    int start;
};

#ifdef COMPILE_DEDCON
class TagParserClientTerritory: public TagParserClient
{
public:
    TagParserClientTerritory( Client * client ):TagParserClient( client ), territory(-1){}

protected:
    // called when value-key pairs of the various types are encountered.
    virtual void OnChar( std::string const & key, int value )
    {
        TagParserClient::OnChar( key, value );

        //! read alliance
        if ( key == C_TEAM_TERRITORY )
            territory = value;
    }

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        TagParserClient::OnFinish( reader );

        SuperPower * power = SuperPower::GetTeam( teamID );
        if ( !power )
            throw ReadException( "No team of given ID found." );

        power->OnlyToggleTerritory( territory );
    }
private:
    int territory;
};
#endif

static StringSetting demoPlayerMessage(  Setting::ExtendedGameOption, "DemoPlayerMessage",
                                        "Sorry, you can't play, the maximal number of allowed demo players is already in the game. Why don't you go buy " GAME_NAME " now? It's only two clicks away from here." );

class TagParserClientEnter: public TagParserClient
{
public:
    TagParserClientEnter( Client * client ):TagParserClient( client )
    {
        human = 1;
    }

protected:
    virtual void OnInt( std::string const & key, int value )
    {
        TagParserClient::OnInt( key, value );

#ifdef COMPILE_DEDCON
        if ( key == C_PLAYER_HUMAN )
            human = value;
#endif
        // else if ( key == C_SEQID_LAST )
        // seqID = value;
    }

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        TagParserClient::OnFinish( reader );

        if ( client->id.IsPlaying() )
            return;

#ifndef COMPILE_DEDWINIA
        {
            char const * veto = PlayVeto( client );
            if ( veto )
            {
                std::ostringstream s;
                s << "Sorry you can't play, " << veto << ".";
                Host::GetHost().Say( s.str(), client );
                return;
            }
        }
#endif

#ifdef COMPILE_DEDCON
        if ( Log::GetLevel() >= 2 )
        {
            Log::Out() << "Team creation request from " << *client << " received.\n";
        }
#endif

        // don't accept join requests when this is just a playback
        if ( gameHistory.Playback() )
            return;

        // don't accept join requests when the game is already rocking
        if ( serverInfo.started )
            return;

        // don't accept join requests when the game is already rocking
        if ( client->noPlay )
        {
            std::stringstream err;
            if ( client->forceReady )
            {
                err << "You are banned from playing for trying to stall the game. You can spectate if you want.";
            }
            else
            {
                err << "Sorry, " << client->id.GetName() << ", you are running a version that is not allowed to play on this server. You can spectate if you want.";
            }

            Host::GetHost().Say( err.str(), *client );
            return;
        }

        // first join command should send new players to spectator mode if the players hit ready
        if ( client->joinedTick == -1 && serverInfo.unstoppable )
        {
            int tps = gameClock.ticksPerSecond;
            client->joinedTick = gameHistory.GetTailSeqID() + tps + tps * tps/5;
            return;
        }

        // count other players in game
        int inGame = 0;
        {
            int demoPlayers = 0;
            for ( ListBase< SuperPower >::const_iterator c = SuperPower::GetFirstSuperPower(); c; ++c )
            {
                if ( c->IsPlaying() )
                {
                    inGame++;
                    if ( c->GetDemo() )
                        demoPlayers++;
                }
            }

            // ignore if teams are full
            if ( inGame >= settings.maxTeams || inGame >= SuperPower::MaxSuperPowers )
            {
                // look for someone with less rights to play
                SuperPower * lowerRank = CanReplacePlayer( client->id );

                // noone? tough luck, can't play
                if ( !lowerRank )
                    return;

                std::ostringstream s;
                s << "Replacing " << *lowerRank << " with " << *client << " because the latter has a higher ranking position.";
                Host::GetHost().Say( s.str() );

                // throw the other guy out
                lowerRank->Spectate();
                inGame--;
                if ( lowerRank->GetDemo() )
                {
                    demoPlayers--;
                }
            }

            // ignore if too many demo players are already playing (unless in demo mode, there, two demo players can fight)
            if ( client->id.GetDemo() && ( demoPlayers >= demoLimits.maxPlayers + ( demoLimits.demoMode ? 1 : 0 ) || settings.demoRestricted > 0 ) )
            {
#ifdef COMPILE_DEDWINIA
                if ( !client->IsOnLAN() )
                {
                    client->Quit( SuperPower::Demo );
                    return;
                }
#else
                Host::GetHost().Say( demoPlayerMessage, *client );
                return;
#endif
            }
        }

        if ( !CheckPing( client ) )
        {
#ifdef COMPILE_DEDWINIA
            client->Quit( SuperPower::ServerFull );
#endif
            return;
        }

        // find and set a good team ID and enter the game.
        client->id.Play();
        if ( client->IsPlaying() )
        {
            inGame++;
        }

        // force advertising if the server has not just become full
        if ( inGame < settings.maxTeams )
        {
            Advertiser::Force();
        }
    }
private:
    int human;  // is it a human?
    // int seqID;  // the clientside seqID

};

//! accounts for admins/moderators
struct AdminAccount
{
    std::string password; //!< the password
    int         level;    //!< the admin level
    int         keyID;    //!< the key ID (-1 for no check)

    AdminAccount(): password(""), level(1000), keyID( -1 ){}
};

// list of all admin accounts
typedef std::multimap< std::string, AdminAccount > AdminAccounts;
static AdminAccounts admins;

//! notifies the client system that the server's key ID is now known
void Client::OnServerKeyID()
{
    // search admin accounts
    for ( AdminAccounts::iterator i = admins.begin(); i != admins.end(); ++i )
    {
        if ( (*i).second.keyID == -2 )
            (*i).second.keyID = serverInfo.keyID;
    }
 }

void Client::Silence()
{
    id.Silence();
    
    Host::GetHost().Say( SilenceMessage( *this ) );
    
    for ( Client::iterator i = Client::GetFirstClient(); i; ++i )
    {
        Client & ignorer = *i;
        if ( &ignorer != this )
        {
            ignoredBy.Add( ignorer.GetID() );
        }
    }
}

//! checks whether the client is on the LAN
bool Client::IsOnLAN() const
{
    return &serverSocket == &lanSocket;
}

//! normalizes a player name by making it lowercase, replacing whitespace with undersocres
//! and removing trailing and leading whitespace
static std::string NormalizeName( std::string const & in )
{
    std::ostringstream s;

    // find the last non-white character
    int lastNonWhite = 0;
    {
        std::istringstream i(in);
        std::ws(i);
        int current = 0;
        while ( !i.eof() )
        {
            if ( !isspace(i.get()) && !i.eof() )
                lastNonWhite = current;

            ++current;
        }
    }

    std::istringstream i(in);

    // eat leading whitespace
    std::ws(i);

    // transform the rest up to the last non-white character
    while ( !i.eof() && lastNonWhite-- >= 0 )
    {
        char c = i.get();
        // replace space by underscores
        if ( isspace(c) )
        {
            s << '_';
        }
        else
        {
            // make everything lowercase
            s << char(tolower(c));
        }
    }

    return s.str();
}

//! finds the admin account of the given player name
static const AdminAccount * FindAdmin( std::string const & name, int keyID, bool matchKey = true )
{
    std::string normalizedName = NormalizeName( name );

    // search admin accounts
    AdminAccounts::const_iterator admin = admins.find( normalizedName );
    if ( admin != admins.end()  )
    {
        AdminAccounts::const_iterator last = admins.upper_bound( normalizedName );
        while( admin != last )
        {
            // name matches, what about the key
            if ( (*admin).second.keyID == keyID || (*admin).second.keyID < 0 || !matchKey )
            {
                // matches, too, so return it
                return &(*admin).second;
            }

            ++admin;
        }
    }

    // failure
    return 0;
}

static void AddAdmin( std::string const & name, AdminAccount const & account )
{
    // search admin accounts
    AdminAccounts::iterator admin = admins.find( name );
    if ( admin != admins.end() )
    {
        // account already given, keep the one with the higher level
        if ( (*admin).second.level > account.level )
        {
            (*admin).second = account;
            return;
        }
    }

    // new account
    admins.insert( std::make_pair( name, account ) );
}

//! additional data about a client sent command
struct CommandData
{
    //! a sequence seserved to take it
    Sequence * reservedSequence;

    //! receiving client ID white-and blacklist
    std::vector< int > whitelist, blacklist;

    //! whether the next chat should be secret
    bool swallowNextChat;

    //! string chat is to be replaced with
    std::string replaceChatWith;

    //! should the client tag be at the back of the sequence?
    bool insertInBack;

    CommandData(): reservedSequence( 0 ), swallowNextChat( false ), insertInBack( false ){}
};

// call when c is spamming
static void Spammer( Client * c, char const * type, bool announce = true )
{
    if ( !c )
    {
        return;
    }

    if ( !c->spammed )
    {
        c->spammed = true;

        // inform players
        if ( announce )
        {
            std::stringstream s;
            s << "Looks like " << *c << " is trying to spam. Feel free to ignore him by saying \"/ignoreid   " << c->id.GetID() << "\".";
            Host::GetHost().Say( s.str() );
        }

        GetGriefLog( *c, "spam" ) << ", spamType = " << type << "\n";
   }
}

// checks whether a chat/name string needs blocking and performs filtering
bool FilterString( std::string & toFilter, Client & client )
{
    std::string old = toFilter;
    bool block = ChatFilterSystem::Get().Filter( client, toFilter );
    
    if ( old != toFilter && Log::GetLevel() >= 1 )
    {
        Log::Out() << "Changing that to \"" << toFilter << "\".\n";
    }
    
    if ( block )
    {
        if ( Log::GetLevel() >= 1 )
        {
            Log::Out() << "Blocking that.";
        }
    }

    return block;
}

// average number of name changes per hour before spam throttling sets in
static IntegerSetting renamesPerHour( Setting::Grief, "RenamesPerHour", 60 );

// flag indicating whether double names are allowes
static IntegerSetting allowDuplicateNames( Setting::Grief, "AllowDuplicateNames", 0, 1 );

class TagParserClientName: public TagParserClient
{
public:
    TagParserClientName( Client * client )
        :TagParserClient( client )
    {
        harmless = true;
    }

protected:
    virtual void OnString( std::string const & key, std::string const & value )
    {
        if ( key == C_PLAYER_NAME )
        {
            newName = value;
        }
    }

    // messages
    void SendMessages( std::vector< std::string > const & messages, bool pub )
    {
        // look for messages left for this player
        for ( std::vector< std::string >::const_iterator i = messages.begin(); i != messages.end(); ++i )
        {
            // send message
            if ( pub )
            {
                Host::GetHost().Say( *i );
            }
            else
            {
                Host::GetHost().Say( *i, *client );
            }
        }
    }

    // checks if a name is taken by anyone else but the owner
    static bool NameTaken( std::string const & name, Client const * owner )
    {
        for ( SuperPower::iterator c = SuperPower::GetFirstSuperPower(); c; ++c )
        {
            // don't count spectators who have left
            if ( c->GetName() == name && ( c->GetState() != SuperPower::NonEx || c->GetAllianceID() >= 0 ) && &(*c) != &(owner->id) )
            {
                return true;
            }
        }

        return false;
    }

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
#ifdef COMPILE_DEDWINIA
        if ( !client->IsPlaying() )
        {
            throw ReadException();
        }
#endif

        TagParserClient::OnFinish( reader );

        if ( newName.size() == 0 )
            return;

        std::string originalName = newName;

        {
#ifndef COMPILE_DEDCON
            int lastWarnLevel = client->chatFilterWarnings;
#endif        
            if ( FilterString( newName, *client ) )
            {
#ifdef COMPILE_DEDCON
                throw ReadException();
#endif
                newName = "<censored>";
            }
#ifndef COMPILE_DEDCON
            // in MW, clients send lots of intermediate rename commands
            client->chatFilterWarnings = lastWarnLevel;
#endif        
        }

        bool newPlayer = false;
        bool significant = true; // flag indicating whether the name change was significant
        if ( client->id.GetName().size() == 0 )
        {
            newPlayer = true;
            client->lastSetName = originalName;
        }
        else if ( client->id.GetName() != newName )
        {
            // check for spam
#ifdef COMPILE_DEDWINIA
            // name changes in the lobby can come character by character.
            // those shouldn't be checked.
            std::string const & oldName = client->lastSetName;
            unsigned int minSize = oldName.size();
            if ( originalName.size() < minSize )
            {
                minSize = originalName.size();
            }
            if ( !serverInfo.started && originalName.size() + oldName.size() - 2*minSize < 4 && ( 0 == strncmp( originalName.c_str(), oldName.c_str(), minSize ) || minSize == 0 ) )
            {
                significant = false;
            }
            client->lastSetName = originalName;
#endif
            if ( significant && !client->nameChangeSpam.Allow( .8, renamesPerHour ) )
            {
                Host::GetHost().Say( "Too many name changes in a row.", *client );
                Spammer( client, "nameChange" );

                newName = client->id.GetName();
            }

            if ( !client->id.IsPlaying() && !settings.spectatorChatChannel  && !FindAdmin( newName, client->keyID, true ) )
            {
                if ( newName != client->id.GetName() )
                {
                    Host::GetHost().Say( "Spectator chat is private, name changes are not allowed.", *client );
                }

                newName = client->id.GetName();
            }

            if ( client->ignoredBy.ignoredBy.size() > 0 )
            {
                if ( newName != client->id.GetName() )
                {
                    Host::GetHost().Say( "You are being ignored by someone, name change not allowed.", *client );
                }

                newName = client->id.GetName();
            }
        }

        // check whether the name is reserved
        AdminAccount const * admin = FindAdmin( newName, client->keyID, true );
        if ( !admin )
        {
            admin = FindAdmin( newName, -1, false );
        }
        if ( ( serverInfo.keyID != 0 ) && admin && ( admin->keyID >= 0 ) && ( admin->keyID != client->keyID ) )
        {
            std::ostringstream message;
            message << "Warning: the nickname " << NormalizeName( newName ) << " is reserved by a different player. This may be an impostor (or someone who picked the same name just by chance).";
            Host::GetHost().Say( message.str() );

            newName = std::string("fake ") + newName;
        }

        // force name
        if ( client->id.ForceName().size() > 0 && client->id.ForceName() != "NONE" && client->id.ForceName() != newName )
        {
            Host::GetHost().Say( "Your name is forced to you by the server administrator.", *client );
            newName = client->id.ForceName();
        }

        // check for duplicate names
        if ( !allowDuplicateNames.Get() && NameTaken( newName, client ) )
        {
            // find a new name
            std::string wishName = newName;

            if ( wishName.size() == 0 )
                wishName = "NewPlayer";

            // remove trailing numbers and append underscore if no trailing number was found
            {
                std::istringstream in( wishName );
                std::ostringstream out;
                bool number = true;

                while ( true )
                {
                    char c = in.get();

                    if ( in.eof() )
                        break;

                    number = isdigit( c );

                    out << c;
                    if ( !number )
                        wishName = out.str();
                }

                if ( !number )
                {
                    out << "_";
                    wishName = out.str();
                }
            }

            // try different numeric suffices
            int suffix = 2;
            do
            {
                std::ostringstream s;
                s << wishName << suffix;
                ++suffix;
                newName = s.str();
            }
            while ( NameTaken( newName, client ) );
        }

        if ( newName != client->id.GetName() )
        {
            if ( Log::GetLevel() >= 1 )
            {
                OStreamProxy out = Log::Out();
                out << "New name for " << *client;
                if ( poser )
                    out << " given by " << *poser;
                out << " : " << newName << "\n";
            }

            Log::Event() << "CLIENT_NAME " << client->id.GetID() << " "
                         << newName << "\n";

            // write comment
            if ( !gameHistory.Playback() )
            {
                Sequence & last = *gameHistory.GetTail(0);

                std::ostringstream out;
                if ( last.GetComment().size() )
                {
                    out << last.GetComment() << "\n";
                }

                out << "New name for " << ClientWriter( *client );
                if ( poser )
                    out << " given by " << ClientWriter( *poser );
                out << " : " << newName;

                last.SetComment( out.str() );
            }
        }

        if ( anonymize.Get() )
        {
            // anonymize without changing the logs
            std::ostringstream s;
            s << client->id.GetID();
            newName = s.str();
        }

        client->id.SetName( newName );

        client->Connect();

        // write name change message (we can't send the received one since the name may
        // have been edited by us)
        AutoTagWriter writer( C_TAG, C_PLAYER_RENAME );
        writer.WriteChar( C_PLAYER_TEAM_ID, teamID );
        writer.WriteFlexString( C_PLAYER_NAME, newName );
        if ( spectator )
        {
            writer.WriteInt( C_PLAYER_SPECTATOR, spectator );
        }
#ifndef COMPILE_DEDWINIA
        gameHistory.AppendClient( writer );
#else
        // Multiwinia does not like it if the server renames the client. The client the message originated from needs to receive it verbatim.
        Sequence * sequence = gameHistory.ForClient();
        std::vector< int > senderList, empty;
        senderList.push_back( client->id.GetID() );
        sequence->InsertClient( writer, empty, senderList ); // send the modified command to everyone but the sender

        AutoTagWriter writer2( C_TAG, C_PLAYER_RENAME );
        writer2.WriteChar( C_PLAYER_TEAM_ID, teamID );
        writer2.WriteFlexString( C_PLAYER_NAME, originalName );

        sequence->InsertClient( writer2, senderList, empty );        // give the sender back what he sent

        if ( newName != originalName && significant )
        {
            Host::GetHost().Say( "The other players see you as \"" + newName + "\".", client );
        }
#endif

        if ( newPlayer )
        {
            // send out messages
            UserInfo const & info = UserInfo::Get( client->saddr.sin_addr, client->keyID, client->version, client->platform );

            SendMessages( info.publicMessages, true );
            SendMessages( info.privateMessages, false );

            // for silenced players, pretend everyone said /ignore
            if ( client->id.IsSilenced() )
            {
                client->Silence();
            }
        }
    }
private:
    std::string newName;
};

class TagParserClientColor: public TagParserClient
{
public:
    TagParserClientColor( Client * client )
        :TagParserClient( client )
    {
        // harmless = true;
        color = -2;
    }

protected:
#ifdef COMPILE_DEDWINIA
    virtual void OnInt( std::string const & key, int value )
    {
        TagParserClient::OnInt( key, value );

        if ( key == C_TEAM_COLOR )
        {
            color = value;
        }
    }
#endif

#ifdef COMPILE_DEDCON
    virtual void OnChar( std::string const & key, int value )
    {
        TagParserClient::OnInt( key, value );

        if ( key == C_TEAM_COLOR )
        {
            color = value;
        }
    }
#endif

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        ColorUse colorUse;
        colorUse.CountCurrent( &client->id );

        if ( !client->IsPlaying() )
        { 
            throw ReadException();
        }

        if( blockClientColorChanges )
        {
            throw ReadException();
        }

        TagParserClient::OnFinish( reader );

        if ( serverInfo.unstoppable )
            throw ReadException();

#ifdef COMPILE_DEDCON        
        SuperPower::UnreadyAll( client->id );
#endif

        if ( color < -1 || color >= MAXCOLOR )
        {
            color = -1;
            client->id.SetAndSendColor( -1 );
            static bool report = true;
            if ( report )
            {
                report = false;
                throw ReadException( "BUG: Illegal color change." );
            }
            else
            {
                throw ReadException();
            }
        }

#ifdef COMPILE_DEDWINIA
        // set the color, if possible
        if( !colorUse.AddPlayer( color ) )
        {
            // hmm, there probably is a misunderstanding. Send all colors, so each client knows
            // what the other colors currently are, but first clear all colours.
            for( SuperPower::const_iterator i = SuperPower::GetFirstSuperPower(); i; ++i )
            {
                if ( i->IsPlaying() )
                {
                    SendColor( *i, -1 );
                }
            }

            for( SuperPower::const_iterator i = SuperPower::GetFirstSuperPower(); i; ++i )
            {
                if ( i->IsPlaying() )
                {
                    SendColor( *i );
                }
            }
        }
        else
#endif
        {
            client->id.SetAndSendColor( color );
        }

        // and swallow the client packet
        throw ReadException();
    }
private:
    int color;
};

#ifdef COMPILE_DEDWINIA
class TagParserClientSelect: public TagParserClient
{
public:
    TagParserClientSelect( Client * client )
        :TagParserClient( client )
    {
        selected = false;
    }

    bool selected;
protected:
    virtual void OnInt( std::string const & key, int value )
    {
        TagParserClient::OnInt( key, value );

        // we're only interested in buildings (turrets) and units (squads?)
        // entities are irrelevant, it seems. (WRONG. All units work via direct control,
        // and directly clicking on them selects them via entity ID)
        if ( key == C_UNIT_SELECT_BUILDING_ID || key == C_UNIT_SELECT_UNIT_ID || key == C_UNIT_SELECT_ENTITY_ID )
        {
            if ( value > -1 )
            {
                // Log::Out() << "selected: " <<  key << "\n";
                selected = true;
            }
        }
    }

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        TagParserClient::OnFinish( reader );
    }
private:
};

static BoolSetting achievementCondom( Setting::ExtendedGameOption, "Quarantine", false );

class TagParserClientAchievement: public TagParserClient
{
public:
    TagParserClientAchievement( Client * client )
        :TagParserClient( client )
    {
        flags = 0;
    }

    int flags;
protected:
    virtual void OnChar( std::string const & key, int value )
    {
        TagParserClient::OnInt( key, value );

        if ( key == C_PLAYER_ACHIEVEMENTS )
        {
            flags = value;
        }
    }

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        client->SetAchievements( flags );

        if ( achievementCondom )
        {
            // ignore the packet to prevent spreading the viral achievements
            throw ReadException();
        }

        // ignore spectators
        if ( teamID >= 4 )
        {
            throw ReadException();
        }

        TagParserClient::OnFinish( reader );
    }
private:
};
#endif

//! reads a user account
class ActionSettingAdminAccount: public ActionSetting
{
public:
    ActionSettingAdminAccount( char const * name ): ActionSetting( Setting::Admin, name ){}

    static const char separator;

    static std::string ReadToDelimiter( std::istream & in )
    {
        std::ostringstream s;
        char c;

        while ( !in.eof() && ( c = in.get() ) != separator )
        {
            s << c;
        }

        return s.str();
    }

    static int ToInt( std::string const & string )
    {
        if ( string.size() == 0 )
            return -1;

        if ( string.size() > 0 && ( string[0] == 'o' || string[0] == 'O' ) )
            return serverInfo.keyID;

        std::istringstream s( string );
        int i;
        s >> i;

        return i;
    }

    virtual void Revert()
    {
        admins.clear();
    }
    
    //! reads an admin account
    virtual void DoRead( std::istream & source )
    {
        std::ws( source );

        AdminAccount account;

        // read user name
        std::string name = NormalizeName( ReadToDelimiter( source ) );

        // read user key ID
        account.keyID = ToInt( ReadToDelimiter( source ) );

        // read admin level
        account.level = ToInt( ReadToDelimiter( source ) );

        if ( source.fail() )
        {
            Log::Err() << "Bad admin account format. Syntax:\nAdmin <username>"
                      << separator << "<key ID>" << separator << "<admin level>"
                      << separator << "<password>\nwhere <key ID> and <password> may be empty.\n";
        }
        else
        {
            // read password
            if( !source.eof() )
            {
                std::ws( source );
                if( !source.eof() )
                {
                    getline( source, account.password );
                }
            }

            AddAdmin( name, account );
        }
    }
};

const char ActionSettingAdminAccount::separator = '~';

//! reads a user account
class ActionSettingPlayerAccount: public ActionSettingAdminAccount
{
public:
    ActionSettingPlayerAccount( char const * name ): ActionSettingAdminAccount( name ){}

    //! reads an admin account
    virtual void DoRead( std::istream & source )
    {
        std::ws( source );

        AdminAccount account;

        // read user name
        std::string name = NormalizeName( ReadToDelimiter( source ) );

        // read user key ID
        std::string key;
        source >> key;
        account.keyID = ToInt( key );

        // set level and password
        account.level = 0x7fffffff;

        // pseudorandom password generation, does not have to be too god, the admin level
        // is low enough.
        {
            std::ostringstream s;
            s << "sf" << rand() << "gHit" << rand();
            account.password = s.str();
        }

        if ( source.fail() )
        {
            Log::Err() << "Bad player account format. Syntax:\nAdmin <username>"
                      << separator << "<key ID>\n";
        }
        else
        {
            AddAdmin( name, account );
        }
    }
};

static ActionSettingAdminAccount adminAccountSetting("Admin");
static SettingAdminLevel adminAccountLevel( adminAccountSetting, 0 );
static ActionSettingPlayerAccount playerAccountSetting("Player");

//! reads a user account
class ActionSettingAdminAccountRemove: public ActionSetting
{
public:
    ActionSettingAdminAccountRemove(): ActionSetting( Setting::Admin, "AdminRemove" ){}

    //! reads an admin account
    virtual void DoRead( std::istream & source )
    {
        std::ws( source );

        std::string name;
        // read name
        getline( source, name );
        name = NormalizeName( name );

        // remove it
        AdminAccounts::iterator found = admins.find( name );
        if ( found != admins.end() )
        {
            admins.erase( found );
            Log::Out() << "Admin account \"" << name << "\" removed.\n";
        }
        else
        {
            Log::Err() << "No admin account of name \"" << name << "\" found.\n";
        }
    }
};

static ActionSettingAdminAccountRemove adminAccountRemoveSetting;

void HandleErrors( std::string error, Client & client )
{
    std::istringstream errRead(error);
    int lines = 10;
    while ( !errRead.eof() && errRead.good() )
    {
        std::string line;
        getline( errRead, line );
        if ( line.size() > 0 )
            Host::GetHost().Say( line, client );

        if ( --lines <= 0 )
        {
            Host::GetHost().Say( "Suppressing further messages.", client );
            break;
        }
    }
}


static void PrintLevel( std::ostream & s, int level )
{
    if ( level <= 0 )
    {
        s << "owner";
        return;
    }

    switch( level )
    {
    case 1:
        s << "admin";
        break;
    case 2:
        s << "metamoderator";
        break;
    default:
        s << "moderator class " << level - 2;
    }
}

class LevelPrinter
{
public:
    explicit LevelPrinter( int level ): l(level){}
    int l;
};

std::ostream & operator << ( std::ostream & s, LevelPrinter const & printer )
{
    PrintLevel( s, printer.l );
    return s;
}

static bool CanHarm( Client & victim, Client & admin )
{
    if ( &victim == &admin )
    {
        Host::GetHost().Say( "In a suicidal mood today?", admin );
        return false;
    }

    if ( victim.admin && admin.admin )
    {
        if( victim.adminLevel == admin.adminLevel )
        {
            Host::GetHost().Say( "Instead of executing the command, admin rights have been taken away from both of you since you are admins of equal level." );
            victim.admin = admin.admin = false;
            victim.adminLevel = admin.adminLevel = 1000;
            return false;
        }
        else if( victim.adminLevel < admin.adminLevel )
        {
            Host::GetHost().Say( "You can't do a superior admin any harm.", admin );
            return false;
        }
    }

    return true;
}

static std::string GetVoteID( Client const & client )
{
    std::string ret = inet_ntoa( client.saddr.sin_addr );

#ifdef DEBUG_X
    // for debugging: I use this IP to connect with multiple clients.
    if ( ret == "192.168.0.1" )
    {
        std::ostringstream s;
        s << client.id.GetID();
        ret = s.str();
    }
#endif

    return ret;
}

// minmum admin level required to execute a kick: default to lowly moderators
static IntegerSetting kickLevel( Setting::Admin, "AdminLevelKick", 4 );
static SettingAdminLevel kickLevelLevel( kickLevel, 0 );

// number of votes it takes at least for a kick (0 to disable kick voting)
static IntegerSetting kickVoteMin( Setting::Grief, "KickVoteMin", 0 );

// number of player votes it takes at least for a kick
static IntegerSetting kickVoteMinPlayers( Setting::Grief, "KickVoteMinPlayers", 0 );

// kick vote bias towards kicks.
static IntegerSetting kickVoteBias( Setting::Grief, "KickVoteBias", 0 );

static void Kick( Client & kicked, Client & admin )
{
    // avoid narrow countdown aborts
    DontStopUnstoppable();

    if ( !admin.admin || admin.adminLevel > kickLevel )
    {
        // enabled?
        if ( kickVoteMin == 0 )
        {
            Host::GetHost().Say( "Kick votes not enabled.", admin );
            return;
        }

        // illegal against admins
        if ( kicked.admin )
        {
            Host::GetHost().Say( "Your selected victim is logged in as administrator/moderator and protected against kick votes.", admin );
            return;
        }

        // illegal against players after game start
        if ( kicked.id.IsPlaying() && serverInfo.started )
        {
            Host::GetHost().Say( "Kick votes are illegal against active players.", admin );
            return;
        }

        // self-kicking is silly
        if ( &kicked == &admin )
        {
            Host::GetHost().Say( "Kicking yourself? Not a good idea.", admin );
            return;
        }

        // look if the vote was already cast
        bool found = false;
        for( std::vector< Client const * >::const_iterator i = kicked.kickVotes.begin(); i != kicked.kickVotes.end(); ++i )
        {
            if ( &admin == (*i) )
            {
                found = true;
                break;
            }
        }
        if ( found )
        {
            Host::GetHost().Say( "Kick vote already registered.", admin );
            return;
        }
        kicked.kickVotes.push_back( &admin );

        // count whether enough votes accumulated for a kick; we need to be careful and
        // check the IP addresses of the voters.
        typedef std::set< std::string > Set;

        // all voters and supporters of the kick
        Set total, pro;

        // same restricted to players
        Set totalPlayers, proPlayers;

        // count voters
        for( std::vector< Client const * >::const_iterator i = kicked.kickVotes.begin(); i != kicked.kickVotes.end(); ++i )
        {
            // only count those that are still online
            if ( (*i)->loginAck )
            {
                std::string ip = GetVoteID( *(*i) );
                pro.insert( ip );
                if ( (*i)->id.IsPlaying() )
                {
                    proPlayers.insert( ip );
                }
            }
        }

        // count total players
        for ( Client::const_iterator i = Client::GetFirstClient(); i; ++i )
        {
            std::string ip = GetVoteID( (*i) );
            total.insert( ip );
            if ( (*i).id.IsPlaying() )
            {
                totalPlayers.insert( ip );
            }
        }

        if ( (int)proPlayers.size() >= kickVoteMinPlayers && (int)pro.size() >= kickVoteMin &&
            ( proPlayers.size() * 2 > totalPlayers.size()
             ||
             pro.size() * 2 > total.size()
                 ) )
        {
            // transform the kick into a removal if the reason seems to be that
            // the kicked player is not readying up
#ifndef COMPILE_DEDWINIA
            if ( !serverInfo.started && kicked.id.IsPlaying() && !kicked.forceReady )
            {
                std::ostringstream s;
                s << "Kick vote against " << kicked << " succeeded and transformed into a removal from the active players.";
                Host::GetHost().Say( s.str() );

                kicked.forceReady = true;
                kicked.id.Spectate();
                kicked.kickVotes.clear();

                return;
            }
#endif

            std::ostringstream s;
            s << "Kick vote against " << kicked << " succeeded.";
            Host::GetHost().Say( s.str() );

            std::ostringstream log;

            // log all supporters
            for( std::vector< Client const * >::const_iterator i = kicked.kickVotes.begin(); i != kicked.kickVotes.end(); ++i )
            {
                Client const & supporter = *(*i);

                // only count those that are still online
                if ( supporter.loginAck )
                {
                    log
                    << ", kicker = " << supporter
                    << ", kickerKeyID = " << KeyIDWriter(supporter.keyID)
                    << ", kickederIP = " <<  inet_ntoa(supporter.saddr.sin_addr)
                    << ", kickerName = " << supporter.id.GetName();

                    std::string ip = GetVoteID( *(*i) );
                    pro.insert( ip );
                    if ( (*i)->id.IsPlaying() )
                    {
                        proPlayers.insert( ip );
                    }
                }
            }
            log << '\n';

            GetGriefLog( kicked, "voteKick" ) << log.str();

            kicked.Quit( SuperPower::Kick );
        }
        else
        {
            // announce it
            std::ostringstream s;
            s << admin << " votes to kick " << kicked << ", say \"/kickid   "
              << kicked.id.GetID() << "\" if you agree.";
            Host::GetHost().Say( s.str() );
        }

        return;
    }

    if ( kicked.inGame && admin.inGame )
    {
        Host::GetHost().Say( "Kicking a player while you are playing yourself is not possible. /op an spectator to do the kick." );
        return;
    }

    // kick that client
    if ( CanHarm( kicked, admin ) )
    {
        GetGriefLog( kicked, "adminKick" )
            << ", kicker = " << admin
            << ", kickerKeyID = " << KeyIDWriter(admin.keyID)
            << ", kickederIP = " <<  inet_ntoa(admin.saddr.sin_addr)
            << ", kickerName = " << admin.adminName
            << '\n';

        Log::Out() << kicked << " kicked.\n";
        kicked.Quit( SuperPower::Kick );
    }
}

#ifndef COMPILE_DEDWINIA
// removes a player from the game
static void Remove( Client & kicked, Client & admin )
{
    // avoid narrow countdown aborts
    DontStopUnstoppable();

    if ( !admin.admin || admin.adminLevel > kickLevel )
    {
        Host::GetHost().Say( "Insufficient rights to remove player.", admin );
    }
    else if ( serverInfo.started )
    {
        Host::GetHost().Say( "It is impossible to remove a player after the game has started, sorry.", admin );
    }
    else
    {
        std::ostringstream message;
        message << "Removing " << kicked << " from the game by order of " << admin << ".\n";
        Host::GetHost().Say( message.str() );
        kicked.id.Spectate();
    }
}
#endif

// maximum admin level a /oped player will get initially
static IntegerSetting maxOpLevel( Setting::Admin, "OpLevel", 2 );

static void Op( Client & op, Client & admin )
{
    if ( !op.admin )
    {
        op.admin = true;
        op.adminName = op.id.GetName();
        op.adminLevel = admin.adminLevel + 1;

        if ( op.adminLevel < maxOpLevel )
            op.adminLevel = maxOpLevel;

        std::ostringstream s;
        s << op << " has been given the rights of " << LevelPrinter(op.adminLevel) << '.';
        Host::GetHost().Say( s.str() );
    }
}

void DeOp( Client & op, Client & admin )
{
    if ( op.admin && CanHarm( op, admin ) )
    {
        std::ostringstream s;
        s << "Administrator rights taken away from " << op << ".";
        Host::GetHost().Say( s.str() );
    }

    op.admin = false;
}

void ChangeLevel( Client & op, Client & admin, int delta )
{
    if ( op.admin )
    {
        // clamp
        if ( op.adminLevel + delta <= admin.adminLevel )
            delta = admin.adminLevel - op.adminLevel;

        std::ostringstream s;
        if ( op.adminLevel <= admin.adminLevel && ( &op != &admin || delta == 0 ) )
        {
            s << "You can only change the admin level of subordinates. You are " << LevelPrinter( admin.adminLevel ) << ", " << op << " is " << LevelPrinter( op.adminLevel ) << '.';
            Host::GetHost().Say( s.str(), admin );
            return;
        }

        if ( delta == 0 )
            return;

        {
            s << ( delta > 0 ? "Demoting " : "Promoting " )
              << op << " from " << LevelPrinter( op.adminLevel ) ;
            op.adminLevel += delta;
            s << " to " << LevelPrinter( op.adminLevel ) << ".";

            Host::GetHost().Say( s.str() );
        }
    }
}

// find a client with a string in his name
template< class T >
struct EntityGetter
{
    static T * Get( std::string const & namePart, Client * origin, typename ListBase< T >::iterator start, bool inGame, std::string const & command )
    {
        int exactMatches = 0;
        T * exactMatch = 0;

        std::vector< T * > partialMatches;

        for ( typename T::iterator c = start; c; ++c )
        {
            if ( inGame && c->GetTeamID() < 0 )
            {
                // only clients actively playing should be included
                continue;
            }

            if ( c->GetName() == namePart )
            {
                exactMatches++;
                exactMatch = &(*c);
            }
            if ( strstr( c->GetName().c_str(), namePart.c_str() ) )
            {
                partialMatches.push_back( &(*c) );
            }
        }

        if ( exactMatches == 1 )
        {
            return exactMatch;
        }
        if ( partialMatches.size() == 1 )
        {
            return partialMatches.front();
        }

        std::ostringstream err;

        if ( partialMatches.size() == 0 )
        {
            err << "No client with name containing " << namePart << " found, check for typos or use " << command << "id instead.";
        }
        else
        {
            err << "Too many clients with name containing " << namePart << " found, be more specific. Found: ";
            bool comma = false;
            for ( typename std::vector< T * >::const_iterator i = partialMatches.begin();
                  i != partialMatches.end(); ++i )
            {
                if ( comma )
                {
                    err << ", ";
                }
                comma = true;

                err << (*i)->GetName();
            }

            err << ".";
        }

        Host::GetHost().Say( err.str(), origin );

        return 0;
    }

    static T * Get( int id, Client * origin, typename ListBase< T >::iterator start )
    {
        typename T::iterator c = start;
        for ( ; c; ++c )
        {
            if ( c->GetID() == id || c->GetTeamID() == id )
            {
                return &(*c);
            }
        }

        std::ostringstream err;
        err << "No client with ID " << id << " found.";
        Host::GetHost().Say( err.str(), origin );

        return 0;
    }

	// find a client identified by stream data.
	// the client is identified by name if the flag says so,
	// otherwise by numeric ID.
    static T * Get( bool byName, std::istream & stream, std::string command, char const * action, Client * origin, typename ListBase<T>::iterator start )
    {
        // recognize chat commands and modify them for printing
        if ( islower( command[0] ) )
        {
             command = "/" + command + "  ";
        }

        if ( byName )
        {
            std::string name;
            getline( stream, name );
            if ( name.size() == 0 )
            {
                Host::GetHost().Say( "Usage: " + command + " <part of the name of the player to " + action + ">", origin );
                return 0;
            }

            return Get( name, origin, start, false, command );
        }
        else
        {
            int id = -1;
            stream >> id;

            if ( id < 0 )
            {
                Host::GetHost().Say( "Usage: " + command + " <id of the player to " + action + ", get a list with /ping>", origin );
                return 0;
            }

            return Get( id, origin, start );
        }
    }
};

// find a client with a string in his name
/*
static Client * GetClient( std::string const & namePart, Client * origin )
{
    return EntityGetter< Client >::Get( namePart, origin, Client::GetFirstClient() );
}

*/

#ifdef COMPILE_DEDCON
static SuperPower * GetSuperPower( std::string const & namePart, Client * origin, bool inGame, std::string const & command )

{
    return EntityGetter< SuperPower >::Get( namePart, origin, SuperPower::GetFirstSuperPower(), inGame, command );
}

static SuperPower * GetSuperPower( int id, Client * origin )
{
    return EntityGetter< SuperPower >::Get( id, origin, SuperPower::GetFirstSuperPower() );
}
#endif

// find a client identified by stream data.
// the client is identified by name if the flag says so,
// otherwise by numeric ID.
static Client * GetClient( bool byName, std::istream & stream, std::string const & command, char const * action, Client * origin )
{
    return EntityGetter< Client >::Get( byName, stream, command, action, origin, Client::GetFirstClient() );
}

// find a client identified by stream data.
static Client * GetClient( std::istream & stream, std::string const & command, char const * action, Client * origin )
{
    int len = command.size();
    bool byName = len < 2 || tolower( command[len-2] ) != 'i' || tolower( command[len-1] ) != 'd';
    return GetClient( byName, stream, command, action, origin );
}

#ifdef COMMENT
// find a client identified by stream data.
// the client is identified by name if the flag says so,
// otherwise by numeric ID.
static SuperPower * GetSuperPower( bool byName, std::istream & stream, std::string const & command, char const * action, Client * origin )
{
    return EntityGetter< SuperPower >::Get( byName, stream, command, action, origin, SuperPower::GetFirstSuperPower() );
}

// find a client identified by stream data.
static SuperPower * GetSuperPower( std::istream & stream, std::string const & command, char const * action, Client * origin )
{
    int len = command.size();
    bool byName = len < 2 || command[len-2] != 'i' || command[len-1] != 'd';
    return GetSuperPower( byName, stream, command, action, origin );
}
#endif

//! kick admin setting
class ActionSettingKick: public ActionSetting
{
public:
    ActionSettingKick( char const * name ): ActionSetting( Setting::Grief, name ){}
private:
    //! reads the value of the setting from a stream.
    virtual void DoRead( std::istream & source )
    {
        std::ws( source );

        Client * toKick = GetClient( source, name, "kick", NULL );
        if ( toKick )
        {
            if ( toKick->inGame )
            {
                Log::Out() << "You cannot kick active players via the console commands, you have to be logged in as a spectator for that.\n";
            }
            else
            {
                Log::Out() << *toKick << " kicked.\n";
                toKick->Quit( SuperPower::Kick );
            }
        }
    }
};

static ActionSettingKick kick( "Kick" ), kickID( "KickID" );

// minimum admin level to bypass chat filters
static IntegerSetting sayLevel( Setting::Admin, "AdminLevelSay", 2 );
static SettingAdminLevel sayLevelLevel( sayLevel, 0 );

// minimum admin level to change server settings
static IntegerSetting setLevel( Setting::Admin, "AdminLevelSet", 1 );
static SettingAdminLevel setLevelLevel( setLevel, 0 );

// minimum admin level to inlcude preset files
static IntegerSetting includeLevel( Setting::Admin, "AdminLevelInclude", 4 );
static SettingAdminLevel includeLevelLevel( includeLevel, 0 );

// minmum admin level required to pass on admin rights, defaults to metamoderators
static IntegerSetting opLevel( Setting::Admin,"AdminLevelOp", 2 );
static SettingAdminLevel opLevelLevel( opLevel, 0 );

static void PrintPing( Client * receiver )
{
    for ( Client::iterator c = Client::GetFirstClient(); c; ++c )
    {
        std::ostringstream s;
        s << *c << " : " <<  int(c->pingAverager.GetAverage()) << ", pl = " << int(c->packetLoss.GetAverage()*100);
        if ( c->notYetProcessed.size() > 3 && c->IsConnected() )
        {
            s << ", client lag = " << c->notYetProcessed.size() * .1 << " seconds";
        }
        Host::GetHost().Say( s.str(), receiver );
    }

    if ( !Client::GetFirstClient() )
    {
        Host::GetHost().Say( "No Players.", receiver );
    }
}

class PingCommand: public ActionSetting
{
public:
    PingCommand(): ActionSetting( Setting::Network, "Ping" ){}

    virtual void DoRead( std::istream & )
    {
        PrintPing( 0 );
    }
};
static PingCommand pingCommand;

// logs pings into event log
class LogPingCommand: public ActionSetting
{
public:
    LogPingCommand(): ActionSetting( Setting::Network, "LogPing" ){}

    virtual void DoRead( std::istream & )
    {
        for ( Client::iterator c = Client::GetFirstClient(); c; ++c )
        {
            Log::Event() << "CLIENT_PING " << c->GetID() 
                         << " " << c->pingAverager.GetAverage() 
                         << " " << c->packetLoss.GetAverage()*100
                         << " " << c->notYetProcessed.size()
                         <<  "\n";
        }
    }
};
static LogPingCommand logPingCommand;

static void UnSignScores( Client & client )
{
    if ( client.scoresSigned )
    {
        client.scoresSigned = false;
    }
}

// unsign all scores
static void UnsignAllScores()
{
    bool wasSigned = false;

    // have everyone unsign the scores
    for ( Client::iterator i = Client::GetFirstClient(); i; ++i )
    {
        wasSigned |= i->scoresSigned;
        UnSignScores( *i );
    }
    for ( Client::iterator i = deletedClients.begin(); i; ++i )
    {
        wasSigned |= i->scoresSigned;
        UnSignScores( *i );
    }

    if ( wasSigned )
    {
        static bool warn = true;
        if ( warn )
        {
            Host::GetHost().Say( "All signatures on the current scores were revoked. This is probably a good time to remind everyone that entering and signing wrong scores is among the worst things you can do and usually results in an immediate and permanent ban. If you were correcting an earlier mistake, then everything is fine." );
            warn = false;
        }
        else
        {
            Host::GetHost().Say( "All signatures on the current scores were revoked." );
        }
    }
}

// fills an array with clients who signed the scores
static void GetScoreSigners( std::vector< Client * > & signers )
{
#if VANILLA
    signers.reserve(6);
#elif EIGHTPLAYER
    signers.reserve(8);
#elif TENPLAYER
    signers.reserve(10);
#endif
    // get all the clients that signed
    for ( Client::iterator i = Client::GetFirstClient(); i; ++i )
    {
        if ( i->scoresSigned )
        {
            signers.push_back( &(*i) );
        }
    }
    for ( Client::iterator i = deletedClients.begin(); i; ++i )
    {
        if ( i->scoresSigned )
        {
            signers.push_back( &(*i) );
        }
    }
}

// checks whether all scores have been entered
static bool ScoresSet( Client * receiver, bool silent = false )
{
    std::vector< SuperPower * > powers;
    SuperPower::GetSortedByScore( powers );

    bool warn = true;

    for ( std::vector< SuperPower * >::const_iterator i = powers.begin(); i != powers.end(); ++i )
    {
        SuperPower const & p = *(*i);

        if ( !p.ScoreSet() )
        {
            if ( !silent )
            {
                if ( warn )
                {
                    Host::GetHost().Say( "Signature not accepted, not all scores have been entered yet. Missing are:", receiver );
                }

                std::ostringstream s;
                if ( receiver )
                {
                    s << "team " << p.GetTeamID() << " (";
                    s << p.GetPrintName();
                    s << ")";
                }
                else
                {
                    s << p;
                }
                Host::GetHost().Say( s.str(), receiver );
            }

            warn = false;
        }
    }

    return warn;
}

// counts the number of players/spectators in an array
static void CountSigners( std::vector< Client * > const & signers, int & players, int & spectators )
{
    players = spectators = 0;
    for ( std::vector< Client * >::const_iterator i = signers.begin(); i != signers.end(); ++i )
    {
        if ( (*i)->inGame )
        {
            players++;
        }
        else
        {
            spectators++;
        }
    }
}

// checks whether signersB is better than signersA
static bool MoreOrEqualSigners( std::vector< Client * > const & signersA, std::vector< Client * > const & signersB )
{
    int playersA = 0, spectatorsA = 0;
    int playersB = 0, spectatorsB = 0;

    // remove players from signersA that are also in singersB; they
    // apparently changed their mind.
    std::vector< Client * > reducedSignersA;
    for ( std::vector< Client * >::const_iterator i = signersA.begin(); i != signersA.end(); ++i )
    {
        // the two sets are identical
        if ( std::find( signersB.begin(), signersB.end(), *i ) == signersB.end() )
            reducedSignersA.push_back( *i );
    }

    CountSigners( reducedSignersA, playersA, spectatorsA );
    CountSigners( signersB, playersB, spectatorsB );

    if ( playersB > playersA )
    {
        return true;
    }
    if ( playersB < playersA )
    {
        return false;
    }

    return spectatorsB > spectatorsA;
}

// have a client sign all scores
static void SignScores( Client & client )
{
    if ( !client.scoresSigned )
    {
#ifndef COMPILE_DEDWINIA
        std::ostringstream s;
        s << client << " signs the current scores.";
        Host::GetHost().Say( s.str() );
#endif

        client.scoresSigned = true;
    }

    // get the current signers
    std::vector< Client * > signers;
    GetScoreSigners( signers );
    if ( MoreOrEqualSigners( bestSigners, signers ) )
    {
        // more signers than ever, yay. Mark that.
        bestSigners = signers;

        for ( SuperPower::iterator i = SuperPower::GetFirstSuperPower(); i; ++i )
        {
            i->SignScore();
        }
    }
    else
    {
        Host::GetHost().Say( "The new signatures are not yet officially accepted, more signatures are needed to fully revoke the previously signed scores." );
    }
}

static void PrintScores( Client * receiver, bool listNonEntered, bool listEmpty = true )
{
    std::vector< SuperPower * > powers;
    SuperPower::GetSortedByScore( powers );

    for ( std::vector< SuperPower * >::const_iterator i = powers.begin(); i != powers.end(); ++i )
    {
        SuperPower const & p = *(*i);

        std::ostringstream s;
        if ( receiver )
        {
            s << "team " << p.GetTeamID() << " (";
            s << p.GetPrintName();
            s << ")";
        }
        else
        {
            s << p;
        }
        s << " : ";
        if ( p.ScoreSet() )
        {
            s << int ( p.GetScore() );
        }
        else
        {
            if ( listNonEntered )
            {
                s << "no score entered.";
            }
            else
            {
                continue;
            }
        }

        Host::GetHost().Say( s.str(), receiver );
    }

    if ( powers.size() == 0 && listEmpty )
    {
        Host::GetHost().Say( "No Teams.", receiver );
    }

    std::vector< Client * > signers;
    GetScoreSigners( signers );

    if ( signers.size() > 0 )
    {
        std::ostringstream s;
        s << "Scores signed by";
        bool comma = false;
        for ( std::vector< Client * >::const_iterator i = signers.begin(); i != signers.end(); ++i )
        {
            if ( comma )
            {
                s << ", ";
            }
            else if ( signers.size() > 1 )
            {
                s << " : ";
            }
            else
            {
                s << " ";
            }
            s << *(*i);
            comma = true;
        }
        s << ".";
        Host::GetHost().Say( s.str(), receiver );
    }

    if ( receiver )
    {
        receiver->scoresLastSeen = scoreRevision;
    }
}

static void LogScores()
{
    std::vector< SuperPower * > powers;
    SuperPower::GetSortedByScore( powers );

    Log::Event() << "SCORE_BEGIN\n";

    for ( std::vector< SuperPower * >::const_iterator i = powers.begin(); i != powers.end(); ++i )
    {
        SuperPower const & p = *(*i);

        std::ostringstream s;
        Log::Event() << "SCORE_TEAM " << p.GetTeamID() << ' ' << p.GetScoreSigned() << ' '
                     << p.GetID() << ' ' << p.GetName() << '\n';
    }

    for ( std::vector< Client * >::const_iterator i = bestSigners.begin(); i != bestSigners.end(); ++i )
    {
        Client & c = *(*i);
        const char * leadIn = 0;
        if ( c.inGame )
        {
            leadIn = "SCORE_SIGNATURE_PLAYER ";
        }
        else
        {
            leadIn = "SCORE_SIGNATURE_SPECTATOR ";
        }

        Log::Event() << leadIn << c.id.GetID() << ' ' << KeyIDWriter( c.keyID ) << ' ' << c.id.GetName() << '\n';
    }

    Log::Event() << "SCORE_END\n";
}

class ScoreCommand: public ActionSetting
{
public:
    ScoreCommand(): ActionSetting( Setting::Network, "Score" ){}

    virtual void DoRead( std::istream & )
    {
        PrintScores( 0, true );
    }
};
static ScoreCommand scoreCommand;

static void HelpAdmin( std::istream & s, std::ostream & o, bool remote )
{
    int charsPerLine = 80;
    if ( remote )
    {
        charsPerLine = 512;
    }

    std::string command;
    std::ws( s );
    if ( !s.eof() )
    {
        s >> command;
    }

    if ( command == "Internal" )
    {
        Setting::ListAll( Setting::Internal, o, charsPerLine );
    }
    else if ( command == "Game" )
    {
        Setting::ListAll( Setting::GameOption, o, charsPerLine );
    }
    else if ( command == "ExtendedGame" )
    {
        Setting::ListAll( Setting::ExtendedGameOption, o, charsPerLine );
    }
    else if ( command == "CPU" )
    {
        Setting::ListAll( Setting::CPU, o, charsPerLine );
    }
    else if ( command == "Admin" )
    {
        Setting::ListAll( Setting::Admin, o, charsPerLine );
    }
    else if ( command == "Player" )
    {
        Setting::ListAll( Setting::Player, o, charsPerLine );
    }
    else if ( command == "Grief" )
    {
        Setting::ListAll( Setting::Grief, o, charsPerLine );
    }
    else if ( command == "Log" )
    {
        Setting::ListAll( Setting::Log, o, charsPerLine );
    }
    else if ( command == "Network" )
    {
        Setting::ListAll( Setting::Network, o, charsPerLine );
    }
    else if ( command == "Flow" )
    {
        Setting::ListAll( Setting::Flow, o, charsPerLine );
    }
    else
    {
        if ( remote )
        {
            o << "Usage: /help admin <group>. ";
        }
        else
        {
            o << "Usage: Help <group>. ";
        }

        o << "Available groups: Game, ExtendedGame, CPU, Admin, Grief, Log, Network, Flow, Internal.\n";
    }
}

//! help for the console
class HelpActionSetting: public ActionSetting
{
public:
    HelpActionSetting()
    : ActionSetting( Setting::Admin, "Help" )
    {}

    //! activates the action with the specified value
    virtual void DoRead( std::istream & source )
    {
        HelpAdmin( source, *Log::Out().GetWrapper().GetStream(), false );
    }
};
static HelpActionSetting help;

class ChatCommand;

struct ChatCommandPointer
{
    ChatCommand * command;

    ChatCommandPointer()
    : command( 0 ){}
};

// the mapping of command names to commands
static std::map< std::string, ChatCommandPointer > chatCommands;
static std::map< std::string, ChatCommandPointer > chatCommandsSecondary;

// the current commant
static std::string currentChatCommand;

// a chat command
class ChatCommand
{
public:
    virtual ~ChatCommand(){}

    virtual char const * GetHelp() = 0;

    void RegisterSecondary( char const * name )
    {
        chatCommandsSecondary[name].command = this;
    }

    void RegisterSecondary()
    {
        RegisterSecondary( (name + "id").c_str() );
    }

    static bool Activate( std::string const & command, Client & client, std::istream & s, CommandData & commandData )
    {
        currentChatCommand = command;
        ChatCommand * p = chatCommands[command].command;
        if ( !p )
        {
            p = chatCommandsSecondary[command].command;
        }
        bool ret = false;
        if ( p )
        {
            if ( !client.admin && p->IsAdminCommand() )
            {
                Host::GetHost().Say( "Admin rights are required for this command.", client );
            }
            else
            {
                p->DoActivate( client, s, commandData );
            }
            ret = true;
        }
        currentChatCommand = "";
        return ret;
    }

    static std::string const & CurrentCommand()
    {
        return currentChatCommand;
    }

    virtual bool IsAdminCommand(){ return false; } 
protected:
    ChatCommand( char const * name_ )
    : name( name_ )
    {
        chatCommands[name].command = this;
    }

    std::string const & GetName() const { return name; }
private:
    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData ) = 0;

    std::string name;
};

// a chat command
class ChatCommandAdmin: public ChatCommand
{
public:
protected:
    ChatCommandAdmin( char const * name ): ChatCommand( name ){}
private:
    virtual bool IsAdminCommand(){ return true; } 
};

#ifdef NEVER_DEFINE_THIS
// prototype to copy/paste
class ChatCommandPrototype: public ChatCommand
{
public:
    ChatCommandPrototype():ChatCommand("prototype"){}
private:
    virtual char const * GetHelp()
    {
        return "";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
    }
};
static ChatCommandPrototype chatCommandPrototype;
#endif

static void ListCommands( bool admin, std::ostream & s )
{
    bool first = true;
    for( std::map< std::string, ChatCommandPointer >::iterator i = 
         chatCommands.begin(); i != chatCommands.end(); ++i )
    {
        if ( (*i).second.command && (*i).second.command->IsAdminCommand() == admin )
        {
            if ( !first )
                s << ", ";
            
            s << "/" << (*i).first;
            first = false;
        }
    }
}

// help
static void Help( std::istream & s, Client & client )
{
    std::string command;
    std::ws( s );
    if ( s.get() != '/' )
        s.unget();

    s >> command;

    if ( command.size() == 0 )
    {
        std::ostringstream regular;
        regular << "Available commands: ";
        ListCommands( false, regular );

        Host::GetHost().Say( regular.str(), client );

        if ( client.admin )
        {
            std::ostringstream admin;
            admin << "Admin commands: ";
            ListCommands( true, admin );

            Host::GetHost().Say( admin.str(), client );
        }

        // Host::GetHost().Say( "All commands referencing other players have two variants, one taking part of the client name, the other taking a client ID (get a list with /ping or /score) instead; for example, there are /kick <name> and /kickid <id>.", client );
        Host::GetHost().Say( "Get more help with, for example, \"/help /ping\".", client );
    }
    else if ( chatCommands[command].command != 0 )
    {
        Host::GetHost().Say( chatCommands[command].command->GetHelp(), client );
        if ( chatCommandsSecondary[command + "id"].command != 0 )
        {
            Host::GetHost().Say( std::string( "There also is the /") + command + "id command which does the same, but accepts a numeric client or team ID (get it with /ping, /score or /info) instead of part of the player name.", client );
        }
    }
    else if ( chatCommandsSecondary[command].command != 0 )
    {
        Host::GetHost().Say( chatCommandsSecondary[command].command->GetHelp(), client );
    }
    else if ( command == "admin" )
    {
        std::ostringstream o;
        HelpAdmin( s, o, true );
        HandleErrors( o.str(), client );
    }
    else
    {
        Host::GetHost().Say( "No help for /" + command + " available.", client );
    }
}

class ChatCommandHelp: public ChatCommand
{
public:
    ChatCommandHelp():ChatCommand("help"){}
private:
    virtual char const * GetHelp()
    {
        return "Processing, please wait... please wait... please wait... (now look what you have done, an endless loop.)";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        ::Help( s, client );
    }
};
static ChatCommandHelp chatCommandHelp;

class ChatCommandPing: public ChatCommand
{
public:
    ChatCommandPing():ChatCommand("ping"){}
private:
    virtual char const * GetHelp()
    {
        return "Gives a list of connected clients with their pings (network round trip times) in milliseconds and packet loss ratio in percent.";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        // hide command from others
        commandData.whitelist.push_back( client.id.GetID() );
        
        PrintPing( &client );

    }
};
static ChatCommandPing chatCommandPing;

// Copied the structure of what berts other chatcommands are and just made /gamealert show a message
// and not just show unknown command :)
static std::map<int, Time> lastGameAlerts;

class ChatCommandGameAlert: public ChatCommand
{
public:
    ChatCommandGameAlert(): ChatCommand( "gamealert" ) {}

private:
    virtual char const * GetHelp()
    {
        return "Sends a Discord game alert to notify others that you want a game, can only be used once every 5 minutes. You can also add a custom message using /gamealert custommessagehere";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        commandData.whitelist.push_back( client.id.GetID() );

        Time current;
        current.GetTime();

        // check if the client has used the command before
        auto it = lastGameAlerts.find( client.id.GetID() );
        if ( it != lastGameAlerts.end() )
        {
            // check if 5 minutes have passed
            if ( ( current - it->second ).Seconds() < 300 )
            {
                Host::GetHost().Say( "You can only send 1 game alert every 5 minutes.", client );
                return;
            }
        }

        // update
        lastGameAlerts[client.id.GetID()] = current;

        Host::GetHost().Say( "Discord game alert has been successfully sent!", client );
    }
};
static ChatCommandGameAlert chatCommandGameAlert;

class ChatCommandName: public ChatCommand
{
public:
    ChatCommandName():ChatCommand("name"){}
private:
    virtual char const * GetHelp()
    {
        return "Changes your name.";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
    }
};
static ChatCommandName chatCommandName;

// version info
class ChatCommandVersion: public ChatCommand
{
public:
    ChatCommandVersion():ChatCommand("version"){}
private:
    virtual char const * GetHelp()
    {
        return "Tells you what version this server is using. To save you the trouble, it's " TRUE_PACKAGE_STRING ".";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        Host::GetHost().Say( "This server is running " TRUE_PACKAGE_STRING ".", client );
    }
};
static ChatCommandVersion chatCommandVersion;

// chatter statistics
class ChatStats
{
public:
    struct ChatStatItem
    {
        Client * client; // the chatting client
        int monologue;   // the monologue length

        ChatStatItem( Client * client_ ): client( client_ ), monologue(1){}
    };

    void AddChat( Client * client )
    {
        if ( statItems.size() > 0 && statItems.back().client == client )
        {
            // add to monologue
            ++statItems.back().monologue;
        }
        else
        {
            // add new monologue
            statItems.push_back( ChatStatItem( client ) );
        }

        // purge a bit
        while ( statItems.front().monologue + 2 < (int)statItems.size() )
            statItems.pop_front();
    }

    // returns the most obnoxious chatter except the one requesting it, of course
    Client * MostObnoxious( Client * self = 0 )
    {
        int longestMonologue = 0;
        Client * worst = 0;
        for ( std::deque< ChatStatItem >::const_iterator iter = statItems.begin(); iter != statItems.end(); ++iter )
        {
            // later monologues count more
            if ( (*iter).monologue >= longestMonologue && (*iter).client != self )
            {
                longestMonologue = (*iter).monologue;
                worst = (*iter).client;
            }
        }

        return worst;
    }
private:
    std::deque< ChatStatItem > statItems;
};

static ChatStats stats;

class ChatCommandMe: public ChatCommand
{
public:
    ChatCommandMe():ChatCommand("me"){}
private:
    virtual char const * GetHelp()
    {
        return "Chat action. \"/me laughs\" gets displayed as \"* <your name> laughs *\".";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
#ifdef COMPILE_DEDWINIA
        // rewrite the chat to come from the bot
        std::ostringstream o;
        std::string chat;
        std::getline( s, chat );

        if ( FilterString( chat, client ) )
        {
            throw ReadException();
        }

        o << "* " << client.GetName() << " " << chat << " *";
        Host::GetHost().Say( o.str(), commandData.whitelist, commandData.blacklist );
        throw ReadException();
#endif  
    }
};
static ChatCommandMe chatCommandMe;

#ifdef COMPILE_DEDWINIA
class ChatCommandTeam: public ChatCommand
{
public:
    ChatCommandTeam():ChatCommand("team")
    {
        RegisterSecondary( "t" );
    }
private:
    virtual char const * GetHelp()
    {
        return "Sends a message to your teammate only.";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        for( SuperPower::const_iterator i = SuperPower::GetFirstSuperPower(); i; ++i )
        {
            if ( i->Color() != client.id.Color() )
            {
                commandData.blacklist.push_back( i->GetID() );
            }
        }
    }
};
static ChatCommandTeam chatCommandTeam;
#endif

char const * rankedHelpOn = "/ranked and /unranked let you indicate you wish this match to count for rankings. The default on this server is /ranked, and all players need to say /unranked before the game starts to make it not count.";
char const * rankedHelpOff = "/ranked and /unranked let you indicate you wish this match to count for rankings. The default on this server is /unranked, and all players need to say /ranked before the game starts to make it not count.";
char const * rankError = "/ranked and /unranked only have an effect before the game starts.";

static void RankDissenters( Client & client )
{
    if ( client.rankWish == rankedDefault.Get() )
    {
        // client conforms with the server default. All's fine.
        return;
    }

    bool warned = false;
    for ( Client::iterator player = Client::GetFirstClient(); player; ++player )
    {
        if ( player->IsPlaying() )
        {
            if ( player->rankWish != client.rankWish )
            {
                if (!warned )
                {
                    warned = true;
                    Host::GetHost().Say( "The following players need to agree to that:", client );
                }
                
                std::ostringstream s;
                s << player->id;
                Host::GetHost().Say( s.str(), client );
            }
        }
    }
}

class ChatCommandRanked: public ChatCommand
{
public:
    ChatCommandRanked():ChatCommand("ranked")
    {
        RegisterSecondary( "rank" );
    }
private:
    virtual char const * GetHelp()
    {
        return rankedDefault.Get() ? rankedHelpOn : rankedHelpOff;
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        if ( serverInfo.started )
        {
            Host::GetHost().Say( rankError, client );
            return;
        }
        Host::GetHost().Say( "You wish this game to be ranked.", client );
        client.rankWish = true;    
        RankDissenters( client );
    }
};
static ChatCommandRanked chatCommandRanked;

class ChatCommandUnranked: public ChatCommand
{
public:
    ChatCommandUnranked():ChatCommand("unranked")
    {
        RegisterSecondary( "unrank" );
    }
private:
    virtual char const * GetHelp()
    {
        return rankedDefault.Get() ? rankedHelpOn : rankedHelpOff;
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        if ( serverInfo.started )
        {
            Host::GetHost().Say( rankError, client );
            return;
        }
        Host::GetHost().Say( "You do not want this game to count for rankings.", client );
        client.rankWish = false;
        RankDissenters( client );
    }
};
static ChatCommandUnranked chatCommandUnranked;

class ChatCommandLogin: public ChatCommand
{
public:
    ChatCommandLogin():ChatCommand("login"){}
private:
    virtual char const * GetHelp()
    {
        return "Activates administrator mode for you. You need to have the right nickname, know the right password and possibly need to be using the right authentication key.";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        commandData.swallowNextChat = true;
        
        // search admin accounts
        AdminAccount const * admin = FindAdmin( client.id.GetName(), client.keyID );
        if ( !admin )
        {
            admin = FindAdmin( client.lastSetName, client.keyID );
        }

        if ( client.loginAttemptsLeft <= 0 )
        {
            Host::GetHost().Say( "You're out of guesses.", client );
        }
        else if ( admin )
        {
            // is the login accepted?
            bool accept = true;
            
            // is there a password set?
            if ( admin->password.size() > 0 )
            {
                std::string password;
                std::ws(s);
                getline( s, password );
                
                if ( password != admin->password )
                {
                    Host::GetHost().Say( "Access denied, wrong password.", client );
                    --client.loginAttemptsLeft;
                    accept = false;
                }
            }
            
            if ( accept )
            {
                std::ostringstream message;
                message << client << " has been logged in as " << LevelPrinter( admin->level ) << ".";
                Host::GetHost().Say( message.str() );
                client.admin = true;
                client.adminName = NormalizeName( client.id.GetName() );
                client.adminLevel = admin->level;
            }
        }
        else
        {
            Host::GetHost().Say( "Access denied, not registered.", client );
        }
    }
};
static ChatCommandLogin chatCommandLogin;

class ChatCommandIgnore: public ChatCommand
{
public:
    ChatCommandIgnore():ChatCommand("ignore")
    {
        RegisterSecondary();
    }
private:
    virtual char const * GetHelp()
    {
        return "Ignores all future chat from a certain player. Without argument, pick the loudest recent chatter.";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        std::string target;
        std::ws(s);
        getline( s, target );
        if ( target.size() == 0 )
        {
            // ignore the most noisy last chatter
            Client * c = stats.MostObnoxious( &client );
            if ( c )
            {
                c->ignoredBy.Add( client.id.GetID() );
                std::ostringstream message;
                message << "Automatically selecting the most noisy recent chatter: You will no longer be listening to anything " << *c << " says. If you change your mind, use /unignore.";
                Host::GetHost().Say( message.str(), client );
            }
            else
            {
                Host::GetHost().Say( "Usage: /ignore   <part of name of player to ignore>. If you leave away the player name and there was some chat, the most recent most annoying chatter will be ignored.", client );
            }
        }
        else
        {
            std::istringstream s( target );
            Client * c = GetClient( s, CurrentCommand(), "ignore", &client );
            if ( c && !c->ignoredBy.IgnoredBy( client.id.GetID() ) )
            {
                if ( c == &client )
                {
                    // igoring yourself
                    Host::GetHost().Say( "Ignoring yourself is not technically possible. Find a more productive pasttime :)", client );
                    return;
                }
                
                c->ignoredBy.Add( client.id.GetID() );
                std::ostringstream message;
                message << "You will no longer be listening to anything " << *c << " says. If you change your mind, use /unignore.";
                Host::GetHost().Say( message.str(), client );
            }
        }
    }
};
static ChatCommandIgnore chatCommandIgnore;

class ChatCommandUnignore: public ChatCommand
{
public:
    ChatCommandUnignore():ChatCommand("unignore")
    {
        RegisterSecondary();
    }
private:
    virtual char const * GetHelp()
    {
        return "Undoes the effect of /ignore.";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        Client * c = GetClient( s, CurrentCommand(), "unignore", &client );
        if ( c && c->ignoredBy.IgnoredBy( client.id.GetID() ) )
        {
            c->ignoredBy.Remove( client.id.GetID() );
            std::ostringstream message;
            message << "You will listen to " << *c << " again.";
            Host::GetHost().Say( message.str(), client );
        }
    }
};
static ChatCommandUnignore chatCommandUnignore;

#ifdef COMPILE_DEDWINIA
class ChatCommandRegister: public ChatCommand
{
public:
    ChatCommandRegister():ChatCommand("register"){}
private:
    virtual char const * GetHelp()
    {
        return "/register <ticket> <username> confirms your registration at the unofficial ladder (www.multiwinia-ladder.net). <ticket> is a token given to you when you initiate the registration on that web page, and <username> is your username there.";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        // hide command from others
        commandData.whitelist.push_back( client.id.GetID() );
        
        std::string ticket, username;
        s >> ticket;
        std::ws(s);
        std::getline( s, username );
        
        if ( ticket.size() != 8 )
        {
            Host::GetHost().Say( "Invalid ticket. Tickets are 8 characters long.", client );
        }
        else
        {
            Host::GetHost().Say( "Registration will be processed when this game session ends.", client );
            Log::Event() << "USER_REGISTER "
                         << KeyIDWriter( client.keyID )
                         << " " << ticket
                         << " " << username << "\n";
        }
    }
};
static ChatCommandRegister chatCommandRegister;
#endif

#ifdef XDEBUG
class ChatCommandDesync: public ChatCommand
{
public:
    ChatCommandDesync():ChatCommand("desync"){}
private:
    virtual char const * GetHelp()
    {
        return "Pretends you just desynced.";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        for( int i = 20; i > 0; --i )
        {
            client.OutOfSync( true, gameHistory.GetTailSeqID() );
        }
    }
};
static ChatCommandDesync chatCommandDesync;

class ChatCommandResync: public ChatCommand
{
public:
    ChatCommandResync():ChatCommand("resync"){}
private:
    virtual char const * GetHelp()
    {
        return "Pretends you just resynced.";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        for( int i = 20; i > 0; --i )
        {
            client.OutOfSync( false, gameHistory.GetTailSeqID() );
        }
    }
};
static ChatCommandResync chatCommandResync;
#endif

#ifdef COMPILE_DEDCON
static char const * setscoreHelp = "/setscore <player name> <score> changes the recorded score of the specified player to the given score. Get a list of scores and team IDs with /score.";

class ChatCommandSetscore: public ChatCommand
{
public:
    ChatCommandSetscore():ChatCommand("setscore"){}
private:
    virtual char const * GetHelp()
    {
        return setscoreHelp;
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        if ( client.outOfSync )
        {
            Host::GetHost().Say( "You are out of sync and the scores you see are very probably wrong. I'm sorry, but I cannot accept them from you as input.", client );
            return;
        }
        
        std::string name;
        float score;
        s >> name >> score;
        if ( s.fail() || !s.eof() )
        {
            Host::GetHost().Say( "Usage: /setscore <player> <score>, where <player> is any part of the name of the player that should get a new score. Get a list with /score.", client );
            return;
        }
        SuperPower * team = GetSuperPower( name, &client, true, "setscore" );
        
        if ( !team )
        {
            return;
        }
        
        // set the score and display the result
        if ( team->GetScore() != score || !team->ScoreSet() )
        {
            team->SetScore( score );
            UnsignAllScores();
        }
        PrintScores( &client, false );
    }
};
static ChatCommandSetscore chatCommandSetscore;

class ChatCommandSetscoreid: public ChatCommand
{
public:
    ChatCommandSetscoreid():ChatCommand("setscoreid"){}
private:
    virtual char const * GetHelp()
    {
        return setscoreHelp;
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        int teamID;
        float score;
        s >> teamID >> score;
        if ( s.fail() )
        {
            Host::GetHost().Say( "Usage: /setscoreid <TeamID> <score>, where <TeamID> is the ID of the player that should get a new score. Get a list with /score.", client );
            return;
        }
        SuperPower * team = GetSuperPower( teamID, &client );
        
        if ( !team )
        {
            return;
        }
        
        // set the score and display the result
        if ( team->GetScore() != score || !team->ScoreSet() )
        {
            team->SetScore( score );
            UnsignAllScores();
        }
        PrintScores( &client, false );
    }
};
static ChatCommandSetscoreid chatCommandSetscoreid;

class ChatCommandSignscore: public ChatCommand
{
public:
    ChatCommandSignscore():ChatCommand("signscore"){}
private:
    virtual char const * GetHelp()
    {
        return "Shows that you acknowledge the scores as displayed with /score as valid.";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        // check whether scores have been set
        if ( !ScoresSet( &client ) )
        {
            return;
        }
        
        if ( client.scoresLastSeen != scoreRevision )
        {
            if ( client.scoresLastSeen == 0 )
            {
                Host::GetHost().Say( "Signature not accepted. You haven't seen the scores yet. They have been entered as:", client );
            }
            else
            {
                Host::GetHost().Say( "Signature not accepted. The scores have been edited since you last saw them. They are now:", client );
            }
            PrintScores( &client, true );
            Host::GetHost().Say( "If they are correct, say \"/signscore\" again, nothing has happened yet.", client );
            return;
        }
        
        SignScores( client );
    }
};
static ChatCommandSignscore chatCommandSignscore;

static int currentChatRunningNumber = 0;

class ChatCommandSay: public ChatCommandAdmin
{
public:
    ChatCommandSay():ChatCommandAdmin("say"){}
private:
    virtual char const * GetHelp()
    {
        return "Say something to everyone passing all filters, /ignore filters and private spectator chat alike.";
    }

    virtual void DoActivate( Client & admin, std::istream & rest, CommandData & commandData )
    {
        if ( admin.adminLevel > sayLevel )
        {
            Host::GetHost().Say( "Insufficient admin rights to execute /say.", admin );
            return;
        }

        std::string line;
        std::ws( rest );
        std::getline( rest, line );

        // rewrite chat message
        bool playing = admin.IsPlaying();
#ifdef COMPILE_DEDWINIA
        playing = false;
#endif
        AutoTagWriter writer(C_TAG, C_CHAT );
        writer.WriteChar(C_PLAYER_TEAM_ID,playing ? admin.GetTeamID() : admin.GetID() );
#ifdef COMPILE_DEDCON
        writer.WriteChar(C_CHAT_CHANNEL,0x64); // public chat
#endif
        writer.WriteFlexString(C_CHAT_STRING, line );
        writer.WriteInt(C_METASERVER_GAME_STARTED, currentChatRunningNumber );
#ifndef COMPILE_DEDWINIA
        if ( !playing )
        {
            writer.WriteInt(C_PLAYER_SPECTATOR, 1 );
        }
#endif
        gameHistory.AppendClient( writer );

        throw ReadException();
    }
};
static ChatCommandSay chatCommandSay;

#endif // #DEFCON

#ifdef COMPILE_DEDWINIA
#ifdef DEBUG
class ChatCommandLeave: public ChatCommandAdmin
{
public:
    ChatCommandLeave():ChatCommandAdmin("leave"){}
private:
    virtual char const * GetHelp()
    {
        return "Leave the game to some form of spectator mode.";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        if ( client.IsPlaying() )
        {
            client.id.Spectate();
            std::ostringstream o;
            o << client.GetName() << " left to spectator mode.";
            Host::GetHost().Say( o.str() );
            throw ReadException();
        }
    }
};
static ChatCommandLeave chatCommandLeave;

class ChatCommandJoin: public ChatCommandAdmin
{
public:
    ChatCommandJoin():ChatCommandAdmin("join"){}
private:
    virtual char const * GetHelp()
    {
        return "Rejoins the game.";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        if ( !client.IsPlaying() && !gameHistory.Playback() )
        {
            if ( serverInfo.started )
            {
                Host::GetHost().Say( "The game is already running, you can't rejoin.", client );
            }
            else
            {
                client.id.Play();
            }
        }
        
        throw ReadException();
    }
};
static ChatCommandJoin chatCommandJoin;
#endif
#endif

class ChatCommandScore: public ChatCommand
{
public:
    ChatCommandScore():ChatCommand("score"){}
private:
    virtual char const * GetHelp()
    {
        return "Gives a list of active players and their score. Scores need to be manually entered with /setscore and signed with /signscore to be considered valid in the logs.";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        // hide command from others
        commandData.whitelist.push_back( client.id.GetID() );
        
        PrintScores( &client, true );
    }
};
static ChatCommandScore chatCommandScore;

// wish speed for playback
static IntegerSetting clientWishSpeed( Setting::Log, "PlaybackSpeed", 10 );

class ChatCommandSpeed: public ChatCommand
{
public:
    ChatCommandSpeed():ChatCommand("speed"){}
private:
    virtual char const * GetHelp()
    {
        if ( gameHistory.Playback() )
        {
            return "Sets the playback speed. 10 is the speed the game was played at, so 5 would be half the original speed (and cause choppy graphics) and 20 fast worwarding with twice to original speed.";
        }
        else
        {
#ifdef COMPILE_DEDCON
            return "Gives a list of players, their requested speeds and slowdown budgets.";
#endif
#ifdef COMPILE_DEDWINIA
            return "Has only a function during game playback.";
#endif
        }
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        if ( gameHistory.Playback() )
        {
            int speed = 10;
            s >> speed;
            
            clientWishSpeed.Set( speed );
        }
#ifdef COMPILE_DEDCON
        else
        {
            // hide command from others
            commandData.whitelist.push_back( client.id.GetID() );
            
            for ( Client::iterator c = Client::GetFirstClient(); c; ++c )
            {
                if ( c->id.IsPlaying() )
                {
                    std::ostringstream s;
                    s << *c
                      << " : requested = " <<  c->speed
                      << ", budget = " << int(c->slowBudget);
                    Host::GetHost().Say( s.str(), client );
                }
            }
        }
#endif
    }
};
static ChatCommandSpeed chatCommandSpeed;

#ifdef XDEBUG
// reports a griefer
class ChatCommandReport: public ChatCommand
{
public:
    ChatCommandReport():ChatCommand("report")
    {
        RegisterSecondary();
    }
private:
    virtual char const * GetHelp()
    {
        return "/report <name part> <reason> reports a player to the server admin. Abuse of this command is usually punished.";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        std::string reason;
        
        Client * griefer = GetClient( s, CurrentCommand(), GetName().c_str(), &client );
        if ( griefer )
        {
            std::getline( s, reason );

            GetGriefLog( *griefer, "report" )
            << ", reporter = " << client
            << ", reason = " << reason;
        }
    }
};
static ChatCommandReport chatCommandReport;
#endif

class ChatCommandKick: public ChatCommand
{
public:
    ChatCommandKick():ChatCommand("kick")
    {
        RegisterSecondary();
    }
private:
    virtual char const * GetHelp()
    {
        return "Kicks a player from the server, takes the name as argument. Partial names work, too. Non-Admins don't kick directly, but issue kick-votes.";
    }

    virtual void DoActivate( Client & client, std::istream & s, CommandData & commandData )
    {
        Client * c = GetClient( s, CurrentCommand(), "kick", &client );
        if ( c )
        {
            Kick( *c, client );
        }
    }
};
static ChatCommandKick chatCommandKick;

// Admin commands begin here
class ChatCommandSet: public ChatCommandAdmin
{
public:
    ChatCommandSet():ChatCommandAdmin("set"){}
private:
    virtual char const * GetHelp()
    {
        return "Changes a game setting. \"/set <stuf>\" has the same effect <stuff> would have in a config file. Use /help admin to get listst of configuration items to change.";
    }

    virtual void DoActivate( Client & admin, std::istream & rest, CommandData & commandData )
    {
        if ( admin.adminLevel > setLevel )
        {
            Host::GetHost().Say( "Insufficient admin rights to execute /set.", admin );
            return;
        }
        
        std::ostringstream err;
        {
            std::string line;
            getline( rest, line );
            SecretStringSetting::ResetSecret();
            OStreamSetter setter( Log::Err().GetWrapper(), &err );
            setter.SetTimestampOut( false );

            // prepare a reader
            SettingsReader reader;
            reader.SetAdminLevel( admin.adminLevel );
            reader.AddLine( "", line );

            // and execute it
            reader.Apply();

            // transfer any leftover children of the reader to the main reader.
            reader.TransferChildren( SettingsReader::GetSettingsReader() );

            // only send secret commands to the player saying them
            if ( SecretStringSetting::SettingWasSecret() )
                commandData.swallowNextChat = true;

            settings.CheckRestrictions();
            settings.SendSettings();
        }

        HandleErrors( err.str(), admin );
    }
};
static ChatCommandSet chatCommandSet;

class ChatCommandInclude: public ChatCommandAdmin
{
public:
    ChatCommandInclude():ChatCommandAdmin("include"){}
private:
    virtual char const * GetHelp()
    {
        return "Includes a script the owner has prepared.";
    }

    virtual void DoActivate( Client & admin, std::istream & rest, CommandData & commandData )
    {
        if ( admin.adminLevel > includeLevel )
        {
            Host::GetHost().Say( "Insufficient admin rights to execute /include.", admin );
            return;
        }

        std::ostringstream err;
        std::string line;
        getline( rest, line );
        SecretStringSetting::ResetSecret();

        OStreamSetter setter( Log::Err().GetWrapper(), &err );
        setter.SetTimestampOut( false );

        // prepare a reader
        SettingsReader * reader = SettingsReader::GetSettingsReader().Spawn();
        reader->SetAdminLevel( admin.adminLevel );
        reader->Read( line.c_str() );

        // and execute it
        reader->Apply();

        // only send secret commands to the player saying them
        if ( SecretStringSetting::SettingWasSecret() )
        {
            commandData.swallowNextChat = true;
        }

        settings.CheckRestrictions();
        settings.SendSettings();

        HandleErrors( err.str(), admin );
    }
};
static ChatCommandInclude chatCommandInclude;

class ChatCommandPromote: public ChatCommandAdmin
{
public:
    ChatCommandPromote():ChatCommandAdmin("promote")
    {
        RegisterSecondary();
    }
private:
    virtual char const * GetHelp()
    {
        return "Raises the rights to someone else.";
    }

    virtual void DoActivate( Client & admin, std::istream & rest, CommandData & commandData )
    {
        Client * c = GetClient( rest, CurrentCommand(), "promote", &admin );
        if ( c )
        {
            ChangeLevel( *c, admin,-1 );
        }
    }
};
static ChatCommandPromote chatCommandPromote;

class ChatCommandDemote: public ChatCommandAdmin
{
public:
    ChatCommandDemote():ChatCommandAdmin("demote")
    {
        RegisterSecondary();
    }
private:
    virtual char const * GetHelp()
    {
        return "Lowers the rights of someone else.";
    }

    virtual void DoActivate( Client & admin, std::istream & rest, CommandData & commandData )
    {
        Client * c = GetClient( rest, CurrentCommand(), "demote", &admin );
        if ( c )
        {
            ChangeLevel( *c, admin, 1 );
        }
    }
};
static ChatCommandDemote chatCommandDemote;

class ChatCommandOp: public ChatCommandAdmin
{
public:
    ChatCommandOp():ChatCommandAdmin("op")
    {
        RegisterSecondary();
    }
private:
    virtual char const * GetHelp()
    {
        return "Gives admin rights to someone else.";
    }

    virtual void DoActivate( Client & admin, std::istream & rest, CommandData & commandData )
    {
        if ( admin.adminLevel > opLevel )
        {
            Host::GetHost().Say( "Insufficient admin rights to execute /op.", admin );
            return;
        }

        Client * c = GetClient( rest, CurrentCommand(), "op", &admin );
        if ( c )
            Op( *c, admin );
    }
};
static ChatCommandOp chatCommandOp;

class ChatCommandDeop: public ChatCommandAdmin
{
public:
    ChatCommandDeop():ChatCommandAdmin("deop")
    {
        RegisterSecondary();
    }
private:
    virtual char const * GetHelp()
    {
        return "Takes admin rights from someone else.";
    }

    virtual void DoActivate( Client & admin, std::istream & rest, CommandData & commandData )
    {
        Client * c = GetClient( rest, CurrentCommand(), "deop", &admin );
        if ( c )
            DeOp( *c, admin );
    }
};
static ChatCommandDeop chatCommandDeop;

#ifndef COMPILE_DEDWINIA
class ChatCommandRemove: public ChatCommandAdmin
{
public:
    ChatCommandRemove():ChatCommandAdmin("remove")
    {
        RegisterSecondary();
    }
private:
    virtual char const * GetHelp()
    {
        return "Removes a player from the game.";
    }

    virtual void DoActivate( Client & admin, std::istream & rest, CommandData & commandData )
    {
        Client * c = GetClient( rest, CurrentCommand(), "remove", &admin );
        if ( c )
        {
            Remove( *c, admin );
        }
    }
};
static ChatCommandRemove chatCommandRemove;
#endif

class ChatCommandInfo: public ChatCommandAdmin
{
public:
    ChatCommandInfo( char const * name, bool full_ ):ChatCommandAdmin( name ), full( full_ ){}
private:
    virtual char const * GetHelp()
    {
        return full ? "Gives extended information about all clients of this session, current and past." : "Gives extended information about the connected clients.";
    }

    static void SayClient( Client const & client, Client const & admin )
    {
        std::ostringstream s;
        s << client << " : IP " << inet_ntoa( client.saddr.sin_addr )
          << ", keyID " << KeyIDWriter( client.keyID )
          << ", version " << client.version;
        if ( client.id.IsPlaying() )
        {
            s << ", teamID " << client.id.GetTeamID();
        }
        if ( client.admin )
        {
            s << ", " << LevelPrinter( client.adminLevel );
        }
        Host::GetHost().Say( s.str(), admin );
    }

    virtual void DoActivate( Client & admin, std::istream & rest, CommandData & commandData )
    {
        // hide command from others
        commandData.whitelist.push_back( admin.id.GetID() );

        for ( Client::iterator c = Client::GetFirstClient(); c; ++c )
        {
            SayClient( *c, admin );
        }

        if ( full )
        {
            for ( Client::iterator c = deletedClients.begin(); c; ++c )
            {
                SayClient( *c, admin );
            }
        }
    }

    bool full;
};
static ChatCommandInfo chatCommandInfo( "info", false );
static ChatCommandInfo chatCommandFullInfo( "fullinfo", true );

class ChatCommandLogout: public ChatCommandAdmin
{
public:
    ChatCommandLogout():ChatCommandAdmin("logout"){}
private:
    virtual char const * GetHelp()
    {
        return "Drops your admin rights.";
    }

    virtual void DoActivate( Client & client, std::istream & rest, CommandData & commandData )
    {
        client.admin = false;
        client.adminLevel = 1000;
        Host::GetHost().Say( client.id.GetName() + " has logged out." );
    }
};
static ChatCommandLogout chatCommandLogout;

// average number of chat lines per minute before spam throttling sets in
static IntegerSetting chatsPerMinute( Setting::Grief, "ChatsPerMinute", 12 );

class TagParserClientChat: public TagParserClient
{
public:
    TagParserClientChat( Client * client, CommandData & commandData_ ):TagParserClient( client ), dv( -1 ), commandData( commandData_ )
    {
        harmless = true;
    }

protected:
    // called when value-key pairs of the various types are encountered.
    virtual void OnChar( std::string const & key, int value )
    {
        TagParserClient::OnChar( key, value );

        if ( key == C_LOGIN_KEY )
        {
            flags = value;
        }
    }

    virtual void OnString( std::string const & key, std::string const & value )
    {
        if ( key == C_CHAT_STRING )
        {
            chat = value;
        }
    }

    virtual void OnInt( std::string const & key, int value )
    {
        TagParserClient::OnInt( key, value );

        if ( key == C_METASERVER_GAME_STARTED )
        {
            dv = value;
#ifdef COMPILE_DEDCON
            currentChatRunningNumber = dv;
#endif
        }
    }

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        try
        {
            TagParserClient::OnFinish( reader );
        }
        catch ( ReadException & e )
        {
            // repeated chat while not connected will increase dv and
            // get the chatter a spam entry
            if ( dv == 10 )
            {
                Spammer( client, "unknownChat", false );
            }

            // don't print this error
            e.Silence();

            // team switching error, make it seem like it succeeded to the sender
            commandData.whitelist.push_back( client->id.GetID() );
        }

        // get the client that really is chatting
        Client * chatter = poser ? poser : client;

#ifdef COMPILE_DEDCON
        if ( dv != client->lastChatID+1 || !client->IsConnected() )
        {
            // ignore.
            throw ReadException();
        }
#endif

        commandData.blacklist = client->ignoredBy.ignoredBy;

        // check for false chat that crashes clients
        if ( spectator != !client->IsPlaying() )
        {
#ifdef COMPILE_DEDCON
            // can't do anything much about this. I tried rewriting the chat message,
            // but the sending client would not recognize it as its own chat, and
            // sending the chat message back to the sending client would crash it.
            // We have to kick the sending client, or it's going to spam us with
            // illegal chat forewer.
            DontStopUnstoppable();
            chatter->Quit( SuperPower::Left );

            throw ReadException();
#endif
        }

        // spam check
        if ( !chatter->chatSpam.Allow( .9, chatsPerMinute * 60 ) )
        {
            Spammer( client, "chat" );
            throw ReadException();
        }

        // bump id
        chatter->lastChatID++;

        chatter->Connect();

        commandData.reservedSequence = gameHistory.ForClient();
        gameHistory.Bump();

        // log public chat only
#ifdef COMPILE_DEDCON
        bool publicChat = ( flags == 0x64 ) || ( flags == 0x66 && settings.spectatorChatChannel );
#else
        bool publicChat = true;
#endif
        if ( Log::GetLevel() >= 1 && publicChat )
        {
            OStreamProxy out = Log::Out();
            if ( poser )
                out << *poser << " posing as ";

            out << *client << " : " << chat << "\n";
        }

        // allow chat commands
        std::istringstream s( chat );
        char first = s.get();
        if (first == '/' )
        {
            std::string command;
            s >> command;
            std::ws(s);

            if ( command != "name" )
            {
                // ok, admin commands should be logged even if they are private.
                if ( Log::GetLevel() >= 1 && !publicChat )
                {
                    Log::Out() << *chatter << " : " << chat << "\n";
                }

                if ( !ChatCommand::Activate( command, *chatter, s, commandData ) )
                {
                    // hide command from others, it may be a typo revealing a password
                    commandData.whitelist.push_back( chatter->id.GetID() );
                    commandData.swallowNextChat = true;
                    Host::GetHost().Say( "Unknown chat command /" + command + ".", *chatter );
                }
            }
        }

        if ( publicChat )
        {
            if ( FilterString( chat, *chatter ) )
            {
#ifdef COMPILE_DEDCON
                commandData.swallowNextChat = true;
#else
                throw ReadException();
#endif
            }
            commandData.replaceChatWith = chat;
        }

        // keep stats
        stats.AddChat( chatter );

        // write comment
        if ( !gameHistory.Playback() )
        {
            Sequence & last = *gameHistory.GetTail(0);

            std::ostringstream out;
            if ( last.GetComment().size() )
            {
                out << last.GetComment() << "\n";
            }

            if ( poser )
                out << ClientWriter( *poser ) << " posing as ";

            out << ClientWriter( *client ) << " : ";

            if( commandData.whitelist.size() == 0 && !commandData.swallowNextChat )
            {
                out << chat;
            }
            else
            {
                out << "<snip>";
            }

            last.SetComment( out.str() );
        }

#ifdef COMPILE_DEDWINIA
        if( !client->IsPlaying() )
        {
            // rewrite the chat to come from the bot
            std::ostringstream s;
            std::string const & name = client->GetName();
            if ( name.size() )
            {
                s << client->GetName();
            }
            else
            {
                s << "SPEC";
            }
            s << ": ";
            if ( commandData.swallowNextChat )
            {
                s << "<snip>";
            }
            else
            {
                s << chat;
            }

            Host::GetHost().SayHere( commandData.reservedSequence );
            Host::GetHost().Say( s.str(), commandData.whitelist, commandData.blacklist );
            throw ReadException();
        }
#endif
    }
private:
    std::string chat; // the chat string
    int dv;           // the always increasing chat line ID
    int flags;        // the chat flags
    CommandData & commandData; // additional data
};

static IntegerSetting maxCountdownAborts( Setting::Grief, "MaxCountdownAborts", 5 );

class TagParserClientLeave: public TagParserClient
{
public:
    TagParserClientLeave( Client * client ):TagParserClient( client ){}

protected:
    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        TagParserClient::OnFinish( reader );

        // avoid narrow countdown aborts
        DontStopUnstoppable();

        // don't allow leaving to spectator mode when the game has already started
        if ( serverInfo.started )
        {
            throw ReadException( "Leave to spectator mode request denied, game has already started." );
        }

         SuperPower::UnreadyAll( client->id );

       // don't allow too many leaves
        if ( maxCountdownAborts >= 0 )
        {
            if ( maxCountdownAborts <= client->numberOfLeaves )
            {
                // trow him out, he doesn't seem to want to play
                client->forceReady = true;
                client->noPlay = true;

                Host::GetHost().Say( "If you don't want to play, stay out.", client );
            }

            ++client->numberOfLeaves;
        }

        client->id.Spectate();

        // don't let him in again if he was forced to be ready, he'll use it
        // to continue to stall the game
        if ( client->forceReady )
        {
            client->noPlay = true;
        }

        if ( Log::GetLevel() >= 1 )
            Log::Out() << *client << " left to spectator mode.\n";
    }
};

void HandleQuit( Client * client )
{
    // woot! a quitter. Log that.
    if ( client->inGame )
        client->quitTime = gameHistory.GameTime();

    // client->id.Vanish();

    // don't let him in again if he was forced to be ready, he'll use it
    // to continue to stall the game
    if ( client->forceReady && client->id.IsPlaying() )
    {
        client->noPlay = true;
    }

    client->Quit( SuperPower::Left );
}


class TagParserClientQuit: public TagParserClient
{
public:
    TagParserClientQuit( Client * client ):TagParserClient( client ){}

protected:

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        TagParserClient::OnFinish( reader );

        if ( Log::GetLevel() >= 1 )
            Log::Out() << *client << " disconnected.\n";

        HandleQuit( client );
    }
};

class TagParserClientReady: public TagParserClient
{
public:
    TagParserClientReady( Client * client ):TagParserClient( client )
    {
        // only allow one ready tick per seqID
        if ( lastReadyTick == gameHistory.GetTailSeqID() )
            gameHistory.Bump();

        lastReadyTick = gameHistory.GetTailSeqID();
    }

protected:
    // called when value-key pairs of the various types are encountered.
    virtual void OnChar( std::string const & key, int value )
    {
        TagParserClient::OnChar( key, value );

#ifdef COMPILE_DEDCON
        if ( key == C_RANDOM_SEED )
        {
            // store random seed
            client->id.SetRandomSeed( value );
        }
#endif
    }

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        TagParserClient::OnFinish( reader );

        // check whether message came from a spectator
        if ( !client->id.IsPlaying() )
        {
            throw ReadException( "Readyness change received for player that is not in the game." );
        }

        // check whether enough players are online
        int teams = 0;
        bool oneFullPlayer = false;
        for ( Client::const_iterator c = Client::GetFirstClient(); c; ++c )
        {
            oneFullPlayer |= !(c->id.GetDemo());
            if ( c->id.IsPlaying() )
                teams++;
        }

        if ( !oneFullPlayer && !demoLimits.demoMode && !metaServer->Answers() )
        {
            std::ostringstream err;
            err << "No contact to metaserver yet, I can't let you start the game.";
            Host::GetHost().Say( err.str(), *client );
            throw ReadException( "No contact to metaserver." );
        }

#ifndef COMPILE_DEDWINIA
        if ( teams < settings.minTeams )
        {
            std::ostringstream err;
            err << "Not enough players online yet. " << settings.minTeams << " are required.";
            Host::GetHost().Say( err.str(), *client );
            throw ReadException( "Not enough players yet." );
        }
#endif

        Time time;
        time.GetTime();
        int secondsLeft = 60 * settings.allStarTime.Get() - ( time - Time::GetStartTime() ).Seconds();
        if ( secondsLeft > 0 )
        {
            std::ostringstream s;
            s << "All Star Mode is set, you cannot start the game yet. The server is programmed to wait for higher ranked players. You'll have to wait ";
            if ( secondsLeft > 180 )
                s << (secondsLeft/60) << " more minutes.";
            else
                s << secondsLeft << " more seconds.";

            Host::GetHost().Say( s.str(), *client );
            throw ReadException( "All Star Mode set, can't start the game yet." );
        }

        // see if the signal comes too late
        if ( serverInfo.unstoppable )
        {
            // ignore it
            throw ReadException();
        }

        // see if the player is allowed to unready
        if ( client->forceReady && client->id.IsReady() )
        {
            // ignore it
            throw ReadException();
        }

        client->Connect();

#ifdef COMPILE_DEDCON
        // check that all alliances are correct
        if ( !client->id.IsReady() )
        {
            CheckColors( true );
        }
#endif

        // avoid narrow countdown aborts
        DontStopUnstoppable();

#ifdef COMPILE_DEDWINIA
        if ( serverInfo.started )
        {
            // recheck readyness

            int inGame = 0;
            int ready = 0;
            for ( SuperPower::iterator player = SuperPower::GetFirstSuperPower(); player; ++player )
            {
                // ready to go?
                if ( player->IsPlaying() && player->NameSet() && player->GetHuman() )
                {
                    inGame++;
                    if ( player->IsReady() )
                    {
                        ready++;
                    }
                }
            }
            
            // determine whether enough people are ready
            serverInfo.everyoneReady = ready >= inGame;
        }
#endif

        // see if the countdown would get aborted; disallow it
        if ( client->id.IsReady() && serverInfo.everyoneReady && maxCountdownAborts >= 0 )
        {
            if ( maxCountdownAborts <= client->numberOfAborts )
            {
                // trow him out, he doesn't seem to want to play
                client->forceReady = true;

#ifdef COMPILE_DEDCON
                client->id.Spectate();
                Client::ResetManualReadyness();
                SuperPower::UnreadyAll();

                Host::GetHost().Say( "If you don't want to play, stay out. Don't rejoin unless you really want to play.", client );
#endif

                throw ReadException();
            }

            ++client->numberOfAborts;
        }

        client->id.ToggleReady( true );

        if ( Log::GetLevel() >= 1 )
        {
            if ( client->id.IsReady() )
                Log::Out() << *client << " is ready to play.\n";
            else
                Log::Out() << *client << " is no longer ready to play.\n";
        }
    }
};

#ifdef COMPILE_DEDCON
// handles speed change
class TagParserClientSpeed: public TagParserClient
{
public:
    TagParserClientSpeed( Client * client ):TagParserClient( client )
    {
        speed = 1;
        if ( settings.gameSpeed != 0 )
        {
            throw ReadException("Speed change requested, but game speed fixed.");
        }
    }

protected:
    // called when value-key pairs of the various types are encountered.
    virtual void OnChar( std::string const & key, int value )
    {
        TagParserClient::OnChar( key, value );

        if ( key == C_PLAYER_NAME )
        {
            speed = value;
        }
    }

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        TagParserClient::OnFinish( reader );

        int minSpeed = settings.MinSpeed();
        if ( speed < settings.MinSpeed() )
        {
            client->SetSpeed( minSpeed );
            throw ReadException("Slow speed exploit attempted.");
        }

        // check whether a speed down is possible
        if ( client->slowBudget <= 1 )
        {
            int minSpeed = Client::TimeBudget(0);
            if ( speed < minSpeed )
            {
                // Host.GetHost().Say( "No budget left to slow the game down.", *client );
                if ( minSpeed < client->speed )
                {
                    client->SetSpeed( minSpeed );
                }

                throw ReadException();
            }
        }

        if ( Log::GetLevel() >= 2 )
        {
            OStreamProxy out = Log::Out();
            if ( poser )
                out << *poser << " posing as ";
            out << *client << " requests a speed factor of " << speed << ".\n";
        }

        Log::Event() << "TEAM_SPEED " << client->id.GetTeamID()
                     << " " << speed << "\n";

        client->speed = speed;
    }
private:
    int speed;
};

// reads a naval blacklist of no-go-areas for ships
class ActionSettingNavalBlacklist: public ActionSetting
{
public:
    ActionSettingNavalBlacklist(): ActionSetting( Setting::ExtendedGameOption, "NavalBlacklist" ){}

    struct NavalBlacklistItem
    {
        double xMin, xMax;
        double yMin, yMax;
    };

    typedef std::deque< NavalBlacklistItem > Blacklist;
    static Blacklist blacklist;

    //! reads the value of the setting from a stream.
    virtual void DoRead( std::istream & source )
    {
        NavalBlacklistItem item;
        std::ws( source );
        source >> item.xMin >> item.xMax >> item.yMin >> item.yMax;
        if ( !source.fail() )
        {
            if ( item.xMin > item.xMax || item.yMin > item.yMax )
            {
            Log::Err() << "Blacklist item format, expected \"NavalBlacklist xMin xMax yMin yMax\", you have some min and max values swapped.\n";
            }
            else
                blacklist.push_back(item);
        }
        else
        {
            Log::Err() << "Wrong blacklist item format, expected \"NavalBlacklist xMin xMax yMin yMax\", some values are missing.x\n";
        }
    }
};

ActionSettingNavalBlacklist::Blacklist ActionSettingNavalBlacklist::blacklist;

static ActionSettingNavalBlacklist blacklistSetting;

// check fleet movement for illegal activity
class TagParserClientFleetMove: public TagParserClient
{
public:
    TagParserClientFleetMove( Client * client )
            :TagParserClient( client )
    {
        x = y = 0;
    }

protected:
    virtual void OnFloat( std::string const & key, float value )
    {
        OnDouble( key, value );
    }

    virtual void OnDouble( std::string const & key, double const & value )
    {
        if ( key == C_FLEET_MOVE_X )
        {
            x = value;
        }
        else if ( key == C_FLEET_MOVE_Y )
        {
            y = value;
        }
    }

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        TagParserClient::OnFinish( reader );

#ifdef DEBUG
        if ( Log::GetLevel() == 3 )
            Log::Out() << "Navy move to " << x << ", " << y << ".\n";
#endif

        // check whether blacklists apply to this client
        if ( !client->navalBlacklist )
        {
            return;
        }

        // check naval target coordinate blacklist
        for ( ActionSettingNavalBlacklist::Blacklist::const_iterator iter = ActionSettingNavalBlacklist::blacklist.begin();
              iter != ActionSettingNavalBlacklist::blacklist.end();
              ++iter )
        {
            // handle wraparound
            if ( x < (*iter).xMin )
                x += 360;
            if ( x > (*iter).xMax )
                x -= 360;

            if ( (*iter).xMin <= x && (*iter).xMax >= x &&
                 (*iter).yMin <= y && (*iter).yMax >= y )
                throw ReadException( "Naval target coordinates in blacklist" );
        }
    }
private:
    double x,y;
};
#endif

#ifdef COMPILE_DEDCON
// handles scores sent automatically by NeoThermic's modification
class TagParserClientScore: public TagParserClient
{
public:
    TagParserClientScore( Client * client ):TagParserClient( client )
    {
        // sending scores for some other team does not count as a team switch violation
        harmless = true;
        score = 0x7fffffff;
    }

protected:
    // called when value-key pairs of the various types are encountered.
    virtual void OnInt( std::string const & key, int value )
    {
        if ( key == "da" )
        {
            score = value;
        }
    }

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        // don't call base finish on purpose, it will rate this message as a team switching
        // exploit and don't do anything useful for our purposes here anyway (like
        // mess with the client pointer)
        // TagParserClient::OnFinish( reader );

        // ignore scores sent from clients that are out of sync completely
        if( client->outOfSync )
        {
            return;
        }

        // count received score packets
#if VANILLA
        static int receivedScoreCountPlayer[SuperPower::MaxSuperPowers+1] = {0,0,0,0,0,0,0};
#elif EIGHTPLAYER
        static int receivedScoreCountPlayer[SuperPower::MaxSuperPowers+1] = {0,0,0,0,0,0,0,0,0};
#elif TENPLAYER
        static int receivedScoreCountPlayer[SuperPower::MaxSuperPowers+1] = {0,0,0,0,0,0,0,0,0,0,0};
#endif
        int & receivedScoreCount = receivedScoreCountPlayer[client->GetTeamID()+1];
        ++receivedScoreCount;

        int sentCount = 0;
        for( int i = SuperPower::MaxSuperPowers; i >= 1; --i )
        {
            if( receivedScoreCountPlayer[i] > 0 )
            {
                sentCount++;
            }
        }

        // if this message comes in, the game has ended
        if ( 2*sentCount > Client::CountPlayers() && !serverInfo.ended )
        {
            Log::Event() << "GAME_END\n";
            serverInfo.ended = true;
        }

        if ( score != 0x7fffffff )
        {
            // find the team
            SuperPower * team = SuperPower::GetTeam( teamID );

            // and store the score
            if ( team && ( team->GetScore() != score || !team->ScoreSet() ) )
            {
                team->SetScore( score );
                UnsignAllScores();
            }
        }

        // sign the scores if they are complete
        if ( ScoresSet( NULL, true ) && receivedScoreCount >= Client::CountPlayers() )
        {
            static int lastPrintedScoreRevision = -1;
            if ( lastPrintedScoreRevision != scoreRevision )
            {
	        // Host::GetHost().Say( "c2 packets received." );
                /*
                lastPrintedScoreRevision = scoreRevision;
                PrintScores( Host::GetBroadcastClient(), false );
                */
            }

            if ( client )
            {
                SignScores( *client );
            }
        }
    }
private:
    int score;
};

class TagParserClientVoteRequest: public TagParserClient
{
public:
    TagParserClientVoteRequest( Client * client_ ): TagParserClient( client_ ), requestType{ -1 }, requestData{ -1 }
    {
    }

    void OnChar( const std::string & key, int value ) override
    {
        TagParserClient::OnChar( key, value );

        if ( key == C_VOTE_REQUEST_TYPE )
            requestType = value;
        if ( key == C_VOTE_REQUEST_DATA )
            requestData = value;
    }

    void OnFinish( TagReader & reader ) override
    {
        TagParserClient::OnFinish( reader );

        gameHistory.allianceVoteHandler.AddVote( client, teamID, static_cast<AllianceVoteHandler::RequestType>( requestType ), requestData );
    }

private:
    int requestType;
    int requestData;
};

class TagParserClientVoteCast: public TagParserClient
{
public:
    TagParserClientVoteCast( Client * client_ ): TagParserClient( client_ ), castID{ -1 }, castVerdict{ -1 }
    {
    }

    void OnChar( const std::string & key, int value ) override
    {
        TagParserClient::OnChar( key, value );
        if ( key == C_VOTE_CAST_ID )
            castID = value;
        if ( key == C_VOTE_CAST_VERDICT )
            castVerdict = value;
    }

    void OnFinish( TagReader & reader ) override
    {
        TagParserClient::OnFinish( reader );

        gameHistory.allianceVoteHandler.CastVote( client, teamID, castID, static_cast<AllianceVoteHandler::Verdict>( castVerdict ) );
    }

private:
    int castID;
    int castVerdict;
};

#endif

#ifdef COMPILE_DEDWINIA
// handles scores sent automatically by Multiwinia
class TagParserClientScore2: public TagParserClient
{
public:
    TagParserClientScore2( Client * client ):TagParserClient( client )
    {
        // sending scores for some other team does not count as a team switch violation
        harmless = true;
        winner = -1;
    }

protected:
    // called when value-key pairs of the various types are encountered.
    virtual void OnIntArray( std::string const & key, std::vector<int> const & value )
    {
        if ( key == C_SCORES )
        {
            scores = value;
        }
    }

    // called when value-key pairs of the various types are encountered.
    virtual void OnInt( std::string const & key, int value )
    {
        if ( key == C_SEQID_LAST )
        {
            if ( gameHistory.GetSeqIDEnd() == 0 || gameHistory.GetSeqIDEnd() > value )
            {
                gameHistory.SetSeqIDEnd( value );
            }
        }
    }

    // called when value-key pairs of the various types are encountered.
    virtual void OnChar( std::string const & key, int value )
    {
        if ( key == C_PLAYER_TEAM_ID )
        {
            winner = value;
        }
    }

    void AddVirtualCPU( CPU & inians, char const * name, int idoffset )
    {
        if ( (int)scores.size() >= 5 + idoffset && ( scores[4 + idoffset] > 0 || winner == 4+idoffset ) )
        if ( !inians.IsPlaying() )
        {
            inians.SetName( name );
            inians.Exist();
            inians.Log();
            inians.Play( 4 + idoffset );
            inians.SetAndSendColor( MAXCOLOR + idoffset );
        }
    }

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        // don't call base finish on purpose, it will rate this message as a team switching
        // exploit and don't do anything useful for our purposes here anyway (like
        // mess with the client pointer)
        // TagParserClient::OnFinish( reader );

        // ignore scores sent from clients that are out of sync completely
        if( client->outOfSync )
        {
            return;
        }

        // no use evaluating this on playback
        if ( gameHistory.Playback() )
        {
            return;
        }

        // add team evil without telling the clients
        {
            StateChangeNetMuter muter;

            // scores[4]=scores[5]=1;

            static CPU evilinians;
            AddVirtualCPU( evilinians, "Evilinians", 0 );

            static CPU futurewinians;
            AddVirtualCPU( futurewinians, "Futurewinians", 1 );
        }

        // if this message comes in, the game has ended
        if ( !serverInfo.ended )
        {
            Log::Event() << "GAME_END\n";
        }
        serverInfo.ended = true;

        // sanity check scores, the winner should have a real lead
        int teamScores[ MAXCOLOR+2 ];
        for( int i = MAXCOLOR+1; i >= 0; --i )
        {
            teamScores[i] = 0;
        }
        int winningTeam = -1;

        for( SuperPower::iterator i = SuperPower::GetFirstSuperPower(); i; ++i )
        {
            SuperPower & p = *i;
            if ( p.GetTeamID() >= 0 && p.GetTeamID() < int(scores.size()) )
            {
                int score = scores[ p.GetTeamID() ];

                // sum up team score
                int team = p.Color();
                int & teamScore = teamScores[ team ];
                teamScore += score;

                if ( p.GetTeamID() == winner )
                {
                    winningTeam = team;
                }
            }
        }

        // make sure the winners have more score than anyone else
        if ( winningTeam >= 0 )
        {
            for( int i = MAXCOLOR+1; i >= 0; --i )
            {
                if ( i != winningTeam )
                {
                    int missing = teamScores[i] - teamScores[winningTeam];
                    if ( missing >= 0 )
                    {
                        teamScores[winningTeam] += missing + 1;
                        scores[winner] += missing + 1;
                    }
                }
            }
        }

        // store scores in player structure
        for( SuperPower::iterator i = SuperPower::GetFirstSuperPower(); i; ++i )
        {
            SuperPower & p = *i;
            if ( p.GetTeamID() >= 0 && p.GetTeamID() < int(scores.size()) )
            {
                int score = scores[ p.GetTeamID() ];

                if ( !p.ScoreSet() || score != p.GetScore() )
                {
                    UnsignAllScores();
                    p.SetScore( score );
                }
            }
        }


        // sign the scores if they are complete
        if ( ScoresSet( NULL, true ) )
        {
            static int lastPrintedScoreRevision = -1;
            if ( lastPrintedScoreRevision != scoreRevision )
            {
                // Host::GetHost().Say( "Score packets received." );
            }

            if ( client )
            {
                SignScores( *client );
            }
        }
    }
private:
    std::vector<int> scores;
    int winner;
};

static BoolSetting logKillStats( Setting::ExtendedGameOption, "LogKillStats", 0 );

// handles game event messages
class TagParserClientEvent: public TagParserClient
{
public:
    TagParserClientEvent( Client * client )
    :TagParserClient( client )
     , teamID(-1)
     , eventID(-1)
     , eventData1(-1)
     , eventData2(-1)
    {
    }

protected:
    // called when value-key pairs of the various types are encountered.
    virtual void OnInt( std::string const & key, int value )
    {
        if ( key == C_EVENT_ID )
        {
            eventID = value;
        }
        else if ( key == C_EVENT_DATA_1 )
        {
            eventData1 = value;
        }
        else if ( key == C_EVENT_DATA_2 )
        {
            eventData2 = value;
        }
        else if ( key == C_PLAYER_TEAM_ID )
        {
            teamID = value;
        }
    }

    // called when value-key pairs of the various types are encountered.
    virtual void OnChar( std::string const & key, int value )
    {
        // lazy. I don't know in which format the values come.
        OnInt( key, value );
    }
    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        // don't call base finish on purpose, it will rate this message as a team switching
        // exploit and don't do anything useful for our purposes here anyway (like
        // mess with the client pointer)
        // TagParserClient::OnFinish( reader );

        if ( eventID < 0 || eventData1 < 0 )
        {
            // invalid
            return;
        }

        // kill stats
        if ( eventID == 0 )
        {
            // no kill stats wanted? abort.
            if ( !logKillStats.Get() )
            {
                return;
            }

            // no kills? Don't log.
            if ( !eventData1 )
            {
                return;
            }
        }

        Log::Event() << "EVENT " << gameHistory.GameTime()
                     << ' ' << teamID
                     << ' ' << eventID
                     << ' ' << eventData1
                     << ' ' << eventData2
                     << '\n';
    }
private:
    // game event data
    int teamID, eventID, eventData1, eventData2;
};
#endif

#ifdef COMPILE_DEDCON
static BoolSetting forbidRadar( Setting::ExtendedGameOption, "ForbidRadar", 0 );
static BoolSetting forbidSilo( Setting::ExtendedGameOption, "ForbidSilo", 0 );
static BoolSetting forbidAirbase( Setting::ExtendedGameOption, "ForbidAirbase", 0 );
static BoolSetting forbidSubmarine( Setting::ExtendedGameOption, "ForbidSubmarine", 0 );
static BoolSetting forbidBattleship( Setting::ExtendedGameOption, "ForbidBattleship", 0 );
static BoolSetting forbidCarrier( Setting::ExtendedGameOption, "ForbidCarrier", 0 );

static BoolSetting forbidCeaseFire( Setting::ExtendedGameOption, "ForbidCeaseFire", 0 );
#endif

#ifdef COMPILE_DEDWINIA
static BoolSetting forbidDirectControl( Setting::ExtendedGameOption, "ForbidDirectControl", 0 );
#endif

class TagParserClientCommand: public TagParserClient
{
public:
    TagParserClientCommand( Client * client, std::string const & clas, CommandData & commandData_ )
            :TagParserClient( client ),
             writer(C_TAG, clas.c_str() ),
             clas( clas ),
             commandData( commandData_ ),
             objectID( -1 )
    {
        harmless = ( clas == C_CHAT  || clas == C_PLAYER_RENAME );
    }

protected:
    // called when value-key pairs of the various types are encountered.
    virtual void OnChar( std::string const & key, int value )
    {
        TagParserClient::OnChar( key, value );

#ifdef COMPILE_DEDCON
        if ( key == C_PLAYER_TEAM_ID )
        {
            if ( value == 255 && harmless )
            {
                // [unknown] bug, client sends this as his chat ID
                // declare him a spectator
                // writer.WriteInt( C_VALUE_DUNNO, 1 );
                spectator = true;
                // and bend the proclaimed team to the client ID
                teamID = client->id.GetID();
                // and keep the message private
                commandData.swallowNextChat = true;
                commandData.whitelist.push_back( teamID );
            }
        }
        else if ( key == C_CHAT_CHANNEL && harmless )
        {
            if ( commandData.swallowNextChat )
            {
                // this should be all secret: a PM to himself
                if ( client->id.IsPlaying() )
                    value = 0x10 * client->id.GetTeamID();
                else
                {
                    value = 0x77; // worth a try
                }
            }

            // check that spectators only chat into the spectator channel
            if ( !client->id.IsPlaying() )
            {
                value = 0x66;
            }
        }
        else if ( key == C_PLACEMENT_TYPE && clas == C_PLACEMENT )
        {
            // unit placement, check legality
            bool veto = false;
            char const * unitname = "";
            switch (value)
            {
            case 2:
                veto = forbidSilo;
                unitname = "silos";
                break;
            case 3:
                veto = forbidRadar;
                unitname = "radars";
                break;
            case 8:
                veto = forbidAirbase;
                unitname = "aribases";
                break;
            case 7:
                veto = forbidBattleship;
                unitname = "battleships";
                break;
            case 0xb:
                veto = forbidCarrier;
                unitname = "carriers";
                break;
            case 6:
                veto = forbidSubmarine;
                unitname = "submarines";
                break;
            }
            if ( veto )
            {
                std::ostringstream s;
                s << "Placing " << unitname << " is forbidden.";
                Host::GetHost().Say( s.str(), *client );
                throw ReadException();
            }
        }
#endif

        // tansfer it to the writer
        writer.WriteChar( key, value );
    }

    virtual void OnInt( std::string const & key, int value )
    {
        TagParserClient::OnInt( key, value );

        if ( key == C_SEQID_LAST )
        {
            // don't retransmit last known seqIDs
            return;
        }
#ifdef COMPILE_DEDCON
        if( key == C_OBJECT_ID )
        {
            objectID = value;
        }
#endif

        // tansfer it to the writer
        writer.WriteInt( key, value );
    }

    virtual void OnLong( std::string const & key, LongLong const & value )
    {
        TagParserClient::OnLong( key, value );

        // tansfer it to the writer
        writer.WriteLong( key, value );
    }

    virtual void OnFloat( std::string const & key, float value )
    {
        // tansfer it to the writer
        writer.WriteFloat( key, value );
    }

    virtual void OnString( std::string const & key, std::string const & value )
    {
        if ( key == C_CHAT_STRING )
        {
            if( harmless && commandData.swallowNextChat )
            {
                // swallow the text in case someone is sniffing
                writer.WriteString( key, "<snip>" );
                return;
            }
            else if ( commandData.replaceChatWith.size() > 0 )
            {
                // replace chat with value stored in command data
                writer.WriteString( key, commandData.replaceChatWith );
            }
            else
            {
                writer.WriteString( key, value );
            }
        }
        else if ( key == C_TAG_TYPE )
        {
            // class already handled
            return;
        }
        else
        {
            // tansfer it to the writer
            writer.WriteString( key, value );
        }
    }

    virtual void OnUnicodeString( std::string const & key, std::wstring const & value )
    {
        if ( key == C_CHAT_STRING )
        {
            if( harmless && commandData.swallowNextChat )
            {
                // swallow the text in case someone is sniffing
                writer.WriteUnicodeString( key, L"<snip>" );
                return;
            }
            else if ( commandData.replaceChatWith.size() > 0 )
            {
                // replace chat with value stored in command data
                writer.WriteFlexString( key.c_str(), commandData.replaceChatWith );
            }
            else
            {
                writer.WriteUnicodeString( key, value );
            }
        }
        else if ( key == C_TAG_TYPE )
        {
            // class already handled
            return;
        }

        // tansfer it to the writer
        writer.WriteUnicodeString( key, value );
    }

    virtual void OnBool( std::string const & key, bool value )
    {
        // tansfer it to the writer
        writer.WriteBool( key, value );
    }

    virtual void OnDouble( std::string const & key, double const & value )
    {
        // tansfer it to the writer
        writer.WriteDouble( key, value );
    }

    virtual void OnRaw( std::string const & key, unsigned char const * data, int len )
    {
        // just copy
        writer.WriteRaw( key, data, len );
    }

    // checkpoints
    virtual void OnFinish( TagReader & reader )
    {
        try
        {
            TagParserClient::OnFinish( reader );
        }
        catch ( ReadException & e )
        {
            // take real team switching errors seriously
            if ( !harmless )
                throw;

            // ignore the rest
        }

        // send secret chat commands only to the client that issued them
        if ( commandData.swallowNextChat )
            commandData.whitelist.push_back( client->id.GetID() );

        if ( teamID < 0 && !settings.teamSwitching )
        {
            if ( !harmless && client->outOfSync )
            {
                // out of sync clients can easily accidentally give commands for
                // units of other teams, and stupid cheaters don't know how to
                // keep their cheat client in sync
                throw TeamSwitchException("Out of sync client tried to issue a command.");
            }

            // no team ID set? write it into the output after checking
            if ( client->id.IsPlaying() )
            {
                writer.WriteChar( C_PLAYER_TEAM_ID, client->id.GetTeamID() );
                teamID = client->id.GetTeamID();
            }
            else if ( !harmless )
            {
                // another case of team switching exploit
                throw TeamSwitchException("Spectator tries to influence the game.");
            }
        }

        if( clas != "c1" )
        {
            Client::CheckOwnership( teamID, objectID );
        }

        // append the transscribed tag to the game log
        if ( !commandData.reservedSequence )
            commandData.reservedSequence = gameHistory.ForClient();

        commandData.reservedSequence->InsertClient( writer, commandData.whitelist, commandData.blacklist, commandData.insertInBack );
    }
private:
    // writer to store the stuff in for later
    AutoTagWriter writer;
    std::string const & clas; // the command class
    bool harmless; // a harmless command (rename, chat)
    CommandData & commandData; // additional data about the command
    int objectID; // object receiving the command
};

static IntegerSetting acceptC2( Setting::Admin, "AcceptC2", 1 );

// read a sequence from a client
void Client::ReadFrom( TagReader & reader )
{
    try
    {
        // TagReaderCopy copy( reader );

        // start reading
        reader.Start();

        // find the message class
        std::string type = "";
        {
            // it usually is right at the beginning,
            // but the format is flexible, so we better
            // look for it all over the place
            TagReaderCopy readerCopy( reader );
            while ( readerCopy.AtomsLeft() > 0 )
            {
                if ( readerCopy.ReadKey() == C_TAG_TYPE )
                {
                    type = readerCopy.ReadString();
                    break;
                }
                else
                {
                    readerCopy.ReadAny();
                }
            }

            // either way, we're not reading to the end
            readerCopy.Obsolete();

            if ( type.size() == 0 )
            {
                // pretend to come from the server, which will make the message ignored.
                type = C_ACK;
            }
        }

        // and handle the varoious special cases.
        if ( type == C_LOGIN )
        {
            TagParserClientLogin(this).Parse( reader );
            return;
        }

        // the first packet received from everyone must be the login packet. With it, the client's ID is set.
        // if it has no ID, it is not logged in, and all other input should be ignored.
        if  ( id.GetID() < 0 )
        {
            reader.Obsolete();

            // the client is probably in a strange state, we should tell it to quit
            Quit( ( type.size() == 0 || type[0] == 's' ) ? SuperPower::NoMessage : SuperPower::Left );
            // ClientList * listItem = dynamic_cast< ClientList * >( this );
            // assert( listItem  );
            // listItem->InsertSafely( deletedClients );
            return;
        }

        if ( type == C_ACK )
        {
            TagParserClientAck(this).Parse( reader );
            return;
        }

        // true activity of this client
        lastActivity.GetTime();

        // dump interesting packets
#ifdef DEBUG
        if ( Log::GetLevel() == 3 
#ifdef COMPILE_DEDWINIA
             && type != C_CLIENT_ALIVE 
#endif
            )
        {
            reader.Dump();
        }
        if ( Log::OrderLog().GetWrapper().IsSet() )
        {
            // bend output to network log
            OStreamSetter setter( Log::Out().GetWrapper(), Log::OrderLog().GetWrapper().GetStream() );
            setter.SetSecondary( 0 );
            setter.SetTimestampOut( true );
            reader.Dump();
        }
#endif

        if ( type == C_PLAYER_REQUEST_TEAM )
        {
            TagParserClientEnter(this).Parse( reader );
        }
        else if ( type == C_CONFIG
#ifdef COMPILE_DEDWINIA
                  || type == C_CONFIG_INT || type == C_CONFIG_STRING
#endif
            )
        {
            // settings change, no way we can accept that.
            throw ReadException( "Client tried to initiate a setting change" );
        }
#ifdef COMPILE_DEDCON
       else if ( type == C_SPECTATE_REQUEST )
        {
            TagParserClientLeave(this).Parse( reader );
        }
        else if ( type == "c2" )
        {
            // yay, scores. Messages have this form:
            // 22:33:40: <- 82.34.240.141:5011 <d c="c2" db=0x0 da=-28 l=8139 >
            // 22:33:40: <- 82.34.240.141:5011 <d c="c2" db=0x1 da=161 l=8139 >
            // db is the team ID, da the score.
            if ( acceptC2 && !gameHistory.Playback() )
            {
                TagParserClientScore( this ).Parse( reader );
            }
            else
            {
                reader.Obsolete();
            }
        }
        else if ( type == "cr" )
        {
            // according to NeoThermic, those are used to control AI aggressiveness
            // and should not be accepted from clients.
            throw ReadException( "AI control message ignored." );
        }
#endif
#ifdef COMPILE_DEDWINIA
        else if ( type == C_EVENT )
        {
            // game events
            TagParserClientEvent( this ).Parse( reader );
        }
        else if ( type == C_SCORES )
        {
            // yay, scores. Messages have this form:
            // <TAG TAG_TYPE=SCORES SCORES = ( 4, 0, 3, 0, 0, 0) PLAYER_TEAM_ID=0x0 SEQID_LAST=762 >
            // db is the team ID, da the score.
            TagParserClientScore2( this ).Parse( reader );
        }
        else if ( type == C_FTP_SEND )
        {
            FTP::Process( *this, reader, true );
        }
#endif
        else if ( type == C_LOGOUT )
        {
            TagParserClientQuit(this).Parse( reader );
        }
        else if ( type == "cres" )
        {
            TagParserClientResync(this).Parse( reader );
        }
        else if ( type == C_PLAYER_RENAME )
        {
            // name change
            TagParserClientName(this ).Parse( reader );
        }
        else
        {
            CommandData commandData; // additional data

            TagReaderCopy readerCopy( reader );
            try
            {
                bool trueActivity = true;

                if ( type == C_CHAT )
                {
                    TagParserClientChat(this, commandData ).Parse( readerCopy );
                    trueActivity = false;
                }
                else if ( type == C_PLAYER_READY )
                {
                    TagParserClientReady(this).Parse( readerCopy );
                }
                else if ( type == C_TEAM_COLOR_CHANGE )
                {
                    // color change
                    TagParserClientColor(this ).Parse( readerCopy );
                }
#ifdef COMPILE_DEDCON
                else if ( type == "cp" )
                {
                    TagParserClientSpeed(this).Parse( readerCopy );
                }
                else if ( type == C_FLEET_MOVE )
                {
                    TagParserClientFleetMove(this).Parse( readerCopy );
                }
                else if ( type == "ce" || type == "cf" )
                {
                    // pre-game alliance changes or territory selections. Unready everyone if it is not too late.
                    if ( type == C_TEAM_TERRITORY_CHANGE )
                    {
                        static bool legal = false;

                        // check whether the change is legal; since clients send unselection messages
                        // when random territories were first disabled, then a territory selected, then
                        // random territories are enabled, we have to accept all territory messags after the first.
                        if ( !legal && settings.randomTerritories != 0 )
                            throw ReadException( "Territory selection message received, but random territories is active" );

                        // territory changes with a game running are not permitted.
                        if( serverInfo.started )
                        {
                            throw ReadException( "Territory selection message received after game start." );
                        }

                        legal = true;

                        // don't want to bother with multiple territories per team
                        if ( settings.territoriesPerTeam.Get() > 1 )
                        {
                            randomizationPossible = false;
                        }

                        SuperPower::UnreadyAll();

                        TagParserClientTerritory(this).Parse( readerCopy );
                    }
                }
                else if ( type == C_VOTE_REQUEST || type == C_VOTE_CAST )
                {
                    // ingame aliance changes. Make sure no two of those get the same seqID.
                    // (to maybe work around the grey alliance bug)
                    static int lastSeqID = -1;
                    if ( gameHistory.GetTailSeqID() == lastSeqID )
                    {
                        gameHistory.Bump();
                    }

                    lastSeqID = gameHistory.GetTailSeqID();

                    if ( type == C_VOTE_REQUEST )
                        TagParserClientVoteRequest( this ).Parse( readerCopy );
                    else if ( type == C_VOTE_CAST )
                        TagParserClientVoteCast( this ).Parse( readerCopy );

                    // those apparently can happen after the game is over. Don't count them as activity.
                    trueActivity = false;
                }
                else if ( type == "c1" )
                {
                    // whiteboard painting
                    trueActivity = false;
                }
                else if ( type == "cv" )
                {
                    // cease fire messages
                    trueActivity = false;

                    if ( forbidCeaseFire.Get() )
                    {
                        Host::GetHost().Say( "Cease fire has been disabled by the server administrator.", this );
                        throw ReadException();
                    }
                }
                else if ( type == "cw" )
                {
                    // radar sharing
                    trueActivity = false;
                }
#endif
#ifdef COMPILE_DEDWINIA
                else if ( type == C_PAUSE )
                {
                    // pausing the game is not allowed
                    throw ReadException();
                }
                else if ( type == C_ENTITY_TOGGLE )
                {
                    // don't complain about team switching here
                    silentTeamSwitchingBlock = true;
                }
                else if ( type == C_ACHIEVEMENTS )
                {
                    TagParserClientAchievement(this).Parse( readerCopy );
                }
                else if ( type == C_UNIT_SELECT )
                {
                    commandData.insertInBack = true;

                    // check whether it was a selection or deselection event
                    TagParserClientSelect select(this);
                    select.Parse( readerCopy );
                    directControl = select.selected;
                    if ( !directControl )
                    {
                        // std::cout << "deselected!\n";
                    }
                }
                else if ( type == C_TASK_SELECT )
                {
                    commandData.insertInBack = true;

                    directControl = true;
                }
                else if ( type == C_CIRCLE_SELECT )
                {
                    if ( forbidDirectControl.Get() )
                    {
                        throw ReadException();
                    }
                }
                else if ( type == C_CIRCLE_SELECT_RELEASE )
                {
                    directControl = false;
                    
                    if ( forbidDirectControl.Get() )
                    {
                        throw ReadException();
                    }
                }
                else if ( type == C_CLIENT_ALIVE )
                {
                    // raw contoler input is irrelevant unless something is selected
                    if ( !directControl )
                    {
                        throw ReadException();
                    }
                }
#endif


                // extra conditions to deny an activity the right to revoke scores:
                if ( outOfSync )
                {
                    // client is out of sync; it cannot be trusted.
                    trueActivity = false;
                }
                {
                    int maxBehind = ( gameHistory.GetTailSeqID() >> 3 ) + 10;
                    if ( maxBehind > 100 )
                        maxBehind = 100;

                    if ( (int)notYetAcknowledged.size() > maxBehind )
                    {
                        // client is not up to date and may not have noticed the game is over already
                        trueActivity = false;
                    }
                }

                // game activity
                if ( trueActivity )
                {
                    Timestamp();
                }

                // does not need to be read fully
                readerCopy.Obsolete();
            }
            catch( ... )
            {
                readerCopy.Obsolete();
                reader.Obsolete();
                throw;
            }

            // else it is a client sent command. compress and store it
            TagParserClientCommand( this, type, commandData ).Parse( reader );

#ifdef COMPILE_DEDCON
            // give fleet placement more time to gather the commands
            if ( type == "cj" )
            {
                // well, and the game probably has started when people place stuff.
                if ( !serverInfo.started )
                {
                    Log::Err() << "WARNING: mispredicted game start time.\n";

                    serverInfo.unstoppable = true;
                    StartGame();
                }

                // mark all players that are online right now as true players.
                // ( we do this here instead of in StartGame() because StartGame()
                // still is called from unreliable sources )
                int minSpeed = settings.MinSpeed();
                for ( Client::iterator c = Client::GetFirstClient(); c; ++c )
                {
                    if ( c->id.IsPlaying() )
                    {
                        c->inGame = true;

                        if ( c->speed < minSpeed )
                            c->SetSpeed( minSpeed );
                    }
                }

                // does not seem neccesary after all, the clients can compose the fleet
                // even when it is split over many messages.
                // gameClock.InhibitTick(3);
            }
#endif

            // bump the sequence ID in lobby mode, clients think their chat got lost easily otherwise
            if ( gameClock.ticksPerSecond <= 2 )
                gameHistory.RequestBump();
        }
    }
    catch( ReadException & e )
    {
        // just ignore packets with read errors.
        reader.Obsolete();
    }
}

// total number of bytes available for sending
static int totalBandwidthControl = 0;

IntegerSetting packetOverhead( Setting::Network, "PacketOverhead", 40 );
static IntegerSetting maxResendPacketSize( Setting::Network, "MaxResendPacketSize", 200, 2048, 1 );
static IntegerSetting maxAhead( Setting::Network, "SeqIDMaxAhead", 300, 0x7fffffff, 10 );
static IntegerSetting maxQueue( Setting::Network, "SeqIDMaxQueue", 300, 0x7fffffff, 0 );

// catch snapshots of past sync times so
// that server CPU blockades don't drop clients
static const int LAST_TIMES = 4;
static Time lastTimes[LAST_TIMES];
static int currentLastTime = 0;

static Time const & LastTime()
{
    static bool init = true;
    if( init )
    {
        init = false;
        
        for( int i = LAST_TIMES-1; i >=0; --i )
        {
            lastTimes[i].GetTime();
        }
    }

    return lastTimes[currentLastTime];
}

static void UpdateLastTime()
{
    LastTime();
    lastTimes[currentLastTime].GetTime();
    currentLastTime++;
    currentLastTime %= LAST_TIMES;
}

// donates bandwidth to client
static bool DistributeBandwidth( Client * client, int bandwidth, int rateReserve )
{
    UpdateLastTime();

    // clamp
    if ( totalBandwidthControl < bandwidth )
    {
        bandwidth = totalBandwidthControl;
    }
    if ( bandwidth < 0 )
    {
        bandwidth = 0;
    }

    // distribute
    client->bandwidthControl += bandwidth;
    client->bandwidthControlThisFrame -= bandwidth;
    totalBandwidthControl -= bandwidth;

    bool needMore = true;
    if ( client->bandwidthControl > 0 )
    {
        // send the data
        // the client may get dropped here.
        client->SendTo();
    }
    if ( client->bandwidthControl > rateReserve )
    {
        // if too much was given, put it back into the pot
        int sentTooMuch = client->bandwidthControl - rateReserve;

        totalBandwidthControl             += sentTooMuch;
        client->bandwidthControlThisFrame += sentTooMuch;
        client->bandwidthControl          -= sentTooMuch;

        needMore = false;
    }
    if ( client->bandwidthControlThisFrame < 0 )
    {
        int sentTooMuch = -client->bandwidthControlThisFrame;

        totalBandwidthControl             += sentTooMuch;
        client->bandwidthControlThisFrame += sentTooMuch;
        client->bandwidthControl          -= sentTooMuch;

        needMore = false;
    }

    if (!needMore)
    {
        client->finishedSending = true;
    }

    return needMore;
}

// send outstanding data to client
void Client::SendToAll()
{
    Time current;
    current.GetTime();
    static Time lastSent = current;
    int msSinceLastSend = ( current - lastSent ).Milliseconds();

    if ( msSinceLastSend < 0 )
    {
        msSinceLastSend = 0;
        lastSent = current;
    }

    if ( msSinceLastSend > 1000 )
    {
        msSinceLastSend = 1000;
        lastSent = current;
    }

    {
        gameHistory.BumpIfRequested();

        int currentSeqID = gameHistory.GetEffectiveTailSeqID();
        static int lastSeqID = currentSeqID - 1;

        if ( currentSeqID <= lastSeqID )
        {
            // no progress in seqIDs. Sending is largely futile, except if
            // we're still in the lobby. Then send ten times per second.
            if ( gameClock.ticksPerSecond >= 10 || msSinceLastSend < 100 )
            {
                // else, do nothing.
                return;
            }
        }

        lastSeqID = currentSeqID;
    }

    lastSent = current;

    // maximal rate for each client left

    int maxRatePerPlayer, maxRatePerSpectator;

    {
        // fetch bandwidths
        int playerBandwidth = settings.playerBandwidth;
        int spectatorBandwidth = settings.spectatorBandwidth;
        int minBandwidth = settings.minBandwidth;
        spectatorBandwidth = spectatorBandwidth ? spectatorBandwidth : playerBandwidth;

        // under high CPU load, msSinceLastSend will get much larger than 100; in
        // that case, we should throttle the bandwidth usage.
        const int max = 150;
        if ( gameClock.ticksPerSecond >= 10 && msSinceLastSend > max )
        {
            // throttle factor
            float factor = float(max)/msSinceLastSend;
            factor *= factor;

            // and throttle. Make sure to stay above minBandwidth.
            playerBandwidth = minBandwidth + int( ( playerBandwidth - minBandwidth ) * factor );
            spectatorBandwidth = minBandwidth + int( ( spectatorBandwidth - minBandwidth ) * factor );
        }

        maxRatePerPlayer = (playerBandwidth * msSinceLastSend)/1000;
        maxRatePerSpectator = (spectatorBandwidth * msSinceLastSend)/1000;
    }

    // number of connected clients
    int connectedClients = 0;

    // the number of total clients
    int clients = 0;

    // rate reserve for each client
    static int rateReservePlayer = 4 * maxRatePerPlayer;
    rateReservePlayer = (7*rateReservePlayer + 2 * maxRatePerPlayer) >> 3;

    // update bandwidth control
    {
        int totalRate = ( settings.totalBandwidth * msSinceLastSend )/1000;
        static int rateReserve = 10 * totalRate;
        rateReserve = (7*rateReserve + 5 * maxRatePerPlayer) >> 3;

        // unused bandwidth from last frame is mostly discarded.
        if ( totalBandwidthControl > rateReserve )
            totalBandwidthControl = rateReserve ;

        totalBandwidthControl += totalRate;

        // the longest process queue
        int maxNotYetProcessed = 0;

        // check if a single client is up to date
        bool upToDate = false;

        // allocate maximum per-client bandwidth.
        for ( Client::iterator c = Client::GetFirstClient(); c; ++c )
        {
            {
                Client * client = &(*c);

                // check up to date status
                {
                    size_t size = client->notYetAcknowledged.size();
                    const size_t back = 3;
                    if ( size < back || client->notYetAcknowledged[ size - back ].timesSent > 0 )
                    {
                        upToDate = true;
                    }
                }

#ifndef COMPILE_DEDWINIA
                // remove players with too high ping from the lobby
                if ( !serverInfo.unstoppable && client->id.IsPlaying() && !CheckPing( client ) )
                {
                    client->id.Spectate();
                }
#endif

                if ( idleTime > 0 && ( lastActivityGlobal - client->lastActivity ).Seconds() > idleTime )
                {
                    // int lastReady = gameHistory.GetTailSeqID() - lastReadyTick;
                    // client is idle, what now?
                    if ( client->inGame )
                    {
                        // send a high speed message if we're already in the game
                        if ( client->speed < 20 )
                        {
                            Host::GetHost().Say( "Speeding up " + client->id.GetName() + " who seems to have left the computer." );
                        int speed = 20;
                        if ( settings.MinSpeed() > speed )
                            speed = settings.MinSpeed();
                        client->SetSpeed( speed );
                        }
                    }
                    /*
                    removed this bit. The ReadyVote mechanism is better suited for this job.
                    else if ( client->id.IsPlaying() && ( lastReady < 9 || lastReady > 60 * gameClock.ticksPerSecond ) )
                    {
                        // the check for the last ready click is to avoid removals to spectator mode just as the game has begun

                        // quit to spectator mode
                        Host::GetHost().Say( "Removing " + client->id.GetName() + " from the game for being idle." );
                        SuperPower::UnreadyAll();
                        client->id.Spectate();
                    }
                    */

                    // no need to further do anything for a while
                    client->lastActivity.AddSeconds( 10 );
                }
            }


            ++clients;

            c->finishedSending = false;
            if ( c->id.IsPlaying() )
            {
                c->bandwidthControlThisFrame = maxRatePerPlayer;
            }
            else
            {
                c->bandwidthControlThisFrame = maxRatePerSpectator;
            }

            // Throttle it according to the maximal queue length
            if ( maxQueue > 0 )
            {
                c->bandwidthControlThisFrame = int(c->bandwidthControlThisFrame * exp( -double(c->notYetProcessed.size())/double(maxQueue) ));
            }

            if ( c->IsConnected() )
            {
                if ( (int)c->notYetProcessed.size() > maxNotYetProcessed )
                {
                    maxNotYetProcessed = c->notYetProcessed.size();
                }

                ++connectedClients;
            }
        }

        // control the game clock speed if a client
        if ( 
            serverInfo.started
#ifndef XDEBUG
            && gameHistory.Playback()
#endif
            )
        {
            if ( gameClock.ticksPerSecond > clientWishSpeed )
            {
                gameClock.ticksPerSecond = clientWishSpeed;
            }
            else
            {
#ifdef COMPILE_DEDWINIA
                float behind = maxNotYetProcessed * .01;
                if ( behind < 1 )
                {
                    behind = 1;
                }
                gameClock.ticksPerSecond = int(clientWishSpeed / behind);
#endif

#ifdef COMPILE_DEDCON
                static int maxMax = 10;
                if ( maxNotYetProcessed > maxMax && gameClock.ticksPerSecond > 10 )
                {
                    gameClock.ticksPerSecond -= (maxMax * ( maxNotYetProcessed - maxMax ));
                    if ( gameClock.ticksPerSecond < 10 )
                        gameClock.ticksPerSecond = 10;
                    maxMax = maxNotYetProcessed;
                }
                else if ( maxNotYetProcessed < 10 && gameClock.ticksPerSecond < clientWishSpeed )
                {
                    maxMax = 10;
                    static int lastBump = 0;
                    if ( gameHistory.GetTailSeqID() > lastBump )
                    {
                        ++gameClock.ticksPerSecond;
                        lastBump = gameHistory.GetTailSeqID();
                    }
                }
#endif
            }

            // keep the game clock speed low if all clients are trailing behind
            static int nonUpToDateCount = 10;
            if ( upToDate )
            {
                nonUpToDateCount = 0;
                if ( gameClock.ticksPerSecond < 10 )
                {
                    gameClock.ticksPerSecond = 10;
                }
            }
            else
            {
                if ( ++nonUpToDateCount > 10 )
                {
                    nonUpToDateCount = 10;
                    gameClock.ticksPerSecond = 0;
                }
            }
        }
    }

    // reserve the minimal bandwidh that every client gets
    int minBandwidth = (settings.minBandwidth * msSinceLastSend * clients) / 1000;
    if ( minBandwidth > totalBandwidthControl )
        minBandwidth = totalBandwidthControl;
    totalBandwidthControl -= minBandwidth;

    // first, server the players.
    {
        int distribute = settings.maxTeams;

        while ( serverInfo.started && totalBandwidthControl > distribute * 10 && distribute > 0 )
        {
            // the rate available for every client in this run. Overestimate it a bit, one byte too
            // much doesn't hurt.
            int forEachClient = totalBandwidthControl/distribute + 1;
            distribute = 0;

            for ( Client::iterator client = Client::GetFirstClient(); client; ++client )
            {
                if ( client->id.IsPlaying() && !client->finishedSending )
                {
                    if ( DistributeBandwidth( &(*client), forEachClient, rateReservePlayer ) )
                    {
                        // this client could use some more
                        distribute++;
                    }
                }
            }
        }
    }

    // re-add the minimal bandwidth
    totalBandwidthControl += minBandwidth;

    // distribute the rest of the bandwidth to everyone
    {
        // urgency 1 handles the players, urgency 0 the spectators
        int distribute = clients;

        while ( totalBandwidthControl > distribute * 10 && distribute > 0 )
        {
            // the rate available for every client in this run. Overestimate it a bit, one byte too
            // much doesn't hurt.
            int forEachClient = totalBandwidthControl/distribute + 1;
            distribute = 0;

            for ( Client::iterator client = Client::GetFirstClient(); client; ++client )
            {
                if ( !client->finishedSending )
                {
                    if ( DistributeBandwidth( &(*client), forEachClient, rateReservePlayer ) )
                    {
                        // this client could use some more
                        distribute++;
                    }
                }
            }
        }
    }

    FTP::Send();

    // purge obsolete demo clients
    for ( Client::iterator c = deletedClients.begin(); c; ++c )
    {
        if ( c->id.GetID() < 0 && c->keyID < 0 )
            delete &(*c);
    }
}

// the acknowledgement waiting queue
typedef std::deque< SequenceAck > Acks;

static void Write( Time const & current, TagWriter & writer, Acks::iterator run, Acks::iterator end, int lowestResent, int tolerance, int lastSeqID, int writtenSoFar, int sendAhead )
{
    // abort condition: no sequence to send (has been acked)
    if ( !(*run).sequence || writtenSoFar > settings.maxPacketSize || (*run).sequence->SeqID() >= lastSeqID || sendAhead <= 0 )
    {
        writer.Obsolete();
        return;
    }

    // is the packet missing in action?
    bool mia = (*run).timesSent > 0 && tolerance < ( current - (*run).lastSent ).Milliseconds();

    // is the packet relatively fresh?
    bool fresh = (*run).timesSent == lowestResent;

    // both are good conditions to allow it to be written into the letter.
    // also, it's a good idea to send continuous packets.
    if ( mia || fresh || writer.GetState() != TagWriter::Raw )
    {
        // timestamp
        (*run).lastSent = current;

        // writing
        int lastLeft = writer.BytesLeft();
        (*run).sequence->Write( writer, lastSeqID, -1, writtenSoFar );
        writtenSoFar += lastLeft - writer.BytesLeft();

        // bookkeeping
        (*run).timesSent++;

        // don't let packets with repeated sends in them get too large
        if ( mia && ( writtenSoFar > maxResendPacketSize ) )
        {
            return;
        }

        // recurse with a nested tag writer
        TagWriter nestedWriter(writer);
        Write( current, nestedWriter, ++run, end, lowestResent, tolerance, lastSeqID, writtenSoFar, sendAhead - 1 );
    }
    else
    {
        // recurse plainly
        Write( current, writer, ++run, end, lowestResent, tolerance, lastSeqID, writtenSoFar, sendAhead - 1 );
    }
}

// returns whether a packet is missing in action
static bool MIA( SequenceAck const & sequence, Time const & current, int tolerance )
{
    return sequence.timesSent > 0 && tolerance < ( current - sequence.lastSent ).Milliseconds();
}

// returns how long it is overdue ( negative or zero if it is not overdue at all )
//static int MIATime( SequenceAck const & sequence, Time const & current, int tolerance )
//{
//    return sequence.timesSent == 0 ? 0x7fffffff : ( current - sequence.lastSent ).Milliseconds() - tolerance;
//}

//! a planned send event
struct PlannedSend
{
    int overdue;            //!< the milliseconds the packet is overdue
    SequenceAck * sequence; //!< the sequence to send

    PlannedSend(): overdue( -1 ), sequence(0)
    {
    }

    // swaps a and b if b is more urgent than a
    static void SwapIf( PlannedSend & a, PlannedSend & b )
    {
        if ( !b.sequence )
            return;

        if ( !a.sequence )
        {
            std::swap( a, b );
            return;
        }

        int minTimesSent = b.sequence->timesSent;
        if ( a.sequence->timesSent < minTimesSent )
        {
            minTimesSent = a.sequence->timesSent;
        }

        // weight packets that have been sent more often heavier, so holes at the beginning
        // of the queue are filled faster.
        if ( ( a.overdue/( b.sequence->timesSent + 1 ) ) < ( b.overdue / ( a.sequence->timesSent + 1 ) ) )
        {
            std::swap( a, b );
        }
    }
};

//! array of planned send events
#if 0
struct PlannedSendArray
{
    enum { MAX = 2 };
    PlannedSend planned[ MAX ];

    // adds a sequence to the list of packets that possibly should be sent.
    // return value: should sending be initiated?
    bool Add( SequenceAck * sequence, int overdue )
    {
        // create packet
        PlannedSend send;
        send.sequence = sequence;
        send.overdue = overdue;

        // flags telling if lost or new sequences have been encountered
        bool containsLost = false, containsNew = false;

        // sort the new packet in (bubble sort :), luckily, MAX isn't going to be big)
        for( int i = 0; i < MAX; ++i )
        {
            PlannedSend::SwapIf( planned[i], send );
            if ( planned[i].overdue < 0x7fffffff )
            {
                containsLost = true;
            }
            else
            {
                containsNew = true;
            }
        }

        // if both types of sequence are in, abort and send now.
        return containsNew && containsLost;
    }

    void Send( Time const & current, AutoTagWriter & writer, Client & client )
    {
        int lastSeqID = gameHistory.GetEffectiveTailSeqID();

        for( int i = 0; i < MAX && planned[i].sequence && writer.Size() <= settings.maxPacketSize; ++i )
        {
            SequenceAck * run = planned[i].sequence;

            // timestamp
            (*run).lastSent = current;

            // writing
            (*run).sequence->Write( writer, lastSeqID, client.id.GetID(), i );

            // bookkeeping
            (*run).timesSent++;

            // don't let packets with repeated sends in them get too large
            // if ( mia && ( writtenSoFar > maxResendPacketSize ) )
            // {
            // break;
            // }
        }
    }
};
#endif

// send outstanding data to client
void Client::SendTo()
{
    Time current;
    current.GetTime();

    if ( (LastTime()-lastAck).Seconds() > connectionTimeout )
    {
        if ( Log::GetLevel() >= 1 )
            Log::Out() << *this << " dropped.\n";

        ClientInTrouble( *this );

        // client dropped
        Quit( SuperPower::Dropped );
        return;
    }

    // determine the number of resends that are required to make a packet
    // arrive with > 99% probability
    int resends = 0;
    {
        float fail = 1;
        float loss = packetLoss.GetAverage();
        while ( fail > .01 && resends < 10 )
        {
            fail *= loss;
            ++resends;
        }
    };

    Acks & acks = notYetAcknowledged;

    int tolerance = int(pingAverager.GetAverage() + 2 * pingAverager.GetVariance());

    int retries = 0;
    while( bandwidthControl > 0 && acks.size() > 0 )
    {
        // if ( tolerance > 500 )
        // tolerance = 500;

        // prepare for sending
        AutoTagWriter writer;

        int lastSeqID = gameHistory.GetEffectiveTailSeqID();

#ifdef COMPILE_DEDWINIA    
        // playback can only proceed right up to the game end
        if ( gameHistory.Playback() && lastSeqID + 2 >= gameHistory.GetSeqIDEnd() && !id.IsPlaying() )
        {
            if ( Log::GetLevel() >= 1 )
                Log::Out() << *this << " thrown out to prevent crash. Use the \"JoinAs 0\" command to watch past this point.\n";

            Quit( SuperPower::Dropped );
            return;
        }
#endif

        // clamp
        if ( writeAhead < 0 )
            writeAhead = 0;
        if ( writeAhead >= (int)acks.size() )
            writeAhead = acks.size() - 1;

        bool restart = false;

        for ( Acks::iterator run = acks.begin()+writeAhead; run != acks.end(); ++run )
        {
            int runSeqID = (*run).sequence->SeqID();

            // abort conditions:
            bool abort = false;

            // no sequence to send (it has been acked), writing after end
            if ( !(*run).sequence || runSeqID >= lastSeqID || writeAhead > maxAhead )
            {
                restart = abort = true;
            }

            // writing further than we have ever written
            if ( runSeqID > lastSentSeqID+1 )
            {
                // update lastSentSeqID so this code is triggered one seqID later
                // on the next repeat
                lastSentSeqID = runSeqID-1;
                restart = true;
            }

            // see if there was a restart wish
            if ( restart )
            {
                // resend packets in bursts; each burst is dimensioned
                // so that it arrives with 99% probability. Between the
                // bursts, we wait for timeouts.
                int timesSent = (*acks.begin()).timesSent;
                if ( ( ( timesSent - 1 ) % resends ) != 0 ||
                       MIA( *(acks.begin()), current, tolerance ) )
                {
                    // record one instance of packet loss
                    if ( timesSent == 1 )
                    {
                        packetLoss.Add( 1 );
                        packetLoss.Decay( 0.95 );
                    }

                    writeAhead = 0;
                    abort = true;
                }
                else
                {
                    // no restart happening
                    restart = false;
                }

                if ( abort )
                    break;
            }

#ifdef COMPILE_DEDWINIA
            // the login ack packet must NOT be received twice by the client.
            // be extra careful resending it.
            if ( runSeqID == -1 )
            {
                // add a couple of seconds to the tolerance
                tolerance += 3500;
            }
#endif

            // is the packet missing in action?
            bool mia = MIA( *run, current, tolerance ); // || (*run).timesSent >= 1;

            // is the packet relatively fresh?
            bool fresh = (*run).timesSent == 0;

            // both are good reasons to send it.
            if ( mia || fresh )
            {
                // we're really writing this
                ++writeAhead;

                // timestamp
                (*run).lastSent = current;

                // only send two sequences
                bool abort = false;
                if ( writer.Size() > 0 )
                    abort = true;

                // writing
                (*run).sequence->Write( writer, lastSeqID, id.GetID(), writer.Size() > 0 );

                // bookkeeping
                (*run).timesSent++;

                if ( abort )
                    break;
            }
        }

        if ( writer.Size() > 0 )
        {
            // wrapping up
            writer.Close();

            // update bandwidth control
            bandwidthControl -= writer.Size() + packetOverhead;

            // Send!
            writer.SendTo( serverSocket,saddr, PLEBIAN_ENCRYPTION );

#ifdef DEBUG
            // for breakpoints
            int x;
            x = 0;
#endif
        }
        else if ( !restart || retries-- <= 0 )
        {
            // nothing more to do, don't send more stuff
            break;
        }
    }
}

// percentage of game time that has to be elapsed before a quitter is no longer logged
static IntegerSetting earlyQuit( Setting::Log, "EarlyQuit", 20, 100 );

// the same in game time minutes
static IntegerSetting earlyQuitMinutes( Setting::Log, "EarlyQuitMinutes", 60, 1200 );

void Client::DeleteAll()
{
    // log and print scores
    PrintScores( 0, true, false );
    LogScores();

    // log early quitters
    int totalMinutes = lastActivityGlobalGameTime/600; // the total game time in minutes
    int totalMinutesAlt = lastPlayerMajority/600; // the last time most players were online
    if ( totalMinutesAlt < totalMinutes )
    {
        totalMinutes = totalMinutesAlt;
    }

    for ( Client::const_iterator c = deletedClients.begin(); c; ++c )
    {
        if ( c->inGame && c->quitTime >= 0 )
        {
            // game time the player was online in minutes before he quit
            int quitMinutes = c->quitTime/600;

            if( quitMinutes < ( totalMinutes * earlyQuit )/100 && quitMinutes < earlyQuitMinutes )
            {
                bool eventLog = true;

                // find possible excuses
                std::ostringstream excuse;
                if ( c->outOfSync || c->numberOfSyncs > 0 )
                {
                    excuse << ", excuse = outofsync";
                    eventLog = false; // this is the only really good excuse
                }
                if ( c->notYetProcessed.size() > 20 )
                {
                    excuse << ", excuse = cpulag";
                }

                // yeah, a cowardly early quitter
                GetGriefLog( *c, "earlyQuit", eventLog )
                    << ", quitTime = (" << quitMinutes
                    << " minutes/" << totalMinutes << " total minutes)"
                    << ", ping = " << int(c->pingAverager.GetAverage())
                    << ", packetLoss = " << int(c->packetLoss.GetAverage()*100)
                    << "%" << excuse.str() << "\n";
            }
        }
    }

    GetClients().Clear();
    deletedClients.Clear();
}

ListBase<Client> & Client::GetClients()
{
    static ListBase<Client> clients;
    return clients;
}

Client::iterator Client::GetFirstClient()
{
    return GetClients().begin();
}
