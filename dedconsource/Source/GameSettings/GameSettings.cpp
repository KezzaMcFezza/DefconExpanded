/*
 *  * DedCon: Dedicated Server for Defcon (and Multiwinia)
 *  *
 *  * Copyright (C) 2008 Manuel Moos
 *  *
 *  */

#include "GameSettings.h"

#include "Main/config.h"

#include <string.h>

#include "Recordings/GameHistory.h"
#include "Network/Exception.h"
#include "Lib/Globals.h"
#include "Settings.h"
#include "Server/Host.h"
#include "Main/Log.h"
#include "Recordings/AutoTagWriter.h"
#include "Lib/Codes.h"

#ifdef COMPILE_DEDWINIA
#include "Map.h"
#endif

#include <sstream>

class DemoKeygen
{
public:
    DemoKeygen()
    {
        checksum = 0;
    }

    void AddChar( char c )
    {
        s << c;
        checksum += c;
    }

    void AddRandom( int num )
    {
        for( int i = num-1; i>=0; --i )
        {
            AddChar( 'A' + rand() % 26 );
        }
    }

    void AddString( char const * string )
    {
        while (*string)
        {
            AddChar( *(string++) );
        }
    }

    void AddChecksum()
    {
        AddChar( 'A' + ( checksum % 26 ) );
    }

    std::string Generate()
    {
        // MW TEST
        // AddString( "DEMOUR-QDXJCA-ODCIUD-QHLZJW-FM" ); // "W"

        // DEFCON TEST
        // AddString( "DEMOOF-MJSHVW-MXLIKG-FNIZFD-XQ" ); // "V"

        AddString( "DEMO" );
        AddRandom(2);
        AddChar('-');
        AddRandom(6);
        AddChar('-');
        AddRandom(6);
        AddChar('-');
        AddRandom(6);
        AddChar('-');
        AddRandom(2);

#ifdef COMPILE_DEDWINIA
        checksum += 4;
#endif

        AddChecksum();
        return s.str();
    }
public:
    std::ostringstream s;
    int checksum;
};

// a pending command; use them to execute
// anything that may not finish immediately
class PendingCommandGame: public PendingCommand
{
public:
    virtual ~PendingCommandGame(){}

    // executes the command
    virtual void Try() = 0;

    // call from within Try() to try the command again later
    static void TryAgainLater( int seconds )
    {
        if ( !settings.dryRun )
        {
            TryAgainLaterBase( seconds );
        }
    }
};

#ifdef COMPILE_DEDCON
#define DEFAULT_SPECTATORS 3
#define DEFAULT_PORT 5010
#if VANILLA
#define DEFAULT_VERSION "1.42 dedicated"
#endif
#ifdef NOTVANILLA
#define DEFAULT_VERSION "1.60.1.5 MINICOM"
#endif
#define DEFAULT_LAN_ADVERTISE 1
#endif
#ifdef COMPILE_DEDWINIA
#define DEFAULT_SPECTATORS 0
#define DEFAULT_PORT 4000
#define DEFAULT_VERSION "retail.iv.1.3.0"
#define DEFAULT_LAN_ADVERTISE 0
#endif


#ifdef COMPILE_DEDWINIA
char const * gameModes[] =
{
    "Domination",
    "KingOfTheHill",
    "CaptureTheStatue",
    "Assault",
    "RocketRiot",
    "Blitzkrieg",
    0
};

#define GAMEMODES2
char const * gameModes2[] =
{
    "DOM",
    "KOTH",
    "CTS",
    "ASLT",
    "RR",
    "BLITZ",
    0
};
#endif

#ifdef COMPILE_DEDCON
#if VANILLA
char const * gameModes[] =
{
    "Default",
    "OfficeMode",
    "SpeedDefcon",
    "Diplomacy",
    "BigWorld",
    "Tournament",
    "Custom",
    0
};
#endif
#ifdef NOTVANILLA
char const * gameModes[] =
{
    "Default",
    "OfficeMode",
    "SpeedDefcon",
    "Diplomacy",
    "BastardDiplomacy",
    "BigWorld",
    "Tournament",
    "Custom",
    0
};
#endif
#endif

GameModeSetting::GameModeSetting()
  : EnumSetting( Setting::GameOption, "GameMode", gameModes, 0 )
{
#ifdef GAMEMODES2
    SetAlternativeValues( gameModes2 );
#endif
}

char const * noScoreModes[] =
{
    "DoesNotApply",
    0
};

char const * dominationScoreModes[] =
{
    "TotalSpawnPonints",
    "SpawnPointsPerSecond",
    0
};

char const * kothScoreModes[] =
{
    "ZonesControlledPerSecond",
    "ZonePopulationPerSecond",
    "FinalZonePopulation",
    "TotalZoneControl",
    0
};


void GameModeSetting::OnChange()
{
    EnumSetting::OnChange();
 
#ifdef COMPILE_DEDWINIA   
    switch( Get() )
    {
    case 0:
        settings.scoreMode.SetValues( dominationScoreModes );
        break;
    case 1:
        settings.scoreMode.SetValues( kothScoreModes );
        break;
    default:
        settings.scoreMode.SetValues( noScoreModes );
        break;
    }
#endif
}


char const * aiLevels[] =
{
    "easy",
    "normal",
    "hard",
    0
};

char const * crateDropModes[] =
{
    "random",
    "weighted",
    0
};

Settings::Settings()
: dryRun( false )

, serverName( Setting::GameOption, "ServerName", "Dedicated Game Server")

