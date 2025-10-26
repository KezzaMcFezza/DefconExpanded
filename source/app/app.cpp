#include "lib/universal_include.h"

#include <stdio.h>
#include <float.h>
#include <time.h>

#include "lib/eclipse/eclipse.h"
#include "lib/gucci/window_manager.h"
#ifdef TARGET_MSVC
#include "lib/gucci/window_manager_win32.h"
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif
#include "lib/gucci/input.h"
#include "lib/resource/resource.h"
#include "lib/render2d/renderer.h"
#include "lib/render3d/renderer_3d.h"
#include "lib/render/styletable.h"
#include "lib/render/renderer_debug_menu.h"
#include "lib/debug_utils.h"
#include "lib/profiler.h"
#include "lib/hi_res_time.h"
#include "lib/language_table.h"
#include "lib/sound/soundsystem.h"
#include "lib/sound/sound_library_3d.h"
#include "lib/sound/sound_debug_overlay.h"
#include "lib/preferences.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/filesys/text_file_writer.h"
#include "lib/filesys/text_stream_readers.h"
#include "lib/filesys/file_system.h"
#include "lib/math/random_number.h"
#include "lib/metaserver/metaserver.h"
#include "lib/metaserver/authentication.h"
#include "lib/metaserver/matchmaker.h"
#include "lib/metaserver/metaserver_defines.h"
#include "lib/preferences.h"

#include "app/globals.h"
#include "app/app.h"
#include "app/game.h"
#include "app/defcon_soundinterface.h"
#include "app/statusicon.h"
#include "app/tutorial.h"
#include "app/modsystem.h"
#include "app/macutil.h"

#include "interface/interface.h"
#include "interface/authkey_window.h"
#include "interface/connecting_window.h"

#include "interface/components/message_dialog.h"
#include "interface/demo_window.h"

#if defined(TARGET_EMSCRIPTEN) || defined(REPLAY_VIEWER) || defined(REPLAY_VIEWER_DESKTOP)
#include "interface/recording_selection.h"
#endif

#ifdef SYNC_PRACTICE
#include "interface/lobby_window.h"
#include "interface/chat_window.h"
#endif

#include "renderer/map_renderer.h"
#include "renderer/lobby_renderer.h"

#include "network/ClientToServer.h"
#include "network/Server.h"

#include "world/world.h"
#include "world/earthdata.h"


#ifdef TRACK_MEMORY_LEAKS
#include "lib/memory_leak.h"
#endif

#if defined(TARGET_OS_LINUX) && !defined(TARGET_EMSCRIPTEN)
#include <fpu_control.h>
#include <sys/stat.h>
#endif

#ifdef TARGET_EMSCRIPTEN
#include <sys/stat.h>
#include <errno.h>
// For WASM, provide a stub mkdir implementation
int mkdir(const char *pathname, mode_t mode) {
    // In WASM, just return success - no actual directory creation needed
    return 0;
}
#endif

#include "lib/resource/image.h"
#include "lib/netlib/net_mutex.h"
#include "interface/recording_file_dialog.h"

#ifdef TARGET_OS_MACOSX
#include <sys/stat.h>
//#include "macosspecific/RetinaDisplaySupportHelper.h"
#endif

//
// if you enable this you will not be dissapointed :)

//#define ENABLE_FOR_FUN

App *g_app = NULL;

App::App()
:   m_interface(NULL),
    m_server(NULL),
    m_clientToServer(NULL),
    m_world(NULL),
    m_statusIcon(NULL),
    m_desiredScreenW(1024),
    m_desiredScreenH(768),
    m_desiredColourDepth(32),
    m_desiredFullscreen(false),
    m_gameRunning(false),
    m_hidden(false),
    m_gameStartTimer(-1.0),
    m_tutorial(NULL),
    m_mapRenderer(NULL),
    m_lobbyRenderer(NULL),
    m_earthData(NULL),
#if RECORDING_PARSING

#endif
    m_netLib(NULL),
	m_mousePointerVisible(false),
	m_inited(false),
    m_achievementTracker(NULL),
    m_debugPrintClientLetters(false),
    m_renderingEnabled(true),
    m_showFps(false),
    m_showDebugMenu(false),
    m_showSoundOverlay(false),
    m_currentFrames(0),
    m_framesPerSecond(0),
    m_frameCountTimer(1.0f),
    m_debugMenu(NULL),
    m_soundOverlay(NULL)
{
    // Initialize replay filename to empty
    m_replayFilename[0] = '\0';
}

App::~App()
{
#if RECORDING_PARSING

#endif

#ifdef TARGET_MSVC
    timeEndPeriod(1);
#endif

    delete m_clientToServer;
	delete m_game;
	delete m_earthData;
	delete m_interface;
	delete m_lobbyRenderer;
	delete m_mapRenderer;
	delete m_server;
	delete m_tutorial;
	delete m_statusIcon;
	delete g_inputManager;
	delete g_languageTable;
	delete g_preferences;
	delete g_fileSystem;
	delete g_profiler;
	delete g_resource;
	delete g_renderer;
	delete g_windowManager;
#ifdef TOGGLE_SOUND
	delete g_soundSystem;
#endif
	delete m_debugMenu;
	delete m_soundOverlay;
}

