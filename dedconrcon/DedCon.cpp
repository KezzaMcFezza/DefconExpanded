/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "Codes.h"

#include "Misc.h"
#include "Globals.h"
#include "Client.h"
#include "Console.h"
#include "GameHistory.h"
#include "Advertiser.h"
#include "FileTagReader.h"
#include "Rotator.h"
#include "AutoTagReader.h"
#include "AutoTagWriter.h"
#include "Exception.h"
#include "GameSettings.h"
#include "CPU.h"
#include "Host.h"
#include "config.h"
#include "Log.h"
#include "RCONServer.h"

#include <iostream>
#include <sstream>
#include <fstream>

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #define strcasecmp _stricmp
#endif

#ifdef HAVE_LZMA
#include "CPP/Common/MyWindows.h"
#include "CPP/Common/MyInitGuid.h"

extern "C"
{
#include "CPP/7zip/Compress/LZMA_Alone/LzmaRamDecode.h"
#include "C/7zCrc.h"
}



#include "CPP/7zip/Compress/LZMA/LZMADecoder.h"
#include "CPP/7zip/Compress/LZMA/LZMAEncoder.h"
#include "CPP/7zip/Compress/LZMA_Alone/LzmaRam.h"
#include "CPP/7zip/Common/FileStreams.h"
#include "CPP/7zip/ICoder.h"
#endif

#ifdef COMPILE_DEDWINIA
#include "Map.h"
#endif

// simple stream cryptographer
class CryptoStream
{
public:
    CryptoStream( unsigned int seed )
    : r( seed )
    {
#ifdef HAVE_LZMA
      // force linking
      CrcCalc( 0, 0 );
#endif
    }

    inline void Advance()
    {
        r = ( ( r * Mult ) + Add ) % Mod;
    }

    inline unsigned char Encrypt( unsigned char in )
    {
        Advance();
        unsigned char out = in ^ ( r & 0xff );
        r += out;

        return out;
    }

    inline unsigned char Decrypt( unsigned char in )
    {
        Advance();
        unsigned char out = in ^ ( r & 0xff );
        r += in;

        return out;
    }

    enum
    {
        Add = 4711,
        Mult = 13,
        Mod = 0x3f36211
    };

private:
    unsigned int r;  // current crypt value
};

//! minimum time the server will stay up, whether there are clients or onot
static IntegerSetting minRunTime( Setting::ExtendedGameOption, "MinRunTime", 0 );

//! maximum runtime without any activity
static IntegerSetting maxIdleRunTime( Setting::ExtendedGameOption, "MaxIdleRunTime", 0 );

// saves all into
static void SaveAll( std::string const & filename )
{
#ifdef COMPILE_DEDCON
    bool encrypt = false;
#else
    bool encrypt = true;
#endif

#ifdef HAVE_LZMA
#ifdef COMPILE_DEDCON
    bool pack = false;
#else
    bool pack = true;
#endif
#else
    bool pack = false;
#endif
    unsigned int seed = rand();

    // write some global info first
    AutoTagWriter writer( "DCGR" );

    // the ID the next client is going to get
    writer.WriteInt( "cid", SuperPower::currentID );

    // the start seqID
    writer.WriteInt( "ssid", gameHistory.GetSeqIDStart() );

    // the end start seqID
    writer.WriteInt( "esid", gameHistory.GetSeqIDEnd() );

    // the server version
    writer.WriteString( "sv", settings.serverVersion );

    // random cryptograpy
    if ( encrypt )
    {
        writer.WriteInt( "cs1", seed );
    }

    writer.WriteBool( "pack", pack );

    InternetAdvertiser::WriteRecording( writer );

    // open file and start writing
    std::ofstream s( filename.c_str(), std::ios::out | std::ios::binary );
    if ( !s.good() )
    {
        Log::Err() << "Could not open recording file " << filename << " for writing.\n";
        return;
    }

    writer.Save( s );

    if ( pack || encrypt )
    {
        // write the game history into memory
        std::ostringstream historyStream;
        gameHistory.Save( historyStream );
        std::string history = historyStream.str();
        
        bool needFree = false;
        unsigned char * buffer = (unsigned char *)&history[0];
        size_t size = history.size();

        // prepare buffer to receive compression
        if ( pack )
        {
#ifdef HAVE_LZMA
            // prepare output buffer
            size_t outSize = ( size * 3 ) >> 1;
            unsigned char * out = new unsigned char[ outSize ];
            size_t processed = 0;

            // compress it
            LzmaRamEncode( buffer, size,
                           out, outSize, &processed,
                           size, SZ_FILTER_AUTO );


            Log::Out() << "Recording compression ratio: " << (float( processed )/size) << "\n";

            // bend buffer pointer
            needFree = true;
            size = processed;
            buffer = out;

            history.clear();
#endif
        }

        if ( encrypt )
        {
            CryptoStream cryptoStream( seed );
            
            size_t i;
            unsigned char * run;
            for( run = buffer, i = size; i > 0; ++run, --i )
            {
                *run = cryptoStream.Encrypt( *run );
            }
        }

        // save big binary blob
        {
            CompressedTag::Save( s, buffer, size );

            if ( needFree )
            {
                delete[] buffer;
            }
        }
    }
    else
    {
        // just write game history
        gameHistory.Save( s );
    }
}

