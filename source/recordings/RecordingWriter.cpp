#include "lib/universal_include.h"

#include "RecordingWriter.h"
#include "SecChecksum.h"

#include "lib/tosser/directory.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/debug/debug_utils.h"
#include "network/Server.h"
#include "network/ServerToClient.h"
#include "network/ClientToServer.h"
#include "network/network_defines.h"
#include "app/globals.h"
#include "app/app.h"
#include "app/game.h"

#include <fstream>
#include <sstream>
#include <cstring>
#include <ctime>
#include <cstdio>

#if defined(WIN32) || defined(_WIN32)
#include <direct.h>
#endif
#if !defined(WIN32) && !defined(_WIN32)
#include <unistd.h>
#endif

#if _MSC_VER == 1500
typedef int int32_t;
#endif

//
// Same byte order as RecordingParser/Directory for 4 byte length prefix

void writeLength( std::ostream &out, int32_t value )
{
#ifndef __BIG_ENDIAN__
    out.write( (const char *)&value, sizeof(value) );
#else
    const char *u = (const char *)&value;
    char data[sizeof(value)];

    for( size_t i = 0; i < sizeof(value); ++i )
    {
        data[i] = u[sizeof(value) - 1 - i];
    }

    out.write( data, sizeof(value) );
#endif
}

//
// Write one packet, 4 byte length (non zero) then Directory bytes

bool writePacket( std::ostream &out, const Directory &dir )
{
    std::ostringstream buf;
    dir.Write( buf );

    std::string data = buf.str();
    int32_t len = (int32_t)data.size();

    if( len <= 0 )
    {
        return true;
    }

    writeLength( out, len );
    out.write( data.data(), len );

    return out.good();
}

template<>
struct RecordingSourceTraits<Server>
{
    static Directory *GetLetterData( Server *s, int index )
    {
        ServerToClientLetter *letter = s->GetRecordingHistoryLetter( index );
        return letter ? letter->m_data : nullptr;
    }
    
    static void GetHeader( Server *s, DcgrHeader &h )
    {
        h.nextClientId   = s->m_nextClientId;
        h.gameStartSeqId = s->m_gameStartSeqId >= 0 ? s->m_gameStartSeqId : 0;
        h.endSeqId       = s->m_sequenceId;
        h.serverVersion  = APP_VERSION;
        h.pack           = false;
    }
    
    static bool GetSyncByte( Server *s, int seqId, unsigned char &out )
    {
        if( s->m_recordingSyncBytes.ValidIndex( seqId ) )
        {
            out = s->m_recordingSyncBytes[seqId];
            return true;
        }

        //
        // Fallback: try to get from connected clients

        for( int j = 0; j < s->m_clients.Size(); ++j )
        {
            if( s->m_clients.ValidIndex( j ) )
            {
                ServerToClient *s2c = s->m_clients[j];
                if( s2c->m_sync.ValidIndex( seqId ) )
                {
                    out = s2c->m_sync[seqId];
                    return true;
                }
            }
        }
        return false;
    }

    static bool IsReplaying( Server *s )
    {
        return s->m_replayingRecording;
    }
    
    static int GetGameStartSeqId( Server *s )
    {
        return s->m_gameStartSeqId;
    }
};


template<>
struct RecordingSourceTraits<ClientToServer>
{
    static Directory *GetLetterData( ClientToServer *c, int index )
    {
        return c->GetRecordingHistoryLetter( index );
    }
    
    static void GetHeader( ClientToServer *c, DcgrHeader &h )
    {
        h.nextClientId   = 0;  // Client doesn't assign client IDs
        h.gameStartSeqId = c->GetGameStartSeqId() >= 0 ? c->GetGameStartSeqId() : 0;
        h.serverVersion  = APP_VERSION;
        h.pack           = false;

        //
        // Get endSeqId from last history entry

        int size = c->GetRecordingHistorySize();
        if( size > 0 )
        {
            Directory *last = c->GetRecordingHistoryLetter( size - 1 );
            if( last && last->HasData( NET_DEFCON_SEQID ) )
            {
                h.endSeqId = last->GetDataInt( NET_DEFCON_SEQID );
            }
            else
            {
                h.endSeqId = 0;
            }
        }
        else
        {
            h.endSeqId = 0;
        }
    }
    
    static bool GetSyncByte( ClientToServer *c, int seqId, unsigned char &out )
    {
        if( c->m_recordingSyncBytes.ValidIndex( seqId ) )
        {
            out = c->m_recordingSyncBytes[seqId];
            return true;
        }
        return false;
    }
    
