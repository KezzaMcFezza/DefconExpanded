#include "systemiv.h"

#include <limits.h>

#include "lib/debug/debug_utils.h"
#include "lib/preferences.h"

#include "imgui.h"
#include "input.h"
#include "crashpad.h"
#include "window_manager.h"
#include "window_manager_d3d11.h"
#include "window_manager_opengl.h"
#include "backends/imgui_impl_sdl2.h"

#ifdef TARGET_OS_LINUX
#include "binreloc.h"
#include "spawn.h"
#endif

#ifdef TARGET_OS_MACOSX
#include "macosxlibrary.h"
#endif

#ifdef RENDERER_OPENGL
#include <SDL2/SDL.h>
#elif defined(RENDERER_DIRECTX11)
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#endif

#ifdef TARGET_MSVC
#include <shellapi.h>
#include <windows.h>
#endif

WindowManager *g_windowManager = NULL;
int g_argc = 0;
char **g_argv = NULL;

WindowManager::WindowManager()
:	m_mousePointerVisible(true),
	m_mouseOffsetX(INT_MAX),
	m_mouseCaptured(false),
    m_highDPIScaleX(1.0f),
    m_highDPIScaleY(1.0f),
    m_secondaryMessageHandler(NULL),
    m_windowResizeHandler(NULL),
    m_sdlWindow(nullptr),
    m_windowDisplayIndex(0),
    m_isMaximized(false),
    m_tryingToCaptureMouse(false)
{
	InitializeSDL();
}


WindowManager::~WindowManager()
{
	CleanupResolutions();
	SDL_Quit();
}


void WindowManager::InitializeSDL()
{
	AppReleaseAssert(SDL_Init(SDL_INIT_VIDEO) == 0, "Couldn't initialise SDL");
	
	m_windowDisplayIndex = GetDefaultDisplayIndex();
	ListAllDisplayModes(m_windowDisplayIndex);
	SaveDesktop();
}


void WindowManager::CleanupResolutions()
{
	while (m_resolutions.ValidIndex(0))
	{
		WindowResolution *res = m_resolutions.GetData(0);
		delete res;
		m_resolutions.RemoveData(0);
	}
	m_resolutions.EmptyAndDelete();
}


void WindowManager::SuggestDefaultRes( int *_width, int *_height, int *_refresh, int *_depth )
{
    *_width = m_desktopScreenW;
    *_height = m_desktopScreenH;
    *_refresh = m_desktopRefresh;
    *_depth = m_desktopColourDepth;
}


// Returns an index into the list of already registered resolutions
int WindowManager::GetResolutionId(int _width, int _height)
{
	for (int i = 0; i < m_resolutions.Size(); ++i)
	{
		WindowResolution *res = m_resolutions[i];
		if (res->m_width == _width && res->m_height == _height)
		{
			return i;
		}
	}

	return -1;
}


WindowResolution *WindowManager::GetResolution( int _id )
{
    if( m_resolutions.ValidIndex(_id) )
    {
        return m_resolutions[_id];
    }

    return NULL;
}


float WindowManager::GetHighDPIScaleX() const
{
    return m_highDPIScaleX;
}


float WindowManager::GetHighDPIScaleY() const
{
    return m_highDPIScaleY;
}


bool WindowManager::Windowed()
{
    return m_windowed;
}


bool WindowManager::Captured()
{
	return m_mouseCaptured;
}


void WindowManager::RegisterMessageHandler( SecondaryEventHandler _messageHandler )
{
    if( !m_secondaryMessageHandler )
    {
        m_secondaryMessageHandler = _messageHandler;
    }
}


SecondaryEventHandler WindowManager::GetSecondaryMessageHandler()
{
    return m_secondaryMessageHandler;
}


void WindowManager::RegisterWindowResizeHandler(WindowResizeHandler _handler)
{
    m_windowResizeHandler = _handler;
}


