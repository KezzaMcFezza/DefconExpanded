#include "lib/universal_include.h"

#include "RecordingParser.h"

#include "lib/tosser/directory.h"
#include "lib/hi_res_time.h"
#include "network/network_defines.h"
#include "network/Server.h"
#include "app/app.h"
#include "app/globals.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <fstream>

inline void reverseCopy( char *_dest, const char *_src, size_t count )
{
	_src += count;
	while (count--)
		*(_dest++) = *(--_src);
} 

//
// Used for ints, floats and doubles, taking into account endianness

template <class T>
std::istream &readNetworkValue( std::istream &input, T &v )
{
#ifndef __BIG_ENDIAN__
	return input.read( (char *) &v, sizeof(T) );
#else
	char u[sizeof(T)];
	input.read( u, sizeof(T) );
	
	char *w = (char *) &v;
	reverseCopy(w, u, sizeof(T));
	return input;
#endif
}


RecordingParser::RecordingParser( std::istream &in, const std::string &filename, Server *server )
    : m_in( &in ),
      m_filename( filename ),
      m_server( server ),
      m_ownStream( false )
{
}

//
// Constructor for direct file loading for WebAssembly

RecordingParser::RecordingParser( const std::string &filename, Server *server )
    : m_filename( filename ),
      m_server( server ),
      m_ownStream( true )
{
 
    m_fileStream.open(filename.c_str(), std::ios::binary | std::ios::in);
    if (!m_fileStream.good())
    {
        AppDebugOut("RecordingParser: Failed to open file '%s'\n", filename.c_str());
        m_in = nullptr;
        return;
    }
    
    //
    // Check file size for debugging

    m_fileStream.seekg(0, std::ios::end);
    size_t fileSize = m_fileStream.tellg();
    m_fileStream.seekg(0, std::ios::beg);
    
    m_in = &m_fileStream;
    AppDebugOut("RecordingParser: Successfully opened file '%s', size: %zu bytes\n", filename.c_str(), fileSize);
}

RecordingParser::~RecordingParser()
{
    if (m_ownStream && m_fileStream.is_open())
    {
        m_fileStream.close();
    }
}

#if _MSC_VER == 1500
typedef int int32_t;
#endif

bool RecordingParser::ReadPacket( Directory &dir, bool &zeroMarker )
{
    if (!m_in) return false; 
    
    int32_t count = 0;
    while( count == 0 )
    {
        readNetworkValue( *m_in, count );
        if( !*m_in ) return false;
        if( count == 0 ) zeroMarker = true;
    }

    if( !dir.Read( *m_in ) )
    {
        return false;
    }

    return true;
}


bool RecordingParser::ReadHeaderPacket( Directory &matchHeader )
{
#ifdef _DEBUG
    AppDebugOut("ReadHeaderPacket: Starting to read header packet\n");
#endif
    
    bool zeroMarker = false;
    if( !ReadPacket( matchHeader, zeroMarker ) ) 
    {
#ifdef _DEBUG
        AppDebugOut("ReadHeaderPacket: Failed to read packet\n");
#endif
        return false;
    }

#ifdef _DEBUG
    AppDebugOut("ReadHeaderPacket: Successfully read packet, name='%s'\n", matchHeader.m_name.c_str() );
#endif

    if( strcmp( matchHeader.m_name.c_str(), "DCGR" ) != 0 )
    {
#ifdef _DEBUG
        AppDebugOut("ReadHeaderPacket: Invalid header name '%s', expected 'DCGR'\n", matchHeader.m_name.c_str() );
#endif
        return false;
    }

#ifdef _DEBUG
    AppDebugOut("ReadHeaderPacket: Valid DCGR header found\n");
#endif
    return true;
}

void RecordingParser::AddToHistory( Directory *dir )
{
    ServerToClientLetter *letter = new ServerToClientLetter;
    letter->m_data = dir;
    
    g_app->GetServer()->m_recordingHistory.PutDataAtEnd( letter );
}

//
// Function to extract game start sequence ID from DCGR header like DedCon does