,advertiseOnInternet( Setting::GameOption, "AdvertiseOnInternet", 1, 1)
,advertiseOnLAN( Setting::GameOption, "AdvertiseOnLAN", DEFAULT_LAN_ADVERTISE, 1)

#ifdef COMPILE_DEDCON
#if VANILLA
,maxTeams( Setting::GameOption, "MaxTeams", 3,SuperPower::MaxSuperPowers,1)
#elif EIGHTPLAYER
,maxTeams( Setting::GameOption, "MaxTeams", 4,SuperPower::MaxSuperPowers,1)
#elif TENPLAYER
,maxTeams( Setting::GameOption, "MaxTeams", 5,SuperPower::MaxSuperPowers,1)
#endif
#endif
#ifdef COMPILE_DEDWINIA
,maxTeams( Setting::GameOption, "MaxTeams", 2,SuperPower::MaxSuperPowers,1)
#endif
#if VANILLA
,minTeams( Setting::ExtendedGameOption, "MinTeams", 2,6)
#elif EIGHTPLAYER
,minTeams( Setting::ExtendedGameOption, "MinTeams", 2,8)
#elif TENPLAYER
,minTeams( Setting::ExtendedGameOption, "MinTeams", 2,10)
#endif

#ifdef COMPILE_DEDCON
#if VANILLA
,territoriesPerTeam( Setting::GameOption, "TerritoriesPerTeam", 1,6,1)
#elif EIGHTPLAYER
,territoriesPerTeam( Setting::GameOption, "TerritoriesPerTeam", 1,8,1)
#elif TENPLAYER
,territoriesPerTeam( Setting::GameOption, "TerritoriesPerTeam", 1,10,1)
#endif
,citiesPerTerritory( Setting::GameOption, "CitiesPerTerritory", 25, 50,1 )
,populationPerTerritory( Setting::GameOption, "PopulationPerTerritory", 100, 200 )
,cityPopulations( Setting::GameOption, "CityPopulations", 0, 3 )

,randomTerritories( Setting::GameOption, "RandomTerritories", 0, 1)

,permitDefection( Setting::GameOption, "PermitDefection", 1, 1)
,radarSharing( Setting::GameOption, "RadarSharing", 1, 3 )

,gameSpeed( Setting::GameOption, "GameSpeed", 0, 4 )
,slowestSpeed( Setting::GameOption, "SlowestSpeed", 1, 4)
,scoreMode( Setting::GameOption, "ScoreMode", 0, 2)
#ifdef NOTVANILLA
,victoryMode( Setting::GameOption, "VictoryMode", 0, 1 )
#endif

,victoryTrigger( Setting::GameOption, "VictoryTrigger", 20, 100, 5)
,victoryTimer( Setting::GameOption, "VictoryTimer", 45, 600, 10)
,maxGameRealTime( Setting::GameOption, "MaxGameRealTime", 0, 600 )

,variableUnitCounts( Setting::GameOption, "VariableUnitCounts", 0, 1 )
,worldScale( Setting::GameOption, "WorldScale", 100, 200, 50 )


,teamSwitching( Setting::GameOption, "TeamSwitching", 0, 1 )

// ,adminPassword("AdminPassword" )

,spectatorChatChannel( Setting::GameOption, "SpectatorChatChannel", 1, 1 )

,serverMods( Setting::ExtendedGameOption, "ServerMods" )
#endif
,serverPassword( Setting::GameOption, "ServerPassword" )
#ifdef NOTVANILLA
,cityBuffer( Setting::GameOption, "CityBuffer", 0,5)
,unitBuffer( Setting::GameOption, "UnitBuffer", 1,10)
,firstStrikeBonus( Setting::GameOption, "FirstStrikeBonus", 0, 15, 0 )
,worldUnitScale( Setting::GameOption, "WorldUnitScale", 100, 200, 25 )
,fighterBehavior( Setting::GameOption, "FighterBehavior", 1, 0 )
,bomberBehavior( Setting::GameOption, "BomberBehavior", 1, 0 )
,nukeTrajectoryCalc( Setting::GameOption, "NukeTrajectoryCalc", 1, 0 )
,emptyBomberLaunch( Setting::GameOption, "EmptyBomberLaunch", 1, 1 )
#endif

,maxSpectators( Setting::GameOption, "MaxSpectators", DEFAULT_SPECTATORS, 100 )

,serverPort( Setting::Network, "ServerPort", DEFAULT_PORT )
,serverLANIP( Setting::Network, "ServerLANIP", "" )
,serverVersion( Setting::ExtendedGameOption, "ServerVersion", DEFAULT_VERSION )

/*
,serverGame( Setting::Internal, "ServerGame", "Defcon")
,metaServer( Setting::Internal, "MetaServer", "metaserver.introversion.co.uk" )
,metaServerPort( Setting::Internal, "MetaServerPort", 5008)
*/

,playerBandwidth( Setting::Network, "PlayerBandwidth", 4000, 1000000000, 600 )
,spectatorBandwidth( Setting::Network, "SpectatorBandwidth", 0, 1000000000, 0 )
,minBandwidth( Setting::Network, "MinBandwidth", 600, 1000000000, 100 )
,totalBandwidth( Setting::Network, "TotalBandwidth", 10000, 1000000000, 600 )
,maxPacketSize( Setting::Internal, "MaxPacketSize", 500, 2048, 1 )

