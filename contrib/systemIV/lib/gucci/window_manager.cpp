#include "systemiv.h"
#include <SDL3/SDL_main.h>

#include <limits.h>

#include "lib/debug/debug_utils.h"
#include "lib/preferences.h"
#include "lib/hi_res_time.h"

#include "imgui.h"
#include "input.h"
#include "crashpad.h"
#include "window_manager.h"
#include "window_manager_d3d11.h"
#include "window_manager_opengl.h"
#include "backends/imgui_impl_sdl3.h"

#ifdef TARGET_OS_LINUX
#include "binreloc.h"
#include "spawn.h"
#endif

#ifdef TARGET_OS_MACOSX
#include "macosxlibrary.h"
#endif

#ifdef TARGET_MSVC
#include <shellapi.h>
#include <windows.h>
#endif

WindowManager *g_windowManager = NULL;
int g_argc = 0;
char **g_argv = NULL;

SDL_DisplayID WindowManager::GetDisplayIDFromIndex( int index )
{
	int count = 0;
	SDL_DisplayID *ids = SDL_GetDisplays( &count );

	if ( !ids || index < 0 || index >= count )
	{
		if ( ids )
		{
			SDL_free( ids );
		}

		return 0;
	}

	SDL_DisplayID id = ids[index];
	SDL_free( ids );
	return id;
}

int WindowManager::GetDisplayIndexFromID( SDL_DisplayID id )
{
	if ( !id )
	{
		return 0;
	}

	int count = 0;
	SDL_DisplayID *ids = SDL_GetDisplays( &count );

	if ( !ids )
	{
		return 0;
	}

	int index = 0;
	for ( int i = 0; i < count; ++i )
	{
		if ( ids[i] == id )
		{
			index = i;
			break;
		}
	}

	SDL_free( ids );
	return index;
}

WindowManager::WindowManager()
	: m_mouseCaptured( false ),
	  m_mousePointerVisible( true ),
	  m_mouseOffsetX( INT_MAX ),
	  m_secondaryMessageHandler( NULL ),
	  m_windowResizeHandler( NULL ),
	  m_sdlWindow( nullptr ),
	  m_windowDisplayIndex( 0 ),
	  m_tryingToCaptureMouse( false )
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
	//
	// SDL_Init now returns success on 1 with SDL 3.4
	// So dont get confused :)

	AppReleaseAssert( SDL_Init( SDL_INIT_VIDEO ) == 1, "Couldn't initialise SDL" );

	m_windowDisplayIndex = GetCurrentDisplayIndex();
	ListAllDisplayModes( m_windowDisplayIndex );
	SaveDesktop();
}