void App::InitMetaServer()
{
    //
    // Load our Auth Key

#if defined(RETAIL_DEMO)
    Anthentication_EnforceDemo();
#endif

#ifdef TARGET_EMSCRIPTEN
    //
    // For WebAssembly, fake successful authentication to enable all functionality

#ifdef EMSCRIPTEN_DEBUG
    AppDebugOut("WebAssembly: Faking authentication for local mode\n");
#endif
    
    char authKey[256];
    // Use a fake full auth key for WebAssembly that passes validation
    strcpy(authKey, "DEMOTE-OYMPOM-XRUHAT-TVSDNH-ZSR");
    Authentication_SetKey( authKey );

    //
    // Fake successful authentication result  
    Authentication_SetStatus( authKey, 1, AuthenticationAccepted );
    
#ifdef EMSCRIPTEN_DEBUG
    AppDebugOut("WebAssembly: Authentication faked as FULL USER (not demo)\n");
#endif
    
    // Skip real metaserver connection - it will fail anyway
    
    return;
#else

    char authKey[256];
    Authentication_LoadKey( authKey, App::GetAuthKeyPath() );
    Authentication_SetKey( authKey );
    
    int result = Authentication_SimpleKeyCheck( authKey );
    Authentication_RequestStatus( authKey );

    if( result < 0 )
    {
#if defined(RETAIL_DEMO)
        char authKeyNew[256];
        Authentication_GenerateKey( authKeyNew, true );
        Authentication_SetKey( authKeyNew );
        Authentication_SaveKey( authKeyNew, App::GetAuthKeyPath() );
    
        EclRegisterWindow( new WelcomeToDemoWindow() );
#else
        AuthKeyWindow *window = new AuthKeyWindow();
        EclRegisterWindow( window );
#endif
    }

    //
    // Connect to the MetaServer

    char *metaServerLocation = "metaserver.introversion.co.uk";
    //metaServerLocation = "10.0.0.4";
    int port = g_preferences->GetInt( PREFS_NETWORKMETASERVER );

    MetaServer_Initialise();

    MetaServer_Connect( metaServerLocation, PORT_METASERVER_LISTEN, port );

    MatchMaker_LocateService( metaServerLocation, PORT_METASERVER_LISTEN );
#endif
}

static void InitialiseFloatingPointUnit()
{
#if defined(TARGET_OS_LINUX) && !defined(TARGET_EMSCRIPTEN)
	/*
	This puts the X86 FPU in 64-bit precision mode.  The default
	under Linux is to use 80-bit mode, which produces subtle
	differences from FreeBSD and other systems, eg,
	(int)(1000*atof("0.3")) is 300 in 64-bit mode, 299 in 80-bit
	mode.
	*/

	fpu_control_t cw;
	_FPU_GETCW(cw);
	cw &= ~_FPU_EXTENDED;
	cw |= _FPU_DOUBLE;
	_FPU_SETCW(cw);
#endif
}


#if defined(TARGET_OS_MACOSX) || defined(TARGET_OS_LINUX)
static const std::string &GetHomeDirectory()
{
    static std::string result;
    if( !result.empty() ) return result;

    const char *home = getenv("HOME");
    if( home ) result = home;

    return result;
}

#endif

static const std::string &GetDefconDirectory()
{
    static std::string result;
    if( !result.empty() ) return result;

#if defined(TARGET_OS_LINUX)
    std::string home = GetHomeDirectory();
    if( !home.empty() )
    {
        result = home + "/.defcon/";
        mkdir(result.c_str(), 0770);
    }
#elif defined(TARGET_OS_MACOSX)
    std::string home = GetHomeDirectory();
    if( !home.empty() )
    {
        result = GetHomeDirectory() + "/Library/Application Support/uk.co.introversion.defcon/";
        mkdir( result.c_str(), 0777 );
    }
#endif
    return result;
}


static std::string GetPrefsDirectory()
{		
#if defined(TARGET_OS_MACOSX)
    return GetHomeDirectory() + "/Library/Preferences/";
#elif defined(TARGET_OS_LINUX) && !defined(TARGET_EMSCRIPTEN)
    return GetDefconDirectory();
#endif
	return "";
}


static std::string GetLogFilePath()
{
    return GetDefconDirectory() + "debug.txt";
}


void App::MinimalInit()
{
#ifdef DUMP_DEBUG_LOG
    AppDebugOutRedirect( GetLogFilePath().c_str() );
#endif
    AppDebugOut("Defcon %s built %s\n", APP_VERSION, __DATE__ );
		
    //
    // Turn everything on

    m_netLib = new NetLib();
    m_netLib->Initialise();

    ParseCommandLine();
    InitialiseFloatingPointUnit();
    InitialiseHighResTime();

    g_fileSystem = new FileSystem();
    
    //
    // load main.dat and sounds.dat in parallel
    
    LList<char *> archivesToLoad;
    archivesToLoad.PutData( newStr("main.dat") );
    archivesToLoad.PutData( newStr("sounds.dat") );
    
    //
    // load both archives in parallel
    // this uses multithreaded RAR decompression

    g_fileSystem->ParseArchivesParallel( &archivesToLoad );
    archivesToLoad.EmptyAndDeleteArray();

    g_preferences = new Preferences();

    g_preferences->Load( "data/prefs_default.txt" );
#ifdef TARGET_OS_MACOSX
	g_preferences->Load( "data/prefs_default_macosx.txt" );
#endif
#if defined(LANG_DEFAULT) && defined(PREF_LANG)
	g_preferences->Load( "data/prefs_default_" LANG_DEFAULT ".txt" );
#endif

    //
    // Load Language from Inno Setup
    g_preferences->Load( "DefconLang.ini" );

#ifdef TESTBED
    g_preferences->Load( "data/prefs_testbed.txt" );
#else
	g_preferences->Load( GetPrefsPath() );
#endif

    //
    // set default playername based on build type
    // better than multiple unique prefs files

#if defined(REPLAY_VIEWER) || defined(REPLAY_VIEWER_DESKTOP)
    g_preferences->SetString( "PlayerName", "ReplayClient" );
#elif defined(SYNC_PRACTICE)
    g_preferences->SetString( "PlayerName", "SyncPracticeClient" );
#endif

    g_modSystem = new ModSystem();
    g_modSystem->Initialise();
    
    g_languageTable = new LanguageTable();
    g_languageTable->SetAdditionalTranslation( "data/earth/cities_%s.txt" );
#if defined(LANG_DEFAULT) && defined(LANG_DEFAULT_ONLY_SELECTABLE)
	g_languageTable->SetDefaultLanguage( LANG_DEFAULT, true );
#elif defined(LANG_DEFAULT)
	g_languageTable->SetDefaultLanguage( LANG_DEFAULT );
#endif
    g_languageTable->Initialise();
	g_languageTable->LoadCurrentLanguage();
    
    g_resource = new Resource();
    g_styleTable = new StyleTable();
    g_styleTable->Load( "default.txt" );
    bool success = g_styleTable->Load( g_preferences->GetString(PREFS_INTERFACE_STYLE) );

    m_clientToServer = new ClientToServer();
    m_clientToServer->OpenConnections();

    m_game = new Game();

    m_earthData = new EarthData();
    m_earthData->Initialise();
    
    g_windowManager = WindowManager::Create();
    InitialiseWindow();
#ifdef TARGET_OS_MACOSX
    // closeRetinaDisplaySupport();
#endif
    
    g_renderer = new Renderer();
    InitFonts();
    
    g_resource->InitializeAtlases();

    m_debugMenu = new RendererDebugMenu(g_renderer);
    delete m_soundOverlay;
    m_soundOverlay = new SoundDebugOverlay();

    m_mapRenderer = new MapRenderer();
    m_mapRenderer->Init();

    m_lobbyRenderer = new LobbyRenderer();
    m_lobbyRenderer->Initialise();

    g_inputManager = InputManager::Create();

}

