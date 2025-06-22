/*
 *  * DedCon: Dedicated Server for Defcon (and Multiwinia)
 *  *
 *  * Copyright (C) 2008 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_GameSettings_INCLUDED
#define DEDCON_GameSettings_INCLUDED

#include "Settings.h"
#include "Main/config.h"

//! map file setting with special action after setting it
class MapFileSetting: public StringSetting
{
public:
    MapFileSetting( Type type, char const * name, char const * def = "" );

    // save and restore map value in lastValue field
    void Save();
    void Restore();
    void SetNet( std::string const & netValue );
private:
    virtual void OnChange();

    //! writes the setting value to the tag
    virtual void DoWrite( TagWriter & writer ) const;

    std::string lastValue; //<! last safe value
    std::string netValue;  //<! value sent to the clients
};

class GameModeSetting: public EnumSetting
{
public:
    GameModeSetting();

    //! adapts other settings to game mode changes
    virtual void OnChange();
};

//! all important settings combined
struct Settings: public SettingHooks
{
    // if true, no real game should be hosted; instead,
    // all configuration files should be given a test run.
    bool dryRun;

    // first, the amply known game settings
    StringSetting serverName;

    IntegerSetting advertiseOnInternet;
    IntegerSetting advertiseOnLAN;

    GameModeSetting gameMode;

    IntegerSetting maxTeams;
    IntegerSetting minTeams;

#ifdef COMPILE_DEDCON
    IntegerSetting territoriesPerTeam;
    IntegerSetting citiesPerTerritory;
    IntegerSetting populationPerTerritory;
    IntegerSetting cityPopulations;

    IntegerSetting randomTerritories;

    IntegerSetting permitDefection;
    IntegerSetting radarSharing;

    IntegerSetting gameSpeed;
    IntegerSetting slowestSpeed;
    IntegerSetting scoreMode;
#ifdef NOTVANILLA
    IntegerSetting victoryMode;
#endif

    IntegerSetting victoryTrigger;
    IntegerSetting victoryTimer;
    IntegerSetting maxGameRealTime;

    IntegerSetting variableUnitCounts;
    IntegerSetting worldScale;


    IntegerSetting teamSwitching;

    IntegerSetting spectatorChatChannel;

    StringSetting  serverMods;    //!< the mods run by the server
#endif
    SecretStringSetting serverPassword;
#ifdef NOTVANILLA
    IntegerSetting cityBuffer;
    IntegerSetting unitBuffer;
    IntegerSetting firstStrikeBonus;
    IntegerSetting worldUnitScale;
    IntegerSetting fighterBehavior;
    IntegerSetting bomberBehavior;
    IntegerSetting nukeTrajectoryCalc;
    IntegerSetting emptyBomberLaunch;
#endif

    IntegerSetting maxSpectators;
    // SecretStringSetting adminPassword;

    IntegerSetting serverPort;    //!< the port the server will listen on
    StringSetting  serverLANIP;   //!< the IP of the server in the LAN
    StringSetting  serverVersion; //!< the version the server pretends to run
    /*
    StringSetting  serverGame;    //!< the game the server pretends to be playing

    StringSetting  metaServer;    //!< DNS address of the metaserver
    IntegerSetting metaServerPort;//!< port of the metaserver
    */

    IntegerSetting playerBandwidth; //!< maximal bandwidth spent for each player
    IntegerSetting spectatorBandwidth; //!< maximal bandwidth spent for each spectator
    IntegerSetting minBandwidth;    //!< minimal bandwidth allocated for every client, even spectators
    IntegerSetting totalBandwidth;  //!< total bandwidth available

    IntegerSetting maxPacketSize;   //!< the maximal size of a data packet (approximate)

    StringSetting forbiddenVersions; //!< semicolon separated list of client versions that will be instantly kicked.
    StringSetting noPlayVersions;   //!< semicolon separated list of client versions that will not be allowed to play

    IntegerSetting demoRestricted;  //!< allows the admin to make the server demo restricted just because he feels like it.

    IntegerSetting allStarTime;

#ifdef COMPILE_DEDWINIA
    MapFileSetting mapFile;  //!< the file name name of the map file to play on
    EnumSetting mapHash;     //!< the map hash
    StringSetting  mapName;  //!< the full name of the map file to play on
    EnumSetting    aiLevel;  //!< skill of the AI

    EnumSetting    crateDropMode;         //!< crate drop mode (?)
    IntegerSetting crateDropTimer;        //!< average time in seconds between crate drops
    IntegerSetting handicap;              //!< handicap spawn bonus for players in distress
    IntegerSetting maxArmour;             //!< maximal number of armours a player can have at one time
    IntegerSetting maxTurrets;            //!< maximal number of gun turrets a player can have at one time
    IntegerSetting numTanks;              //!< number of tanks (?) 
    IntegerSetting reinforcementCount;    //!< number of DGs a player receives each reinforcement wave
    IntegerSetting reinforcementTimer;    //!< time between reinforcement waves
    BoolSetting    retributionMode;       //!< retribution mode, gives eliminated players something to do
    EnumSetting    scoreMode;             //!< score mode games
    IntegerSetting startingPowerups;      //!< number of powerups a player starts with
    BoolSetting    suddenDeath;           //!< sudden death mode to avoid draws
    IntegerSetting timeLimit;             //!< game time limit in minutes
    IntegerSetting turretFrequency;       //!< number of reinforcement waves between turret powerups
    BoolSetting    basicCrates;           //!< whether only basic crates should drop
    BoolSetting    coopMode;              //!< cooperative mode
    IntegerSetting numAttackers;          //!< number of attacking teams in Assault mode

    enum { teamSwitching = false };
    enum { spectatorChatChannel = true };
#endif

    Settings();

    //! checks restrictions
    void CheckRestrictions();

    //! send settings to all attached clients
    void SendSettings();

    //! apply demo restrictions
    void RestrictDemo();

    //! returns the server key
    std::string const & GetServerKey();
    void SetServerKey( std::string const & key );
    bool ServerKeyDefault();

    //! gets the minimal speed factor
    int MinSpeed() const;

    //! writes the server info to a tag
    void WriteServerInfo( TagWriter & writer ) const;

    //! query whether demo players should be locked out
    bool DemoRestricted() const;

    //! apply locked settings in the various game modes
    void LockSettingsAccordingToGameMode();

    //! get a null-terminated list of network aware settings in the correct order
    Setting * * GetNetworkSettings();

private:
    SecretStringSetting  serverKey;     //!< the authentication key of the server

    //! returns a list of important settings
    virtual Setting * * DoGetImportantSettings();

    //! runs checks on setting changes
    virtual void DoRunChecks();
};

extern Settings settings;

#endif // DEDCON_GameSettings_INCLUDED