, forbiddenVersions( Setting::ExtendedGameOption, "ForbiddenVersions","")
, noPlayVersions( Setting::ExtendedGameOption, "NoPlayVersions","")
, demoRestricted( Setting::ExtendedGameOption, "DemoRestricted", 0, 2 )
, allStarTime( Setting::Player, "AllStarTime", 0, 600 )
#ifdef COMPILE_DEDWINIA
, mapFile( Setting::GameOption, "MapFile", "" )
, mapHash( Setting::GameOption, "MapHash", 0 )
, mapName( Setting::GameOption, "MapName", "" )
, aiLevel( Setting::GameOption, "AILevel", aiLevels, 1, 2 )
, crateDropMode( Setting::GameOption, "CrateDropMode", crateDropModes, 0, 1 )
, crateDropTimer( Setting::GameOption, "CrateDropTimer", 40, 300 )
, handicap( Setting::GameOption, "Handicap", 90, 90 )
, maxArmour( Setting::GameOption, "MaxArmour", 2, 10 )
, maxTurrets( Setting::GameOption, "MaxTurrets", 3, 5 )
, numTanks( Setting::GameOption, "NumTanks", 3, 10, 3 )
, reinforcementCount( Setting::GameOption, "ReinforcementCount", 40, 100 )
, reinforcementTimer( Setting::GameOption, "ReinforcementTimer", 60, 300 )
, retributionMode( Setting::GameOption, "RetributionMode", 1 )
, scoreMode( Setting::GameOption, "ScoreMode", dominationScoreModes, 0 )
, startingPowerups( Setting::GameOption, "StartingPowerups", 0, 3 )
, suddenDeath( Setting::GameOption, "SuddenDeath", 1 )
, timeLimit( Setting::GameOption, "TimeLimit", 10, 60 )
, turretFrequency( Setting::GameOption, "TurretFrequency", 2, 10 )
, basicCrates( Setting::GameOption, "BasicCratesOnly", 0 )
, coopMode( Setting::GameOption, "CoopMode", 0 )
, numAttackers( Setting::GameOption, "NumAttackers", 0, 4 )
#endif
, serverKey(Setting::Network, "ServerKey", static_cast<const char*>(DemoKeygen().Generate().c_str()))
{
#ifdef COMPILE_DEDWINIA
    gameMode.SetNumericName( 0 );
    mapFile.SetNumericName( 3 );
    aiLevel.SetNumericName( 4 );
    coopMode.SetNumericName( 5 );
    basicCrates.SetNumericName( 7 );
    mapHash.SetNumericName( 8 );
#endif
}

#define MAX_SETTINGS 50
static Setting * networkSettings[MAX_SETTINGS];

class SettingsFiller
{
public:
    void Add( Setting & setting )
    {
        assert( index < MAX_SETTINGS-2 );
        networkSettings[index++] = &setting;
    }

    SettingsFiller(): index( 0 ){}
private:
    int index;
};

//! revert demo-restricted settings
void Settings::RestrictDemo()
{
    // maximal client counts
    if ( maxSpectators > 3 )
        maxSpectators.Set(3);
    if ( maxTeams > demoLimits.maxGameSize )
        maxTeams.Set(demoLimits.maxGameSize);

    // all settings that go over the network are restricted, with exceptions:
    // the limits on player and spectator count are handled above, and it is allowed
    // to disable internet advertising.
    // on multiwinia, the map is also allowed to change.
    Setting * * run = GetNetworkSettings();
    while ( *run )
    {
        Setting * s = *run;

        if ( s != &serverName && s != &maxTeams && s != &maxSpectators && s != &advertiseOnInternet && s != &advertiseOnLAN 
#ifdef COMPILE_DEDWINIA
             && s != &mapFile
             && s != &gameMode
#endif
             && s != &serverPassword 
            )
        {
            s->Revert();
        }

        run++;
    }
}

void Settings::CheckRestrictions()
{
    // check for demo limits
    demoLimits.demoRestricted = DemoRestricted();

    if ( demoLimits.set && Setting::AnythingChanged() )
    {
        demoLimits.applied = false;
        demoLimits.ApplyLimits();
    }

    LockSettingsAccordingToGameMode();

    // check that all players have the minimal speed set
    int speed = MinSpeed();
    for ( Client::iterator c = Client::GetFirstClient(); c; ++c )
        if ( c->speed < speed )
            c->SetSpeed( speed );

    // sanity check player counts
    if ( Setting::CheckLimits() )
    {
        // get configured bandwidth
        int playerBandwidthReal = playerBandwidth;
        int spectatorBandwidthReal = playerBandwidth;

        // sanity check min bandwidth, it needs to be smaller than either
        if ( playerBandwidthReal < minBandwidth )
            minBandwidth.Set( playerBandwidthReal );

        // override spectator bandwidth if the admin wants it
        if ( spectatorBandwidth != 0 )
        {
            if ( spectatorBandwidth < minBandwidth )
                spectatorBandwidth.Set( minBandwidth );

            spectatorBandwidthReal = spectatorBandwidth;
        }

        // more than 1000 bytes/s are not used in practice
        if ( spectatorBandwidthReal > 1000 )
            spectatorBandwidthReal = 1000;
        if ( playerBandwidthReal > 1000 )
            playerBandwidthReal = 1000;
        
        int teams = serverInfo.unstoppable ? Client::CountPlayers() : maxTeams;

        // first, check whether we can support all the spectators
        int supportedSpectators = ( totalBandwidth - teams * playerBandwidthReal )/spectatorBandwidthReal;
        
        // check spectator count
        if ( maxSpectators > supportedSpectators )
        {
            if ( Log::GetLevel() >= 0 )
            {
                Log::Err() << "Too many clients allowed for the configured total bandwidth. Fixing that for you.\n";
            }
            
            if ( supportedSpectators >= 0 )
            {
                // we can at least handle the players, reduce spectator count
                maxSpectators.Set( supportedSpectators );
            }
            else
            {
                maxSpectators.Set( 0 );

#ifdef COMPILE_DEDCON
                if ( !serverInfo.unstoppable )
                {
                    // check how many players we can support
                    int supportedTeams = totalBandwidth / playerBandwidthReal;
                    
                    // check team count
                    if ( teams > supportedTeams )
                    {
                        maxTeams.Set( supportedTeams );
                    }
                }
#endif
            }
        }
    }

#ifndef COMPILE_DEDCON
    // kick out demo clients
    if ( demoLimits.demoRestricted )
    {
        for( Client::iterator client = Client::GetFirstClient(); client; ++client )
        {
            if ( !client->IsOnLAN() && client->id.GetDemo() )
            {
                client->Quit( SuperPower::Demo );
            }
        }
    }
#endif
}