// looks for the modpath in a loaded recording
static void ScanGameHistory()
{
    std::string serverName = "";

    for( GameHistory::iterator iter = gameHistory.begin(); iter != gameHistory.end(); ++iter )
    {
        Sequence * s = *iter;

        if ( s->ServerTag().Size() )
        {

            // scan for modpath
            TagReader reader( s->ServerTag() );

#ifdef COMPILE_DEDCON
            if ( reader.Start() == C_TAG )
            {
                while ( reader.AtomsLeft() > 0 )
                {
                    std::string key = reader.ReadKey();
                    if ( key == C_TAG_TYPE )
                    {
                        if ( reader.ReadString() != C_MODS )
                        {
                            break;
                        }
                    }
                    else if ( key == C_MODS )
                    {
                        settings.serverMods.Set( reader.ReadString() );
                    }
                    else
                    {
                        reader.ReadAny();
                    }
                }
            }
#endif
            reader.Obsolete();
        }

        ListBase< CompressedTag >::const_iterator client = s->ClientTag();
        while ( client )
        {
            // scan for server name
            TagReader reader( *client );
            if ( reader.Start() == C_TAG )
            {
                bool isServerName = false;
#ifdef COMPILE_DEDWINIA
                bool isMapName = false;
#endif
                while ( reader.AtomsLeft() > 0 )
                {
                    std::string key = reader.ReadKey();
                    if ( key == C_TAG_TYPE )
                    {
                        std::string value = reader.ReadString();
                        if ( value == C_CONFIG )
                        {
                            isServerName = true;
                        }
#ifdef COMPILE_DEDWINIA
                        else if ( value == C_CONFIG_STRING )
                        {
                            isMapName = true;
                        }
#endif
                        else
                        {
                            // no setting
                            break;
                        }
                    }
                    else if ( isServerName )
                    {
                        if ( key == C_METASERVER_MAX_SPECTATORS )
                        {
                            if ( reader.ReadChar() != 0 )
                            {
                                // not the server name
                                break;
                            }
                        }
                        else if ( key == C_PLAYER_NAME )
                        {
                            // that's the server name!
                            serverName = reader.ReadString();
                        }
                        else
                        {
                            reader.ReadAny();
                        }
                    }
#ifdef COMPILE_DEDWINIA
                    else if ( isMapName )
                    {
                        if ( key == C_CONFIG_NUMERIC_NAME )
                        {
                            if ( reader.ReadInt() != 3 )
                            {
                                break;
                            }
                        }
                        else if ( key == C_CONFIG_VALUE )
                        {
                            settings.mapFile.Set( reader.ReadString() );
                        }
                        else
                        {
                            reader.ReadAny();
                        }
                    }
#endif
                    else
                    {
                        reader.ReadAny();
                    }
                }
            }
            reader.Obsolete();

            ++client;
        }
    }

    if ( serverName.size() > 0 )
        settings.serverName.Set( serverName + " playback" );
}