void WindowManager::CleanupResolutions()
{
	while ( m_resolutions.ValidIndex( 0 ) )
	{
		WindowResolution *res = m_resolutions.GetData( 0 );
		delete res;
		m_resolutions.RemoveData( 0 );
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


int WindowManager::GetCurrentRefreshRate()
{
	if ( !m_sdlWindow )
	{
		return m_desktopRefresh;
	}

	SDL_DisplayID displayID = SDL_GetDisplayForWindow( m_sdlWindow );

	if ( !displayID )
	{
		return m_desktopRefresh;
	}

	const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode( displayID );
	if ( !mode )
	{			
		return m_desktopRefresh;
	}

	int rate = (int)( mode->refresh_rate > 0.0f ? mode->refresh_rate : (float)m_desktopRefresh );
	return rate > 0 ? rate : m_desktopRefresh;
}


// Returns an index into the list of already registered resolutions
int WindowManager::GetResolutionId( int _width, int _height )
{
	for ( int i = 0; i < m_resolutions.Size(); ++i )
	{
		WindowResolution *res = m_resolutions[i];
		if ( res->m_width == _width && res->m_height == _height )
		{
			return i;
		}
	}

	return -1;
}


WindowResolution *WindowManager::GetResolution( int _id )
{
	if ( m_resolutions.ValidIndex( _id ) )
	{
		return m_resolutions[_id];
	}

	return NULL;
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
	if ( !m_secondaryMessageHandler )
	{
		m_secondaryMessageHandler = _messageHandler;
	}
}


SecondaryEventHandler WindowManager::GetSecondaryMessageHandler()
{
	return m_secondaryMessageHandler;
}


void WindowManager::RegisterWindowResizeHandler( WindowResizeHandler _handler )
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
	int uiScale = g_preferences ? g_preferences->GetInt( PREFS_SCREEN_UI_SCALE, 100 ) : 100;

	switch ( uiScale )
	{
		case 0:
			return 1.0f;
		case 25:
			return 1.75f; // I mean sure but its a bit small
		case 50:
			return 1.5f; // Incase people want to?
		case 75:
			return 1.25f; // I can see this looking okay on smaller displays
		case 100:
			return 1.0f; // Use monitor resolution instead of logical resolution (default - no scaling)
		case 110:
			return 0.90f; // Can be good for 1440p monitors
		case 125:
			return 0.75f; // Sweetspot for 4k monitors
		case 133:
			return 0.67f; // Looks good on 4k monitors but a tad big
		case 150:
			return 0.50f; // You might need an 8k monitor for this
		case 200:
			return 0.25f; // Why?
		default:
			return 1.0f;
	}
}

//
// Return drawable height scaled by UI scale preference
// This makes sure the games internal resolution matches
// the actual pixel resolution rather than the logical
// window size, which is important for DPI scaling.

const int WindowManager::GetLogicalWidth()
{
	return (int)( m_screenW * ScaleFactor() );
}


const int WindowManager::GetLogicalHeight()
{
	return (int)( m_screenH * ScaleFactor() );
}

//
// Return the actual drawable width and height of the window

const int WindowManager::GetPhysicalWidth()
{
	if ( !m_sdlWindow )
	{
		return 0;
	}
	
	int width = 0;
	SDL_GetWindowSizeInPixels( m_sdlWindow, &width, nullptr );
	return width;
}

const int WindowManager::GetPhysicalHeight()
{
	if (!m_sdlWindow)
	{
		return 0;
	}
	
	int height = 0;
	SDL_GetWindowSizeInPixels( m_sdlWindow, nullptr, &height );
	return height;
}

//
// Kept for backwards compatibility
// basically just a wrapper for GetLogicalWidth/Height()

const int WindowManager::WindowW()
{
	return GetLogicalWidth();
}

const int WindowManager::WindowH()
{
	return GetLogicalHeight();
}

int WindowManager::GetDisplayIndexForPoint( int x, int y )
{
	SDL_Point point = { x, y };
	SDL_DisplayID id = SDL_GetDisplayForPoint( &point );
	if ( !id )
	{
#ifdef _DEBUG
		AppDebugOut( "GetDisplayIndexForPoint() could not determine which display is under (%d, %d), returning first display.\n", x, y );
#else
		printf( "GetDisplayIndexForPoint() could not determine which display is under (%d, %d), so returning the first display.\n", x, y );
#endif
		return 0;
	}
	return GetDisplayIndexFromID( id );
}


int WindowManager::GetDefaultDisplayIndex()
{
	float x, y;
	SDL_GetGlobalMouseState( &x, &y );
	return GetDisplayIndexForPoint( (int)x, (int)y );
}


int WindowManager::GetNumDisplays()
{
	int count = 0;
	SDL_DisplayID *ids = SDL_GetDisplays( &count );

	if ( ids )
	{
		SDL_free( ids );
	}

	return count;
}


const char* WindowManager::GetDisplayName(int displayIndex)
{
	SDL_DisplayID id = GetDisplayIDFromIndex( displayIndex );
	return id ? SDL_GetDisplayName( id ) : nullptr;
}


int WindowManager::GetCurrentDisplayIndex()
{
	if ( g_preferences )
	{
		int preferredIndex = g_preferences->GetInt( PREFS_SCREEN_DISPLAY_INDEX, -1 );
		int numDisplays = GetNumDisplays();
		
		if ( preferredIndex >= 0 && preferredIndex < numDisplays )
		{
			return preferredIndex;
		}
	}
	
	return GetDefaultDisplayIndex();
}


void WindowManager::GetDisplayUsableSize( int displayIndex, int *outW, int *outH )
{
	SDL_DisplayID displayID = GetDisplayIDFromIndex( displayIndex );
	if ( !displayID || !outW || !outH )
	{
		if ( outW ) 
		{
			*outW = 1024;
		}
		if ( outH ) 
		{
			*outH = 768;
		}
		return;
	}

	SDL_Rect usableBounds;
	if ( !SDL_GetDisplayUsableBounds( displayID, &usableBounds ) )
	{
		*outW = 1024;
		*outH = 768;
		return;
	}

	*outW = usableBounds.w;
	*outH = usableBounds.h;

#ifdef _DEBUG
	AppDebugOut( "Display %d usable bounds: %dx%d\n", displayIndex, *outW, *outH );
#endif

}


void WindowManager::ListAllDisplayModes( int displayIndex )
{
	SDL_DisplayID displayID = GetDisplayIDFromIndex( displayIndex );

	if ( !displayID )
	{
		return;
	}

	SDL_Rect bounds;
	if ( !SDL_GetDisplayBounds( displayID, &bounds ) )
	{
		return;
	}

	int maxWidth = bounds.w;
	int maxHeight = bounds.h;

	const SDL_DisplayMode *desktopMode = SDL_GetDesktopDisplayMode( displayID );
	if ( desktopMode )
	{
		maxWidth = desktopMode->w;
		maxHeight = desktopMode->h;
	}
	else
	{
		//
        // If desktop mode detection fails, try current display mode
		
		const SDL_DisplayMode *currentMode = SDL_GetCurrentDisplayMode( displayID );
		if ( currentMode )
		{
			maxWidth = currentMode->w;
			maxHeight = currentMode->h;
		}
	}

	m_resolutions.EmptyAndDelete();

	int modeCount = 0;
	SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes( displayID, &modeCount );
	if ( !modes )
	{
		return;
	}

	for ( int modeIndex = 0; modeIndex < modeCount; ++modeIndex )
	{
		const SDL_DisplayMode *mode = modes[modeIndex];

		if ( !mode )
		{
			continue;
		}

		if ( SDL_BITSPERPIXEL( mode->format ) < 15 || mode->w < 640 || mode->h < 480 )
		{
			continue;
		}

		if ( mode->w > maxWidth || mode->h > maxHeight )
		{
			continue;
		}

		int resId = GetResolutionId( mode->w, mode->h );
		WindowResolution *res;

		if ( resId == -1 )
		{
			res = new WindowResolution( mode->w, mode->h );
			m_resolutions.PutData( res );
		}
		else
		{
			res = m_resolutions[resId];
		}

		int refreshRate = (int)( mode->refresh_rate + 0.5f );
		if ( refreshRate != 0 )
		{
			if ( res->m_refreshRates.FindData( refreshRate ) == -1 )
				res->m_refreshRates.PutDataAtEnd( refreshRate );
		}
	}

	SDL_free( modes );
}


void WindowManager::SaveDesktop()
{
	int displayIndex = GetDefaultDisplayIndex();
	SDL_DisplayID displayID = GetDisplayIDFromIndex( displayIndex );

	const SDL_DisplayMode *desktopMode = displayID ? SDL_GetDesktopDisplayMode( displayID ) : nullptr;
	if ( !desktopMode )
	{
		AppDebugOut( "Couldn't get desktop display mode for display: %d, error: %s\n", displayIndex, SDL_GetError() );
		AppDebugOut( "Trying again with a different method...\n" );

		SDL_Rect displayBounds;
		if ( !displayID || !SDL_GetDisplayBounds( displayID, &displayBounds ) )
		{
			AppDebugOut( "Still couldn't get display bounds, error: %s\n", SDL_GetError() );
			AppDebugOut( "Using 1024x768 as fallback resolution\n" );
			m_desktopScreenW = 1024;
			m_desktopScreenH = 768;
		}
		else
		{
			m_desktopScreenW = displayBounds.w;
			m_desktopScreenH = displayBounds.h;
			AppDebugOut( "Successfully got display bounds: %dx%d\n", m_desktopScreenW, m_desktopScreenH );
		}

		m_desktopColourDepth = 32;
		m_desktopRefresh = 0;
	}
	else
	{
		m_desktopScreenW = desktopMode->w;
		m_desktopScreenH = desktopMode->h;

		AppDebugOut( "Desktop resolution determined to be: %dx%d\n", m_desktopScreenW, m_desktopScreenH );

		m_desktopColourDepth = SDL_BITSPERPIXEL( desktopMode->format );
		m_desktopRefresh = (int)( desktopMode->refresh_rate + 0.5f );
	}
}


void WindowManager::RestoreDesktop()
{
	// SDL handles this
}


void WindowManager::WindowHasMoved()
{
	if ( m_sdlWindow )
	{
		SDL_DisplayID id = SDL_GetDisplayForWindow( m_sdlWindow );
		m_windowDisplayIndex = GetDisplayIndexFromID( id );
		ListAllDisplayModes( m_windowDisplayIndex );
	}
}


bool WindowManager::MouseVisible()
{
	return m_mousePointerVisible;
}


void WindowManager::HandleResize( int newWidth, int newHeight )
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

	if ( !g_inputManager || !g_inputManager->m_windowHasFocus )
	{
		m_tryingToCaptureMouse = true;
		return;
	}

	if ( !m_sdlWindow )
		return;

	//
	// Dont grab until mouse is in the window

	int windowFlags = SDL_GetWindowFlags( m_sdlWindow );
	if ( !( windowFlags & SDL_WINDOW_MOUSE_FOCUS ) )
	{
		m_tryingToCaptureMouse = true;
		return;
	}

	SDL_SetWindowRelativeMouseMode( m_sdlWindow, true );
	m_mouseCaptured = true;
	m_tryingToCaptureMouse = false;
}


