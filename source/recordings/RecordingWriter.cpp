#include "lib/universal_include.h"

#include "RecordingWriter.h"
#include "RecordingParser.h"
#include "SecChecksum.h"

#include "lib/tosser/directory.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/debug/debug_utils.h"
#include "lib/preferences.h"
#include "network/Server.h"
#include "network/ServerToClient.h"
#include "network/ClientToServer.h"
#include "network/network_defines.h"
#include "app/globals.h"
#include "app/app.h"
#include "app/game.h"
#include "world/world.h"

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
    
    //
    // Only write sync when we have a resolved value. The server stores
    // this in m_recordingSyncBytes only when all clients agreed.

    static bool GetSyncByte( Server *s, int seqId, unsigned char &out )
    {
        if( s->m_recordingSyncBytes.ValidIndex( seqId ) )
        {
            out = s->m_recordingSyncBytes[seqId];
            return true;
        }

        return false;
    }

    static bool WriteSyncValueThisSequence( Server *, int ) 
    { 
        return true; 
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

    //
    // Client has sync for every sequence it processed. To prevent massive file sizes, 
    // only write every 2 sequences. This somewhat matches Dedcon. Could be decreased
    // to 1in the future if we want more sync bytes at the expense of file size.

    static bool WriteSyncValueThisSequence( ClientToServer *, int seqId )
    {
        const int CLIENT_SYNC_CHECKPOINT_INTERVAL = 2;
        return ( seqId % CLIENT_SYNC_CHECKPOINT_INTERVAL ) == 0;
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
// High level save on leave function that decides what to do
// when its time to save a recording.

SaveRecordingResult RecordingWriter::SaveRecording()
{
    //
    // Restrictions:
    // - Servers can always save
    // - Spectators can save mid game
    // - Players can only save after the game has ended

    bool shouldAskToSave = false;
    bool isRecordingPlayback = ( g_app->GetServer() && g_app->GetServer()->IsRecordingPlaybackMode() );

    if( isRecordingPlayback )
    {
        shouldAskToSave = false;
    }
    else if( g_app->GetServer() )
    {
        shouldAskToSave = true;
    }
    else if( g_app->GetClientToServer() && g_app->GetClientToServer()->IsConnected() )
    {
        if( g_app->GetWorld() && g_app->GetGame() )
        {
            bool isSpectator = g_app->GetWorld()->AmISpectating();
            bool gameFinished = g_app->GetGame()->m_winner != -1;

            //
            // Only ask to save if spectator or game finished to
            // prevent cheating!!!

            shouldAskToSave = isSpectator || gameFinished;
        }
    }

    int autoSaveBehaviour = g_preferences->GetInt( PREFS_RECORDING_AUTO_SAVE_BEHAVIOUR, 0 );

    if( autoSaveBehaviour == 1 )
    {
        if( g_app->GetServer() && !isRecordingPlayback )
        {
            RecordingWriter writer;
            writer.SaveToRecordingsFolder( g_app->GetServer() );
        }
        return SaveRecordingResult_Leave;
    }

    if( shouldAskToSave && autoSaveBehaviour == 2 )
    {
        RecordingWriter writer;
        Server *server = g_app->GetServer();
        if( server )
        {
            writer.SaveToRecordingsFolder( server );
        }
        else
        {
            ClientToServer *client = g_app->GetClientToServer();
            if( client )
            {
                writer.SaveToRecordingsFolder( client );
            }
        }
        return SaveRecordingResult_Leave;
    }

    if( shouldAskToSave )
    {
        return SaveRecordingResult_ShowSaveDialog;
    }

    return SaveRecordingResult_Leave;
}

//
// True if this history entry is an empty update. Client expands server 
// NUMEMPTYUPDATES into one such entry per empty, collapse them back when writing.

static bool isEmptyUpdate( const Directory &dir )
{
    if( dir.m_name != NET_DEFCON_MESSAGE || dir.m_subDirectories.Size() != 0 )
    {   
        return false;
    }   
    if( !dir.HasData( NET_DEFCON_COMMAND ) || !dir.HasData( NET_DEFCON_SEQID ) )    
    {
        return false;
    }
    const char *cmd = dir.GetDataString( NET_DEFCON_COMMAND );
    return ( cmd && strcmp( cmd, NET_DEFCON_UPDATE ) == 0 );
}

//
// Precompute game update count per sequence in a single pass.

template<typename SourceType>
void precomputeGameUpdateCounts( SourceType *source, int maxSeqId, std::vector<int> &outCounts )
{
    outCounts.assign( maxSeqId + 1, 0 );
    int size = source->GetRecordingHistorySize();

    for( int i = 0; i < size; ++i )
    {
        Directory *data = RecordingSourceTraits<SourceType>::GetLetterData( source, i );
        if( !data || !data->HasData( NET_DEFCON_COMMAND ) )
        {
            continue;
        }
        if( data->GetDataString( NET_DEFCON_COMMAND ) && *data->GetDataString( NET_DEFCON_COMMAND ) != 'c' )
        {
            continue;
        }
        int seqId = data->GetDataInt( NET_DEFCON_SEQID );
        if( seqId >= 0 && seqId <= maxSeqId )
        {
            ++outCounts[seqId];
        }
    }
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

    std::vector<int> gameUpdateCountBySeq;
    precomputeGameUpdateCounts( source, header.endSeqId, gameUpdateCountBySeq );

    int lastSeqId = -1;
    int historySize = source->GetRecordingHistorySize();

    for( int i = 0; i < historySize; )
    {
        Directory *data = RecordingSourceTraits<SourceType>::GetLetterData( source, i );
        if( !data )
        {
            ++i;
            continue;
        }

        int seqId = data->GetDataInt( NET_DEFCON_SEQID );

        if( seqId != lastSeqId )
        {
            lastSeqId = seqId;
            int ct = ( seqId >= 0 && seqId
                             <= header.endSeqId
                             && (int)gameUpdateCountBySeq.size() > seqId )
                             ? gameUpdateCountBySeq[seqId] : 0;
            Directory sq;
            sq.SetName( "sq" );
            sq.CreateData( "z", seqId );
            sq.CreateData( "ct", ct );
            sq.CreateData( "vs", (int)secChecksum.Get() );

            //
            // Sync value:
            // Server: only for sequences where all clients.
            // Client: use the checkpoint interval, we dont need a sync byte every seqid

            unsigned char syncByte = 0;
            if( RecordingSourceTraits<SourceType>::GetSyncByte( source, seqId, syncByte ) &&
                RecordingSourceTraits<SourceType>::WriteSyncValueThisSequence( source, seqId ) )
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

        if( isEmptyUpdate( *data ) )
        {
            int runLength = 1;
            while( i + runLength < historySize )
            {
                Directory *next = RecordingSourceTraits<SourceType>::GetLetterData( source, i + runLength );
                if( !next || !isEmptyUpdate( *next ) )
                    break;
                if( next->GetDataInt( NET_DEFCON_SEQID ) != seqId + runLength )
                    break;
                ++runLength;
            }

            Directory collapsed;
            collapsed.SetName( NET_DEFCON_MESSAGE );
            collapsed.CreateData( NET_DEFCON_COMMAND, NET_DEFCON_UPDATE );
            collapsed.CreateData( NET_DEFCON_SEQID, seqId );
            collapsed.CreateData( NET_DEFCON_NUMEMPTYUPDATES, runLength );

            if( !writePacket( out, collapsed ) )
            {
#ifdef _DEBUG
                AppDebugOut( "RecordingWriter: Failed to write collapsed empty run at index %d\n", i );
#endif
                return false;
            }

            for( int k = 0; k < runLength; ++k )
            {
                Directory empty;
                empty.SetName( NET_DEFCON_MESSAGE );
                empty.CreateData( NET_DEFCON_COMMAND, NET_DEFCON_UPDATE );
                empty.CreateData( NET_DEFCON_SEQID, seqId + k );
                secChecksum.Add( empty );
            }

            i += runLength;
        }
        else
        {
            if( !writePacket( out, *data ) )
            {
#ifdef _DEBUG
                AppDebugOut( "RecordingWriter: Failed to write packet at history index %d\n", i );
#endif
                return false;
            }
            secChecksum.Add( *data );
            ++i;
        }
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

    //
    // Get save location from preferences (default is set at app startup)

    std::string recordingsDir;
    const char *saveLocation = g_preferences->GetString( PREFS_RECORDING_SAVE_LOCATION, "" );
    if( saveLocation && saveLocation[0] != '\0' )
    {
        recordingsDir = saveLocation;
    }
    else
    {
        recordingsDir = App::GetRecordingsDirectory();
    }

    std::string normalizedDir = recordingsDir;
#if defined(TARGET_MSVC)
    for( size_t i = 0; i < normalizedDir.length(); ++i )
    {
        if( normalizedDir[i] == '\\' )
        {
            normalizedDir[i] = '/';
        }
    }
#endif

    CreateDirectoryRecursively( normalizedDir.c_str() );

    //
    // Build filename based on naming format preference

    int namingFormat = g_preferences->GetInt( PREFS_RECORDING_NAMING_FORMAT, 1 );
    char path[1024];
    
#if defined(TARGET_MSVC)
    const char *pathSep = "\\";
#else
    const char *pathSep = "/";
#endif
    
    if( namingFormat == 0 )
    {
        snprintf( path, sizeof(path), "%s%s%s.dcrec", recordingsDir.c_str(), pathSep, dateTime );
    }
    else
    {
        snprintf( path, sizeof(path), "%s%s%s_%s.dcrec", recordingsDir.c_str(), pathSep, serverName, dateTime );
    }

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