WindowResizeHandler WindowManager::GetWindowResizeHandler()
{
    return m_windowResizeHandler;
}

//
// New functions that set our logical resolution based on what the user has set in preferences.
// This is used to set the logical internal resolution of the game to trick DEFCON into rendering
// all the game elements at a specific scale. It works extremely well for larger displays.

static float ScaleFactor()
{
    int uiScale = g_preferences ? g_preferences->GetInt(PREFS_SCREEN_UI_SCALE, 100) : 100;
    
    switch(uiScale) {
        case 0:   return 1.0f;
        case 25:  return 1.75f;  // I mean sure but its a bit small
        case 50:  return 1.5f;   // Incase people want to?
        case 75:  return 1.25f;  // I can see this looking okay on smaller displays
        case 100: return 1.0f;   // Use monitor resolution instead of logical resolution (default - no scaling)
		case 110: return 0.90f;  // Can be good for 1440p monitors
        case 125: return 0.75f;  // Sweetspot for 4k monitors
		case 133: return 0.67f;  // Looks good on 4k monitors but a tad big
        case 150: return 0.50f;  // You might need an 8k monitor for this
        case 200: return 0.25f;  // Why?
        default:  return 1.0f;
    }
}

int WindowManager::GetLogicalWidth()
{
    return (int)(m_screenW * ScaleFactor());
}

int WindowManager::GetLogicalHeight() 
{
    return (int)(m_screenH * ScaleFactor());
}

int WindowManager::GetDisplayIndexForPoint(int x, int y)
{
    int numVideoDisplays = SDL_GetNumVideoDisplays();
    
    for (int displayIndex = 0; displayIndex < numVideoDisplays; ++displayIndex)
    {
        SDL_Rect bounds;
        SDL_GetDisplayBounds(displayIndex, &bounds);
        
        SDL_Point mouse = { x, y };
        if (SDL_PointInRect(&mouse, &bounds))
        {
            return displayIndex;
        }
    }
    
#ifdef _DEBUG
    AppDebugOut("GetDisplayIndexForPoint() could not determine which display is under (%d, %d), returning first display.\n", x, y);
#else
    printf("GetDisplayIndexForPoint() could not determine which display is under (%d, %d), so returning the first display.\n", x, y);
#endif
    return 0;
}

int WindowManager::GetDefaultDisplayIndex()
{
    int x, y;
    SDL_GetGlobalMouseState(&x, &y);
    return GetDisplayIndexForPoint(x, y);
}

void WindowManager::ListAllDisplayModes(int displayIndex)
{
    SDL_Rect bounds;
    SDL_GetDisplayBounds(displayIndex, &bounds);
    
    m_resolutions.EmptyAndDelete();
    
    for (int modeIndex = 0; modeIndex < SDL_GetNumDisplayModes(displayIndex); ++modeIndex)
    {
        SDL_DisplayMode mode;
        SDL_GetDisplayMode(displayIndex, modeIndex, &mode);
        
        //
        // Skip low quality modes

        if (SDL_BITSPERPIXEL(mode.format) < 15 || mode.w < 640 || mode.h < 480)
            continue;
        
        //
        // Skip high DPI resolutions

        if (mode.w > bounds.w || mode.h > bounds.h)
            continue;
        
        int resId = GetResolutionId(mode.w, mode.h);
        WindowResolution *res;
        
        if (resId == -1)
        {
            res = new WindowResolution(mode.w, mode.h);
            m_resolutions.PutData(res);
        }
        else
        {
            res = m_resolutions[resId];
        }
        
        if (mode.refresh_rate != 0)
        {
            //
            // Only add if this refresh rate isnt already in the list

            if (res->m_refreshRates.FindData(mode.refresh_rate) == -1)
            {
                res->m_refreshRates.PutDataAtEnd(mode.refresh_rate);
            }
        }
    }
}