//! runs checks on setting changes
void Settings::DoRunChecks()
{
    CheckRestrictions();
    SendSettings();
    gameHistory.Check();
    Client::OnSettingChange();
}

//! returns a list of important settings
Setting * * Settings::DoGetImportantSettings()
{
    return GetNetworkSettings();
}

Setting * * Settings::GetNetworkSettings()
{
    static std::string lastVersion = "xcvf,fasf"; // guaranteed to be no legal version
    static int lastGameMode = -1;

    // on change of version or game mode, the order of network settings changes
    if ( lastVersion != serverVersion.Get() || lastGameMode != gameMode.Get() )
    {
        // clear
        memset( networkSettings, 0, sizeof( networkSettings ) );

        // and fill
        SettingsFiller filler;

#ifdef COMPILE_DEDCON
        filler.Add( serverName );

        filler.Add( advertiseOnInternet        ); // 1
        filler.Add( advertiseOnLAN             ); // 2

        filler.Add( gameMode                   ); // 3

        filler.Add( maxTeams                   ); // 4
        filler.Add( territoriesPerTeam         ); // 5
        filler.Add( citiesPerTerritory         ); // 6
        filler.Add( populationPerTerritory     ); // 7

        // this setting was added in 1.4 and shifts all later
        // settings one slot down. Unfortunately, it was not present
        // in the 1.42 release candidate, so clients of this version
        // are fundamentally incompatible with this server, since there is
        // no way to distinguish them from 1.42 final.
        if ( CompareVersions( "1.4 beta1", serverVersion ) <= 0 )
            filler.Add( cityPopulations            ); // 8

        filler.Add( randomTerritories          ); // 9

        filler.Add( permitDefection            ); // 10
        filler.Add( radarSharing               ); // 11

        filler.Add( gameSpeed                  ); // 12
        filler.Add( slowestSpeed               ); // 13
        filler.Add( scoreMode                  ); // 14
#ifdef NOTVANILLA
        filler.Add( victoryMode                ); // 14.5
#endif

        filler.Add( victoryTrigger             ); // 15
        filler.Add( victoryTimer               ); // 16
        filler.Add( maxGameRealTime            ); // 17

        filler.Add( variableUnitCounts         ); // 18
        filler.Add( worldScale                 ); // 19

        filler.Add( maxSpectators              ); // 20
        filler.Add( spectatorChatChannel       ); // 21
        filler.Add( teamSwitching              ); // 22

        filler.Add( serverPassword             ); // 23
#ifdef NOTVANILLA
        filler.Add( cityBuffer                 ); // 20
        filler.Add( unitBuffer                 ); // 20
        filler.Add( firstStrikeBonus           ); // 22
        filler.Add( worldUnitScale             ); // 20
        filler.Add( fighterBehavior            ); // 27
        filler.Add( bomberBehavior             ); // 28
        filler.Add( nukeTrajectoryCalc         ); // 29
        filler.Add( emptyBomberLaunch          ); // 21
#endif
#endif

#ifdef COMPILE_DEDWINIA
        filler.Add( gameMode );
        filler.Add( aiLevel );
        filler.Add( basicCrates );
        filler.Add( coopMode );

        switch (gameMode)
        {
        case 0:
            filler.Add( scoreMode );
            filler.Add( timeLimit );
            filler.Add( handicap );
            filler.Add( crateDropTimer );
            filler.Add( startingPowerups );
            filler.Add( retributionMode );
            filler.Add( suddenDeath );
            filler.Add( crateDropMode );
            break;
        case 1:
            filler.Add( scoreMode );
            filler.Add( timeLimit );
            filler.Add( handicap );
            filler.Add( crateDropTimer );
            filler.Add( startingPowerups );
            filler.Add( retributionMode );
            filler.Add( suddenDeath );
            filler.Add( crateDropMode );
            break;
        case 2:
            filler.Add( timeLimit );
            filler.Add( crateDropTimer );
            filler.Add( retributionMode );
            filler.Add( suddenDeath );
            filler.Add( crateDropMode );
            break;
        case 3:
            filler.Add( handicap );
            filler.Add( crateDropTimer );
            break;
        case 4:
            filler.Add( crateDropTimer );
            filler.Add( reinforcementTimer );
            filler.Add( reinforcementCount );
            filler.Add( maxArmour );
            filler.Add( turretFrequency );
            filler.Add( maxTurrets );
            break;
        case 5:
            filler.Add( timeLimit );
            filler.Add( crateDropTimer );
            filler.Add( reinforcementTimer );
            filler.Add( reinforcementCount );
            filler.Add( maxArmour );
            filler.Add( turretFrequency );
            filler.Add( retributionMode );
            filler.Add( suddenDeath );
            filler.Add( maxTurrets );
            break;
        default:
            // unknown game mode
        {
            static bool warn = true;
            if ( warn )
            {
                Log::Err()  << "Unknown game mode, no settings available.\n";
                warn = false;
            }
        }
        }

        filler.Add( mapFile );
        filler.Add( mapHash );
#endif

        lastGameMode = gameMode;
        lastVersion = serverVersion;
    }

    return networkSettings;
}