void WindowManager::UncaptureMouse()
{
	if ( m_sdlWindow )
	{
		SDL_SetWindowRelativeMouseMode( m_sdlWindow, false );
	}

	m_mouseCaptured = false;
	m_tryingToCaptureMouse = false;
}


void WindowManager::HideMousePointer()
{
	SDL_HideCursor();
	m_mousePointerVisible = false;
}


void WindowManager::UnhideMousePointer()
{
	SDL_ShowCursor();
	m_mousePointerVisible = true;
}


void WindowManager::SetMousePos( int x, int y )
{
	if ( !m_sdlWindow )
		return;

	float scaleX = (float)GetPhysicalWidth() / (float)GetLogicalWidth();
	float scaleY = (float)GetPhysicalHeight() / (float)GetLogicalHeight();

	int physicalX = (int)( x * scaleX );
	int physicalY = (int)( y * scaleY );

	SDL_WarpMouseInWindow( m_sdlWindow, (float)physicalX, (float)physicalY );
}


void WindowManager::PollForMessages()
{
	SDL_Event sdlEvent;

	while ( SDL_PollEvent( &sdlEvent ) )
	{
		//
		// Process ImGui events first

		if ( ImGui::GetCurrentContext() != nullptr )
		{
			ImGui_ImplSDL3_ProcessEvent( &sdlEvent );
		}

		switch ( sdlEvent.type )
		{
			case SDL_EVENT_WINDOW_MOVED:
				WindowHasMoved();
				break;

			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			case SDL_EVENT_WINDOW_RESIZED:
			case SDL_EVENT_WINDOW_MAXIMIZED:
			case SDL_EVENT_WINDOW_RESTORED:
			{
				int w, h;
				if ( m_sdlWindow && SDL_GetWindowSize( m_sdlWindow, &w, &h ) )
				{
					HandleResize( w, h );
				}
				else
				{
					HandleResize( sdlEvent.window.data1, sdlEvent.window.data2 );
				}
				break;
			}

			case SDL_EVENT_WINDOW_FOCUS_GAINED:
			case SDL_EVENT_WINDOW_SHOWN:
				HandleWindowFocusGained();
				break;

			default:
				break;
		}

		//
		// Quit event

		if ( sdlEvent.type == SDL_EVENT_QUIT )
		{
			if ( g_inputManager )
			{
				g_inputManager->m_quitRequested = true;
			}
		}

		//
		// Pass all SDL events to input manager

		if ( g_inputManager )
		{
			int result = g_inputManager->EventHandler( sdlEvent.type, (long long)&sdlEvent, 0 );

			if ( result == -1 && m_secondaryMessageHandler )
			{
				m_secondaryMessageHandler( sdlEvent.type, (long long)&sdlEvent, 0 );
			}
		}
	}

	if ( m_tryingToCaptureMouse )
	{
		CaptureMouse();
	}
}


