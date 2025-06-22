/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "CPU.h"

#include "Lib/Codes.h"

#include "Recordings/GameHistory.h"
#include "GameSettings/GameSettings.h"
#include "Main/Log.h"
#include "Recordings/AutoTagWriter.h"
#include "Lib/Globals.h"

#include <iostream>
#include <sstream>

static CPU & GetCPU( int index )
{
    assert( index >= 0 && index < CPU::MaxCPUs );
    
    static CPU cpus[CPU::MaxCPUs];

    return cpus[ index ];
}

CPU::CPU()
{
    SetHuman( false );
}

// static int alliance = 0;
// static int allianceSize = 0;

void CPU::Log()
{
    Log::Event() << "CPU_NEW " << GetID() << "\n";
    Log::Event() << "CPU_NAME " <<GetID()
                 << " " << GetName() << "\n";

}

//! adds a CPU player
CPU * CPU::Add( char const * name )
{
    if ( serverInfo.started )
        return 0;

    // count the active players
    int inGame = 0;
    for ( SuperPower::const_iterator c = SuperPower::GetFirstSuperPower(); c; ++c )
    {
        if ( c->IsPlaying() )
        {
            inGame++;
        }
    }
    
    // ignore if teams are full
    if ( inGame >= settings.maxTeams )
        return 0;

    for ( int i = 0; i < MaxCPUs; ++i )
    {
        CPU & cpu = GetCPU( i );
        if ( !cpu.IsPlaying() )
        {
            cpu.Exist();

            Log::Event() << "CPU_NEW " << cpu.GetID() << "\n";

            if ( cpu.Play() )
            {
                // give it a name
                if ( name )
                {
                    cpu.SetAndSendName( name );
                }   
#ifdef COMPILE_DEDCON
                // dedwinia handles naming CPUs later
                else    
                {   
                    std::ostringstream s;
                    s << "Player " << cpu.GetTeamID();
                    cpu.SetAndSendName( s.str() );
                }   
#endif
                if ( name )
                {
                    Log::Event() << "CPU_NAME " << cpu.GetID()
                                 << " " << name << "\n";
                }

                return & cpu;
            }
            else
            {
                // no spot free
                cpu.Remove();
                return 0;
            }
        }
    }
    
    return 0;
}

//! removes a CPU player
void CPU::Remove()
{
    if ( serverInfo.started )
        return;

    int id = GetID();

    Vanish();

    if ( id > 0 )
    {
        Log::Event() << "CPU_QUIT " << id << "\n";
    }
}

//! removes all CPU players
void CPU::RemoveAll()
{
    for ( int i = MaxCPUs-1; i >= 0; --i )
    {
        GetCPU( i ).Remove();
    }
}

//! removes all CPU players
void CPU::ReadyAll()
{
    bool readied = false;

    for ( int i = MaxCPUs-1; i >= 0; --i )
    {
        CPU & cpu = GetCPU( i );
        if ( cpu.IsPlaying() && !cpu.IsReady() )
        {
            readied = true;
            cpu.ToggleReady( true );
            cpu.SendToggleReady();
        }
    }

    if  ( readied )
        gameHistory.RequestBump();
}

//! for executing script callbacks at the specified time
class CallbackSetting: public StringSetting
{
public:
    CallbackSetting( char const * name )
    : StringSetting( CPU, name )
    {
        adminLevel = 1000;
    }

    virtual void Set( std::string const & value )
    {
        // determine and store admin level
        adminLevel = 1000;
        if ( SettingsReader::GetActive() )
            adminLevel = SettingsReader::GetActive()->GetAdminLevel();

        StringSetting::Set( value );
    }

    void Call()
    {
        if ( Get().size() == 0 )
            return;

        // prepare reader
        SettingsReader reader;
        reader.SetAdminLevel( adminLevel );

        // fill it
        reader.AddLine( "callback", Get() );

        // and execute
        reader.Apply();
    }
private:
    int adminLevel;        //! the admin level of the one setting the callback
};

// callbacks for countdown events
static CallbackSetting onStartCountdown( "OnStartCountdown" );
static CallbackSetting onAbortCountdown( "OnAbortCountdown" );

//! callback for start of countdown
void CPU::OnStartCountdown()
{
    onStartCountdown.Call();

    ReadyAll();
}

//! callback for abort of countdown
void CPU::OnAbortCountdown()
{
    onAbortCountdown.Call();

    ReadyAll();
}

