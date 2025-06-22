/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifdef COMPILE_DEDWINIA
#include "config.h"

#include "Map.h"
#include "GameSettings.h"
#include "Log.h"
#include "Globals.h"
#include "Client.h"
#include "GameHistory.h"
#include "Advertiser.h"
#include "CPU.h"
#include "FTP.h"

#include "introversion/crc.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <map>

#ifdef HAVE_ZLIB_H
#include "TagReader.h"
#include "TagParser.h"
#include "DumpBuff.h"
#include <zlib.h>

class TagParserMap: public TagParserLax
{
public:
    virtual void OnRaw( std::string const & key, unsigned char const * data, int len )
    {
        // Log::Out() << key << " = (" << len << ")\n";
        
        if ( key == "MapData" )
        {
            map = std::string( (char const *) data, len );
        }
    }

    virtual void OnInt( std::string const & key, int data )
    {
        // Log::Out() << key << " = " << data << "\n";
    }

    virtual void OnString( std::string const & key, std::string const & data )
    {
        // Log::Err() << key << " = " << data << "\n";
    }

    virtual void OnNested( TagReader & reader )
    {
    }

    std::string map;
};

std::string ExtractMap( std::string const & compressed )
{
    unsigned char const * data = (unsigned char const *)compressed.c_str();
    size_t len = compressed.size();

    if ( len <= 4 )
    {
        Log::Err() << "Error during uncompression: .mwm file too short.\n";
        return "";
    }

    // read uncompressed data size
    size_t uncompressedSize = *(data++);
    uncompressedSize += *(data++) << 8;
    uncompressedSize += *(data++) << 16;
    uncompressedSize += *(data++) << 24;
    len -= 4;

    // allocate buffer
    unsigned char * buffer = new unsigned char[ uncompressedSize ];
    
    // uncompress
    uLongf realsize = uncompressedSize;
    int err = uncompress( (Bytef*)buffer, &realsize, (Bytef*)data, len );
    if ( err != Z_OK || uncompressedSize != realsize )
    {
        Log::Err() << "Error during uncompression: " << err << ", " << realsize << ", " << uncompressedSize << ".\n";
        delete[] buffer;
        return "";
    }
    // Log::Out() << "Uncompression: " << err << ", " << realsize << ", " << uncompressedSize << ".\n";

    TagReader reader( buffer, realsize );
    TagParserMap parser;
    parser.Parse( reader );
    reader.Obsolete();

    delete[] buffer;

    return parser.map;
}
#endif

static bool IsCustomMap( std::string const & mapFile )
{
    return mapFile.size() < 4 || ( std::string( mapFile, mapFile.size() - 4 ) == ".mwm" );
}

StringSetting mapPrefix( Setting::GameOption, "MapPrefix" );

void Map::SetHash( int hash )
{
    settings.mapHash.Set( hash );
}

