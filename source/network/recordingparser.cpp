#include "lib/universal_include.h"

#include "recordingparser.h"

#include "lib/tosser/directory.h"
#include "lib/hi_res_time.h"
#include "network/network_defines.h"
#include "network/Server.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <fstream>

// added the preprocessor for visual studio, this means we can have the parsing files included in the configuration but will only get used if we define RECORDING_PARSING
#if RECORDING_PARSING

inline void reverseCopy( char *_dest, const char *_src, size_t count )
{
	_src += count;
	while (count--)
		*(_dest++) = *(--_src);
} 

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

// NEW: Constructor for direct file loading (WebAssembly-friendly)
RecordingParser::RecordingParser( const std::string &filename, Server *server )
    : m_filename( filename ),
      m_server( server ),
      m_ownStream( true )
{
    // Open file directly for WebAssembly and native builds
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
    AppDebugOut("RecordingParser: Opening file '%s'\n", filename.c_str());
#endif
    
    m_fileStream.open(filename.c_str(), std::ios::binary | std::ios::in);
    if (!m_fileStream.good())
    {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("RecordingParser: Failed to open file '%s'\n", filename.c_str());
#endif
        m_in = nullptr;
        return;
    }
    
    // Check file size for debugging
    m_fileStream.seekg(0, std::ios::end);
    size_t fileSize = m_fileStream.tellg();
    m_fileStream.seekg(0, std::ios::beg);
    
    m_in = &m_fileStream;
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
    AppDebugOut("RecordingParser: Successfully opened file '%s', size: %zu bytes\n", filename.c_str(), fileSize);
#endif
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
    if (!m_in) return false;  // NEW: Check for valid stream
    
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
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
    AppDebugOut("ReadHeaderPacket: Starting to read header packet\n");
#endif
    
    bool zeroMarker = false;
    if( !ReadPacket( matchHeader, zeroMarker ) ) 
    {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("ReadHeaderPacket: Failed to read packet\n");
#endif
        return false;
    }

#ifdef EMSCRIPTEN_DEBUG
    AppDebugOut("ReadHeaderPacket: Successfully read packet, name='%s'\n", matchHeader.m_name);
#endif

    if( strcmp( matchHeader.m_name, "DCGR" ) != 0 )
    {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("ReadHeaderPacket: Invalid header name '%s', expected 'DCGR'\n", matchHeader.m_name);
#endif
        return false;
    }

#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
    AppDebugOut("ReadHeaderPacket: Valid DCGR header found\n");
#endif
    return true;
}

void RecordingParser::AddToHistory( Directory *dir )
{
    ServerToClientLetter *letter = new ServerToClientLetter;
    letter->m_data = dir;
    
    m_server->m_recordingHistory.PutDataAtEnd( letter );
}

// NEW: Function to extract game start sequence ID from DCGR header (like DedCon does)
int RecordingParser::ExtractGameStartFromHeader()
{
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
    AppDebugOut("Extracting game start sequence ID from DCGR header...\n");
#endif
    
    // Parse header using new direct file loading constructor
    Directory matchHeader;
    RecordingParser headerParser(m_filename, m_server);
    if (!headerParser.ReadHeaderPacket(matchHeader))
    {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("Failed to read DCGR header\n");
#endif
        return 0;
    }

    // Parse header metadata like DedCon does
    int seqIDStart = 0;  // Default to 0 if not found
    int seqIDEnd = 0x7fffffff;
    
    // Check if the header contains metadata fields
    if (matchHeader.HasData("ssid"))
    {
        seqIDStart = matchHeader.GetDataInt("ssid");
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("Found 'ssid' in header: %d\n", seqIDStart);
#endif
    }
    else
    {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("No 'ssid' field found in header, using default 0\n");
#endif
    }
    
    if (matchHeader.HasData("esid"))
    {
        seqIDEnd = matchHeader.GetDataInt("esid");
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("Found 'esid' in header: %d\n", seqIDEnd);
#endif
    }
    
    if (matchHeader.HasData("cid"))
    {
        int clientID = matchHeader.GetDataInt("cid");
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("Found 'cid' in header: %d\n", clientID);
#endif
    }
    
    if (matchHeader.HasData("sv"))
    {
        const char* serverVersion = matchHeader.GetDataString("sv");
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("Found 'sv' in header: %s\n", serverVersion);
#endif
    }

    return seqIDStart;
}

bool RecordingParser::ParseToHistory()
{
    if (!m_in) 
    {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("RecordingParser::ParseToHistory: No valid input stream\n");
#endif
        return false;  // NEW: Check for valid stream
    }
    
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
    AppDebugOut("RecordingParser::ParseToHistory: Starting to parse recording to history\n");
#endif
    
    Directory matchHeader;
    if( !ReadHeaderPacket( matchHeader  ) ) 
    {
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
        AppDebugOut("RecordingParser::ParseToHistory: Failed to read header packet\n");
#endif
        return false;
    }
    
#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
    AppDebugOut("RecordingParser::ParseToHistory: Successfully read header packet\n");
#endif

    bool zeroMarker;
    bool suppressUpdate = false;
    int lastRecordedSeqId = 0;
    int maxClientId = -1;
    std::auto_ptr<Directory> gameUpdates;
    int expectedGameCommandCount = 0;

    while( true )
    {
        Directory dir;
        if( !ReadPacket( dir, zeroMarker ) ) break;

        if( strcmp( dir.m_name, "sq" ) == 0 )
        {
            suppressUpdate = false;

            if( expectedGameCommandCount != 0 )
            {
                // Insert any remaining updates from previous sequence
                if( gameUpdates.get() && gameUpdates->m_subDirectories.Size() > 0 )
                {
                    AddToHistory( gameUpdates.release() );
                }
                gameUpdates.reset();
            }

            expectedGameCommandCount = dir.GetDataInt( "ct" );
            int nextSeqId = dir.GetDataInt( "z" );

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
            if( strcmp( dir.m_name, NET_DEFCON_MESSAGE ) == 0 )
            {
                if( dir.HasData( NET_DEFCON_COMMAND ) )
                {
                    const char *cmd = dir.GetDataString( NET_DEFCON_COMMAND );
                    if( *cmd == 'c' )
                    {
                        // Game update: bundle it into game updates
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

                        // Server message can interrupt a sequence of updates
                        if( gameUpdates.get() ) suppressUpdate = true;

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

    m_server->m_nextClientId = maxClientId + 1;
    m_server->m_replayingRecording = true;
    m_server->m_lastRecordedSeqId = lastRecordedSeqId;
    
    // Set up recording playback state
    m_server->m_recordingPlaybackMode = true;
    m_server->m_recordingCurrentSeqId = 0;
    m_server->m_recordingStartSeqId = 0;
    m_server->m_recordingEndSeqId = lastRecordedSeqId;
    m_server->m_recordingLastAdvanceTime = GetHighResTime();

    // NEW: Find and report the game start sequence ID
    int gameStartSeqId = ExtractGameStartFromHeader();

#ifdef EMSCRIPTEN_PLAYBACK_TESTBED
    AppDebugOut("=== RECORDING ANALYSIS ===\n");
    AppDebugOut("Recording loaded: %s\n", m_filename.c_str());
    AppDebugOut("Total sequence IDs: %d (0 to %d)\n", lastRecordedSeqId + 1, lastRecordedSeqId);
    AppDebugOut("Game starts at sequence ID: %d\n", gameStartSeqId);
    AppDebugOut("=========================\n");
#endif

    return true;
}

#endif