void WindowManager::SaveDesktop()
{
    int displayIndex = GetDefaultDisplayIndex();

    SDL_DisplayMode desktopMode;
    if( SDL_GetDesktopDisplayMode( displayIndex, &desktopMode ) != 0 )
    {
        AppDebugOut("Couldn't get desktop display mode for display: %d, error: %s\n", displayIndex, SDL_GetError());
        AppDebugOut("Trying again with a different method...\n");

        
        //
        // fallback: get bounds from SDL_GetDisplayBounds which might work

        SDL_Rect displayBounds;
        if( SDL_GetDisplayBounds( displayIndex, &displayBounds ) != 0 )
        {
            AppDebugOut("Still couldn't get display bounds, error: %s\n", SDL_GetError());
            AppDebugOut("Using 1024x768 as fallback resolution\n");
            m_desktopScreenW = 1024;
            m_desktopScreenH = 768;
        }
        else
        {
            m_desktopScreenW = displayBounds.w;
            m_desktopScreenH = displayBounds.h;
            AppDebugOut("Successfully got display bounds: %dx%d\n", m_desktopScreenW, m_desktopScreenH);
        }
    }
    else
    {
        //
        // success! hopefully, print the result incase its not.

        m_desktopScreenW = desktopMode.w;
        m_desktopScreenH = desktopMode.h;
        AppDebugOut("Desktop resolution determined to be: %dx%d\n", m_desktopScreenW, m_desktopScreenH);
    }
    
    m_desktopColourDepth = SDL_BITSPERPIXEL(desktopMode.format);
    m_desktopRefresh = desktopMode.refresh_rate;

#if defined(TARGET_OS_MACOSX)
    int width = 0, height = 0;
    GetMainDisplayResolution( width, height );
    AppDebugOut("macOS native resolution verification: %dx%d\n", width, height );
#endif
}

void WindowManager::RestoreDesktop()
{
    // SDL handles this
}

void WindowManager::CalculateHighDPIScaleFactors()
{
    //
    // Disabled because HiDPI Scaling is not supported for now

    //if (!m_sdlWindow)
    //    return;
    
    //int clientW, clientH;
    //SDL_GetWindowSize(m_sdlWindow, &clientW, &clientH);
    
    //m_highDPIScaleX = 1.0f;
    //m_highDPIScaleY = 1.0f;
}

void WindowManager::WindowHasMoved()
{
    //CalculateHighDPIScaleFactors();
    
    if (m_sdlWindow)
    {
        m_windowDisplayIndex = SDL_GetWindowDisplayIndex(m_sdlWindow);
        ListAllDisplayModes(m_windowDisplayIndex);
    }
}

void WindowManager::UpdateStoredMaximizedState()
{
    if (!m_sdlWindow || !g_preferences)
        return;
    
    Uint32 windowFlags = SDL_GetWindowFlags(m_sdlWindow);
    bool currentlyMaximized = false;
    
    if (m_windowed && !(windowFlags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)))
    {
        currentlyMaximized = (windowFlags & SDL_WINDOW_MAXIMIZED) != 0;
    }
    
    if (currentlyMaximized == m_isMaximized)
        return;
    
    m_isMaximized = currentlyMaximized;
    g_preferences->SetInt(PREFS_SCREEN_MAXIMIZED, m_isMaximized ? 1 : 0);
}

bool WindowManager::MouseVisible()
{
    return m_mousePointerVisible;
}

void WindowManager::HandleResize(int newWidth, int newHeight)
{
    // Base implementation does nothing
}

void WindowManager::HandleWindowFocusGained()
{
    // Base implementation does nothing
}

void WindowManager::CaptureMouse()
{
    //
    // Dont grab if we don't have focus

    if (!g_inputManager || !g_inputManager->m_windowHasFocus)
    {
        m_tryingToCaptureMouse = true;
        return;
    }
    
    if (!m_sdlWindow)
        return;
    
    //
    // Dont grab until mouse is in the window

    int windowFlags = SDL_GetWindowFlags(m_sdlWindow);
    if (!(windowFlags & SDL_WINDOW_MOUSE_FOCUS))
    {
        m_tryingToCaptureMouse = true;
        return;
    }
    
    SDL_SetRelativeMouseMode(SDL_TRUE);
    m_mouseCaptured = true;
    m_tryingToCaptureMouse = false;
}