// saves all into
static void LoadAll( std::string const & filename )
{
    if ( gameHistory.Playback() )
    {
    }

    std::ifstream s( filename.c_str(), std::ios::in | std::ios::binary );
    if ( !s.good() )
    {
        Log::Err() << "Recording file " << filename << " not found.\n";
        return;
    }

    s.exceptions( std::ios::eofbit | std::ios::failbit | std::ios::badbit );

    // write some global info first
    FileTagReader reader( s, 10000 );
    if ( reader.Start() != "DCGR" )
    {
        Log::Err() << "Recording file " << filename << " is not a DedCon game recording.\n";
        return;
    }

    if ( Log::GetLevel() >= 2 )
    {
        Log::Out() << "Loading recording file " << filename << ".\n";
    }

    bool decrypt = false;
    bool unpack = false;
    unsigned int seed = 0;
    int seqIDStart = 0, seqIDEnd = 0x7fffffff;
    while ( reader.AtomsLeft() > 0 )
    {
        std::string key = reader.ReadKey();
        if ( key == "cid" )
        {
            SuperPower::currentID = reader.ReadInt();
        }
        else if ( key == "sv" )
        {
            settings.serverVersion.Set( reader.ReadString() );
        }
        else if ( key == "ssid" )
        {
            seqIDStart = reader.ReadInt();
        }
        else if ( key == "esid" )
        {
            seqIDEnd = reader.ReadInt();
        }
        else if ( key == "cs1" )
        {
            seed = reader.ReadInt();
            decrypt = true;
        }
        else if( key == "pack" )
        {
            unpack = reader.ReadBool();
        }
        else
        {
            if ( !InternetAdvertiser::ParsePlayback( key, reader ) )
            {
                reader.ReadAny();
            }
        }
    }

    // not interested in nested tags
    reader.Obsolete();

    // load game history
    if ( unpack || decrypt )
    {
        std::string const * buffer = 0;

        // load big binary blob
        size_t size = CompressedTag::ReadSize( s );
        std::string read( size, 0 );
        unsigned char * start = (unsigned char *)&read[0];
        CompressedTag::Load( s,start , size );
        buffer = &read;

        if ( decrypt )
        {
            CryptoStream cryptoStream( seed );
            
            size_t i;
            unsigned char * run;
            for( run = start, i = size; i > 0; ++run, --i )
            {
                *run = cryptoStream.Decrypt( *run );
            }
        }

        std::string unpacked;
        if ( unpack )
        {
#ifdef HAVE_LZMA
            size_t uncompressedSize;
            int result = LzmaRamGetUncompressedSize( start, size, &uncompressedSize );
            if ( result )
            {
                Log::Err() << "Error in compression headers.\n";
                throw QuitException(-1);
            }

            // prepare output buffer
            unpacked = std::string( uncompressedSize, ' ' );
            size_t processed = 0;

            // compress it
            LzmaRamDecompress( start, size,
                               (unsigned char *) unpacked.c_str(), uncompressedSize, &processed,
                               malloc, free );

            read.clear();
            buffer = &unpacked;
#endif
        }

        std::istringstream b( *buffer );
        gameHistory.Load( b );
        
    }
    else
    {
        gameHistory.Load( s );
    }

    gameHistory.SetSeqIDStart( seqIDStart );
    gameHistory.SetSeqIDEnd( seqIDEnd );

    // bump right to start of game
    gameHistory.GetTail( seqIDStart );
    gameClock.Set( seqIDStart );
    gameHistory.SetEffectiveTailSeqID( seqIDStart );

    // scan the historty
    ScanGameHistory();

    // declare game started
    serverInfo.unstoppable = serverInfo.started = true;

    // and go to the right clock 
    // 
    // 
    // (wait for the first client to connect)
    gameClock.ticksPerSecond = 0;

#ifndef COMPILE_DEDWINIA
    // and advertise on the LAN, not the internet
    settings.advertiseOnLAN.Set(1);
    settings.advertiseOnInternet.Set(0);
#endif

    if ( settings.maxSpectators.Get() <= 0 )
    {
        settings.maxSpectators.Set(1);
    }
}

class TimeStringSetting: public StringSetting
{
public:
    TimeStringSetting( Type type, char const * name, char const * def = "" )
    : StringSetting( type, name, def )
    {
    }

    //! set the value
    virtual void Set( std::string const & val )
    {
        StringSetting::Set( Time::GetStartTimeString( val.c_str() ) );
    }
};

static TimeStringSetting recordTo( Setting::Log, "Record" );

struct sockaddr_in targetSAddr;

#ifdef WIN32

static void WinsockInit()
{
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
    wVersionRequested = MAKEWORD(1, 1);

    err = WSAStartup(wVersionRequested, &wsaData);

    if (err != 0)
        return;

    if ( LOBYTE( wsaData.wVersion ) != 1 ||
            HIBYTE( wsaData.wVersion ) != 1 ) {
        WSACleanup();
        return;
    }
}

#endif

bool wasActive = false;

class ClientDestroyer
{
public:
    void f(){};

    ~ClientDestroyer()
    {
        CPU::RemoveAll();
        Client::DeleteAll();

        Log::Event() << "SERVER_QUIT\n";
    }
};

// returns the better of two keys
static std::string const & BestKey( std::string const & key1, std::string const & key2 )
{
    if ( key1.size() < 4 )
        return key2;
    if ( key2.size() < 4 )
        return key1;
    if ( key1[0] == 'D' && key1[1] == 'E' && key1[2] == 'M' && key1[3] == 'O' )
        return key2;
    return key1;
}

// loads the key from a file, overwrites old key only if the new one is better
static bool LoadKey( std::string const & filename, std::string & key )
{
    if ( Log::GetLevel() >= 3 )
    {
        Log::Out() << "Loading key from " << filename << "\n";
    }
    std::ifstream s( filename.c_str() );
    if ( s.good() )
    {
        std::string key2;
        std::string oldKey = key;
        getline( s, key2 );
        key = BestKey( key, key2 );

        if ( key == key2 && key != oldKey && Log::GetLevel() >= 2 )
            Log::Out() << "Server key found in file " << filename << ".\n";

        return true;
    }

    return false;
}