    static bool IsReplaying( ClientToServer *c )
    {
        return false;
    }
    
    static int GetGameStartSeqId( ClientToServer *c )
    {
        return c->GetGameStartSeqId();
    }
};

//
// Return number of game update packets in history for the 
// given sequence id. Used to fill "ct" in "sq" packets

template<typename SourceType>
int countGameUpdatesInSequence( SourceType *source, int seqId )
{
    int count = 0;
    int size = source->GetRecordingHistorySize();

    for( int i = 0; i < size; ++i )
    {
        Directory *data = RecordingSourceTraits<SourceType>::GetLetterData( source, i );
        if( !data )
        {
            continue;
        }
        if( data->GetDataInt( NET_DEFCON_SEQID ) != seqId )
        {
            continue;
        }
        if( !data->HasData( NET_DEFCON_COMMAND ) )
        {
            continue;
        }

        const char *cmd = data->GetDataString( NET_DEFCON_COMMAND );
        if( cmd && *cmd == 'c' )
        {
            ++count;
        }
    }
    return count;
}

//
// Build DCGR header from server state and write as first packet

static bool writeDcgrHeader( std::ostream &out, const DcgrHeader &h )
{
    Directory dcgr;
    dcgr.SetName( "DCGR" );
    dcgr.CreateData( "cid", h.nextClientId );
    dcgr.CreateData( "ssid", h.gameStartSeqId );
    dcgr.CreateData( "esid", h.endSeqId );
    dcgr.CreateData( "sv", h.serverVersion.c_str() );
    dcgr.CreateData( "pack", h.pack );
    return writePacket( out, dcgr );
}

DcgrHeader::DcgrHeader()
:   nextClientId( 0 ),
    gameStartSeqId( 0 ),
    endSeqId( 0 ),
    serverVersion( APP_VERSION ),
    pack( false )
{
}


template<typename SourceType>
bool RecordingWriter::WriteToFile( SourceType *source, const std::string &filename )
{
    if( !source || filename.empty() )
    {
        return false;
    }

    std::ofstream out( filename.c_str(), std::ios::binary | std::ios::out );
    if( !out.good() )
    {
#ifdef _DEBUG
        AppDebugOut( "RecordingWriter: Failed to open file for writing '%s'\n", filename.c_str() );
#endif
        return false;
    }

#ifdef _DEBUG
    AppDebugOut( "RecordingWriter: Writing to file '%s'\n", filename.c_str() );
#endif

    DcgrHeader header;
    RecordingSourceTraits<SourceType>::GetHeader( source, header );

    if( !writeDcgrHeader( out, header ) )
    {
#ifdef _DEBUG
        AppDebugOut( "RecordingWriter: Failed to write DCGR header\n" );
#endif
        return false;
    }

#ifdef _DEBUG
    AppDebugOut( "RecordingWriter: Wrote DCGR header\n" );
#endif

    //
    // Initialize security checksum for this recording

    static SecChecksum secChecksum;
    secChecksum = SecChecksum();

    //
    // Packet stream: 
    // for each sequence, one "sq" then the "d" packets in order.
    // Precompute game update count per seq for "ct".

    int lastSeqId = -1;
    int historySize = source->GetRecordingHistorySize();

    for( int i = 0; i < historySize; ++i )
    {
        Directory *data = RecordingSourceTraits<SourceType>::GetLetterData( source, i );
        if( !data )
        {
            continue;
        }

        int seqId = data->GetDataInt( NET_DEFCON_SEQID );

        if( seqId != lastSeqId )
        {
            lastSeqId = seqId;
            int ct = countGameUpdatesInSequence( source, seqId );
            Directory sq;
            sq.SetName( "sq" );
            sq.CreateData( "z", seqId );
            sq.CreateData( "ct", ct );
            sq.CreateData( "vs", (int)secChecksum.Get() );

            //
            // Sync value: 
            // Use the servers stored bytes if present, otherwise use a connected clients
            // m_sync for this sequence.

            unsigned char syncByte = 0;
            if( RecordingSourceTraits<SourceType>::GetSyncByte( source, seqId, syncByte ) )
            {
                sq.CreateData( NET_DEFCON_SYNCVALUE, syncByte );
            }
            
            if( !writePacket( out, sq ) )
            {
#ifdef _DEBUG
                AppDebugOut( "RecordingWriter: Failed to write 'sq' packet for seq %d\n", seqId );
#endif
                return false;
            }
        }

        //
        // Write the data packet and add it to the cumulative checksum

        if( !writePacket( out, *data ) )
        {
#ifdef _DEBUG
            AppDebugOut( "RecordingWriter: Failed to write packet at history index %d\n", i );
#endif
            return false;
        }

        //
        // Add this packet to the security checksum for next sequence

        secChecksum.Add( *data );
    }

#ifdef _DEBUG
    AppDebugOut( "RecordingWriter: Successfully wrote %d history entries to '%s'\n", historySize, filename.c_str() );
#endif

    return out.good();
}