void WindowManager::UncaptureMouse()
{
    if (m_sdlWindow)
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
    
    m_mouseCaptured = false;
    m_tryingToCaptureMouse = false;
}

void WindowManager::HideMousePointer()
{
    SDL_ShowCursor(SDL_DISABLE);
    m_mousePointerVisible = false;
}

void WindowManager::UnhideMousePointer()
{
    SDL_ShowCursor(SDL_ENABLE);
    m_mousePointerVisible = true;
}

void WindowManager::SetMousePos(int x, int y)
{
    if (!m_sdlWindow)
        return;
    
    float scaleX = (float)PhysicalWindowW() / (float)WindowW();
    float scaleY = (float)PhysicalWindowH() / (float)WindowH();
    
    int physicalX = (int)(x * scaleX);
    int physicalY = (int)(y * scaleY);
    
    SDL_WarpMouseInWindow(m_sdlWindow, physicalX, physicalY);
}

void WindowManager::PollForMessages()
{
    SDL_Event sdlEvent;
    
    while (SDL_PollEvent(&sdlEvent))
    {
        //
        // Process ImGui events first

        if (ImGui::GetCurrentContext() != nullptr)
        {
            ImGui_ImplSDL2_ProcessEvent(&sdlEvent);
        }
        
        if (sdlEvent.type == SDL_WINDOWEVENT)
        {
            switch (sdlEvent.window.event)
            {
                case SDL_WINDOWEVENT_MOVED:
                    WindowHasMoved();
                    break;
                
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_MAXIMIZED:
                case SDL_WINDOWEVENT_RESTORED:
                    HandleResize(sdlEvent.window.data1, sdlEvent.window.data2);
                    break;
                
                case SDL_WINDOWEVENT_FOCUS_GAINED:
                case SDL_WINDOWEVENT_SHOWN:
                    HandleWindowFocusGained();
                    break;
                
                default:
                    break;
            }
        }
        
        //
        // Quit event

        if (sdlEvent.type == SDL_QUIT)
        {
            if (g_inputManager)
            {
                g_inputManager->m_quitRequested = true;
            }
        }
        
        //
        // Pass all SDL events to input manager
        
        if (g_inputManager)
        {
            int result = g_inputManager->EventHandler(sdlEvent.type, (long long)&sdlEvent, 0);
            
            if (result == -1 && m_secondaryMessageHandler)
            {
                m_secondaryMessageHandler(sdlEvent.type, (long long)&sdlEvent, 0);
            }
        }
    }
    
    if (m_tryingToCaptureMouse)
    {
        CaptureMouse();
    }
}

void WindowManager::HideWin()
{
#ifndef TARGET_OS_MACOSX
    if (m_sdlWindow)
    {
        SDL_HideWindow(m_sdlWindow);
    }
#else
    MacOSXHideWin();
#endif
}

void WindowManager::OpenWebsite(const char *_url)
{
#ifdef TARGET_OS_MACOSX
#ifdef AMBROSIA_REGISTRATION
    IT_OpenBrowser(_url);
#else
    const char *cmd = "/usr/bin/open";
    char buffer[strlen(_url) + 1 + strlen(cmd) + 1];
    sprintf(buffer, "%s '%s'", cmd, _url);
    system(buffer);
#endif
#elif defined TARGET_OS_LINUX
    /* Child */
    char * const args[4] = { "/bin/sh", "xdg-open.sh", (char *)_url,  NULL };
    spawn("/bin/sh", args);
#elif defined TARGET_MSVC
    ShellExecute(nullptr, "open", _url, nullptr, nullptr, SW_SHOWNORMAL);
#endif
}

