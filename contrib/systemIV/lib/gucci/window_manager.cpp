#include "lib/universal_include.h"

#include <limits.h>

#include "lib/debug_utils.h"
#include "lib/preferences.h"

#include "input.h"
#include "window_manager.h"


WindowManager *g_windowManager = NULL;
int g_argc = 0;
char **g_argv = NULL;

WindowManager::WindowManager()
:	m_mousePointerVisible(true),
	m_mouseOffsetX(INT_MAX),
	m_mouseCaptured(false),
    m_highDPIScaleX(1.0f),
    m_highDPIScaleY(1.0f),
    m_secondaryMessageHandler(NULL)
{
}


WindowManager::~WindowManager()
{
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

#if !defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN)
//
// New functions that set our logical resolution based on what the user has set in preferences
//
// this is used to set the logical internal resolution of the game to trick DEFCON into rendering
// all the game elements at a specific scale, it works extremely well for larger displays

int WindowManager::GetLogicalWidth()
{
    // Default to No Scaling, this is what most DEFCON players expect
    int uiScale = g_preferences ? g_preferences->GetInt(PREFS_SCREEN_UI_SCALE, 100) : 100;
    
    float scaleFactor;
    switch(uiScale) {
        case 50:  scaleFactor = 1.5f;  break;     // Incase people want to?
        case 25:  scaleFactor = 1.75f;  break;    // I mean sure but its a bit small
        case 75:  scaleFactor = 1.25f; break;     // I can see this looking okay on smaller displays
        case 100: scaleFactor = 1.0f;  break;     // Use monitor resolution instead of logical resolution
		case 110: scaleFactor = 0.90f;  break;    // Can be good for 1440p monitors
        case 125: scaleFactor = 0.75f;  break;    // Sweetspot for 4k monitors
		case 133: scaleFactor = 0.67f;  break;    // Looks good on 4k monitors but a tad big
        case 150: scaleFactor = 0.50f; break;     // You might need an 8k monitor for this
        case 200: scaleFactor = 0.25f; break;     // Why?
        default:  scaleFactor = 1.0f;  break;     
    }
    
    return (int)(m_screenW * scaleFactor);
}


int WindowManager::GetLogicalHeight() 
{

    int uiScale = g_preferences ? g_preferences->GetInt(PREFS_SCREEN_UI_SCALE, 100) : 100;
    
    float scaleFactor;
    switch(uiScale) {
        case 0:   scaleFactor = 1.0f;  break;
        case 25:  scaleFactor = 1.75f;  break;
        case 50:  scaleFactor = 1.5f;  break;  
        case 75:  scaleFactor = 1.25f; break;  
        case 100: scaleFactor = 1.0f;  break;  
		case 110: scaleFactor = 0.90f;  break;  
        case 125: scaleFactor = 0.75f;  break;  
		case 133: scaleFactor = 0.67f;  break;  
        case 150: scaleFactor = 0.50f; break;  
        case 200: scaleFactor = 0.25f; break;  
        default:  scaleFactor = 1.0f;  break;  
    }
    
    return (int)(m_screenH * scaleFactor);
}
#endif

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

#if defined TARGET_OS_MACOSX || TARGET_OS_LINUX || WINDOWS_SDL
#include "window_manager_sdl.h"
#else
#include "window_manager_win32.h"
#endif 

WindowManager *WindowManager::Create()
{
#if defined TARGET_OS_MACOSX || TARGET_OS_LINUX || WINDOWS_SDL
	return new WindowManagerSDL();
#else
	return new WindowManagerWin32();
#endif 
}