void WindowManager::HideWin()
{
#ifndef TARGET_OS_MACOSX
	if ( m_sdlWindow )
	{
		SDL_HideWindow( m_sdlWindow );
	}
#else
	MacOSXHideWin();
#endif
}


void WindowManager::OpenWebsite( const char *_url )
{
#ifdef TARGET_OS_MACOSX
#ifdef AMBROSIA_REGISTRATION
	IT_OpenBrowser( _url );
#else
	const char *cmd = "/usr/bin/open";
	char buffer[strlen( _url ) + 1 + strlen( cmd ) + 1];
	sprintf( buffer, "%s '%s'", cmd, _url );
	system( buffer );
#endif
#elif defined TARGET_OS_LINUX
	/* Child */
	char *const args[4] = { "/bin/sh", "xdg-open.sh", (char *)_url, NULL };
	spawn( "/bin/sh", args );
#elif defined TARGET_MSVC
	ShellExecute( nullptr, "open", _url, nullptr, nullptr, SW_SHOWNORMAL );
#endif
}


void WindowManager::Flip()
{
	LimitFrameRate();
	DoFlip();
}


class DeleteWindowManagerOnExit
{
  public:
	~DeleteWindowManagerOnExit()
	{
		if ( g_windowManager )
			g_windowManager->DestroyWin();

		delete g_windowManager;
		g_windowManager = NULL;
	}
};