bool GetHashDatabase( std::string & map )
{
    bool found = false;
    unsigned int hash = 0;
    int gameMode = -1, maxTeams = -1, timeLimit = 0, coop = 0, forceCoop = 0, attackers = 0;

    // BEGIN AUTOGEN

    if( 0 == strcasecmp( map.c_str(), "mp_assault_2P_1.txt" ) ) {  map = "mp_assault_2P_1.txt"; found = true; hash = 3570215542u; gameMode = 3; maxTeams = 2; timeLimit = 15; attackers = 1; }

    if( 0 == strcasecmp( map.c_str(), "mp_assault_2P_2.txt" ) ) {  map = "mp_assault_2P_2.txt"; found = true; hash = 2881474743u; gameMode = 3; maxTeams = 2; timeLimit = 15; attackers = 1; }

    if( 0 == strcasecmp( map.c_str(), "mp_assault_2p_3.txt" ) ) {  map = "mp_assault_2p_3.txt"; found = true; hash = 1456120637u; gameMode = 3; maxTeams = 2; timeLimit = 15; attackers = 1; }

    if( 0 == strcasecmp( map.c_str(), "mp_assault_3p_1.txt" ) ) {  map = "mp_assault_3p_1.txt"; found = true; hash = 3042704207u; gameMode = 3; maxTeams = 2; timeLimit = 15; attackers = 1; }

    if( 0 == strcasecmp( map.c_str(), "mp_assault_3P_2.txt" ) ) {  map = "mp_assault_3P_2.txt"; found = true; hash = 2262858796u; gameMode = 3; maxTeams = 3; timeLimit = 15; coop = 1; forceCoop = 1; attackers = 2; }

    if( 0 == strcasecmp( map.c_str(), "mp_assault_3p_3.txt" ) ) {  map = "mp_assault_3p_3.txt"; found = true; hash = 102234836u; gameMode = 3; maxTeams = 3; timeLimit = 15; coop = 1; forceCoop = 1; attackers = 2; }

    if( 0 == strcasecmp( map.c_str(), "mp_assault_4P_1.txt" ) ) {  map = "mp_assault_4P_1.txt"; found = true; hash = 4188331124u; gameMode = 3; maxTeams = 4; timeLimit = 15; coop = 1; forceCoop = 1; attackers = 2; }

    if( 0 == strcasecmp( map.c_str(), "mp_assault_4P_2.txt" ) ) {  map = "mp_assault_4P_2.txt"; found = true; hash = 2589260504u; gameMode = 3; maxTeams = 4; timeLimit = 20; coop = 1; forceCoop = 1; attackers = 2; }

    if( 0 == strcasecmp( map.c_str(), "mp_blitzkrieg_2p_1.txt" ) ) {  map = "mp_blitzkrieg_2p_1.txt"; found = true; hash = 1601026241u; gameMode = 5; maxTeams = 2; timeLimit = 15; }

    if( 0 == strcasecmp( map.c_str(), "mp_blitzkrieg_2p_2.txt" ) ) {  map = "mp_blitzkrieg_2p_2.txt"; found = true; hash = 3110880239u; gameMode = 5; maxTeams = 2; timeLimit = 15; }

    if( 0 == strcasecmp( map.c_str(), "mp_blitzkrieg_2P_3.txt" ) ) {  map = "mp_blitzkrieg_2P_3.txt"; found = true; hash = 3723417187u; gameMode = 5; maxTeams = 2; timeLimit = 15; }

    if( 0 == strcasecmp( map.c_str(), "mp_blitzkrieg_2P_4.txt" ) ) {  map = "mp_blitzkrieg_2P_4.txt"; found = true; hash = 4247601929u; gameMode = 5; maxTeams = 2; timeLimit = 15; }

    if( 0 == strcasecmp( map.c_str(), "mp_blitzkrieg_3P_1.txt" ) ) {  map = "mp_blitzkrieg_3P_1.txt"; found = true; hash = 646201242u; gameMode = 5; maxTeams = 3; timeLimit = 15; }

    if( 0 == strcasecmp( map.c_str(), "mp_blitzkrieg_3P_2.txt" ) ) {  map = "mp_blitzkrieg_3P_2.txt"; found = true; hash = 1418983205u; gameMode = 5; maxTeams = 3; timeLimit = 15; }

    if( 0 == strcasecmp( map.c_str(), "mp_blitzkrieg_4P_2.txt" ) ) {  map = "mp_blitzkrieg_4P_2.txt"; found = true; hash = 2323290491u; gameMode = 5; maxTeams = 4; timeLimit = 15; }

    if( 0 == strcasecmp( map.c_str(), "mp_cts_2p_1.txt" ) ) {  map = "mp_cts_2p_1.txt"; found = true; hash = 4747184u; gameMode = 2; maxTeams = 2; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_cts_2p_2.txt" ) ) {  map = "mp_cts_2p_2.txt"; found = true; hash = 2789751786u; gameMode = 2; maxTeams = 2; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_cts_2p_3.txt" ) ) {  map = "mp_cts_2p_3.txt"; found = true; hash = 3912488473u; gameMode = 2; maxTeams = 2; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_cts_2p_4.txt" ) ) {  map = "mp_cts_2p_4.txt"; found = true; hash = 453949229u; gameMode = 2; maxTeams = 2; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_cts_3p_1.txt" ) ) {  map = "mp_cts_3p_1.txt"; found = true; hash = 2315861411u; gameMode = 2; maxTeams = 3; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_cts_3P_2.txt" ) ) {  map = "mp_cts_3P_2.txt"; found = true; hash = 3175143702u; gameMode = 2; maxTeams = 3; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_cts_4P_1.txt" ) ) {  map = "mp_cts_4P_1.txt"; found = true; hash = 1395567834u; gameMode = 2; maxTeams = 4; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_cts_4P_2.txt" ) ) {  map = "mp_cts_4P_2.txt"; found = true; hash = 2046241043u; gameMode = 2; maxTeams = 4; timeLimit = 10; coop = 1; }

    if( 0 == strcasecmp( map.c_str(), "mp_cts_4p_3.txt" ) ) {  map = "mp_cts_4p_3.txt"; found = true; hash = 2456707185u; gameMode = 2; maxTeams = 4; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_domination_2P_1.txt" ) ) {  map = "mp_domination_2P_1.txt"; found = true; hash = 425323806u; gameMode = 0; maxTeams = 2; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_domination_2P_2.txt" ) ) {  map = "mp_domination_2P_2.txt"; found = true; hash = 367459372u; gameMode = 0; maxTeams = 2; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_domination_3P_1.txt" ) ) {  map = "mp_domination_3P_1.txt"; found = true; hash = 695979515u; gameMode = 0; maxTeams = 3; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_domination_4P_1.txt" ) ) {  map = "mp_domination_4P_1.txt"; found = true; hash = 3848699721u; gameMode = 0; maxTeams = 4; timeLimit = 15; coop = 1; }

    if( 0 == strcasecmp( map.c_str(), "mp_domination_4P_2.txt" ) ) {  map = "mp_domination_4P_2.txt"; found = true; hash = 1824305055u; gameMode = 0; maxTeams = 4; timeLimit = 10; coop = 1; }

    if( 0 == strcasecmp( map.c_str(), "mp_koth_2P_1.txt" ) ) {  map = "mp_koth_2P_1.txt"; found = true; hash = 4200724040u; gameMode = 1; maxTeams = 2; timeLimit = 5; }

    if( 0 == strcasecmp( map.c_str(), "mp_koth_2P_2.txt" ) ) {  map = "mp_koth_2P_2.txt"; found = true; hash = 3490260251u; gameMode = 0; maxTeams = 2; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_koth_2P_3b.txt" ) ) {  map = "mp_koth_2P_3b.txt"; found = true; hash = 4131162951u; gameMode = 1; maxTeams = 2; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_koth_2P_3.txt" ) ) {  map = "mp_koth_2P_3.txt"; found = true; hash = 2727448736u; gameMode = 0; maxTeams = 2; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_koth_3P_1.txt" ) ) {  map = "mp_koth_3P_1.txt"; found = true; hash = 3319522846u; gameMode = 1; maxTeams = 3; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_koth_3P_2.txt" ) ) {  map = "mp_koth_3P_2.txt"; found = true; hash = 2807568726u; gameMode = 1; maxTeams = 3; timeLimit = 5; }

    if( 0 == strcasecmp( map.c_str(), "mp_koth_3P_3.txt" ) ) {  map = "mp_koth_3P_3.txt"; found = true; hash = 3676537063u; gameMode = 1; maxTeams = 3; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_koth_3P_4.txt" ) ) {  map = "mp_koth_3P_4.txt"; found = true; hash = 3315151087u; gameMode = 1; maxTeams = 3; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_koth_4P_1.txt" ) ) {  map = "mp_koth_4P_1.txt"; found = true; hash = 2290711831u; gameMode = 1; maxTeams = 4; timeLimit = 10; coop = 1; }

    if( 0 == strcasecmp( map.c_str(), "mp_koth_4P_3.txt" ) ) {  map = "mp_koth_4P_3.txt"; found = true; hash = 732105611u; gameMode = 1; maxTeams = 4; timeLimit = 10; coop = 1; }

    if( 0 == strcasecmp( map.c_str(), "mp_koth_4P_4.txt" ) ) {  map = "mp_koth_4P_4.txt"; found = true; hash = 493569080u; gameMode = 1; maxTeams = 4; timeLimit = 5; coop = 1; }

    if( 0 == strcasecmp( map.c_str(), "mp_rocketriot_2P_2.txt" ) ) {  map = "mp_rocketriot_2P_2.txt"; found = true; hash = 16884426u; gameMode = 4; maxTeams = 2; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_rocketriot_2p_3.txt" ) ) {  map = "mp_rocketriot_2p_3.txt"; found = true; hash = 2650330222u; gameMode = 4; maxTeams = 2; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_rocketriot_2P_4.txt" ) ) {  map = "mp_rocketriot_2P_4.txt"; found = true; hash = 749030024u; gameMode = 4; maxTeams = 2; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_rocketriot_3p_1.txt" ) ) {  map = "mp_rocketriot_3p_1.txt"; found = true; hash = 2222632563u; gameMode = 4; maxTeams = 3; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_rocketriot_3P_2.txt" ) ) {  map = "mp_rocketriot_3P_2.txt"; found = true; hash = 2901381360u; gameMode = 4; maxTeams = 3; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_rocketriot_3P_3.txt" ) ) {  map = "mp_rocketriot_3P_3.txt"; found = true; hash = 322932390u; gameMode = 4; maxTeams = 3; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_rocketriot_4P_1.txt" ) ) {  map = "mp_rocketriot_4P_1.txt"; found = true; hash = 3284306593u; gameMode = 4; maxTeams = 4; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_skirmish_2p_4.txt" ) ) {  map = "mp_skirmish_2p_4.txt"; found = true; hash = 2745372533u; gameMode = 0; maxTeams = 2; timeLimit = 10; }

    if( 0 == strcasecmp( map.c_str(), "mp_skirmish_3P_1.txt" ) ) {  map = "mp_skirmish_3P_1.txt"; found = true; hash = 394053555u; gameMode = 0; maxTeams = 3; timeLimit = 10; }

    // END AUTOGEN

    if ( !found )
    {
        return false;
    }

    if ( forceCoop )
    {
        coop = 1;
    }

    settings.timeLimit.Set( timeLimit );
    settings.gameMode.Set( gameMode );
    settings.maxTeams.Set( maxTeams );
    settings.coopMode.SetMax( coop );
    settings.coopMode.SetMin( forceCoop );
    settings.numAttackers.Set( attackers );
    settings.mapHash.Set( hash );

    return true;

    settings.mapFile.Save();
}