#ifdef COMPILE_DEDWINIA
#define GAME "multiwinia"
#define GAME_CAP "Multiwinia"
#endif

#ifdef COMPILE_DEDCON
#define GAME "defcon"
#define GAME_CAP "Defcon"
#endif

#ifdef WIN32

#include <shlobj.h>

// reads the key on Windows
static void ReadKey( HKEY root, std::string & key )
{
    // read from the registry
    DWORD type;
    HKEY dmKey;

    char value[1024];
    DWORD len = sizeof( value );

    {
        // first, try steam
        DWORD rc = RegOpenKeyEx( root,
        TEXT("Software\\Valve\\Steam"),0,KEY_QUERY_VALUE, &dmKey );
        if ( rc == ERROR_SUCCESS )
        {
            rc = RegQueryValueEx( dmKey, TEXT("SteamPath"), 0, &type, (LPBYTE)value, &len );
            if ( rc == ERROR_SUCCESS )
            {
                if ( Log::GetLevel() >= 3 )
                {
                    Log::Out() << "SteamPath = " << value << "\n";
                }
                std::stringstream filename;
                filename << value << "/steamapps/common/" GAME "/authkey";
                LoadKey( filename.str(), key );
            }

            RegCloseKey( dmKey );
        }
    }

    // Try the standalone version
    {
        len = sizeof( value );

        DWORD rc = RegOpenKeyEx( root,
        TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" GAME_CAP "_is1"),0,KEY_QUERY_VALUE, &dmKey );
        if ( rc == ERROR_SUCCESS )
        {
            rc = RegQueryValueEx( dmKey, TEXT("InstallLocation"), 0, &type, (LPBYTE)value, &len );
            if ( rc == ERROR_SUCCESS )
            {

                std::stringstream filename;
                filename << value << "authkey";
                LoadKey( filename.str(), key );
            }

            RegCloseKey( dmKey );
        }
    }
}
#endif

static StringSetting serverKeyFile( Setting::Network, "ServerKeyFile" );

IntegerSetting cheatProxy( Setting::Network, "CheatProxy", 0 );

// tag reader with mutable buffer
class MutableAutoTagReader: public AutoTagReader
{
public:
    MutableAutoTagReader( Socket & socket ): AutoTagReader( socket ){}

    int Len(){ return len; }
    unsigned char * Buffer(){ return buffer; }
};

static void ReadFrom( Socket & socket, InternetAdvertiser & internetAdvertiser, Time const & current )
{
    // socket.Reset();
    MutableAutoTagReader reader( socket );
    
    if ( reader.Received() )
    {
        // gameHistory.Check();
        
        ClientList * c = 0;
        for ( Client::iterator iter = Client::GetFirstClient() ; iter; ++iter )
        {
            c = &(*iter);
            if ((c->saddr.sin_addr.s_addr==reader.Sender().sin_addr.s_addr)
                && (c->saddr.sin_port==reader.Sender().sin_port))
            {
                break;
            }
            else
            {
                c = 0;
            }
        }
        if (!c && ( !serverInfo.dedicated || !internetAdvertiser.Process( reader, reader.Sender() ) ) )
        {
            c=new ClientList( socket, Client::GetClients() );
            c->SetAddr( reader.Sender() );
            
            if ( !serverInfo.dedicated )
            {
                if ( Log::GetLevel() >= 2 )
                    Log::Out() << "New connection from " << inet_ntoa(c->saddr.sin_addr) << ":" << ntohs(c->saddr.sin_port) << "\n";
                
                c->sock.Open();
                c->sock.Bind();
            }
        }
        
        if ( c )
        {
            // generate the command from it to insert into the global sequence list
            if ( serverInfo.dedicated )
            {
                try
                {
                    c->ReadFrom( reader );
                }
                catch( ReadException & e )
                {
                    delete c;
                    c = 0;
                }
            }
            else
            {
                Log::Out() << inet_ntoa(c->saddr.sin_addr) << ":" << ntohs(c->saddr.sin_port) << " ";
                reader.SendTo( c->sock, targetSAddr, METASERVER_ENCRYPTION );
                reader.Obsolete();
            }
        }
    }
}

// organizes command line settings
class CommandLineSettings
{
public:
    struct CommandLineSetting
    {
        // the value
        std::string setting;

        // whether it is a file or direct command
        bool isFile;
        
        CommandLineSetting( std::string const & value, bool isFile_ )
        : setting( value ), isFile( isFile_ )
        {
        }
    };

    // adds a direct config line
    void AddCommand( std::string const & command )
    {
        commands.push_back( CommandLineSetting( command, false ) );
    }

    // adds an include file
    void AddFile( std::string const & file )
    {
        commands.push_back( CommandLineSetting( file, true ) );
    }

    // applies the collected commands and files
    void Apply()
    {
        SettingsReader & settingsReader = SettingsReader::GetSettingsReader();

        int count = 0;
        for( std::vector< CommandLineSetting >::const_iterator i = commands.begin(); i != commands.end(); ++i )
        {
            if ( (*i).isFile )
            {
                settingsReader.Spawn()->Read( (*i).setting.c_str() );
            }
            else
            {
                ++count;
                std::ostringstream s;
                s << "command line argument " << count;
                settingsReader.AddLine( s.str(), (*i).setting );
            }

            settingsReader.Apply();
        }

        if ( settings.ServerKeyDefault() )
        {
            std::string key;
#ifndef WIN32
            {
                // no? load it from ~/.defcon/autkey
                std::string filename = std::string(getenv("HOME")) +
#ifdef MACOSX
                "/Library/Preferences/uk.co.introversion." GAME ".authkey";
#else
                "/." GAME "/authkey";
#endif
                LoadKey( filename, key );
            }
#else // WIN32
            {
#ifdef COMPILE_DEDWINIA
                // get path to local app data
                char appDataPath[MAX_PATH];
                SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath);
                // construct auth key path from there
                std::ostringstream s;
                s << appDataPath << "\\Introversion\\Multiwinia\\authkey";
                LoadKey( s.str(), key );
#endif

                ReadKey( HKEY_CURRENT_USER, key );
                ReadKey( HKEY_LOCAL_MACHINE, key );
           }
#endif
            // first fallback: load from given file
            if ( !serverKeyFile.IsDefault() )
            {
                if ( !LoadKey( serverKeyFile, key ) )
                {
                    Log::Err() << "Could not load server key from file ServerKeyFile = " << serverKeyFile.Get() << ".\n";
                }
            }

            settings.SetServerKey( key );
        }
    }

    // reloads all settings
    void Reload()
    {
        // abort all setting readers
        SettingsReader & settingsReader = SettingsReader::GetSettingsReader();
        settingsReader.AbortRecursive();

        // reset everything to default
        SettingBase::RevertAll();

        // and start anew
        Apply();
    }
private:
    //! commands to execute
    std::vector< CommandLineSetting > commands;
};
static CommandLineSettings commandLineSettings;