//! query whether demo players should be locked out
bool Settings::DemoRestricted() const
{

#ifdef COMPILE_DEDCON
    if (!gameMode.IsDefault())
        return true;
    if (!scoreMode.IsDefault())
        return true;
    if (!serverMods.IsDefault())
        return true;
#endif

#ifdef COMPILE_DEDWINIA
    if ( Map::DemoRestricted() )
    {
        return true;
    }
#endif

    return demoRestricted >= 2;
}

//! apply locked settings in the various game modes
void Settings::LockSettingsAccordingToGameMode()
{
#ifdef COMPILE_DEDCON
    // nothing to do at all
    if ( !Setting::AnythingChanged() )
        return;

    switch( (int)gameMode )
    {
#if VANILLA
        case 0: // default
            radarSharing.Revert();
            gameSpeed.Revert();
            variableUnitCounts.Revert();
            worldScale.Revert();
            teamSwitching.Revert();
            break;
        case 1: // office
            gameSpeed.Set( 1 );
            slowestSpeed.Revert();
            teamSwitching.Revert();
            victoryTimer.Set( 60 );
            maxGameRealTime.Set( 360 );
            break;
        case 2: // speed
            gameSpeed.Set( 4 );
            slowestSpeed.Set( 4 );
            teamSwitching.Revert();
            maxGameRealTime.Set( 15 );
            break;
        case 3: // diplo
            scoreMode.Set( 1 );
            radarSharing.Revert();
            permitDefection.Revert();
            teamSwitching.Revert();
            break;
        case 4: // big
            citiesPerTerritory.Set( 40 );
            worldScale.Set( 200 );
            teamSwitching.Revert();
            break;
        case 5: // tournament
            // lock everything except TerritoriesPerTeam and GameMode
            citiesPerTerritory.Revert();
            populationPerTerritory.Revert();
            cityPopulations.Revert();

            randomTerritories.Set( 1 );

            permitDefection.Revert();
            radarSharing.Revert();

            gameSpeed.Revert();
            slowestSpeed.Revert();

            // scoreMode.Revert();

            victoryTrigger.Revert();
            victoryTimer.Revert();
            maxGameRealTime.Revert();

            variableUnitCounts.Revert();
            worldScale.Revert();

            if ( maxSpectators < 10 )
                maxSpectators.Set( 10 );

            spectatorChatChannel.Set( 0 );
            teamSwitching.Revert();

            break;
        case 6: // custom
            break;
        }
#endif
#ifdef NOTVANILLA
        case 0: // default
            radarSharing.Revert();
            gameSpeed.Revert();
            variableUnitCounts.Revert();
            worldScale.Revert();
            teamSwitching.Revert();
            break;
        case 1: // office
            gameSpeed.Set( 1 );
            slowestSpeed.Revert();
            teamSwitching.Revert();
            victoryTimer.Set( 60 );
            maxGameRealTime.Set( 360 );
            break;
        case 2: // speed
            gameSpeed.Set( 4 );
            slowestSpeed.Set( 4 );
            teamSwitching.Revert();
            maxGameRealTime.Set( 15 );
            break;
        case 3: // diplo
            scoreMode.Set( 1 );
            radarSharing.Revert();
            permitDefection.Revert();
            teamSwitching.Revert();
            break;
        case 4: // bastard diplo
            scoreMode.Set( 0 );
            radarSharing.Revert();
            permitDefection.Revert();
            teamSwitching.Revert();
            break;
        case 5: // big
            citiesPerTerritory.Set( 40 );
            worldScale.Set( 200 );
            worldUnitScale.Set( 50 );
            teamSwitching.Revert();
            break;
        case 6: // tournament
            // lock everything except TerritoriesPerTeam and GameMode
            citiesPerTerritory.Revert();
            populationPerTerritory.Revert();
            cityPopulations.Revert();

            randomTerritories.Set( 1 );

            permitDefection.Revert();
            radarSharing.Revert();

            gameSpeed.Revert();
            slowestSpeed.Revert();

            // scoreMode.Revert();

            victoryTrigger.Revert();
            victoryTimer.Revert();
            maxGameRealTime.Revert();

            variableUnitCounts.Revert();
            worldScale.Revert();
            worldUnitScale.Revert();

            if ( maxSpectators < 10 )
                maxSpectators.Set( 10 );

            spectatorChatChannel.Set( 0 );
            teamSwitching.Revert();

            break;
        case 7: // custom
            break;
        }
#endif

    static int oldGameMode = -1;
    if ( gameMode != oldGameMode )
    {
        oldGameMode = gameMode;
        // mark all settings to be resent over the network.
        Setting * * run = GetNetworkSettings();
        while ( *run )
        {
            Setting * s = *run;
            s->Change();
            run++;
        }
    }

    Setting::ResetAnythingChanged();
