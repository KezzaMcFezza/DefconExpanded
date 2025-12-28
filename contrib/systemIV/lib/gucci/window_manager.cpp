#include "systemiv.h"

#include <limits.h>

#include "lib/debug_utils.h"
#include "lib/preferences.h"

#include "input.h"
#include "window_manager.h"
#include "window_manager_sdl.h"


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
    m_windowResizeHandler(NULL)
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
	return new WindowManagerSDL();
}