class ReloadSetting: public ActionSetting
{
public:
    ReloadSetting() : ActionSetting( Setting::Admin, "Reload" ){}

    virtual void DoRead( std::istream & )
    {
        commandLineSettings.Reload();
    }
};
static ReloadSetting reloadSetting;

int main2(int argc, char *argv[])
{
    Time::GetStartTime();

    // atvertise on internet
    InternetAdvertiser internetAdvertiser;
    metaServer = &internetAdvertiser;

    // int bytes_in=0, bytes_out=0; // , old_bytes_in=0, old_bytes_out=0;
    time_t last_stat;

    if ( argc==2 && strcmp("-h", argv[1] ) == 0 )
    {
        Log::Out() << "\nUsage:\n";
#ifdef DEBUG
        Log::Out() << "  " << argv[0] << " -p LocalPort ServerAddress ServerPort\n    or\n";
#endif
        Log::Out() << "  "  << argv[0] << " [OPTIONS] [CONFIGFILES]\n";
        Log::Out() <<
        "\n Available options:\n\n"
        "   -c \"<config command>\"      executes config command as if it appeared\n"
        "                              in one of the config files.\n"
        "   -l \"<recording file>\"      loads a recording for playback\n"
        "   -v, --version              prints version and exits\n";

        Log::Out() << "\n";

        return 0;
    }
#ifdef WIN32
    // initialize winsock
    WinsockInit();

    // while we're at it, increase priority
    SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS );
#endif

    last_stat=time(NULL);
    serverSocket.Open();

    // make socket broadcastable
    serverSocket.Broadcastable();

    targetSAddr.sin_family=AF_INET;

    SettingsReader & settingsReader = SettingsReader::GetSettingsReader();

    //seed the random number generator
    Time current;
    current.GetTime();
    srand( current.Seconds() + current.Microseconds() );

    // Reserve sequence for modpath
    Sequence * modPath = 0;

    int port;

#ifdef DEBUG
    if ( argc > 3 && strcmp("-p", argv[1] ) == 0 )
    {
        // just act as a proxy for debugging
        targetSAddr.sin_port=htons((unsigned short) atoi(argv[4]));
        targetSAddr.sin_addr.s_addr=inet_addr(argv[3]);

        port = atoi(argv[2]);

        Log::SetLevel(4);
    }
    else