#endif
}

int Settings::MinSpeed() const
{
#ifdef COMPILE_DEDCON
    switch( slowestSpeed.Get() )
    {
    case 0:
        return 0;
    case 1:
        return 1;
    case 2:
        return 5;
    case 3:
        return 10;
    case 4:
        return 20;
    case 5:
        return 50;
    case 6:
        return 100;
    default:
        return 255;
    }
#else
    return 1;
#endif
}

void Settings::WriteServerInfo( TagWriter & writer ) const
{
    char const * serverGame = GAME_NAME;

    writer.WriteString( C_METASERVER_GAME, serverGame );
    writer.WriteString( C_METASERVER_VERSION, serverVersion );
}

//! send settings to all attached clients
void Settings::SendSettings()
{
    // don't do this during playback
    if ( gameHistory.Playback() )
        return;

#ifdef COMPILE_DEDCON
    // warnings about unconventional settings
    static bool lastPermitDefection = true;
    static bool lastTeamSwitching = false;

    // Cast permitDefection to bool for comparison
    if ( lastPermitDefection != static_cast<bool>(permitDefection) )
    {
        if ( !permitDefection )
            Host::GetHost().Say( "Warning: defection has been turned off, you cannot make or break alliances after the game started." );
        else
            Host::GetHost().Say( "Warning: Defection enabled again." );

        lastPermitDefection = static_cast<bool>(permitDefection);  // Ensure same type assignment
    }

    if ( lastTeamSwitching != static_cast<bool>(teamSwitching) )
    {
        if ( teamSwitching )
            Host::GetHost().Say( "Warning: Team switching is enabled. Everyone, even spectators, will be able to control any unit in the game." );
        else
            Host::GetHost().Say( "Warning: Team switching disabled again." );

        lastTeamSwitching = static_cast<bool>(teamSwitching);  // Ensure same type assignment
    }
#else
    // Dedwinia: no settings transfer without a map
    if ( mapFile.IsDefault() )
    {
        return;
    }
#endif

    Setting * * run = GetNetworkSettings();
    int index = 0;
    bool unready = true;

    bool force = false;
#ifdef COMPILE_DEDWINIA
    // game mode changes shuffle settings around. Bummer. We need to resend them all every time.
    static int lastGameMode = -1;
    force = ( lastGameMode != gameMode );
    if ( force )
    {
        Setting::lastSentRevision = Setting::currentRevision;
        Setting::currentRevision++;
    }
    lastGameMode = gameMode;
#endif

    while ( *run )
    {
        Setting * s = *run;

        if ( ( s->needToSend || force ) 
#ifdef COMPILE_DEDWINIA
             && ( !serverInfo.started || ( s != &mapFile && s != &coopMode ) )
#endif
            )
        {
            if ( unready )
            {
                if ( !serverInfo.unstoppable )
                {
                    // setting changes should stop the game countdown
#ifdef COMPILE_DEDCON
                    SuperPower::UnreadyAll();
#endif
                }
                else
                {
                    while ( !serverInfo.started )
                    {
                        // unless the countdown cannot be stopped any more. In that case, proceed,
                        // but bump the seqID counter so that the game has really started (otherwise,
                        // the clients may unready themselves)
                        gameHistory.Bump();
                    }
                }
            }
            unready = false;

#ifdef COMPILE_DEDCON
            AutoTagWriter writer(C_TAG, C_CONFIG);
            writer.WriteChar( C_CONFIG_ID, index );
            s->DoWrite( writer );
            s->needToSend = false;

            // strange, but this needs to be treated as a client command. I wonder...
            gameHistory.RequestBump();
            gameHistory.AppendClient( writer );
#endif

#ifdef COMPILE_DEDWINIA
            char const * tag = C_CONFIG;
            bool writeID = true;
      
            if ( s->GetNumericName() != 1 )
            {
                writeID = false;
                switch ( s->GetClass() )
                {
                case Setting::Generic:
                    writeID = true;
                    break;
                case Setting::String:
                    tag = C_CONFIG_STRING;
                    break;
                case Setting::Enum:
                    tag = C_CONFIG_INT;
                    break;
                }
            }

            AutoTagWriter writer(C_TAG, tag);
            writer.WriteInt( C_CONFIG_REVISION, Setting::currentRevision );
            Setting::lastSentRevision = Setting::currentRevision;
            writer.WriteInt( C_CONFIG_NUMERIC_NAME, s->GetNumericName() );

            if ( writeID )
            {
                if ( force && index == 0 )
                {
                    // put the advanced settings into the next sequence
                    // so the clients have time to adapt to the game mode change
                    gameHistory.Bump();
                }
                writer.WriteInt( C_CONFIG_ID, index );
            }

            s->DoWrite( writer );
            s->needToSend = false;

            // strange, but this needs to be treated as a client command. I wonder...
            gameHistory.RequestBump();
            gameHistory.AppendClient( writer );
#endif
        }

#ifdef COMPILE_DEDWINIA
        if ( s->GetNumericName() == 1 )
#endif
        {
            index++;
        }

        run++;
    }
}


Settings settings;

