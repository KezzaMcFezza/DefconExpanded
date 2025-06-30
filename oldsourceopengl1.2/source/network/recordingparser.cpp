#include "lib/universal_include.h"

#include "recordingparser.h"

#include "lib/tosser/directory.h"
#include "network/network_defines.h"
#include "network/Server.h"

#include <algorithm>
#include <cstring>
#include <memory>

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
    : m_in( in ),
      m_filename( filename ),
      m_server( server )
{
}


RecordingParser::~RecordingParser()
{
}

#if _MSC_VER == 1500
typedef int int32_t;
#endif

bool RecordingParser::ReadPacket( Directory &dir, bool &zeroMarker )
{
    int32_t count = 0;
    while( count == 0 )
    {
        readNetworkValue( m_in, count );
        if( !m_in ) return false;
        if( count == 0 ) zeroMarker = true;
    }

    int position = m_in.gcount();
    if( !dir.Read( m_in ) )
    {
        AppDebugOut( "%s: Failed to read packet at position %d\n",
                     m_filename.c_str(),
                     position );
        return false;
    }

    return true;
}


bool RecordingParser::ReadHeaderPacket( Directory &matchHeader )
{
    bool zeroMarker = false;
    if( !ReadPacket( matchHeader, zeroMarker ) ) return false;

    int position = m_in.gcount();
    if( strcmp( matchHeader.m_name, "DCGR" ) != 0 )
    {
        AppDebugOut( "%s: Failed to read find DCGR marker at position %d\n",
                     m_filename.c_str(),
                     position );
        return false;
    }

    return true;
}


bool RecordingParser::Parse( bool debugPrint )
{
    Directory matchHeader;
    if( !ReadHeaderPacket( matchHeader  ) ) return false;

    bool finishedClientUpdate = false;
    bool zeroMarker;
    bool suppressUpdate = false;
    int lastRecordedSeqId = 0;
    int maxClientId = -1;
    std::auto_ptr<Directory> gameUpdates;
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

        if( strcmp( dir.m_name, "sq" ) == 0 )
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
            if( strcmp( dir.m_name, NET_DEFCON_MESSAGE ) == 0 )
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


void RecordingParser::Send( Directory *dir )
{
    ServerToClientLetter *letter = new ServerToClientLetter;
    letter->m_data = dir;
    m_server->SendLetter( letter );
}

#endif