void App::FinishInit()
{
#ifdef TOGGLE_SOUND
	g_soundSystem = new SoundSystem();
    g_soundSystem->Initialise( new DefconSoundInterface() );            
    g_soundSystem->TriggerEvent( "Bunker", "StartAmbience" );
#endif

    m_interface = new Interface();
    m_interface->Init();
    // Need to query the metaserver to know if we should show the Buy Now button
    InitMetaServer();
    if (EclGetWindows()->Size() == 0)
    {
#if defined(SYNC_PRACTICE)

        //
        // Skip main menu and open lobby window directly
        // Start a local game server automatically for silo practice
        
        LobbyWindow *lobby = new LobbyWindow();
        bool success = lobby->StartNewServer();

        if( success )
        {
            ChatWindow *chat = new ChatWindow();
            chat->SetPosition( g_windowManager->WindowW()/2 - chat->m_w/2, 
                               g_windowManager->WindowH() - chat->m_h - 30 );
            EclRegisterWindow( chat );

            float lobbyX = g_windowManager->WindowW()/2 - lobby->m_w/2;
            float lobbyY = chat->m_y - lobby->m_h - 30;
            lobbyY = std::max( lobbyY, 0.0f );
            lobby->SetPosition(lobbyX, lobbyY);
            EclRegisterWindow( lobby );
        }
#elif defined(TARGET_EMSCRIPTEN) || defined(REPLAY_VIEWER) || defined(REPLAY_VIEWER_DESKTOP)

        //
        // Skip main menu and open recording selection window directly

        RecordingSelectionWindow *recordingWindow = new RecordingSelectionWindow();
        EclRegisterWindow(recordingWindow);
#ifdef EMSCRIPTEN_DEBUG
        AppDebugOut("Opened recording selection window directly and skipped main menu)\n");
#endif
#else
        m_interface->OpenSetupWindows();
#endif
    }

    InitStatusIcon();
    NotifyStartupErrors();
#if RECORDING_PARSING

#endif

	m_inited = true;
}

void App::NotifyStartupErrors()
{
#ifdef TARGET_EMSCRIPTEN

    //
    // In WebAssembly local mode, we're faking successful connections,
    // so don't show error dialogs about network issues
    
    return;
#else

    //
    // Inform the user of any Network difficulties
    // detected during MetaServer startup

    if( !MetaServer_IsConnected() )
    {
        char msgtext[1024];
#if defined(RETAIL_BRANDING_UK)
		sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_metaserver_failedconnect"), LANGUAGEPHRASE("website_support_retail_uk") );
#elif defined(RETAIL_BRANDING)
		sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_metaserver_failedconnect"), LANGUAGEPHRASE("website_support_retail") );
#elif defined(RETAIL_BRANDING_MULTI_LANGUAGE)
		sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_metaserver_failedconnect"), LANGUAGEPHRASE("website_support_retail_multi_language") );
#elif defined(TARGET_OS_MACOSX)
		sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_metaserver_failedconnect"), LANGUAGEPHRASE("website_support_macosx") );
#else
		sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_metaserver_failedconnect"), LANGUAGEPHRASE("website_support") );
#endif

        MessageDialog *msg = new MessageDialog( "FAILED TO CONNECT TO METASERVER", 
                                                msgtext, false,
                                                "dialog_metaserver_failedtitle", true );
        EclRegisterWindow( msg );
    }


    //
    // ClientToServer listener opened ok?

    if( !m_clientToServer->m_listener )
    {
        char msgtext[1024];
#if defined(RETAIL_BRANDING_UK)
		sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_retail_uk") );
#elif defined(RETAIL_BRANDING)
		sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_retail") );
#elif defined(RETAIL_BRANDING_MULTI_LANGUAGE)
		sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_retail_multi_language") );
#elif defined(TARGET_OS_MACOSX)
		sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support_macosx") );
#else
		sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_client_failedconnect"), LANGUAGEPHRASE("website_support") );
#endif

        MessageDialog *msg = new MessageDialog( "NETWORK ERROR",
                                                msgtext, false,
                                                "dialog_client_failedtitle", true );
        EclRegisterWindow( msg );        
    }
#endif
}


void App::InitFonts()
{
    g_renderer->SetDefaultFont( "zerothre" );
    g_renderer->SetFontSpacing( "zerothre", 0.1f );
    g_renderer->SetFontSpacing( "bitlow", 0.0f );
    g_renderer->SetFontSpacing( "lucon", 0.12f );
}


void App::ParseCommandLine()
{
    AppDebugOut("Parsing command line: %d arguments\n", g_argc);
    for( int i = 0; i < g_argc; ++i )
    {
        AppDebugOut("  argv[%d] = '%s'\n", i, g_argv[i]);
    }
    
    for( int i = 1; i < g_argc; ++i )
    {
        if( strcmp( g_argv[i], "-print-client-letters" ) == 0 )
        {
            m_debugPrintClientLetters = true;
        }
        else if( strcmp( g_argv[i], "-l" ) == 0 )
        {
            // Handle -l argument for loading replay files
            if( i + 1 < g_argc )
            {
                const char* filename = g_argv[i + 1];
                
                // Validate filename length
                if( strlen(filename) < sizeof(m_replayFilename) )
                {
                    // Copy the filename
                    strncpy( m_replayFilename, filename, sizeof(m_replayFilename) - 1 );
                    m_replayFilename[sizeof(m_replayFilename) - 1] = '\0';
                    
                    // Validate file extension (.dcrec)
                    const char* extension = strrchr( m_replayFilename, '.' );
                    if( extension && strcmp( extension, ".dcrec" ) == 0 )
                    {
                        AppDebugOut("Command line: Loading replay file '%s'\n", m_replayFilename);
                    }
                    else
                    {
                        AppDebugOut("Command line: Warning - '%s' does not have .dcrec extension\n", m_replayFilename);
                        // Keep the filename anyway - let the parser handle invalid files
                    }
                }
                else
                {
                    AppDebugOut("Command line: Error - replay filename too long: '%s'\n", filename);
                    m_replayFilename[0] = '\0'; // Clear filename
                }
                
                ++i; // Skip the filename argument
            }
            else
            {
                AppDebugOut("Command line: Error - no filename specified after -l\n");
            }
        }
    }
    
    // Debug output for final state
    if( strlen(m_replayFilename) > 0 )
    {
        AppDebugOut("Command line parsing complete: Replay filename set to '%s'\n", m_replayFilename);
    }
    else
    {
        AppDebugOut("Command line parsing complete: No replay filename specified\n");
    }
}