std::string const & Settings::GetServerKey()
{
    static std::string lastKey = "/";
    if ( lastKey == serverKey.Get() )
    {
        return serverKey.Get();
    }
    lastKey = serverKey.Get();

    // last fallback: one random demo key
    if ( serverKey.Get().size() == 0 )
    {
        serverKey.Set( DemoKeygen().Generate() );
    }
    
    std::string const & key = serverKey.Get();
    SetServerKey( key );

    // check key format
    if ( key.size() != 31 )
    {
        Log::Err() << "Probably malformated ServerKey. A key should be 31 characters long, and \"" << key << "\" is " << key.size() << " characters long.\n";
    }
    
    // int checksum = 0;
    for( unsigned int i = 0; i < key.size(); ++i )
    {
        if ( ( i % 7 ) == 6 )
        {
            if ( key[i] != '-' )
            {
                Log::Err() << "Probably malformated ServerKey \"" << key << "\". A key should have a hyphen (-) at position " << i+1 << ". Generally, a hyphen should comeafter each group of six letters.\n";
                break;
            }
        }
        else
        {
            if ( !isupper(key[i]) )
            {
                Log::Err() << "Probably malformated ServerKey \"" << key << "\". A key should have a capital letter at position " << i+1 << ". Generally, a key should be four groups of six capital letters each, separated by hpyhens, and followed by three more capital letters.\n";
                break;
            }
        }
    }

    return serverKey.Get();
}

void Settings::SetServerKey( std::string const & key )
{
    // normalize serverKey by removing all whitespace
    std::istringstream in( key );
    std::ws( in );
    std::string normalized;
    in >> normalized;

    serverKey.Set( normalized );
}

bool Settings::ServerKeyDefault()
{
    return serverKey.IsDefault();
}

class WaitTimePending: public PendingCommandGame
{
public:
    Time time;
    WaitTimePending( Time const & t ): time( t ){}

    void Try()
    {
        Time current;
        current.GetTime();

        // we're already waiting
        int toWait = ( time - current ).Seconds();
        if ( toWait > 0 )
        {
            TryAgainLater( toWait );
        }
    }
};

//! waits some seconds of real time
class WaitTimeSetting: public IntegerActionSetting
{
public:
    WaitTimeSetting( char const * name, bool fromStart_ ):IntegerActionSetting( Flow, name ), fromStart( fromStart_ )
    {
    }

    void Activate( int val )
    {
        Time current;
        if ( fromStart )
        {
            current = Time::GetStartTime();
        }
        else
        {
            current.GetTime();
        }

        // store time to wait
        current.AddSeconds(val);
        SettingsReader::AddPending( new WaitTimePending( current ) );
    }
private:
    bool fromStart;
};

static WaitTimeSetting waitTime("WaitSeconds", false );
static WaitTimeSetting waitTimeFromStart("WaitSecondsFromStart", true );

class WaitGameTimePending: public PendingCommandGame
{
public:
    int time; // game ticks to wait for
    WaitGameTimePending( int t ): time( t ){}

    void Try()
    {
        int current = gameHistory.GameTime();

        // we're already waiting, this many 1/10 seconds left
        int toWait = time - current;
        if ( toWait > 0 )
        {
            TryAgainLater( toWait/(10*255) );
        }
    }
};

#ifdef COMPILE_DEDCON
#define TICKS_PER_GAME_SECOND 10
#endif

#ifdef COMPILE_DEDWINIA
#define TICKS_PER_GAME_SECOND 11
#endif


//! waits some seconds of game time
class WaitGameTimeSetting: public IntegerActionSetting
{
public:
    WaitGameTimeSetting( char const * name ):IntegerActionSetting( Flow, name )
    {
    }

    void Activate( int val )
    {
        int current = gameHistory.GameTime();

        SettingsReader::AddPending( DoCreateCustomData( current, val ) );
    }
protected:
    // creates custom wait data
    virtual WaitGameTimePending * DoCreateCustomData( int currentGameTime, int userValue )
    {
        return new WaitGameTimePending( currentGameTime + userValue * TICKS_PER_GAME_SECOND );
    }
};

#ifdef COMPILE_DEDCON
static WaitGameTimeSetting waitGameTime("WaitGameSeconds");
#endif

//! waits some seconds of game time
class WaitGameTimeAbsoluteSetting: public WaitGameTimeSetting
{
public:
    WaitGameTimeAbsoluteSetting( char const * name ):WaitGameTimeSetting( name )
    {
    }

protected:
    // creates custom wait data
    virtual WaitGameTimePending * DoCreateCustomData( int currentGameTime, int userValue )
    {
#ifdef COMPILE_DEDWINIA
        if ( userValue < 0 && settings.timeLimit.Get() > 0 )
        {
            userValue = settings.timeLimit.Get() * 60 + userValue;
        }
#endif

        return new WaitGameTimePending( userValue * TICKS_PER_GAME_SECOND );
    }
};

static WaitGameTimeAbsoluteSetting waitGameTimeAbsolute("WaitGameSecondsAbsolute");

#ifdef COMPILE_DEDCON
//! waits some seconds of game time
class WaitDefconSetting: public WaitGameTimeSetting
{
public:
    WaitDefconSetting( char const * name ):WaitGameTimeSetting( name )
    {
    }

protected:
    // creates custom wait data
    virtual WaitGameTimePending * DoCreateCustomData( int currentGameTime, int userValue )
    {
        // minutes to wait for
        int minutes = 0;
        switch ( userValue )
        {
            case 5:
                minutes = 0;
                break;
            case 4:
                minutes = 6;
                break;
            case 3:
                minutes = 12;
                break;
            case 2:
                minutes = 20;
                break;
            case 1:
                minutes = 30;
                break;
        default:
            Log::Err() << "Invalid Defcon level " << userValue << ", needs to be between 1 and 5.\n";
        }

        // convert to 1/10 seconds
        return new WaitGameTimePending( minutes * 600 );
    }
};

