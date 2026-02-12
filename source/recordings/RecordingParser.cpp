#include "lib/universal_include.h"

#include "RecordingParser.h"
#include "RecordingWriter.h"
#include "SecChecksum.h"

#include "lib/tosser/directory.h"
#include "lib/eclipse/eclipse.h"
#include "lib/eclipse/components/message_dialog.h"
#include "lib/preferences.h"
#include "network/network_defines.h"
#include "network/Server.h"
#include "app/app.h"
#include "app/globals.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <fstream>
#include <vector>
#include <sstream>

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

bool RecordingParser::ReadPacket( Directory &dir, bool &zeroMarker, SecChecksum *checksumToAdd, std::vector<char> *rawBytesOut )
{
    if (!m_in) return false; 
    
    int32_t count = 0;
    while( count == 0 )
    {
        readNetworkValue( *m_in, count );
        if( !*m_in ) return false;
        if( count == 0 ) zeroMarker = true;
    }

    //
    // Read raw packet bytes for checksum
    // Dedcon adds raw CompressedTag bytes, not Directory serialization

    std::vector<char> rawBytes( count );
    m_in->read( rawBytes.data(), count );
    
    if( !*m_in || m_in->gcount() != count )
    {
        return false;
    }

    //
    // Return raw bytes 
    
    if( rawBytesOut )
    {
        *rawBytesOut = rawBytes;
    }

    //
    // Add raw bytes to checksum
    
    if( checksumToAdd )
    {
        checksumToAdd->Add( reinterpret_cast<const unsigned char *>( rawBytes.data() ), count );
    }

    //
    // Parse the Directory from the raw bytes
    
    std::istringstream packetStream( std::string( rawBytes.data(), count ) );
    if( !dir.Read( packetStream ) )
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
    
    g_app->GetServer()->m_recordingParseBuffer.PutDataAtEnd( letter );
}

static void FillDcgrHeaderFromDirectory( Directory &matchHeader, DcgrHeader &header )
{
    header.gameStartSeqId = 0;
    header.endSeqId = 0x7fffffff;
    header.nextClientId = 0;
    header.serverVersion = "unknown";
    header.pack = false;

    if (matchHeader.HasData("ssid")) header.gameStartSeqId = matchHeader.GetDataInt("ssid");
    if (matchHeader.HasData("esid")) header.endSeqId = matchHeader.GetDataInt("esid");
    if (matchHeader.HasData("cid"))  header.nextClientId = matchHeader.GetDataInt("cid");
    if (matchHeader.HasData("sv"))   header.serverVersion = matchHeader.GetDataString("sv");
    if (matchHeader.HasData("pack")) header.pack = matchHeader.GetDataBool("pack");
}

//
// Function to extract game start sequence ID from DCGR header like DedCon does

bool RecordingParser::ExtractHeader(const std::string &filename, DcgrHeader &header, Server *server)
{
    //
    // Open the file and read just the header
    
    std::ifstream file(filename.c_str(), std::ios::in | std::ios::binary);
    if (!file.good())
    {
#ifdef _DEBUG
        AppDebugOut("RecordingParser::ExtractHeader: Failed to open file '%s'\n", filename.c_str());
#endif
        return false;
    }

    //
    // Parse header metadata like DedCon does
    
    Directory matchHeader;
    RecordingParser parser(file, filename, server);
    if (!parser.ReadHeaderPacket(matchHeader))
    {
#ifdef _DEBUG
        AppDebugOut("RecordingParser::ExtractHeader: Failed to read DCGR header\n");
#endif
        return false;
    }

    //
    // Extract all metadata fields from the header

    FillDcgrHeaderFromDirectory( matchHeader, header );

#ifdef _DEBUG
    AppDebugOut("RecordingParser::ExtractHeader: ssid=%d, esid=%d, cid=%d, sv=%s\n",
                header.gameStartSeqId, header.endSeqId, header.nextClientId, header.serverVersion.c_str());
#endif

    return true;
}

//
// Parse the recording file