static void ReplaceStringFlagColorBit( char *caption, char flag, int colourDepth )
{
	if( colourDepth == 16 )
	{
		LPREPLACESTRINGFLAG( flag, LANGUAGEPHRASE("dialog_colourdepth_16"), caption );
	}
	else if( colourDepth == 24 )
	{
		LPREPLACESTRINGFLAG( flag, LANGUAGEPHRASE("dialog_colourdepth_24"), caption );
	}
	else if( colourDepth == 32 )
	{
		LPREPLACESTRINGFLAG( flag, LANGUAGEPHRASE("dialog_colourdepth_32"), caption );
	}
	else
	{
		char numberBit[128];
        strcpy( numberBit, LANGUAGEPHRASE("dialog_colourdepth_X") );
		LPREPLACEINTEGERFLAG( 'C', colourDepth, numberBit );
		LPREPLACESTRINGFLAG( flag, numberBit, caption );
	}
}


static void ReplaceStringFlagWindowed( char *caption, char flag, bool windowed )
{
	if( windowed )
	{
		LPREPLACESTRINGFLAG( flag, LANGUAGEPHRASE("dialog_windowed"), caption );
	}
	else
	{
		LPREPLACESTRINGFLAG( flag, LANGUAGEPHRASE("dialog_fullscreen"), caption );
	}
}


void App::InitialiseWindow()
{
    int screenW         = g_preferences->GetInt( PREFS_SCREEN_WIDTH );
    int screenH         = g_preferences->GetInt( PREFS_SCREEN_HEIGHT );
    int colourDepth     = g_preferences->GetInt( PREFS_SCREEN_COLOUR_DEPTH );
    int screenRefresh   = g_preferences->GetInt( PREFS_SCREEN_REFRESH );
    int zDepth          = g_preferences->GetInt( PREFS_SCREEN_Z_DEPTH );
	int antiAlias		= g_preferences->GetInt( PREFS_SCREEN_ANTIALIAS, 0 );
    bool windowed       = g_preferences->GetInt( PREFS_SCREEN_WINDOWED );
    bool borderless     = g_preferences->GetInt( PREFS_SCREEN_BORDERLESS );

    if( screenW == 0 || screenH == 0 )
    {
        g_windowManager->SuggestDefaultRes( &screenW, &screenH, &screenRefresh, &colourDepth );
        g_preferences->SetInt( PREFS_SCREEN_WIDTH, screenW );
        g_preferences->SetInt( PREFS_SCREEN_HEIGHT, screenH );
        g_preferences->SetInt( PREFS_SCREEN_REFRESH, screenRefresh );
        g_preferences->SetInt( PREFS_SCREEN_COLOUR_DEPTH, colourDepth );
    }

    if(m_gameRunning &&
       m_game->GetOptionValue("GameMode") == GAMEMODE_OFFICEMODE ) 
    {
        windowed = true;
    }


    bool success = g_windowManager->CreateWin( screenW, screenH, windowed, colourDepth, screenRefresh, zDepth, antiAlias, borderless, "DEFCON" );

    if( !success )
    {
        // Safety values
        int safeScreenW = 800;
        int safeScreenH = 600;
        bool safeWindowed = 1;
        int safeColourDepth = 16;
        int safeZDepth = 16;
        int safeScreenRefresh = 60;

		char caption[512];
        strcpy( caption, LANGUAGEPHRASE("dialog_screen_error_caption") );

		LPREPLACEINTEGERFLAG( 'W', screenW, caption );
		LPREPLACEINTEGERFLAG( 'H', screenH, caption );
		ReplaceStringFlagColorBit( caption, 'C', colourDepth );
		ReplaceStringFlagWindowed( caption, 'S', windowed );

		LPREPLACEINTEGERFLAG( 'w', safeScreenW, caption );
		LPREPLACEINTEGERFLAG( 'h', safeScreenH, caption );
		ReplaceStringFlagColorBit( caption, 'c', safeColourDepth );
		ReplaceStringFlagWindowed( caption, 's', safeWindowed );

		
		MessageDialog *dialog = new MessageDialog( "Screen Error", caption, false, "dialog_screen_error_title", true );
        EclRegisterWindow( dialog );
        dialog->m_x = 100;
        dialog->m_y = 100;


        // Go for safety values
        screenW = safeScreenW;
        screenH = safeScreenH;
        windowed = safeWindowed;
        colourDepth = safeColourDepth;
        zDepth = safeZDepth;
        screenRefresh = safeScreenRefresh;
		antiAlias = 0;
		borderless = false;

        success = g_windowManager->CreateWin(screenW, screenH, windowed, colourDepth, screenRefresh, zDepth, antiAlias, borderless, "DEFCON");
        AppReleaseAssert( success, "Failed to set screen mode" );

        g_preferences->SetInt( PREFS_SCREEN_WIDTH, screenW );
        g_preferences->SetInt( PREFS_SCREEN_HEIGHT, screenH );
        g_preferences->SetInt( PREFS_SCREEN_WINDOWED, windowed );
        g_preferences->SetInt( PREFS_SCREEN_COLOUR_DEPTH, colourDepth );
        g_preferences->SetInt( PREFS_SCREEN_Z_DEPTH, zDepth );
        g_preferences->SetInt( PREFS_SCREEN_REFRESH, screenRefresh );
        g_preferences->Save();
    }    

#if defined TARGET_MSVC && !defined WINDOWS_SDL
	WindowManagerWin32 *wm32 = (WindowManagerWin32*) g_windowManager;

	DWORD dwStyle = GetWindowLong( wm32->m_hWnd, GWL_STYLE );
	dwStyle &= ~(WS_MAXIMIZEBOX);
	SetWindowLong( wm32->m_hWnd, GWL_STYLE, dwStyle );

	HICON hIcon = LoadIcon( wm32->GethInstance(), MAKEINTRESOURCE(IDI_ICON1) );
	//SendMessage( wm32->m_hWnd, (UINT)WM_SETICON, ICON_SMALL, (LPARAM)hIcon );
	SendMessage( wm32->m_hWnd, (UINT)WM_SETICON, ICON_BIG, (LPARAM)hIcon );
#endif // TARGET_MSVC && !WINDOWS_SDL

    g_windowManager->HideMousePointer();
	SetMousePointerVisible(true);
}