class DeleteWindowManagerOnExit {
	public:
	~DeleteWindowManagerOnExit()
	{
		if (g_windowManager)
			g_windowManager->DestroyWin();

		delete g_windowManager;
		g_windowManager = NULL;
	}
};


static DeleteWindowManagerOnExit please;

WindowManager *WindowManager::Create()
{
#if !defined(RENDERER_OPENGL) && !defined(RENDERER_DIRECTX11)
#error "No renderer selected, please define RENDERER_OPENGL or RENDERER_DIRECTX11"
#endif

    RendererType requestedType = GetRendererType();
    
#ifdef RENDERER_DIRECTX11
    if (requestedType == RENDERER_TYPE_DIRECTX11)
    {
        return new WindowManagerD3D11();
    }
#endif
    
#ifdef RENDERER_OPENGL
    if (requestedType == RENDERER_TYPE_OPENGL)
    {
        return new WindowManagerOpenGL();
    }
#endif
    
    AppReleaseAssert(false, "Failed to create window manager for renderer type: %s", 
                     GetRendererTypeName(requestedType));
    return NULL;
}

#if defined(TARGET_OS_LINUX) || defined(TARGET_OS_MACOSX)
void SetupMemoryAccessHandlers();
void SetupPathToProgram(const char *program);

#include <string>
#include <unistd.h>

char *g_origWorkingDir = NULL;

void ChangeToProgramDir(const char *program)
{
	std::string dir(program);
	std::string::size_type pos = dir.find_last_of('/');

	// Store the original working directory
	g_origWorkingDir = new char[PATH_MAX];
    if (!getcwd(g_origWorkingDir, PATH_MAX))
    {
        strcpy(g_origWorkingDir, "/");
    }

	if (pos != std::string::npos) 
		dir.erase(pos);
	AppReleaseAssert(
		chdir(dir.c_str()) == 0, 
		"Failed to change directory to %s", dir.c_str());
}

#endif // TARGET_OS_LINUX || TARGET_OS_MACOSX

extern "C" int main(int argc, char **argv)
{
#if TARGET_OS_LINUX && !defined(TARGET_EMSCRIPTEN)
    BrInitError error;
    if (!br_init(&error))
    {
        printf("*** BinReloc failed to initialize. Error: %d\n", error);
        exit(1);
    }
    char *exedir;
    exedir = br_find_exe(NULL);
	SetupPathToProgram(exedir);
	ChangeToProgramDir(exedir);
	free(exedir);
#endif

#if TARGET_OS_MACOSX
    // we need to change into the "Resource" directory one level up from
    // the executable; this used to happen in SDLMain.mm, but that is
    // no longer feasible.
    if (argc > 0)
    {
        printf("%s\n", argv[0]);
        ChangeToProgramDir(argv[0]);
        chdir("../Resources");
    }
#endif

	if (argc > 1)
    {
		if (strncmp(argv[1], "-v", 2) == 0)
        {
			puts(GetAppVersion());
			return 0;
		}
	}

    // Catch every error known to man
#ifdef USE_CRASHREPORTING
    InitializeCrashpad();
#endif
    
    SDL_version linkedVersion, compiledVersion;
    SDL_VERSION(&compiledVersion);
    SDL_GetVersion(&linkedVersion);
        
    AppDebugOut("SDL Version: Compiled against %d.%d.%d, running with %d.%d.%d\n",
        compiledVersion.major, compiledVersion.minor, compiledVersion.patch,
        linkedVersion.major, linkedVersion.minor, linkedVersion.patch);

#if TARGET_OS_LINUX
    // Setup illegal memory access handler
    // See debug_utils_gcc.cpp

    // Skip custom handlers if crashpad is enabled
#ifndef USE_CRASHREPORTING
	SetupMemoryAccessHandlers();
#endif
#endif

    g_argc = argc;
    g_argv = argv;
    
	AppMain();
	
	return 0;
}