static WaitDefconSetting waitDefcon("WaitDefcon");
#endif

//! waits for the game start
class WaitStartSetting: public ActionSetting
{
public:
    WaitStartSetting():ActionSetting( Flow, "WaitStart" )
    {
    }

    //! reads the value of the setting from a stream.
    virtual void DoRead( std::istream & source )
    {
        if ( serverInfo.started )
        {
            // game has started, go on
            return;
        }
        else
        {
            // wait a second
            PendingCommandGame::TryAgainLater( 1 );
        }
    }
};

static WaitStartSetting waitStart;

//! waits for the game end
class WaitEndSetting: public ActionSetting
{
public:
    WaitEndSetting():ActionSetting( Flow, "WaitEnd" )
    {
    }

    //! reads the value of the setting from a stream.
    virtual void DoRead( std::istream & source )
    {
        if ( serverInfo.ended )
        {
            // game has ended, go on
            return;
        }
        else
        {
            // wait a second
            PendingCommandGame::TryAgainLater( 1 );
        }
    }
};

static WaitEndSetting waitEnd;

//! waits for someone to go online
class WaitClientSetting: public ActionSetting
{
public:
    WaitClientSetting():ActionSetting( Flow, "WaitClient" )
    {
    }

    //! reads the value of the setting from a stream.
    virtual void DoRead( std::istream & source )
    {
        // read number of players to wait for
        int numWait = 1;
        std::ws( source );
        if ( !source.eof() )
        {
            source >> numWait;
        }

        int count = 0;
        Client::iterator i = Client::GetFirstClient();
        while ( i )
        {
            if ( i->id.NameSet() )
            {
                ++count;
            }
            ++i;
        }

        if ( count >= numWait )
        {
            // enough players. Go on!
            return;
        }

        // wait a second
        PendingCommandGame::TryAgainLater( 1 );
    }
};

static WaitClientSetting waitClient;

//! loads a configuration file and forks it
class ForkSetting: public ActionSetting
{
public:
    ForkSetting():ActionSetting( Flow, "Fork" )
    {
    }

    //! reads the file to include
    virtual void DoRead( std::istream & source )
    {
        SettingsReader * reader = SettingsReader::GetActive();

        // check if context is right
        if ( !reader )
        {
            reader = &SettingsReader::GetSettingsReader();
        }

        std::string filename;
        std::ws(source);
        getline( source, filename );

        // spawn a child reader and make it read the file
        SettingsReader * child = reader->Spawn();
        child->Read( filename.c_str() );
    }
};

static ForkSetting forkSetting;

NestedPending::NestedPending(){}

void NestedPending::Try()
{
    // apply settings from reader
    reader.Apply();
    
    // done?
    if ( reader.LinesLeft() != 0 )
    {
        // no, try again
        Time current;
        current.GetTime();
        PendingCommandGame::TryAgainLater( (reader.TryAgain() - current).Seconds() );
    }
    else
    {
        // yes, wrap things up
        reader.TransferChildren( *SettingsReader::GetActive() );
    }
}

//! loads a configuration file and executes it immediately
class IncludeSetting: public ActionSetting
{
public:
    IncludeSetting():ActionSetting( Flow, "Include" )
    {
    }

    //! reads the file to include
    virtual void DoRead( std::istream & source )
    {
        // first call, create custom data containing the nested reader
        NestedPending * pending = new NestedPending();

        // read the filename
        std::string filename;
        std::ws(source);
        getline( source, filename );

        // read the file to include
        pending->reader.Read( filename.c_str() );

        SettingsReader::AddPending( pending );
    }
};

static IncludeSetting includeSetting;

//! quits the server
class QuitSetting: public ActionSetting
{
public:
    QuitSetting():ActionSetting( Flow, "Quit" )
    {
    }

    //! reads the value of the setting from a stream.
    virtual void DoRead( std::istream & source )
    {
        for ( Client::iterator c = Client::GetFirstClient(); c; ++c )
        {
            c->Quit( SuperPower::ServerQuit );
        }

        throw QuitException( 0 );
    }
};

static QuitSetting quit;

void MapFileSetting::OnChange()
{
    StringSetting::OnChange();
#ifdef COMPILE_DEDWINIA
    Map::UpdateHash();
#endif
}

// save and restore map value in lastValue field
void MapFileSetting::Save()
{
    netValue = lastValue = Get();
}

// save and restore map value in lastValue field
void MapFileSetting::SetNet( std::string const & value )
{
    netValue = value;
}

void MapFileSetting::Restore()
{
    static bool recursion = false;
    if ( ! recursion )
    {
        recursion = true;
        Set( lastValue );
        recursion = false;
    }
}

//! writes the setting value to the tag
void MapFileSetting::DoWrite( TagWriter & writer ) const
{
    writer.WriteString( C_CONFIG_VALUE, netValue );
}

MapFileSetting::MapFileSetting( Type type, char const * name, char const * def )
: StringSetting( type, name, def )
  , lastValue( def )
{
}

class DryRun: public ActionSetting
{
public:
    DryRun(): ActionSetting(  Setting::Internal, "DryRun" ) {}
    
    virtual void DoRead( std::istream & )
    {
        settings.dryRun = true;
    }
};

static DryRun dryRun;