void App::ReinitialiseWindow()
{
	g_windowManager->DestroyWin();

    //
    // The old OpenGL context was destroyed, so we need to recreate 
    // the Renderer with fresh shaders, VAO, VBO for the new context

    delete g_renderer;
    g_renderer = NULL;

    InitialiseWindow();
    
    g_renderer = new Renderer();
    
    g_resource->Restart();
    
    InitFonts();

    delete m_debugMenu;
    m_debugMenu = new RendererDebugMenu(g_renderer);
    delete m_soundOverlay;
    m_soundOverlay = new SoundDebugOverlay();

    m_mapRenderer->Init();
    m_interface->Init(); 
    
}


void App::Update()
{
#ifdef TARGET_EMSCRIPTEN
    static int updateCount = 0;
    updateCount++;
    if (updateCount % 120 == 0) {
        //AppDebugOut("WebGL: Update() called %d times\n", updateCount);
    }
#endif    

    //
    // global debug menu key handling

    //
    // F1 toggles FPS counter only

    if( g_keys[KEY_F1] && g_keyDeltas[KEY_F1] ) m_showFps = !m_showFps;
    
    //
    // F2 toggles debug menu

    if( g_keys[KEY_F2] && g_keyDeltas[KEY_F2] ) m_showDebugMenu = !m_showDebugMenu;

    //
    // F3 toggles sound overlay

    if( g_keys[KEY_F3] && g_keyDeltas[KEY_F3] ) m_showSoundOverlay = !m_showSoundOverlay;

    if( m_soundOverlay )
    {
        m_soundOverlay->Update(g_advanceTime);
    }
    
    //
    // Update the interface

    START_PROFILE("Interface");
    g_inputManager->Advance();
    m_interface->Update();
    END_PROFILE("Interface");
	

    if( m_tutorial )
    {
        m_tutorial->Update();
    }


    if( m_hidden )
    {
        m_statusIcon->Update();
    }
}


void App::Render()
{
#ifdef TARGET_EMSCRIPTEN
    static int renderCount = 0;
    renderCount++;
    if (renderCount % 120 == 0) {
        //AppDebugOut("WebGL: Render() called %d times\n", renderCount);
    }
#endif
    START_PROFILE("Render");

    //
    // begin frame for global draw call tracking

    g_renderer->BeginFrame();
    
#ifdef ENABLE_FOR_FUN
    g_renderer->ClearScreen( true, false );
    g_renderer->BeginScene();
    g_renderer->BeginMSAARendering();
#else
    g_renderer->BeginMSAARendering();
    g_renderer->ClearScreen( true, false );
    g_renderer->BeginScene();
#endif
    
    //
    // World map
    // Don't render anything if we are resyncing
    
    bool connectionScrewed = EclGetWindow("Connection Status");
    if( m_gameRunning && !connectionScrewed )
    {
        bool render = m_renderingEnabled;

#ifdef TESTBED
        render = g_keys[KEY_M];
#endif
        if( render )
        {
            GetMapRenderer()->Update();
            GetMapRenderer()->Render();
        }
    }


    //
    // Lobby graphics

    if( !m_gameRunning )
    {    
        GetLobbyRenderer()->Render();
    }


    g_renderer->Reset2DViewport();
    g_renderer->BeginScene();

    //
    // eclipse buttons and windows, but first check if UI should be hidden
#if RECORDING_PARSING
    extern bool g_hideUI;
    if( !g_hideUI )
    {
        GetInterface()->Render();
    }
#else
    GetInterface()->Render();
#endif
    START_PROFILE( "Eclipse GUI" );
    //g_renderer->SetBlendMode( Renderer::BlendModeNormal );
    
#if RECORDING_PARSING
    extern bool g_hideUI;
    if( !g_hideUI )
    {
        EclRender();
    }
#else
    EclRender();
#endif
    
    END_PROFILE( "Eclipse GUI" );
    
    //
    // update frame timing when either FPS counter or debug menu is shown

    if( m_showFps || m_showDebugMenu || m_showSoundOverlay )
    {
        static double s_lastRender = 0;
        static double s_biggest = 0;
        double timeNow = GetHighResTime();
        double lastFrame = timeNow - s_lastRender;        
        s_lastRender = timeNow;
        if( lastFrame > s_biggest ) s_biggest = lastFrame;
        
        m_frameCountTimer -= g_advanceTime;
        m_currentFrames++;
        if( m_frameCountTimer <= 0.0f )
        {
            m_frameCountTimer = 1.0f;
            m_framesPerSecond = m_currentFrames;
            m_currentFrames = 0;
            s_biggest = 0;
        }
        
        g_renderer->BeginTextBatch();

        //
        // show FPS counter if F1 was pressed

        if( m_showFps )
        {
            char fps[64];
            strcpy( fps, LANGUAGEPHRASE("dialog_mapr_fps") );
            LPREPLACEINTEGERFLAG( 'F', m_framesPerSecond, fps );
            g_renderer->TextSimple( 25, 25, White, 20.0f, fps );
        }
        
        //
        // Show debug menu if F2 was pressed

        if( m_showDebugMenu && m_debugMenu )
        {
            m_debugMenu->Update(lastFrame);
            m_debugMenu->RenderDebugMenu();
        }

        //
        // Show sound overlay if F3 was pressed

        if( m_showSoundOverlay && m_soundOverlay )
        {
            m_soundOverlay->Render();
        }
        
        g_renderer->EndTextBatch();
    }
    
    
    //
    // Mouse

#if RECORDING_PARSING
    extern bool g_hideUI;
    if( !g_hideUI )
    {
        GetInterface()->RenderMouse();
    }
#else
    GetInterface()->RenderMouse();
#endif
   

#ifdef SHOW_OWNER
    RenderOwner(); 
#endif

    //
    // Flip with FPS limiting

    START_PROFILE( "GL Flip" );
    
#if !defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN)
    static double s_lastFlipTime = 0.0;
    int fpsLimit = g_preferences->GetInt(PREFS_SCREEN_FPS_LIMIT, 0);
    
    if( fpsLimit == 69 )
    {
        // Use refresh rate from screen preferences, please dont flame me for this method 
        fpsLimit = g_preferences->GetInt(PREFS_SCREEN_REFRESH, 60);
    }
    
    if( fpsLimit > 0 )
    {
        double targetFrameTime = 1.0 / (double)fpsLimit;
        
        #ifdef TARGET_MSVC
            // High resolution timer for better Sleep() precision
            static bool s_timerResolutionSet = false;
            if( !s_timerResolutionSet )
            {
                timeBeginPeriod(1); // 1ms timer resolution
                s_timerResolutionSet = true;
            }
        #endif
        
        // Wait until target frame time has passed
        double currentTime = GetHighResTime();
        double timeSinceLastFlip = currentTime - s_lastFlipTime;
        
        if( timeSinceLastFlip < targetFrameTime )
        {
            double timeToWait = targetFrameTime - timeSinceLastFlip;
            
            #ifdef TARGET_MSVC
                // Use Sleep() for most of the wait, then busy-wait for precision
                if( timeToWait > 0.002 ) 
                {
                    double sleepTime = timeToWait - 0.001; // Leave 1ms for busy-wait
                    Sleep( (DWORD)(sleepTime * 1000.0) );
                }
                
                // Busy wait for the remaining time 
                double endTime = s_lastFlipTime + targetFrameTime;
                while( GetHighResTime() < endTime )
                {
                    // Hey guys its scarce here
                }
            #else
                // On Unix we use nanosleep
                if( timeToWait > 0.0001 )
                {
                    struct timespec ts;
                    ts.tv_sec = (time_t)timeToWait;
                    ts.tv_nsec = (long)((timeToWait - ts.tv_sec) * 1000000000.0);
                    nanosleep(&ts, NULL);
                }
            #endif
        }
        
        s_lastFlipTime = GetHighResTime();
    }
#endif
    
    g_renderer->EndMSAARendering();
    
    g_windowManager->Flip();
    END_PROFILE( "GL Flip" );   
    
    //
    // End frame for global draw call tracking

    g_renderer->EndFrame();
    
    END_PROFILE( "Render" );
}