// fills the server with default named CPUs
class FillWithCPUs: public ActionSetting
{
public:
    FillWithCPUs()
    : ActionSetting( CPU, "FillWithCPUs" )
    {
    }

    virtual void DoRead( std::istream & source )
    {
        for( int i = CPU::MaxCPUs-1; i >= 0; --i )
        {
            CPU::Add();
        }
    }
};
static FillWithCPUs fillWithCPUs;

// fills the server with default named CPUs
class AddCPU: public StringActionSetting
{
public:
    AddCPU()
    : StringActionSetting( CPU, "AddCPU" )
    {
    }

    virtual void Activate( std::string const & value )
    {
        if ( value.size() == 0 )
            CPU::Add();
        else
            CPU::Add( value.c_str() );
    }
};
static AddCPU addCPU;

// fills the server with default named CPUs
class RemoveCPUs: public ActionSetting
{
public:
    RemoveCPUs()
    : ActionSetting( CPU, "RemoveCPUs" )
    {
    }

    virtual void DoRead( std::istream & source )
    {
        CPU::RemoveAll();
    }
};
static RemoveCPUs removeCPUs;

// commands taking two integer parameters
class ActionSettingIntegerPair: public ActionSetting
{
public:
    ActionSettingIntegerPair( char const * name )
    : ActionSetting( CPU, name )
    {
    }

    virtual void Activate( ::CPU & cpu, int v2 ) = 0;
    
    virtual char const * Target() const = 0;

    virtual void DoRead( std::istream & source )
    {
        if ( serverInfo.unstoppable )
        {
            Log::Err() << name << " is only legal while the game has not yet started.\n";
            return;
        }

        int v1 = 0, v2 = 0;
        source >> v1 >> v2;

        if ( source.fail() )
        {
            Log::Err() << "Usage: " << name << " <cpu ID (0-5)> <" << Target() << " ID (0-5)>.\n";
            return;
        }

        if ( v1 < 0 || v1 >= CPU::MaxCPUs )
        {
            Log::Err() << "CPU index " << v1 << " out of range, must be between 0 and 5.\n";
            return;
        }
        ::CPU & cpu = GetCPU( v1 );
        if ( !cpu.IsPlaying() )
        {
            Log::Err() << "CPU " << v1 << " is not playing.\n";
            return;
        }

        if ( v2 < 0 || v2 >= 6  )
        {
            Log::Err() << Target() << " ID " << v2 << " out of range, must be between 0 and 5.\n";
            return;
        }

        CPU::UnreadyAll();

        Activate( cpu, v2 );
    }
};

// give an AI a new alliance
class ActionSettingCPUAlliance: public ActionSettingIntegerPair
{
public:
    ActionSettingCPUAlliance()
    : ActionSettingIntegerPair( "CPUAlliance" )
    {
    }

    virtual char const * Target() const{ return "alliance"; }

    virtual void Activate( ::CPU & cpu , int v2 )
    {
        // send alliance change message
        cpu.SetAndSendColor( v2 );
    }
};
static ActionSettingCPUAlliance cpuAlliance;

#ifdef COMPILE_DEDCON
// give an AI a new alliance
class ActionSettingCPUTerritory: public ActionSettingIntegerPair
{
public:
    ActionSettingCPUTerritory()
    : ActionSettingIntegerPair( "CPUTerritory" )
    {
    }

    virtual char const * Target() const{ return "territory"; }

    virtual void Activate( ::CPU & cpu , int v2 )
    {
        cpu.ToggleTerritory( v2 );
    }
};
static ActionSettingCPUTerritory cpuTerritory;
#endif

// turns all regular players into CPUs
class CPUIze: public ActionSetting
{
public:
    CPUIze()
    : ActionSetting( CPU, "CPUIze" )
    {
    }

    virtual void DoRead( std::istream & source )
    {
        if ( !serverInfo.started )
        {
            Log::Err() << "Only available after the game started.\n";
        }

        for ( Client::iterator client = Client::GetFirstClient(); client; ++client )
        {
            if ( client->inGame )
            {
                // speed him up
                client->SetSpeed( 20 );

                // don't let him reconnect as a regular player
                client->noPlay = true;

                // and remove him from the game
                client->id.Spectate();
            }
        }
        
    }
};
static CPUIze cpuize;