int RecordingParser::ExtractGameStartFromHeader()
{
    //
    // Parse header using new direct file loading constructor

    Directory matchHeader;
    RecordingParser headerParser(m_filename, g_app->GetServer());
    if (!headerParser.ReadHeaderPacket(matchHeader))
    {
        AppDebugOut("Failed to read DCGR header\n");
        return 0;
    }

    //
    // Parse header metadata like DedCon does

    int seqIDStart = 0;        // Default to 0 if not found
    int seqIDEnd = 0x7fffffff;

    if (matchHeader.HasData("ssid"))
    {
        seqIDStart = matchHeader.GetDataInt("ssid");
#ifdef _DEBUG
        AppDebugOut("Found 'ssid' in header: %d\n", seqIDStart);
#endif
    }
    else
    {
#ifdef _DEBUG
        AppDebugOut("No 'ssid' field found in header, using default 0\n");
#endif
    }
    
    if (matchHeader.HasData("esid"))
    {
        seqIDEnd = matchHeader.GetDataInt("esid");
#ifdef _DEBUG
        AppDebugOut("Found 'esid' in header: %d\n", seqIDEnd);
#endif
    }
    
    if (matchHeader.HasData("cid"))
    {
        int clientID = matchHeader.GetDataInt("cid");
#ifdef _DEBUG
        AppDebugOut("Found 'cid' in header: %d\n", clientID);
#endif
    }
    
    if (matchHeader.HasData("sv"))
    {
        const char* serverVersion = matchHeader.GetDataString("sv");
#ifdef _DEBUG
        AppDebugOut("Found 'sv' in header: %s\n", serverVersion);
#endif
    }

    return seqIDStart;
}

//
// Parse function that is used by synctestrecordings

bool RecordingParser::Parse( bool debugPrint )
{
    Directory matchHeader;
    if( !ReadHeaderPacket( matchHeader  ) ) return false;

    bool zeroMarker;
    bool suppressUpdate = false;
    int lastRecordedSeqId = 0;
    int maxClientId = -1;
    std::unique_ptr<Directory> gameUpdates;
    int expectedGameCommandCount = 0;
    bool syncErrorInRecording = false;

    m_server->m_recordingSyncBytes.Empty();

    while( true )
    {
        Directory dir;
        if( !ReadPacket( dir, zeroMarker ) ) break;

        if( debugPrint )
        {
            dir.DebugPrint( 0 );
        }

        if( strcmp( dir.m_name.c_str(), "sq" ) == 0 )
        {
            suppressUpdate = false;

            if( expectedGameCommandCount != 0 )
            {
                AppDebugOut( "WARNING: Was expecting some more updates before starting a new game update\n" );
            }

            expectedGameCommandCount = dir.GetDataInt( "ct" );
            int nextSeqId = dir.GetDataInt( "z" );

            for( int i = 0; i < nextSeqId - lastRecordedSeqId - 1; ++i )
            {
                // Insert empty updates to catch up.

                Directory *empty = new Directory;
                empty->SetName( NET_DEFCON_MESSAGE );
                empty->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_UPDATE );

                Send(empty);
            }

            if( dir.HasData( NET_DEFCON_SYNCVALUE ) && !syncErrorInRecording )
            {
                m_server->m_recordingSyncBytes.PutData( dir.GetDataUChar( NET_DEFCON_SYNCVALUE ), nextSeqId );
            }

            lastRecordedSeqId = nextSeqId;

            gameUpdates.reset( new Directory );
            gameUpdates->SetName( NET_DEFCON_MESSAGE );
            gameUpdates->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_UPDATE );
        }
        else
        {
            if( strcmp( dir.m_name.c_str(), NET_DEFCON_MESSAGE ) == 0 )
            {
                if( dir.HasData( NET_DEFCON_COMMAND ) )
                {
                    const char *cmd = dir.GetDataString( NET_DEFCON_COMMAND );
                    if( *cmd == 'c' )
                    {
                        // Game update: bundle it into game updates.
                        gameUpdates->AddDirectory( new Directory( dir ) );
                        --expectedGameCommandCount;
                    }
                    else
                    {
                        // Server message
                        if( dir.HasData( NET_DEFCON_CLIENTID ) )
                        {
                            maxClientId = std::max( maxClientId, dir.GetDataInt( NET_DEFCON_CLIENTID ) );
                        }

                        if( strcmp( cmd, NET_DEFCON_NETSYNCERROR ) == 0 )
                        {
                            // The recording has a sync error. Record this so that we don't
                            // compare any sync values after this point.
                            AppDebugOut( "WARNING: Recording has a sync error at sequence id %d\n", lastRecordedSeqId );
                            syncErrorInRecording = true;
                        }

                        // Due to a bug in DEDCON, sometimes a server message can
                        // interrupt a sequence of updates.
                        if( gameUpdates.get() ) suppressUpdate = true;

                        // Management/Server command: send it separately.
                        Send( new Directory( dir ) );
                    }
                }
            }
        }

        if( expectedGameCommandCount == 0 && gameUpdates.get() &&
            gameUpdates->m_subDirectories.Size() > 0 )
        {
            if( !suppressUpdate ) Send( gameUpdates.release() );
        }
    }

    m_server->m_nextClientId = maxClientId + 1;
    m_server->m_replayingRecording = true;
    m_server->m_lastRecordedSeqId = lastRecordedSeqId;

    return true;
}