static bool UpdateHash()
{
    if( settings.mapFile.IsDefault() )
    {
        return true;
    }

    // read map
    std::ifstream s( ( mapPrefix.Get() + settings.mapFile.Get() ).c_str() );

    if ( !s.good() )
    {
        // try to look map up in database
        std::string mapFile = settings.mapFile;
        if ( GetHashDatabase( mapFile ) )
        {
            settings.mapFile.Set( mapFile );
            return true;
        }
        else
        {
            return false;
        }
    }
    
    std::ostringstream o;
    int c = s.get();
    do
    {
        o << (char)( c );
        c = s.get();
    } while( c >= 0 );
    
    // calculate hash (only the first 100000 bytes count)
    std::string content = o.str();
    size_t size = content.size();
    
#define MAX_CRC 100000
    
    if ( size > MAX_CRC )
        size = MAX_CRC;
    
    int crc = CRC( 0, content.c_str(), size );
    
    // add EOF to crc
    if ( size < MAX_CRC )
    {
        char eof = -1;
        crc = CRC( crc, &eof, 1 );
    }

    Map::SetHash( crc );

    // decompress and extract map 
    if ( IsCustomMap( settings.mapFile.Get() ) )
    {
#ifdef HAVE_ZLIB_H
        content = ExtractMap( content );
#else
        Log::Err() << "Custom map support not compiled in. Sorry. You'll need to set game mode and player count manually, and be very careful about coop mode.\n";
#endif
    }
    else
    {
        // included maps need no map name
        settings.mapName.Revert();
    }
    
    // while we're at it, fetch the number of players from the map
    std::istringstream r( content );
    bool goon = true;
    bool inoptions = false;
    int timeLimit = 0;
    int coop = 0;
    int forceCoop = 0;
    while ( goon && r.good() && !r.eof() )
    {
        std::string line;
        std::getline( r, line );
        std::istringstream l( line );
        std::ws( l );
        std::string first;
        l >> first;
        std::ws( l );
        if ( first == "MultiwiniaOptions_StartDefinition" )
        {
            inoptions = true;
        }
        else if ( inoptions )
        {
            if ( first == "Name" )
            {
                std::string name;
                std::getline( l, name );
                settings.mapName.Set( name );
            }
            else if ( first == "CoopMode" )
            {
                coop = 1;
            }
            else if ( first == "ForceCoop" )
            {
                forceCoop = 1;
                coop = 1;
            }
            else if ( first == "NumPlayers" )
            {
                int numPlayers = 0;
                l >> numPlayers;
                settings.maxTeams.Set( numPlayers );
            }
            else if ( first == "AttackingTeams" )
            {
                int dummy = 0;
                int attackers = 0;
                do
                {
                    std::ws( l );
                    l >> dummy;
                    std::ws( l );
                    attackers++;
                }
                while( l.good() && !l.eof() );

                settings.numAttackers.Set( attackers );
            }
            else if ( first == "GameTypes" )
            {
                std::string gameMode;
                l >> gameMode;
                settings.gameMode.Set( gameMode );
                
                // set default time
                switch( settings.gameMode.Get() )
                {
                case 1:
                    timeLimit = 5;
                    break;
                case 0:
                case 2:
                    timeLimit = 10;
                    break;
                case 5:
                    timeLimit = 15;
                }
            }
            else if ( first == "PlayTime" )
            {
                l >> timeLimit;
            }
            else if ( first == "MultiwiniaOptions_EndDefinition" )
            {
                goon = false;
            }
        }
    }

    // set time limit
    if ( timeLimit > 0 )
        settings.timeLimit.Set( timeLimit );

    settings.coopMode.SetMax( coop );
    settings.coopMode.SetMin( forceCoop );

    settings.mapFile.Save();

    return true;
}