void App::RenderTitleScreen()
{
    Image *gasMask = g_resource->GetImage( "graphics/gasmask.bmp" );
    
    float windowW = g_windowManager->WindowW();
    float windowH = g_windowManager->WindowH();


    float baseLine = windowH * 0.7f;

    g_renderer->SetBlendMode( Renderer::BlendModeNormal );
    g_renderer->SetDepthBuffer( false, false );
    

    //
    // Render explosions

    static float bombTimer = GetHighResTime() + 1;
    static float bombX = -1;
    
    if( GetHighResTime() > bombTimer )
    {
        bombX = frand(windowW);
        bombTimer = GetHighResTime() + 10 + frand(10);
    }

    if( bombX > 0 )
    {
        Image *blur = g_resource->GetImage( "graphics/explosion.bmp" );
        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );        

        float bombSize = 500;
        Colour col(255,155,155, 255*(bombTimer-GetHighResTime())/10 );
        g_renderer->EclipseSprite( blur, bombX, baseLine-300, bombSize, bombSize, col );
    }


    //
    // Render city

    float x = 0;
    
    AppSeedRandom(0);
    
    g_renderer->SetBlendMode( Renderer::BlendModeNormal );        

    g_renderer->EclipseRectFill( 0, baseLine, windowW, windowH-baseLine, Black );

    while( x < windowW )
    {        
        float thisW = 50+sfrand(30);
        float thisH = 40+frand(140);

        g_renderer->EclipseRectFill( x, baseLine - thisH, thisW, thisH, Black );

        x += thisW;
    }

}


void App::RenderOwner()
{
    static char s_message[1024];
    static bool s_dealtWith = false;

    if( !s_dealtWith )
    {
        // First try to read data/world-encode.dat
        // If it is present open it, encode it into world.dat, then delete it
        TextFileReader *encode = new TextFileReader( "data/world-encode.txt" );
        if( encode->IsOpen() )
        {
            encode->ReadLine();
            char *encodeMe = encode->GetRestOfLine();
            TextFileWriter encodeTo( "data/world.dat", true );
            encodeTo.printf( "%s\n", encodeMe );
        }
        delete encode;
        DeleteThisFile( "data/world-encode.txt" );


        // Now open the encoded world.dat file
        // Decrypt the contents for printing onto the screen
        TextFileReader reader( "data/world.dat" );
        AppAssert( reader.IsOpen() );
        reader.ReadLine();
        char *line = reader.GetRestOfLine();
        sprintf( s_message, "%s", line );
        char *nextLine = strchr( s_message, '\n' );
        if( nextLine ) *nextLine = '\x0';
        //strupr( s_message );

        s_dealtWith = true;
    }

    g_renderer->Text( 10, g_windowManager->WindowH() - 20, Colour(255,50,50,255), 20, s_message );
}


void App::Shutdown()
{       
    //
    // Shut down each sub system

    ShutdownCurrentGame();

    Authentication_Shutdown();
    MetaServer_Disconnect();

    m_interface->Shutdown();
    delete m_interface;
    m_interface = NULL;

    g_renderer->Shutdown();
    delete g_renderer;
    g_renderer = NULL;
  
    m_statusIcon->RemoveIcon();
    delete m_statusIcon;
    m_statusIcon = NULL;

    g_preferences->Save();

    //
    // save the last recording folder to preferences

    RecordingFileDialog::SaveLastFolderToPreferences();

#ifdef TRACK_MEMORY_LEAKS
    AppPrintMemoryLeaks( "memoryleaks.txt" );
#endif

    delete m_netLib;
    m_netLib = NULL;

    m_inited = false;
        
    exit(0);
}