//
// Parse function that is used by the recording playback system

bool RecordingParser::ParseToHistory()
{
    if (!m_in) 
    {
        return false; 
    }
    
    Directory matchHeader;
    if( !ReadHeaderPacket( matchHeader  ) ) 
    {
#ifdef _DEBUG
        AppDebugOut("RecordingParser::ParseToHistory: Failed to read header packet\n");
#endif
        return false;
    }

    bool zeroMarker;
    bool suppressUpdate = false;
    int lastRecordedSeqId = 0;
    int maxClientId = -1;
    std::unique_ptr<Directory> gameUpdates;
    int expectedGameCommandCount = 0;

    while( true )
    {
        Directory dir;
        if( !ReadPacket( dir, zeroMarker ) ) break;

        if( strcmp( dir.m_name.c_str(), "sq" ) == 0 )
        {
            suppressUpdate = false;

            if( expectedGameCommandCount != 0 )
            {
                //
                // Insert any remaining updates from previous sequence
                
                if( gameUpdates.get() && gameUpdates->m_subDirectories.Size() > 0 )
                {
                    AddToHistory( gameUpdates.release() );
                }
                gameUpdates.reset();
            }

            expectedGameCommandCount = dir.GetDataInt( "ct" );
            int nextSeqId = dir.GetDataInt( "z" );

            //
            // Insert empty updates to catch up

            for( int i = 0; i < nextSeqId - lastRecordedSeqId - 1; ++i )
            {
                Directory *empty = new Directory;
                empty->SetName( NET_DEFCON_MESSAGE );
                empty->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_UPDATE );
                AddToHistory(empty);
            }

            lastRecordedSeqId = nextSeqId;

            gameUpdates.reset( new Directory );
            gameUpdates->SetName( NET_DEFCON_MESSAGE );
            gameUpdates->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_UPDATE );
        }
        else
        {
            if( strcmp( dir.m_name.c_str(), NET_DEFCON_MESSAGE ) == 0 )
            {
                if( dir.HasData( NET_DEFCON_COMMAND ) )
                {
                    const char *cmd = dir.GetDataString( NET_DEFCON_COMMAND );
                    if( *cmd == 'c' )
                    {
                        //
                        // Game update: bundle it into game updates
                        
                        gameUpdates->AddDirectory( new Directory( dir ) );
                        --expectedGameCommandCount;
                    }
                    else
                    {
                        //
                        // Server message
                        
                        if( dir.HasData( NET_DEFCON_CLIENTID ) )
                        {
                            maxClientId = std::max( maxClientId, dir.GetDataInt( NET_DEFCON_CLIENTID ) );
                        }

                        //
                        // Server message can interrupt a sequence of updates
                        
                        if( gameUpdates.get() ) suppressUpdate = true;

                        //
                        // Management/Server command: add to history
                        
                        AddToHistory( new Directory( dir ) );
                    }
                }
            }
        }

        if( expectedGameCommandCount == 0 && gameUpdates.get() &&
            gameUpdates->m_subDirectories.Size() > 0 )
        {
            if( !suppressUpdate ) AddToHistory( gameUpdates.release() );
        }
    }

    g_app->GetServer()->m_nextClientId = maxClientId + 1;
    g_app->GetServer()->m_replayingRecording = true;
    g_app->GetServer()->m_lastRecordedSeqId = lastRecordedSeqId;
    
    //
    // Set up recording playback state
    
    g_app->GetServer()->m_recordingPlaybackMode = true;
    g_app->GetServer()->m_recordingCurrentSeqId = 0;
    g_app->GetServer()->m_recordingStartSeqId = 0;
    g_app->GetServer()->m_recordingEndSeqId = lastRecordedSeqId;
    g_app->GetServer()->m_recordingLastAdvanceTime = GetHighResTime();

    //
    // Find and report the game start sequence ID
    
    int gameStartSeqId = ExtractGameStartFromHeader();

    return true;
}

void RecordingParser::Send( Directory *dir )
{
    ServerToClientLetter *letter = new ServerToClientLetter;
    letter->m_data = dir;
    m_server->SendLetter( letter );
}
