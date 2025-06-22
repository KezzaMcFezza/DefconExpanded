/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "Lib/Codes.h"

#include "Host.h"
#include "GameSettings/Settings.h"
#include "Recordings/AutoTagWriter.h"
#include "Recordings/GameHistory.h"
#include "Recordings/Sequence.h"
#include "Lib/Globals.h"
#include "Lib/Unicode.h"
#include "Main/Log.h"
#include <sstream>

class HostNameSetting: public StringActionSetting
{
public:
    HostNameSetting():StringActionSetting( Setting::Flow, "HostName" ){}
    
    // sets the hostname
    virtual void Activate( std::string const & val )
    {
        Host::GetHost().SetName( val );
    }
};

void Host::SetDefaultName()
{
#ifdef COMPILE_DEDWINIA
    SetName( "Dedwinians" );
#else
    SetName( "Host" );
#endif
}

#ifdef COMPILE_DEDWINIA
class EvilNameSetting: public StringActionSetting
{
public:
    EvilNameSetting():StringActionSetting( Setting::Flow, "EvilName" ){}
    
    // sets the hostname
    virtual void Activate( std::string const & val )
    {
        Host evilinians;
        evilinians.Play(4);
        evilinians.SetAndSendName( val );
        evilinians.Vanish();
    }
};

static EvilNameSetting evil;
#endif

class HostSaySetting: public StringActionSetting
{
public:
    HostSaySetting():StringActionSetting( Setting::Flow, "Say" ){}
    
    // says something
    virtual void Activate( std::string const & val )
    {
        Host::GetHost().Say( val );
    }
};

static HostSaySetting say;
static HostNameSetting name;

Host::Host(): nameSet( false ), reserved( 0 )
{
    gameHistory.RequestBump();
#ifndef COMPILE_DEDWINIA
    Spectate();
#endif
}

Host::~Host()
{
    StateChangeLogMuter logMuter;
    Vanish();
}

#ifdef COMPILE_DEDWINIA
IntegerSetting hostTeamID( Setting::Flow, "HostTeamID", 5, 5, 0 );
#endif

//! sets the name of the host
void Host::SetName( std::string const & name )
{
    nameSet = true;

    this->name = name;
#ifndef COMPILE_DEDWINIA
    SetAndSendName( name );
#else
    if ( serverInfo.started )
    {
        // do nothing
        return;
    }

    StateChangeLogMuter logMuter;
    std::auto_ptr< StateChangeNetMuter > muter;
    if ( gameHistory.Playback() )
    {
        muter = std::auto_ptr< StateChangeNetMuter >( new StateChangeNetMuter() );
    }

    if ( !IsPlaying() )
    {
        Play(hostTeamID.Get());
    }
    SetAndSendName( name );
    SetAndSendColor( -1 );
#endif
}

#ifdef COMPILE_DEDWINIA
static void SayTo( std::string const & line, int fakesender, std::vector< int > const & whitelist, std::vector< int > const & blacklist, Sequence * & reserved, bool clarify )
{
    AutoTagWriter writer(C_TAG, C_CHAT );
    writer.WriteChar( C_PLAYER_ID, fakesender );
    std::wostringstream s;
    if ( clarify )
    {
        s << L"(BOT) ";
    }
    s << line;
    writer.WriteUnicodeString(C_CHAT_STRING, s.str() );

    if ( !reserved )
    {
        reserved = gameHistory.ForClient();
    }
    reserved->InsertClient( writer, whitelist, blacklist );
    reserved = 0;
}

static void SayTo( std::string const & line, int receiver, std::vector< int > const & blacklist, Sequence * & reserved, bool clarify )
{
    std::vector< int > whitelist;
    whitelist.push_back( receiver );

    SayTo( line, receiver, whitelist, blacklist, reserved, clarify );
}
#endif

//! says something
void Host::Say( std::string const & line, std::vector< int > const & whitelist, std::vector< int > const & blacklist_in )
{
    if ( !nameSet )
    {
        SetDefaultName();
    }

    if ( Log::GetLevel() >= 1 )
        Log::Out() << name << " : " << line << '\n';

#ifdef COMPILE_DEDWINIA
    // fetch one ID
    if ( serverInfo.started )
    {
        int id = -1;
        bool isCPU = false;
        {
            SuperPower::iterator c = SuperPower::GetFirstSuperPower();
            while ( c )
            {
                if ( c->IsPlaying() )
                {
                    id = c->GetID();
                    isCPU = !c->GetHuman();
                    break;
                }
                ++c;
            }
            
            if ( id < 0 )
            {
                return;
            }
            SetID( id );
        }
        
        if ( isCPU )
        {
            SayTo( line, id, whitelist, blacklist_in, reserved, false );
        }
        else if ( whitelist.size() )
        {
            for( std::vector< int >::const_iterator i = whitelist.begin();
                 i != whitelist.end(); ++i )
            {
                SayTo( line, *i, blacklist_in, reserved, true );
            }
        }
        else
        {
            std::vector< int > blacklist = blacklist_in;
            
            for( Client::const_iterator i = Client::GetFirstClient(); i; ++i )
            {
                SayTo( line, i->GetID(), blacklist_in, reserved, true );
                blacklist.push_back( i->GetID() );
            }
            
            SayTo( line, id, whitelist, blacklist, reserved, true );
        }

        return;
    }

    SayTo( line, GetID(), whitelist, blacklist_in, reserved, false );
#else
    // running number
    static int dv=0;
    
    gameHistory.RequestBump();
    AutoTagWriter writer(C_TAG, C_CHAT );
    writer.WriteChar(C_LOGIN_KEY,0x64); // public chat
    writer.WriteInt(C_METASERVER_GAME_STARTED, dv++ );
    writer.WriteInt(C_PLAYER_SPECTATOR, 1 );
    writer.WriteChar( C_PLAYER_TEAM_ID, GetID() );
    writer.WriteFlexString(C_CHAT_STRING, line );

    if ( !reserved )
    {
        reserved = gameHistory.ForClient();
    }
    reserved->InsertClient( writer, whitelist, blacklist_in );
    reserved = 0;
#endif
}

//! call to put the next chat into a reserved sequence
void Host::SayHere( Sequence * sequence )
{
    reserved = sequence;
}


//! says something
void Host::Say( std::string const & line, std::vector< int > const & whitelist )
{
    // no blacklist
    static std::vector< int > const blacklist;

    Say( line, whitelist, blacklist );
}

//! says something to everyone
void Host::Say( std::string const & line )
{
    static std::vector< int > const whitelist;
    Say( line, whitelist );
}

//! says something to a single player
void Host::Say( std::string const & line, Client const & client )
{
    std::vector< int > whitelist;
    whitelist.push_back( client.id.GetID() );
    Say( std::string("[private] ") + line, whitelist );
}

//! says something to a single player (or logs it to the console )
void Host::Say( std::string const & line, Client const * client )
{
    if ( client )
    {
        Say( line, *client );
    }
    else if ( Log::GetLevel() > 0 )
    {
        Log::Err() << line << '\n';
    }
}

//! returns the singleton host
Host & Host::GetHost()
{
    static Host host;
    return host;
}
