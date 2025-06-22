#include "lib/universal_include.h"

#include "synctestrecordings.h"

#include "app.h"
#include "app/globals.h"

#include "interface/connecting_window.h"

#include "lib/filesys/filesys_utils.h"
#include "lib/gucci/window_manager.h"
#include "lib/hi_res_time.h"
#include "lib/netlib/net_lib.h"

#include "network/ClientToServer.h"
#include "network/Server.h"

#include <string.h>

// added the preprocessor for visual studio, this means we can have the parsing files included in the configuration but will only get used if we define RECORDING_PARSING
#if RECORDING_PARSING

void DumpSyncLogs( const char *syncId, bool force );

SyncTestRecordings::SyncTestRecordings()
:   m_enabled( false ),
    m_printRecording( false ),
    m_dumpSyncAt( -1 ),
    m_currentRecordingStartTime( 0.0 )
{
}


static void AddFileOrDirectory( const char *path, std::queue<std::string> &paths )
{
    if( IsDirectory( path ) )
    {
        std::auto_ptr< LList<char *> > files( ListDirectory( path, "*.dcrec" ) );
        if( files.get() )
        {
            while( files->Size() )
            {
                char *filePath = files->GetData( 0 );
                paths.push( std::string( filePath ) );
                delete[] filePath;
                files->RemoveData( 0 );
            }
        }
    }
    else if( DoesFileExist( path ) )
    {
        paths.push( std::string( path ) );
    }
    else
    {
        AppDebugOut( "SyncTestRecordings::Initialise - No such file or directory %s\n", path );
    }
}


static void DisableVSync()
{
#ifdef TARGET_MSVC
	typedef GLboolean(APIENTRY *FuncTypeWglSwapIntervalEXT)(GLint interval);

	FuncTypeWglSwapIntervalEXT setSwapInterval = (FuncTypeWglSwapIntervalEXT) wglGetProcAddress("wglSwapIntervalEXT");
	if( setSwapInterval ) (*setSwapInterval)(0);
#endif
}


void SyncTestRecordings::Initialise()
{
    // Parse the command line to see if we're enabled.
    // We're looking for "-test-recording" command line option
    // followed by the directory to search for *.dcrec files.

    for( int i = 1; i < g_argc; ++i )
    {
        if( strcmp( g_argv[i], "-test-recording" ) == 0 )
        {
            ++i;
            if( i >= g_argc ) break;

            AddFileOrDirectory( g_argv[i], m_paths );
        }
        else if( strcmp( g_argv[i], "-print-recording" ) == 0 )
        {
            m_printRecording = true;
        }
        else if( strcmp( g_argv[i], "-dump-sync-at" ) == 0 )
        {
            ++i;
            if( i >= g_argc ) break;

            m_dumpSyncAt = atoi( g_argv[i] );
        }
    }

    m_enabled = !m_paths.empty();

	if (m_enabled)
	{
		DisableVSync();
	}
}


std::string basename( const std::string &path )
{
    size_t index = path.find_last_of( "/\\" );
    if( index != std::string::npos ) return path.substr( index + 1 );    
    return path;
}


void SyncTestRecordings::Update()
{
    if( !m_enabled ) return;

    if( !g_app->GetServer() && m_paths.empty() )
    {
        g_app->Shutdown();
        return;
    }

    if( !g_app->GetServer() )
    {
        // Start a new recording playing.

        char ourIp[256];
        GetLocalHostIP( ourIp, sizeof(ourIp) );

        bool success = g_app->InitServer();

        if( success )
        {
            std::string recordingPath = m_paths.front();
            m_paths.pop();

            AppDebugOut( "\n\nRECORDING Load: %s\n", recordingPath.c_str() );
            if( !g_app->GetServer()->LoadRecording( recordingPath, m_printRecording ) )
            {
                g_app->ShutdownCurrentGame();
                return;
            }

            int ourPort = g_app->GetServer()->GetLocalPort();
            g_app->GetClientToServer()->ClientJoin( ourIp, ourPort );

            g_app->InitWorld();

            g_app->GetServer()->m_unlimitedSend = true;
            g_app->m_renderingEnabled = false;
            EclRegisterWindow( new ConnectingWindow() );

            m_currentRecordingFilename = recordingPath;
            m_currentRecordingStartTime = GetHighResTime();
        }
        else
        {
            AppDebugOut( "\n\nRECORDING Failed to restart server.\n" );
        }

        return;
    }

    if( g_app->GetServer()->IsRunning() && g_app->GetServer()->RecordingFinishedPlaying() )
    {
        // Check to see if the recording's sync bytes matched what our
        // local client calculated.
        int bytesCompared = 0;
        int firstMismatch;
        bool result = g_app->GetServer()->CheckRecordingSyncBytes( bytesCompared, firstMismatch );
        double elapsedTime = GetHighResTime() - m_currentRecordingStartTime;
        if( result )
        {
            AppDebugOut( "RECORDING Finished in %.1fs: %s. %d Sync Bytes matched.\n\n\n",
                         elapsedTime,
                         m_currentRecordingFilename.c_str(),
                         bytesCompared );
        }
        else
        {
            AppDebugOut( "RECORDING Finished in %.1fs: %s. %d Sync Bytes compared, MISMATCH at %d.\n\n\n",
                         elapsedTime,
                         m_currentRecordingFilename.c_str(),
                         bytesCompared,
                         firstMismatch );
        }
        g_app->ShutdownCurrentGame();
        g_app->m_renderingEnabled = true;
    }
        
    if( m_dumpSyncAt != -1 && g_lastProcessedSequenceId == m_dumpSyncAt )
    {

        char seqIdAsStr[32];
        snprintf( seqIdAsStr, sizeof(seqIdAsStr), "%d", m_dumpSyncAt );
        std::string syncId = basename(m_currentRecordingFilename) + "-" + seqIdAsStr;
        
        AppDebugOut("** Dumping sync log at seq id %d\n", m_dumpSyncAt );

        DumpSyncLogs( syncId.c_str(), true );
        m_dumpSyncAt = -1;
    }
    
}
#endif