template bool RecordingWriter::WriteToFile<Server>        ( Server *source, const std::string &filename );
template bool RecordingWriter::WriteToFile<ClientToServer>( ClientToServer *source, const std::string &filename );

template<typename SourceType>
bool RecordingWriter::SaveToRecordingsFolder( SourceType *source )
{
    if( !source || RecordingSourceTraits<SourceType>::IsReplaying( source ) || source->GetRecordingHistorySize() == 0 )
    {
        return false;
    }

    //
    // Only save .dcrec if the game actually makes it past the lobby

    if( RecordingSourceTraits<SourceType>::GetGameStartSeqId( source ) < 0 )
    {
#ifdef _DEBUG
        AppDebugOut( "RecordingWriter: Skipping save (game not started)\n" );
#endif
        return false;
    }

    if( !g_app->GetGame() )
    {
        return false;
    }

    int optIndex = g_app->GetGame()->GetOptionIndex( "ServerName" );
    if( optIndex < 0 )
    {
        return false;
    }

    const char *serverNameRaw = g_app->GetGame()->GetOption( "ServerName" )->m_currentString;
    char serverName[256];
    serverName[0] = '\0';
    if( serverNameRaw )
    {
        int j = 0;
        for( int i = 0; serverNameRaw[i] && j < (int)sizeof(serverName) - 1; ++i )
        {
            char c = serverNameRaw[i];
            if( c == ' ' || c == '\t' )
            {
                c = '_';
            }
            else if( c == '/' 
                  || c == '\\' 
                  || c == ':' 
                  || c == '*' 
                  || c == '?' 
                  || c == '"' 
                  || c == '<' 
                  || c == '>' 
                  || c == '|' 
                )
            {
                continue;
            }
            serverName[j++] = c;
        }
        serverName[j] = '\0';
    }

    //
    // This should never happen, but just in case...

    if( serverName[0] == '\0' )
    {
#ifdef _DEBUG
        AppDebugOut( "RecordingWriter: Server name is empty, falling back to Defcon'\n" );
#endif
        strncpy( serverName, APP_NAME, sizeof(serverName) - 1 );
    }

    time_t now = time( NULL );
    struct tm *tm = localtime( &now );
    char dateTime[32];
    if( tm )
    {
        snprintf( dateTime, sizeof(dateTime), "%04d_%02d_%02d_%02d-%02d",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min );
    }
    else
    {
#ifdef _DEBUG
        AppDebugOut( "RecordingWriter: Failed to get local time, writing placeholder timestamp'\n" );
#endif
        strncpy( dateTime, "0000_00_00_00-00", sizeof(dateTime) - 1 );
    }

    dateTime[sizeof(dateTime)-1] = '\0';

    char baseDir[512];
    baseDir[0] = '\0';

#if defined(WIN32) || defined(_WIN32)
    if( !_getcwd( baseDir, (int)sizeof(baseDir) ) )
#else
    if( !getcwd( baseDir, (int)sizeof(baseDir) ) )
#endif
    {
        return false;
    }

    std::string recordingsDir = std::string( baseDir ) + "/recordings";
    CreateDirectoryRecursively( recordingsDir.c_str() );

    char path[1024];
    snprintf( path, sizeof(path), "%s/%s_%s.dcrec", recordingsDir.c_str(), serverName, dateTime );
    path[sizeof(path)-1] = '\0';

    if( !WriteToFile( source, path ) )
    {
        return false;
    }

    AppDebugOut( "RecordingWriter: Saved recording to %s\n", path );

    return true;
}

template bool RecordingWriter::SaveToRecordingsFolder<Server>        ( Server *source );
template bool RecordingWriter::SaveToRecordingsFolder<ClientToServer>( ClientToServer *source );