void App::StartGame()
{    
    int numSpectators = g_app->GetGame()->GetOptionValue( "MaxSpectators" );
    if( numSpectators == 0 )
    {
        m_clientToServer->StopIdentifying();
    }

    bool connectingWindowOpen = EclGetWindow( "Connection Status" );
    
    EclRemoveAllWindows();

#ifdef TOGGLE_SOUND
    g_soundSystem->TriggerEvent( "Music", "StartMusic" );
#endif

    m_interface->OpenGameWindows();
    
    if( connectingWindowOpen ) EclRegisterWindow( new ConnectingWindow() );

    int randSeed = 0;
    for( int i = 0; i < m_world->m_teams.Size(); ++i )
    {
        randSeed += m_world->m_teams[i]->m_randSeed;
    }
    syncrandseed( randSeed );
    AppDebugOut( "App RandSeed = %d\n", randSeed );

    GetWorld()->LoadNodes();
    GetWorld()->AssignCities();

    if( GetGame()->GetOptionValue("GameMode") == GAMEMODE_OFFICEMODE )
    {
#ifdef TOGGLE_SOUND
        g_soundLibrary3d->SetMasterVolume(0);
#endif
        if( !g_windowManager->Windowed() )
        {
            ReinitialiseWindow();
        }
    }
    
    if( GetGame()->GetOptionValue("MaxGameRealTime") != 0.0f )
    {
        GetGame()->m_maxGameTime = GetGame()->GetOptionValue("MaxGameRealTime");
        GetGame()->m_maxGameTime *= 60;
    }

    //
    // Set radar sharing and ceasefire defaults

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *firstTeam = g_app->GetWorld()->m_teams[i];

        for( int j = 0; j < g_app->GetWorld()->m_teams.Size(); ++j )
        {
            if( i != j )
            {
                Team *secondTeam = g_app->GetWorld()->m_teams[j];
            
                if( g_app->GetWorld()->IsFriend(firstTeam->m_teamId, secondTeam->m_teamId) )
                {
                    firstTeam->m_sharingRadar[secondTeam->m_teamId] = true;
                    firstTeam->m_ceaseFire[secondTeam->m_teamId] = true;
                    firstTeam->m_alwaysSolo = false;
                }
            }            
        }
    }


    //
    // Set game speeds

    int minSpeedSetting = g_app->GetGame()->GetOptionValue("SlowestSpeed");
    int minSpeed = ( minSpeedSetting == 0 ? 0 :
                     minSpeedSetting == 1 ? 1 :
                     minSpeedSetting == 2 ? 5 :
                     minSpeedSetting == 3 ? 10 :
                     minSpeedSetting == 4 ? 20 : 20 );

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        if( team->m_type != Team::TypeAI )
        {
            team->m_desiredGameSpeed = max( team->m_desiredGameSpeed, minSpeed );
        }
    }

	m_gameRunning = true;
    
    //
    // invalidate globe VBOs if the globe radius has changed from default
    
    if (g_preferences->GetFloat(PREFS_GLOBE_SIZE, 1.0f) != 1.0f) {
        g_renderer3d->InvalidateCached3DVBO("GlobeCoastlines");
        g_renderer3d->InvalidateCached3DVBO("GlobeBorders");
    }
}


void App::StartTutorial( int _chapter )
{
    m_tutorial = new Tutorial( _chapter );
}


bool App::InitServer()
{
  	m_server = new Server();    
	bool result = m_server->Initialise();


    //
    // Inform the user of the problem, and shut it down

    if( !result )
    {
        delete m_server;
        m_server = NULL;

		char msgtext[1024];
#if defined(RETAIL_BRANDING_UK)
		sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_server_failedconnect"), LANGUAGEPHRASE("website_support_retail_uk") );
#elif defined(RETAIL_BRANDING)
		sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_server_failedconnect"), LANGUAGEPHRASE("website_support_retail") );
#elif defined(RETAIL_BRANDING_MULTI_LANGUAGE)
		sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_server_failedconnect"), LANGUAGEPHRASE("website_support_retail_multi_language") );
#elif defined(TARGET_OS_MACOSX)
		sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_server_failedconnect"), LANGUAGEPHRASE("website_support_macosx") );
#else
		sprintf( msgtext, "%s%s", LANGUAGEPHRASE("dialog_server_failedconnect"), LANGUAGEPHRASE("website_support") );
#endif

        MessageDialog *msg = new MessageDialog( "FAILED TO START SERVER",
                                                msgtext, false, 
                                                "dialog_server_failedtitle", true );
        EclRegisterWindow( msg );        
        return false;
    }


    //
    // Everything Ok

    int len = min<int>( (int) m_game->m_options[0]->m_max, sizeof( m_game->m_options[0]->m_currentString ) );
    strncpy( m_game->m_options[0]->m_currentString, g_preferences->GetString( "ServerName1", "" ), len );
    m_game->m_options[0]->m_currentString[ len - 1 ] = '\x0';
    SafetyString( m_game->m_options[0]->m_currentString );
    ReplaceExtendedCharacters( m_game->m_options[0]->m_currentString );

    if( strlen( m_game->m_options[0]->m_currentString ) == 0 || 
        strcmp( m_game->m_options[0]->m_currentString, g_languageTable->LookupBasePhrase( "gameoption_New_Game_Server" ) ) == 0 )
    {
        if( g_languageTable->DoesTranslationExist( "gameoption_New_Game_Server" ) || 
            g_languageTable->DoesPhraseExist( "gameoption_New_Game_Server" ) )
        {
            strncpy( m_game->m_options[0]->m_currentString, LANGUAGEPHRASE("gameoption_New_Game_Server"), len );
            m_game->m_options[0]->m_currentString[ len - 1 ] = '\x0';
            SafetyString( m_game->m_options[0]->m_currentString );
            ReplaceExtendedCharacters( m_game->m_options[0]->m_currentString );
        }
    }

    return true;
}

