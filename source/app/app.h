 
/*
 * ===
 * APP
 * ===
 *
 * Overall Glue object
 *
 */

#ifndef _included_app_h
#define _included_app_h

#include "network/Server.h"
#include "lib/preferences.h"

class AchievementTracker;
class ClientToServer;
class EarthData;
class Game;
class Interface;
class WorldRenderer;
class LobbyRenderer;
class MapRenderer;
class GlobeRenderer;
class NetLib;
class NetMutex;
class Server;
class SoundSystem;
class StatusIcon;
class Tutorial;
class World;
class RecordingFileDialog;
class RendererOverlay;
class SoundDebugOverlay;

extern bool g_hideUI;

class App
{
public:                 // STARTUP OPTIONS   
    int         m_desiredScreenW;
    int         m_desiredScreenH;
    int         m_desiredColourDepth;
    bool        m_desiredFullscreen;
    
    bool        m_gameRunning;
    float       m_gameStartTimer;
    bool        m_hidden;
    bool        m_inited;
    bool        m_globeMode;

    World       *m_world;

    AchievementTracker	*m_achievementTracker;

    bool        m_debugPrintClientLetters;
    bool        m_renderingEnabled;

    char        m_replayFilename[512];
    
    bool        m_showFps;
    bool        m_showRendererOverlay;
    bool        m_showSoundOverlay;
    int         m_currentFrames;
    int         m_framesPerSecond;
    float       m_frameCountTimer;
    bool        m_pendingWindowReinit;

private:
    WorldRenderer       *m_worldRenderer;
    MapRenderer         *m_mapRenderer;
    GlobeRenderer       *m_globeRenderer;
    LobbyRenderer       *m_lobbyRenderer;
    Interface           *m_interface;
    Game                *m_game;
    EarthData           *m_earthData;
    Server              *m_server;                  // Server process, can be NULL if client
    ClientToServer      *m_clientToServer;          // Clients connection to Server
    StatusIcon			*m_statusIcon;
    Tutorial            *m_tutorial;
    NetLib              *m_netLib;
    RendererOverlay     *m_rendererOverlay;
    SoundDebugOverlay   *m_soundOverlay;
    
    bool        m_mousePointerVisible;

private:
    void   ParseCommandLine();

public:
    App();
    ~App();

    void    MinimalInit();
	void    FinishInit();
    void    InitMetaServer();
    void    NotifyStartupErrors();
    void    Update();
    void    Render();
    void    RenderMouse();
    void    Shutdown();
   
    bool    InitServer();
    void    InitWorld(); 
    void    StartGame();
    void    StartTutorial( int _chapter );

    void    ShutdownCurrentGame();                  // Closes everything, ie client, server, world etc

    static  RendererType SelectRendererFromPreferences(Preferences* prefs);
    
    void    InitialiseWindow();                     // First time
    void    ReinitialiseWindow();                   // Window already exists, destroy first
    void    RequestWindowReinit();                  // Request deferred window recreation 
    void    OnWindowResized(int newWidth, int newHeight, int oldWidth, int oldHeight);
    
    void    InitStatusIcon();
    bool    IsGlobeMode();
    void    InitFonts();
    void    InitialiseTestBed();
	void    RestartAmbienceMusic();                 // Restart the Ambience sounds and Music if necessary (call after applying mods)
    
    void    RenderOwner();
    void    RenderTitleScreen();
    void    MergeStyles ();                         // Loads base styles, then merges mod styles, then user preference

    WorldRenderer   *GetWorldRenderer();
    MapRenderer     *GetMapRenderer();
    GlobeRenderer   *GetGlobeRenderer();
    LobbyRenderer   *GetLobbyRenderer();
    Interface       *GetInterface();
    Server          *GetServer();
    ClientToServer  *GetClientToServer();
    World           *GetWorld();
    EarthData       *GetEarthData();
    Game            *GetGame();
    StatusIcon      *GetStatusIcon();
    Tutorial        *GetTutorial();
	
	static const char *GetAuthKeyPath();
    static const char *GetPrefsPath();
    static const char *GetGameHistoryPath();
    static const char *GetLogDirectoryPath();

    void    HideWindow();   // panic button pressed in office mode
    bool    MousePointerIsVisible();
    void    SetMousePointerVisible(bool visible);

	void    SaveGameName();
	
	const char* GetReplayFilename() const;
	bool HasReplayFilename() const;


    bool                HasServerPrivileges()           { return m_server && m_server->ShouldAllowServerControls(); }
    void                ToggleHideUI();
};

void	ConfirmExit( const char *_parentWindowName );
void	AttemptQuitImmediately();
void	ToggleFullscreenAsync();

#endif
