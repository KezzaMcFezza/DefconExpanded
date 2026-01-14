#include "systemiv.h"

#ifdef RENDERER_OPENGL
#include <SDL2/SDL.h>

#ifdef TARGET_OS_MACOSX
#include "macosxlibrary.h"
#endif

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include "lib/debug/debug_utils.h"
#include "lib/preferences.h"
#include "window_manager_opengl.h"

// Uncomment if you want lots of output in debug mode.
// #define VERBOSE_DEBUG

// *** Constructor
WindowManagerOpenGL::WindowManagerOpenGL()
	: m_glContext( 0 )
{
}


WindowManagerOpenGL::~WindowManagerOpenGL()
{
	DestroyWin();
}


bool WindowManagerOpenGL::CreateWin( int _width, int _height, bool _windowed, int _colourDepth, int _refreshRate, int _zDepth, int _antiAlias, bool _borderless, const char *_title )
{
	int displayIndex = GetDefaultDisplayIndex();
	AppReleaseAssert( displayIndex >= 0, "Failed to get current SDL display index.\n" );

	SDL_DisplayMode displayMode;
	SDL_GetCurrentDisplayMode( displayIndex, &displayMode );

	m_windowed = _windowed;

	int bpp = SDL_BITSPERPIXEL( displayMode.format );

	SDL_Rect displayBounds;
	SDL_GetDisplayBounds( displayIndex, &displayBounds );

	//
	// Set the flags for creating the mode

	int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
	bool requestMaximized = false;
	if ( !_windowed )
	{
		if ( _borderless )
		{
			flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		}
		else
		{
			flags |= SDL_WINDOW_FULLSCREEN;
		}

		m_screenW = _width;
		m_screenH = _height;
	}
	else
	{
		SDL_Rect usableBounds;
		SDL_GetDisplayUsableBounds( displayIndex, &usableBounds );
		if ( g_preferences && g_preferences->GetInt( PREFS_SCREEN_MAXIMIZED, 0 ) )
		{
			flags |= SDL_WINDOW_MAXIMIZED;
			requestMaximized = true;
		}

#ifdef TARGET_OS_MACOSX
		// Mac OS X will allow the window to go off the bottom of the screen
		// if you choose the maximum usable bounds as reported by SDL. This
		// is because the usable bounds reported by SDL don't take into account
		// the window borders (e.g. the drag bar at the top of the window).
		// To calculate this properly, we should create a (hopefully hidden)
		// NSWindow, and use the difference between frame and contentRect
		// to get a good idea of the border size.
		//
		// We try another way, which is to double the top menu bar width
		// as an estimate of the standard window drag bar height.

		int dragbarHeight = usableBounds.y - displayBounds.y;
		usableBounds.h -= dragbarHeight;
#endif

		//
		// Usually any combination is OK for windowed mode.

		m_screenW = std::min( usableBounds.w, _width );
		m_screenH = std::min( usableBounds.h, _height );

		flags |= SDL_WINDOW_RESIZABLE;

		//
		// Add it to the list of screen resolutions if need be

		WindowResolution *found = NULL;
		for ( int i = 0; i < m_resolutions.Size(); i++ )
		{
			WindowResolution *res = m_resolutions.GetData( i );
			if ( res->m_width == _width && res->m_height == _height )
			{
				found = res;
				break;
			}
		}

		if ( !found )
		{
			WindowResolution *res = new WindowResolution( _width, _height );
			m_resolutions.PutData( res );
		}
	}

	switch ( bpp )
	{
		case 24:
		case 32:
			SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
			SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
			SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
			break;

		case 16:
		default:
			SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 5 );
			SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 5 );
			SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 5 );
			break;
	}

	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, _zDepth );

#if defined( TARGET_EMSCRIPTEN )

	//
	// Request OpenGL ES 3.0 context

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
	SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );

#elif defined( TARGET_OS_MACOSX )

	//
	// MacOS only supports 3.2 Core Profile on older hardware
	// since arm64 uses metal and layers onto opengl anyway, this
	// is a good compromise. MacOS in the future will likely use Metal.

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 2 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG );
	SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );
#else
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 3 );
	SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );
#endif

	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 0 );
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 0 );

	bool tryingToCreateWindow = true;
	while ( tryingToCreateWindow )
	{
		tryingToCreateWindow = false;

		//
		// Destroy window from a previous try if, for example, the
		// GL context failed to be created.

		if ( m_sdlWindow )
		{
			SDL_DestroyWindow( m_sdlWindow );
			m_sdlWindow = nullptr;
		}

		m_glContext = 0;

		//
		// Use SDL_WINDOWPOS_CENTERED to center the window instead of top-left positioning

		m_sdlWindow = SDL_CreateWindow( _title,
										SDL_WINDOWPOS_CENTERED_DISPLAY( displayIndex ),
										SDL_WINDOWPOS_CENTERED_DISPLAY( displayIndex ),
										m_screenW,
										m_screenH,
										flags );

		if ( m_sdlWindow )
		{
			//
			// for true fullscreen mode (not borderless), set the display mode with desired refresh rate

			if ( !_windowed && !_borderless && _refreshRate != 0 )
			{
				SDL_DisplayMode targetMode;
				targetMode.w = m_screenW;
				targetMode.h = m_screenH;
				targetMode.refresh_rate = _refreshRate;
				targetMode.format = 0;

				if ( SDL_SetWindowDisplayMode( m_sdlWindow, &targetMode ) != 0 )
				{
					AppDebugOut( "Failed to set display mode to %dx%d @ %dHz: %s\n",
								 m_screenW, m_screenH, _refreshRate, SDL_GetError() );
				}
			}

			m_glContext = SDL_GL_CreateContext( m_sdlWindow );
		}

		if ( !m_sdlWindow || !m_glContext )
		{
			if ( _antiAlias )
			{
				printf( "Falling back to not anti-aliased.\n" );
				_antiAlias = 0;
				SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 0 );
				SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 0 );
				tryingToCreateWindow = true;
			}
			else if ( _zDepth != 16 )
			{
				printf( "Falling back to z-depth of 16.\n" );
				_zDepth = 16;
				tryingToCreateWindow = true;
			}
		}
	}

	if ( !m_sdlWindow )
	{
		printf( "SDL failed to open %d x %d window", m_screenW, m_screenH );
		return false;
	}

	if ( !m_glContext )
	{
		printf( "Could not create SDL OpenGL context: %s\n", SDL_GetError() );

		SDL_DestroyWindow( m_sdlWindow );
		m_sdlWindow = nullptr;
		return false;
	}

	if ( SDL_GL_MakeCurrent( m_sdlWindow, m_glContext ) < 0 )
	{
		printf( "Could not make SDL OpenGL context current: %s\n", SDL_GetError() );

		SDL_GL_DeleteContext( m_glContext );
		m_glContext = 0;

		SDL_DestroyWindow( m_sdlWindow );
		m_sdlWindow = nullptr;

		return false;
	}

#if ( defined( TARGET_OS_LINUX ) && !defined( TARGET_EMSCRIPTEN ) ) || defined( TARGET_OS_MACOSX )
	printf( "Initializing GLAD...\n" );
	fflush( stdout );

	if ( !gladLoadGL() )
	{
		printf( "GLAD initialization failed\n" );

		SDL_GL_DeleteContext( m_glContext );
		m_glContext = 0;

		SDL_DestroyWindow( m_sdlWindow );
		m_sdlWindow = nullptr;

		return false;
	}

	printf( "GLAD initialized successfully\n" );

#elif defined( TARGET_MSVC )
	printf( "Initializing GLAD...\n" );
	fflush( stdout );

	if ( !gladLoadGLLoader( (GLADloadproc)SDL_GL_GetProcAddress ) )
	{
		printf( "GLAD initialization failed\n" );

		SDL_GL_DeleteContext( m_glContext );
		m_glContext = 0;

		SDL_DestroyWindow( m_sdlWindow );
		m_sdlWindow = nullptr;

		return false;
	}

	printf( "GLAD initialized successfully\n" );