bool RecordingParser::ParseRecording( bool sendDirectly, bool debugPrint, bool trackSyncBytes )
{
    Directory matchHeader;
    if( !ReadHeaderPacket( matchHeader  ) ) return false;

    FillDcgrHeaderFromDirectory( matchHeader, m_parsedHeader );

    //
    // Initialize security checksum for this recording

    bool enableChecksumValidation = g_preferences->GetInt( PREFS_RECORDING_ENABLE_CHECKSUM, 1 ) != 0;
    static SecChecksum secChecksum;
    static bool warnedAboutChecksum = false;
    static bool warnedAboutMissingChecksum = false;
    
    if( enableChecksumValidation )
    {
        secChecksum = SecChecksum();
        warnedAboutChecksum = false;
        warnedAboutMissingChecksum = false;
    }

    //
    // Initialize base recording variables

    bool zeroMarker;
    bool suppressUpdate = false;
    int lastRecordedSeqId = 0;
    int maxClientId = -1;
    std::unique_ptr<Directory> gameUpdates;
    int expectedGameCommandCount = 0;
    bool syncErrorInRecording = false;

    //
    // Initialize sync bytes tracking if enabled

    if( trackSyncBytes )
    {
        m_server->m_recordingSyncBytes.Empty();
    }

    while( true )
    {
        Directory dir;
        std::vector<char> rawPacketBytes;
        
        //
        // Read packet and capture raw bytes
        // sq packets are NOT added to checksum

        if( !ReadPacket( dir, zeroMarker, nullptr, &rawPacketBytes ) ) 
        {
            break;
        }
        
        bool isSqPacket = ( strcmp( dir.m_name.c_str(), "sq" ) == 0 );
        
        //
        // Add raw bytes to checksum for non sq

        if( enableChecksumValidation && !isSqPacket && rawPacketBytes.size() > 0 )
        {
            secChecksum.Add( reinterpret_cast<const unsigned char *>( rawPacketBytes.data() ), (unsigned int)rawPacketBytes.size() );
        }

        if( debugPrint )
        {
            dir.DebugPrint( 0 );
        }

        if( isSqPacket )
        {
            suppressUpdate = false;

            if( expectedGameCommandCount != 0 )
            {
                AppDebugOut( "WARNING: Was expecting some more updates before starting a new game update\n" );
            }

            expectedGameCommandCount = dir.GetDataInt( "ct" );
            int nextSeqId = dir.GetDataInt( "z" );

            //
            // Dedcon compatibility:
            // Comments must be added to checksum BEFORE validating vs

            if( enableChecksumValidation && dir.HasData( "c" ) )
            {
                std::string comment = dir.GetDataString( "c" );
                secChecksum.Add( comment );
            }

            //
            // Validate security checksum if present

            if( enableChecksumValidation )
            {
                if( dir.HasData( "vs" ) )
                {
                    int expectedChecksum = dir.GetDataInt( "vs" );
                    if( (int)secChecksum.Get() != expectedChecksum )
                    {
                        if( !warnedAboutChecksum )
                        {
                            #ifdef _DEBUG
                            AppDebugOut( "ERROR: Invalid security checksum at sequence %d. The recording has been modified or corrupted.\n", nextSeqId );
                            #else
                            AppDebugOut( "ERROR: Invalid security checksum. The recording has been modified or corrupted.\n");
                            #endif
                            warnedAboutChecksum = true;

                            //
                            // Show error dialog and refuse to load

                            MessageDialog *msg = new MessageDialog( "CHECKSUM VALIDATION FAILED",
                                                                    "This recording has an invalid security checksum.\n\n"
                                                                    "The file may have been corrupted during transfer\n"
                                                                    "or modified manually.\n\n"
                                                                    "Recording playback has been blocked for safety.",
                                                                    false,
                                                                    "dialog_checksum_failed", true );
                            EclRegisterWindow( msg );
                        }
                        return false;
                    }
                }
                else if( !warnedAboutMissingChecksum )
                {
                    AppDebugOut( "ERROR: No security checksum found in recording.\n" );
                    warnedAboutMissingChecksum = true;

                    //
                    // Show error dialog and refuse to load

                    MessageDialog *msg = new MessageDialog( "MISSING SECURITY CHECKSUM",
                                                            "This recording does not contain security checksums.\n\n"
                                                            "It may be an old recording from Dedcon 0.7 or earlier,\n"
                                                            "Recording playback has been blocked for safety.\n\n",
                                                            false,
                                                            "dialog_checksum_missing", true );
                    EclRegisterWindow( msg );
                    return false;
                }
            }

            for( int i = 0; i < nextSeqId - lastRecordedSeqId - 1; ++i )
            {
                //
                // Insert empty updates to catch up.

                Directory *empty = new Directory;
                empty->SetName( NET_DEFCON_MESSAGE );
                empty->CreateData( NET_DEFCON_COMMAND, NET_DEFCON_UPDATE );

                if( sendDirectly )
                    Send(empty);
                else
                    AddToHistory(empty);
            }

            if( trackSyncBytes && dir.HasData( NET_DEFCON_SYNCVALUE ) && !syncErrorInRecording )
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

                        if( trackSyncBytes && strcmp( cmd, NET_DEFCON_NETSYNCERROR ) == 0 )
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
                        if( sendDirectly )
                            Send( new Directory( dir ) );
                        else
                            AddToHistory( new Directory( dir ) );
                    }
                }
            }
        }

        if( expectedGameCommandCount == 0 && gameUpdates.get() &&
            gameUpdates->m_subDirectories.Size() > 0 )
        {
            if( !suppressUpdate )
            {
                if( sendDirectly )
                    Send( gameUpdates.release() );
                else
                    AddToHistory( gameUpdates.release() );
            }
        }
    }

    m_server->m_nextClientId = maxClientId + 1;
    m_server->m_replayingRecording = true;
    m_server->m_lastRecordedSeqId = lastRecordedSeqId;

    return true;
}

const DcgrHeader &RecordingParser::GetParsedHeader() const
{ 
    return m_parsedHeader; 
}

void RecordingParser::Send( Directory *dir )
{
    ServerToClientLetter *letter = new ServerToClientLetter;
    letter->m_data = dir;
    m_server->SendLetter( letter );
}