#endif
    {
        // char * x = 0;
        // *x = 0;

        // act as a real server. Add sequence 0.
        serverInfo.dedicated = 1;

        // read the settings
        {
            int i = 1;
            while ( i < argc )
            {
                if ( strcmp( argv[i], "-c" ) == 0 )
                {
                    if ( i < argc-1 )
                    {
                        commandLineSettings.AddCommand( argv[i+1] );

                       ++i;
                    }
                    else
                    {
                        Log::Err() << "Expected configuration line after -c.\n";
                        return 1;
                    }
                }
                else if ( strcmp( argv[i], "-l" ) == 0 )
                {
                    if ( i < argc-1 )
                    {
                        LoadAll( argv[i+1] );
                        ++i;
                    }
                    else
                    {
                        Log::Err() << "Expected game recording file after -l.\n";
                        return 1;
                    }
                }
                else if ( strcmp( argv[i], "-v" ) == 0 || strcmp( argv[i], "--version" ) == 0 )
                {
                    Log::Out() << TRUE_PACKAGE_VERSION << "\n";
                    return 0;
                }
#ifdef DEBUG
#ifndef WIN32
#ifdef COMPILE_DEDCON
                else if ( strcmp( argv[i], "-o" ) == 0 )
                {
                    if ( i < argc-2 )
                    {
                        // send a "open sesame" setting bomb
                        targetSAddr.sin_port=htons((unsigned short) atoi(argv[i+2]));
                        targetSAddr.sin_addr.s_addr=inet_addr(argv[i+1]);
                        
                        AutoTagWriter writer(C_TAG, C_CONFIG);
                        writer.WriteChar( C_METASERVER_MAX_SPECTATORS, 20 );
                        writer.WriteInt( C_PLAYER_NAME, 100 );
                        
                        writer.SendTo( serverSocket, targetSAddr, PLEBIAN_ENCRYPTION );
                        
                        usleep(5000000);
                        
                        AutoTagWriter writer2(C_TAG, C_CONFIG);
                        writer2.WriteChar( C_METASERVER_MAX_SPECTATORS, 20 );
                        writer2.WriteInt( C_PLAYER_NAME, 0 );
                        
                        writer2.SendTo( serverSocket, targetSAddr, PLEBIAN_ENCRYPTION );
                        
                        return 0;
                    }
                    else
                    {
                        Log::Err() << "Expected IP and port after -o.\n";
                        return 1;
                    }
                }
#endif
#ifdef COMPILE_DEDWINIA
                else if ( strcmp( argv[i], "-s" ) == 0 )
                {
                    if ( i < argc-2 )
                    {
                        targetSAddr.sin_port=htons((unsigned short) atoi(argv[i+2]));
                        targetSAddr.sin_addr.s_addr=inet_addr(argv[i+1]);
                        
                        // send a matchmakter bomb
                        // AutoTagWriter writer(C_METASERVER_MATCH, "ad" );
                        
                        // send an ack bomb
                        AutoTagWriter writer(C_TAG, C_ACK );
                        writer.WriteInt( C_SEQID_LASTCOMPLETE, 1000000 );
                        writer.WriteChar( C_SYNC_VALUE, 0 );
                        writer.WriteInt( C_SEQID_LAST, 2000000 );
                        
                        writer.SendTo( serverSocket, targetSAddr, PLEBIAN_ENCRYPTION );
                        
                        return 0;
                    }
                    else
                    {
                        Log::Err() << "Expected IP and port after -o.\n";
                        return 1;
                    }
                }
                else if ( strcmp( argv[i], "-f" ) == 0 )
                {
                    if ( i < argc-1 )
                    {
                        Log::SetLevel(0);
                        settings.mapFile.Set( argv[i+1] );
                        unsigned int hash = Map::GetHash();
                        
                        std::cout << "    if( 0 == strcasecmp( map.c_str(), \"" << settings.mapFile.Get() << "\" ) ) { "
                                  << " map = \"" << settings.mapFile.Get() << "\""
                                  << "; found = true"
                                  << "; hash = " << hash
                                  << "u; gameMode = " << settings.gameMode.Get()
                                  << "; maxTeams = " << settings.maxTeams.Get()
                                  << "; timeLimit = " << settings.timeLimit.Get();
                        if ( settings.coopMode.GetMax() > 0 )
                        {
                            std::cout << "; coop = 1";
                        }
                        if ( settings.coopMode.GetMin() > 0 )
                        {
                            std::cout << "; forceCoop = 1";
                        }
                        if ( settings.numAttackers > 0 )
                        {
                            std::cout << "; attackers = " << settings.numAttackers.Get();
                        }
                        std::cout << "; }\n";
                        
                        return 0;
                    }
                    else
                    {
                        Log::Err() << "Expected map file after -f.\n";
                        return 1;
                        
                    }
                }
#endif
#endif
#endif

                else  if ( argv[i][0] == '-' )
                {
                    Log::Err() << "Unknown command line switch " << argv[i] << ", try '" << argv[0] << " -h'\n";
                    return 1;
                }
                else
                {
                    commandLineSettings.AddFile( argv[i] );
                }

                i++;
            }

            if ( !gameHistory.Playback() )
            {
                modPath = gameHistory.ForServer();
            }

            if ( Log::GetLevel() > 0 )
            {
#ifdef COMPILE_DEDCON
                Log::Out() << "DedCon: Dedicated Server for Defcon " << TRUE_PACKAGE_VERSION << " - Copyright (C) 2007 Manuel Moos\n";
#endif

#ifdef COMPILE_DEDWINIA
                Log::Out() << "Dedwinia: Dedicated Server for Multiwinia " << TRUE_PACKAGE_VERSION << " - Copyright (C) 2008 Manuel Moos and Introversion Software\n";
#endif
            }

            commandLineSettings.Apply();
        }

        Log::Event() << "SERVER_START " TRUE_PACKAGE_STRING "\n";

#ifdef COMPILE_DEDWINIA
        Log::Event() << "CHARSET utf-8\n";
#endif

#ifdef COMPILE_DEDCON
        Log::Event() << "CHARSET latin1\n";
        if ( modPath )
        {
            // Send mod path only once
            AutoTagWriter writer(C_TAG,C_MODS);
            writer.WriteString( C_MODS, settings.serverMods );
            modPath->InsertServer( writer );
        }
#endif

        // Host::GetHost();

        // see if a key was set

        port = settings.serverPort;
    }

    int tries = 100;
    while ( --tries > 0 )
    {
        serverSocket.SetPort( port );
        if ( serverSocket.Bind() )
            break;

        ++port;
    }
    if ( tries > 0 )
    {
        if ( Log::GetLevel() >= 0 )
            Log::Out() << "Listening on port " << port << ".\n";

        settings.serverPort.Set(port);
    }
    else
    {
        Log::Err() << "Could not open listening port.\n";
        return 1;
    }

    internetAdvertiser.Init();

    // initialize RCON server if enabled
    RCONServer::GetInstance().Initialize();

    // query demo limits
    QueryFactory::QueryDemoLimits();

    // query key validity
    QueryFactory::QueryServerKey();

    // make the host exist
    // if ( !gameHistory.Playback() )
    // Host::GetHost();

    while (1)
    {
        fd_set r_fdset;
        Time nextBump;
        int maxfd;
        Time current;

        // read stdin
        Console::Read();
        
        // process any incoming RCON commands
        RCONServer::GetInstance().ProcessCommands();

        // apply settings
        settingsReader.Apply();

        if ( serverInfo.dedicated )
        {
            // gameHistory.Check();

            static LanAdvertiser lanAdvertiser;
            lanAdvertiser.Advertise();

            internetAdvertiser.Advertise();

            // send to and receive from the metaserver
            internetAdvertiser.SendQueries();
            internetAdvertiser.Receive();

            // print playback ready message
            if ( metaServer->Answers() && gameHistory.Playback() )
            {
                static int countdown = 10;
                if ( --countdown == 0 )
                {
                    static bool print = true;
                    if ( print )
                    {
                        Log::Out() << "\n";
#ifdef COMPILE_DEDCON
                        Log::Out() << "Playback is ready for watching. Connect your DEFCON client to the server that should now be visible in the LAN browser, server name \"" << settings.serverName.Get() << "\".\n";
#endif
#ifdef COMPILE_DEDWINIA
                        Log::Out() << "Playback is ready for watching. Connect your Multiwinia client to the server that should now be visible in the server browser, server name \"" << settings.serverName.Get() << "\".\n";
#endif
                        print = false;
                    }

                    countdown = -1;
                }
            }

            // send settings
            if ( !serverInfo.started )
            {
                settings.LockSettingsAccordingToGameMode();
                settings.SendSettings();
            }
        }

        gameClock.GetNextBump( nextBump );

        FD_ZERO(&r_fdset);
        FD_SET(serverSocket.GetSocket(), &r_fdset);
        maxfd=serverSocket.GetSocket();
        if ( lanSocket.Bound() )
        {
            FD_SET(lanSocket.GetSocket(), &r_fdset);
            if ( lanSocket.GetSocket() > maxfd )
                maxfd=lanSocket.GetSocket();
        }
        if (Client::GetFirstClient() && !serverInfo.dedicated )
        {
            for ( Client::const_iterator c = Client::GetFirstClient(); c; ++c )
            {
                FD_SET( c->sock.GetSocket(), &r_fdset );
                if ( (int)c->sock.GetSocket() > maxfd )
                {
                    maxfd=c->sock.GetSocket();
                }
            }
        }
        // break on input
        #ifdef BREAKONINPUT
        {
            int in = fileno(stdin);
            FD_SET( in, &r_fdset );
            if ( in > maxfd )
            {
                maxfd = in;
            }
        }
        #endif

        select(maxfd+1, &r_fdset, NULL, NULL, &nextBump.time);

        current.GetTime();

        ReadFrom( serverSocket, internetAdvertiser, current );
        if( lanSocket.Bound() )
        {
            ReadFrom( lanSocket, internetAdvertiser, current );
        }

        // syncronize clock and game history
        int lastSeqID = gameHistory.GetTailSeqID();
        gameClock.AutoBump( lastSeqID );
        gameHistory.GetTail( gameClock.Current() );

        if ( serverInfo.dedicated )
        {
            Client::SendToAll();
        }
        else
        {
            for ( Client::iterator client = Client::GetFirstClient(); client; ++client )
            {
                MutableAutoTagReader reader( client->sock );
                if ( reader.Received() )
                {
                    client->lastAck = current;
                    
                    Log::Out() << inet_ntoa(targetSAddr.sin_addr) << ":" << ntohs(targetSAddr.sin_port) << " ";
                    
//#ifdef asdfsdf
                    // evil replacing
                    // reader.Buffer()[reader.Len()] = 0;
                    bool login = false;
                    for ( int i = 0; i < reader.Len()-10; ++i )
                    {
                        char * b = (char *)reader.Buffer() + i;

                        // mess with LAN IP
                        if ( 0 == strncmp( b, "192.168.0.1", 10 ) )
                        {
                            b[10]='3';
                        }

                        // mess with random seed
                        if ( 0 == strncmp( b, "\002ed", 3 ) )
                        {
                            login = true;
                        }

                        // mess with random seed
                        if ( login && 0 == strncmp( b, "\002dr", 3 ) )
                        {
                            // b[5]++;
                        }
                        /*
                        char const * c = "\002dB\001";
                        if ( 0 == strncmp( b, c, 4 ) )
                        {
                             b[4]++;
                             b[6]++;
                        }
                        char const * d = "\002dC";
                        if ( 0 == strncmp( b, d, 3 ) )
                        {
                             b[5]++;
                             b[6]++;
                        }
                        */
                    }
//#endif

                    reader.SendTo(serverSocket, client->saddr, METASERVER_ENCRYPTION );
                    reader.Obsolete();
                }

                if ((current-client->lastAck).Seconds()>30)
                {
                    client->sock.Close();
                    if ( Log::GetLevel() >= 2 )
                        Log::Out() << "Lost connection with " << inet_ntoa(client->saddr.sin_addr) << ":" << ntohs(client->saddr.sin_port) << "\n";

                    client->Quit( SuperPower::Left );
                }
            }
        }

        // check for any client activity
        static Time lastActivityGlobal = current;

        if ( Client::GetFirstClient() )
        {
            // start clock, slowly
            if ( gameClock.ticksPerSecond == 0 && !gameHistory.Playback() )
            {
                gameClock.AutoBump(0);
                gameClock.ticksPerSecond = 1;
            }

            wasActive = true;

            // mark activity
            lastActivityGlobal = current;
        }
        else
        {
            // no clients, no time progression
            gameClock.ticksPerSecond = 0;
            serverInfo.totalPlayers = 0;
        }

        // quit when nothing has happened for a while 
        if ( !Client::GetFirstClient() &&
             ( ( current - Time::GetStartTime() ).Seconds() >= minRunTime ) &&
             ( minRunTime > 0 || ( wasActive && ( current - lastActivityGlobal ).Seconds() >= maxIdleRunTime ) )
            )
        {
            return 0;
        }
    }
}

int main(int argc, char *argv[])
{
    Log::Out();
    Log::Err();
    Log::Event(); // initialize logs (so it gets destroyed after clientDestroyer)
#ifdef DEBUG
    Log::NetLog();
#endif

    Time::GetStartTimeString("");

    int ret = 0;
    try
    {
        ClientDestroyer clientDestroyer; // make sure all clients are deleted before the main program ends
        clientDestroyer.f(); // just to make the compiler think this is used

        try
        {
            ret = main2( argc, argv );
        }
        catch( QuitException const & e )
        {
            ret = e.ret;
        }

        // record game
        if ( !recordTo.IsDefault() )
        {
            SaveAll( Log::Prefix() + recordTo.Get() );
        }

    }
    catch( QuitException const & e )
    {
        ret = e.ret;
    }
    catch( std::bad_alloc const & e )
    {
        Log::Out() <<
        "Out of memory error. The only documented cause of this so far would be loading a corrupt recording.\n";
    }

    Rotator::Save();

    return ret;
}