static bool mapLocked = false;

void Map::Lock()
{
    mapLocked = true;
}

int Map::GetHash()
{
    return settings.mapHash.Get();
}

bool unknownMap = true;

bool Map::DemoRestricted()
{
    return !gameHistory.Playback() && std::find( demoLimits.demoMaps.begin(), demoLimits.demoMaps.end(), settings.mapHash ) == demoLimits.demoMaps.end();
}

bool Map::CheckDemo()
{
    // check for DEMO restrictions
    if( demoLimits.demoMode && demoLimits.set )
    {
        static std::string lastDemoMap = "";
        if ( lastDemoMap != settings.mapFile.Get() )
        {
            if ( DemoRestricted() )
            {
                Log::Err() << "Map \"" << settings.mapFile.Get() << "\" not unlocked for DEMO mode play.\n";
                
                settings.mapFile.Set( lastDemoMap );
                return false;
            }
        }
        
        lastDemoMap = settings.mapFile.Get();
    }

    return true;
}

static FTPFile * mapFTP = 0;

void Map::UpdateHash()
{
    std::string mapFile = settings.mapFile.Get();

    if ( mapLocked )
    {
        // don't accept map changes any more
        settings.mapFile.Restore();
        return;
    }

    if ( ::UpdateHash() )
    {
        if ( !CheckDemo() )
        {
            return;
        }
        
        unknownMap = false;

        // check for game mode changes
        static int gameMode = -1;

        if ( gameMode != settings.gameMode.Get() )
        {   
            // 
            switch( settings.gameMode.Get() )
            {
                // change default settings according to game mode
            case 4:
                settings.turretFrequency.SetDefault(7);
                settings.maxTurrets.SetDefault(3);
                break;
            case 5:
                settings.turretFrequency.SetDefault(2);
                settings.maxTurrets.SetDefault(2);
                break;
            default:
                break;
            }
        }

        gameMode = settings.gameMode.Get();
        // yay, everything OK.
        settings.mapFile.Save();

        // create FTP transfer
        if ( IsCustomMap( mapFile ) ) 
        {
            if ( Client::CountClients() > 0 )
            {
                Log::Err() << "Can't change to a custom map when clients are online.\n";
                settings.mapFile.Restore();
            }

            mapFTP = FTP::LoadFile( ( mapPrefix.Get() + mapFile ).c_str(), "mapfile" );

            if ( !mapFTP )
            {   
                Log::Err() << "Custom map " << mapFile << " not found.\n";
                settings.mapFile.Restore();
            }

            // std::ostringstream filename;
            // filename << std::hex << (unsigned int)settings.mapHash.Get() << ".mwm";
            // settings.mapFile.SetNet( filename.str() );
        }
    }
    else
    {
        unknownMap = true;

        if ( !settings.mapFile.IsDefault() )
        {
            if ( Client::CountClients() > 0 )
            {
                // somebody is online. We can't afford to set an unknown map.
                Log::Err() << "Map \"" << settings.mapFile.Get() << "\" neither found as file nor in the list of official maps. Reverting.\n";
                settings.mapFile.Restore();
            }
            else
            {
                Log::Err() << "Map \"" << settings.mapFile.Get() << "\" neither found as file nor in the list of official maps. You'll need to supply game mode, map hash and time limit manually.\n";
                settings.mapHash.Set( 0 );
            }
        }
    }
}