#elif defined( TARGET_EMSCRIPTEN )
	printf( "Initializing GLAD for Emscripten...\n" );
	fflush( stdout );

	//
	// For Emscripten, we need to use gladLoadGLES2Loader with SDL_GL_GetProcAddress

	if ( !gladLoadGLES2Loader( (GLADloadproc)SDL_GL_GetProcAddress ) )
	{
		printf( "GLAD initialization failed\n" );

		SDL_GL_DeleteContext( m_glContext );
		m_glContext = 0;

		SDL_DestroyWindow( m_sdlWindow );
		m_sdlWindow = nullptr;

		return false;
	}

	printf( "GLAD initialized successfully\n" );
#endif

	SDL_GL_SetSwapInterval( 0 );

	m_windowDisplayIndex = displayIndex;

	//
	// Verify actual window size matches what we requested
	// SDL may adjust the size due to decorations, DPI, or other constraints

	int actualW, actualH;
	SDL_GetWindowSize( m_sdlWindow, &actualW, &actualH );

	if ( actualW != m_screenW || actualH != m_screenH )
	{
#ifdef _DEBUG
		AppDebugOut( "Window size adjusted from requested %dx%d to actual %dx%d\n",
					 m_screenW, m_screenH, actualW, actualH );
#endif
		m_screenW = actualW;
		m_screenH = actualH;
	}

	UpdateStoredMaximizedState();

	if ( requestMaximized )
	{
		//
		// SDL respects SDL_WINDOW_MAXIMIZED at creation time, but double check
		// in case we need to sync our cached dimensions after decoration logic.

		SDL_GetWindowSize( m_sdlWindow, &m_screenW, &m_screenH );
		UpdateStoredMaximizedState();
	}

	//
	// Pass back the actual values to the Renderer

	_width = m_screenW;
	_height = m_screenH;
	_colourDepth = bpp;

	//
	// Hide the mouse pointer again

	if ( !m_mousePointerVisible )
		HideMousePointer();

	if ( m_mouseCaptured )
		CaptureMouse();

	return true;
}


void WindowManagerOpenGL::DestroyWin()
{
	if ( m_glContext )
	{
		SDL_GL_DeleteContext( m_glContext );
		m_glContext = 0;
	}

	if ( m_sdlWindow )
	{
		SDL_DestroyWindow( m_sdlWindow );
		m_sdlWindow = nullptr;
	}
}


void WindowManagerOpenGL::Flip()
{
	if ( m_tryingToCaptureMouse )
		CaptureMouse();

	SDL_GL_SwapWindow( m_sdlWindow );
}


void WindowManagerOpenGL::HandleResize( int newWidth, int newHeight )
{
	if ( !m_sdlWindow || newWidth <= 0 || newHeight <= 0 )
		return;

	Uint32 windowFlags = SDL_GetWindowFlags( m_sdlWindow );

	UpdateStoredMaximizedState();

	if ( windowFlags & ( SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP ) )
		return;

	if ( newWidth == m_screenW && newHeight == m_screenH )
		return;

	int oldWidth = m_screenW;
	int oldHeight = m_screenH;

	m_screenW = newWidth;
	m_screenH = newHeight;

	// CalculateHighDPIScaleFactors();

	//
	// Check if window moved to a different display

	int newDisplayIndex = SDL_GetWindowDisplayIndex( m_sdlWindow );
	if ( newDisplayIndex >= 0 && newDisplayIndex != m_windowDisplayIndex )
	{
		m_windowDisplayIndex = newDisplayIndex;
		ListAllDisplayModes( m_windowDisplayIndex );
	}

	//
	// Add current resolution to list if not present

	if ( GetResolutionId( newWidth, newHeight ) == -1 )
	{
		WindowResolution *res = new WindowResolution( newWidth, newHeight );
		m_resolutions.PutData( res );
	}

	//
	// Call resize handler

	if ( m_windowResizeHandler )
	{
		m_windowResizeHandler( newWidth, newHeight, oldWidth, oldHeight );
	}
}


void WindowManagerOpenGL::HandleWindowFocusGained()
{
}

#endif // RENDERER_OPENGL