static DeleteWindowManagerOnExit please;

WindowManager *WindowManager::Create()
{
#if !defined( RENDERER_OPENGL ) && !defined( RENDERER_DIRECTX11 )
#error "No renderer selected, please define RENDERER_OPENGL or RENDERER_DIRECTX11"
#endif

	RendererType requestedType = GetRendererType();

#ifdef RENDERER_DIRECTX11
	if ( requestedType == RENDERER_TYPE_DIRECTX11 )
	{
		return new WindowManagerD3D11();
	}
#endif

#ifdef RENDERER_OPENGL
	if ( requestedType == RENDERER_TYPE_OPENGL )
	{
		return new WindowManagerOpenGL();
	}
#endif

	AppReleaseAssert( false, "Failed to create window manager for renderer type: %s",
					  GetRendererTypeName( requestedType ) );
	return NULL;
}


#if defined( TARGET_OS_LINUX ) || defined( TARGET_OS_MACOSX )
void SetupMemoryAccessHandlers();
void SetupPathToProgram( const char *program );

#include <string>
#include <unistd.h>

char *g_origWorkingDir = NULL;

void ChangeToProgramDir( const char *program )
{
	std::string dir( program );
	std::string::size_type pos = dir.find_last_of( '/' );

	// Store the original working directory
	g_origWorkingDir = new char[PATH_MAX];
	if ( !getcwd( g_origWorkingDir, PATH_MAX ) )
	{
		strcpy( g_origWorkingDir, "/" );
	}

	if ( pos != std::string::npos )
		dir.erase( pos );
	AppReleaseAssert(
		chdir( dir.c_str() ) == 0,
		"Failed to change directory to %s", dir.c_str() );
}

#endif // TARGET_OS_LINUX || TARGET_OS_MACOSX

extern "C" int main( int argc, char **argv )
{
#if TARGET_OS_LINUX && !defined( TARGET_EMSCRIPTEN )
	BrInitError error;
	if ( !br_init( &error ) )
	{
		printf( "*** BinReloc failed to initialize. Error: %d\n", error );
		exit( 1 );
	}

	char *exedir;
	exedir = br_find_exe( NULL );
	SetupPathToProgram( exedir );
	ChangeToProgramDir( exedir );
	free( exedir );
#endif

#if TARGET_OS_MACOSX
	// we need to change into the "Resource" directory one level up from
	// the executable; this used to happen in SDLMain.mm, but that is
	// no longer feasible.
	if ( argc > 0 )
	{
		printf( "%s\n", argv[0] );
		ChangeToProgramDir( argv[0] );
		chdir( "../Resources" );
	}
#endif

	if ( argc > 1 )
	{
		if ( strncmp( argv[1], "-v", 2 ) == 0 )
		{
			puts( GetAppVersion() );
			return 0;
		}
	}

	int linkedVersion = SDL_GetVersion();
	int compiledVersion = SDL_VERSION;

	AppDebugOut( "SDL Version: Compiled against %d.%d.%d, running with %d.%d.%d\n",
				 SDL_VERSIONNUM_MAJOR( compiledVersion ), 
				 SDL_VERSIONNUM_MINOR( compiledVersion ), 
				 SDL_VERSIONNUM_MICRO( compiledVersion ),
				 SDL_VERSIONNUM_MAJOR( linkedVersion ), 
				 SDL_VERSIONNUM_MINOR( linkedVersion ), 
				 SDL_VERSIONNUM_MICRO( linkedVersion ) );

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