void App::ShutdownCurrentGame()
{
    //
    // Stop the MetaServer registration

    if( MetaServer_IsRegisteringOverLAN() )
    {
        MetaServer_StopRegisteringOverLAN();
    }

    if( MetaServer_IsRegisteringOverWAN() )
    {
        MetaServer_StopRegisteringOverWAN();
    }
    
	if( m_gameRunning )
    {
        EclRemoveAllWindows();
    }

    //
    // If there is a client, shut it down now
    //
    // be sure to toggle globe mode off first
    // to prevent an m_world assertion :)

    if( m_mapRenderer->m_3DGlobeMode )
    {
        m_mapRenderer->m_3DGlobeMode = false;
    }

    if( m_world )
    {
        delete m_world;
        m_world = NULL;
    }

    if( m_clientToServer->IsConnected() )
    {
        m_clientToServer->ClientLeave(true);
    }
    else
    {
        m_clientToServer->ClientLeave(false);
    }
    

    //
    // If there is a server, shut it down now
    
    if( m_server )
    {
        m_server->Shutdown();
        delete m_server;
        m_server = NULL;
    }

    if( m_tutorial )
    {
        delete m_tutorial;
        m_tutorial = NULL;
    }

    m_gameRunning = false;
    delete m_game;
    m_game = new Game();


    g_startTime = FLT_MAX;
    g_gameTime = 0.0f;
    g_advanceTime = 0.0f;
    g_lastServerAdvance = 0.0f;
    g_predictionTime = 0.0f;
    g_lastProcessedSequenceId = -2;                         // -2=not yet ready to begin. -1=ready for first update (id=0)

#ifdef TOGGLE_SOUND
    g_soundSystem->StopAllSounds( SoundObjectId(), "StartMusic StartMusic" );
#endif

    m_mapRenderer->Reset();
    m_mapRenderer->m_renderEverything = false;
    m_gameStartTimer = -1.0f;
	GetInterface()->SetMouseCursor();
	SetMousePointerVisible(true);
}


void App::InitWorld()
{
    m_game->ResetOptions();
    
    m_world = new World();
    m_world->Init();
}


MapRenderer *App::GetMapRenderer()
{
    AppAssert( m_mapRenderer );
    return m_mapRenderer;
}


LobbyRenderer *App::GetLobbyRenderer()
{
    AppAssert( m_lobbyRenderer );
    return m_lobbyRenderer;
}


Interface *App::GetInterface()
{
    AppAssert( m_interface );
    return m_interface;
}

ClientToServer *App::GetClientToServer()
{
    AppAssert( m_clientToServer );
    return m_clientToServer;
}

Server *App::GetServer()
{
    return m_server;
}

World *App::GetWorld()
{
    AppAssert( m_world );
    return m_world;
}


EarthData *App::GetEarthData()
{
    AppAssert(m_earthData);
    return m_earthData;
}


Game *App::GetGame()
{
    AppAssert( m_game );
    return m_game;
}

StatusIcon *App::GetStatusIcon()
{
    return m_statusIcon;
}

Tutorial *App::GetTutorial()
{
    return m_tutorial;
}

const char *App::GetPrefsPath()
{
    static std::string result;

    if( result.empty() )
    {
#if defined(TARGET_OS_MACOSX)
        result = GetPrefsDirectory() + "uk.co.introversion.defcon.prefs";
#else 
        result = GetPrefsDirectory() + "preferences.txt";
#endif
    }

    return result.c_str();
}


const char *App::GetAuthKeyPath()
{
    static std::string result;

    if( result.empty() )
    {
#if defined(TARGET_OS_MACOSX)
        result = GetPrefsDirectory() + "uk.co.introversion.defcon.authkey";
#elif defined(TARGET_OS_LINUX)
        result = GetPrefsDirectory() + "authkey";
#else
        result = "authkey";
#endif
    }

    return result.c_str();
}


const char *App::GetGameHistoryPath()
{
    static std::string result;

    if( result.empty() ) result = GetDefconDirectory() + "game-history.txt";

    return result.c_str();
}


const char *App::GetLogDirectoryPath()
{
    static std::string result;

    if( result.empty() ) result = GetDefconDirectory();

    return result.c_str();
}


void App::HideWindow()
{
#ifdef TARGET_OS_MACOSX
	// Mac OS X doesn't like it if we try to hide a fullscreen app.
	if (!g_windowManager->Windowed())
	{
		g_preferences->SetInt(PREFS_SCREEN_WINDOWED, true);
		ReinitialiseWindow();
	}
#endif
	
    g_windowManager->HideWin();
    if( GetStatusIcon() )
    {
        GetStatusIcon()->SetIcon(STATUS_ICON_MAIN);
        m_hidden = true;
    }
}

void App::InitStatusIcon()
{
    m_statusIcon = StatusIcon::Create();
    if ( m_statusIcon )
		m_statusIcon->SetCaption( "" );
}

bool App::MousePointerIsVisible()
{
	return m_mousePointerVisible;
}

void App::SetMousePointerVisible(bool visible)
{
	m_mousePointerVisible = visible;
}

void App::SaveGameName()
{
	// Check m_server to be sure it's a game created by us.
	if( m_server && m_game )
	{
		int serverNameIndex = m_game->GetOptionIndex( "ServerName" );
		if( serverNameIndex != -1 )
		{
			const char *newServerName = m_game->GetOption( "ServerName" )->m_currentString;
			if( strcmp( newServerName, LANGUAGEPHRASE("gameoption_New_Game_Server") ) == 0 )
			{
				newServerName = g_languageTable->LookupBasePhrase( "gameoption_New_Game_Server" );
			}

			const char *serverName1 = g_preferences->GetString( "ServerName1", "" );
			const char *serverName2 = g_preferences->GetString( "ServerName2", "" );
			const char *serverName3 = g_preferences->GetString( "ServerName3", "" );
			const char *serverName4 = g_preferences->GetString( "ServerName4", "" );

			if( strcmp( serverName1, newServerName ) != 0 )
			{
				g_preferences->SetString( "ServerName5", serverName4 );
				g_preferences->SetString( "ServerName4", serverName3 );
				g_preferences->SetString( "ServerName3", serverName2 );
				g_preferences->SetString( "ServerName2", serverName1 );
				g_preferences->SetString( "ServerName1", newServerName );
			}
		}
	}
}

void App::RestartAmbienceMusic()
{
#ifdef TOGGLE_SOUND
	g_soundSystem->TriggerEvent( "Bunker", "StartAmbience" );
	if( g_app->m_gameRunning )
	{
		g_soundSystem->TriggerEvent( "Music", "StartMusic" );
	}
#endif
}

const char* App::GetReplayFilename() const
{
	return m_replayFilename;
}

bool App::HasReplayFilename() const
{
	return strlen(m_replayFilename) > 0;
}