class MapFileFlexSetting: public StringSetting
{
public:
    MapFileFlexSetting( char const * name )
    : StringSetting( Setting::GameOption, name )
    {
    }

    virtual void OnChange()
    {
        // force recall of Map::Players
        if ( !IsDefault() )
        {
            settings.mapFile.Revert();
        }

        StringSetting::OnChange();
    }
};

static MapFileFlexSetting mapFile2( "MapFile2" );
static MapFileFlexSetting mapFile3( "MapFile3" );
static MapFileFlexSetting mapFile4( "MapFile4" );

void Map::Players( int players, bool final )
{
    if ( serverInfo.started )
    {
        return;
    }

    if( players > 4 )
    {
        players = 4;
    }
    
    // go to four players while someone is connecting, so they see enough open slots
    if ( !final )
    {
        for ( Client::const_iterator c = Client::GetFirstClient(); c; ++c )
        {
            if( !c->id.NameSet() )
                players = 4;
        }
    }

    static int lastPlayers = -1;
    if ( players != lastPlayers || final )
    {
        lastPlayers = players;

        std::string lastMap = settings.mapFile.Get();

        bool change = false;
        if ( players <= 2 && !mapFile2.IsDefault() )
        {
            settings.mapFile.Set( mapFile2.Get() );
            change = true;
        }
        else if ( players <= 3 && !mapFile3.IsDefault() )
        {
            settings.mapFile.Set( mapFile3.Get() );
            change = true;
        }
        else if ( !mapFile4.IsDefault() )
        {
            settings.mapFile.Set( mapFile4.Get() );
            change = true;
        }

        if ( change )
        {
            UpdateHash();

            bool revert = false;

            if ( settings.maxTeams.Get() < players )
            {
                Log::Err() << "Selected map has not enough slots. Reverting.\n";
                revert = true;
            }
            
            if ( change && unknownMap )
            {
                Log::Err() << "Unknown map. Reverting.\n";
                revert = true;
            }

            if ( revert )
            {
                settings.mapFile.Set( lastMap );
                UpdateHash();
            }

            settings.SendSettings();

            Advertiser::Force();
        }


        /*
        static CPU filler[4];
        int fillFrom = players;
        if ( final )
        {
            fillFrom = 4;
        }
        for( int i = 3; i >= 0; --i )
        {
            if ( fillFrom > i )
            {
                filler[i].Vanish();
            }
            else
            {
                filler[i].Play(i);
                filler[i].SetAndSendName("");
            }
        }
        */
    }
}

void Map::SendMap( Client & client )
{
    if ( mapFTP )
    {
        // just delegate
        FTP::SendFile( mapFTP, client );
    }
    else
    {
        // can't send map
        client.Quit( SuperPower::Kick );
    }
}

#endif

