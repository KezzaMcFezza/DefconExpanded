
#include "lib/universal_include.h"

#ifdef TARGET_OS_LINUX
#include "binreloc.h"
#include "spawn.h"
#endif


#include <limits.h>


#include <string.h>


#include <stdlib.h>
#include <fenv.h>
#include <algorithm>



#include "lib/debug_utils.h"
#include "lib/preferences.h"
#include "window_manager_sdl.h"
#include "input.h"

//#include "app.h"
//#include "renderer.h"

#ifdef TARGET_OS_LINUX
#include "binreloc.h"
#include "spawn.h"
#endif

#ifdef AMBROSIA_REGISTRATION
#include "lib/ambrosia.h"
#include "file_tool.h"
#include "interface_tool.h"
#include "platform.h"
#endif

#ifdef TARGET_OS_MACOSX
#include <ApplicationServices/ApplicationServices.h>
#endif

static int GetDisplayIndexForPoint( int x, int y )
{
    int numVideoDisplays = SDL_GetNumVideoDisplays();
    
    for( int displayIndex = 0; displayIndex < numVideoDisplays; ++displayIndex )
    {
        SDL_Rect bounds;
        SDL_GetDisplayBounds( displayIndex, &bounds );
        
        SDL_Point mouse = { x, y };
        if( SDL_PointInRect( &mouse, &bounds ) )
        {
            return displayIndex;
        }
    }
    
    printf( "GetDisplayIndexForPoint() could not determine which display is under (%d, %d), so returning the first display.\n", x, y);
    return 0;
}


static int GetDefaultDisplayIndex()
{
    // Select the display under the mouse.
    
    int x, y;
    SDL_GetGlobalMouseState(&x, &y);
    
    return GetDisplayIndexForPoint(x, y);
}


// Uncomment if you want lots of output in debug mode.
//#define VERBOSE_DEBUG

// *** Constructor
WindowManagerSDL::WindowManagerSDL()
	:	m_tryingToCaptureMouse(false),
        m_window(NULL),
        m_glContext(0)
{
	AppReleaseAssert(SDL_Init(SDL_INIT_VIDEO) == 0, "Couldn't initialise SDL");

	ListAllDisplayModes( GetDefaultDisplayIndex() );
	SaveDesktop();
}


WindowManagerSDL::~WindowManagerSDL()
{
	while ( m_resolutions.ValidIndex ( 0 ) )
	{
		WindowResolution *res = m_resolutions.GetData ( 0 );
		delete res;
		m_resolutions.RemoveData ( 0 );
	}
	m_resolutions.EmptyAndDelete();
	SDL_Quit();
}


void WindowManagerSDL::ListAllDisplayModes( int displayIndex )
{
    SDL_Rect bounds;
    SDL_GetDisplayBounds( displayIndex, &bounds );
    
    m_resolutions.EmptyAndDelete();
    
    for( int modeIndex = 0; modeIndex < SDL_GetNumDisplayModes( displayIndex ); ++modeIndex )
    {
        SDL_DisplayMode mode;
        SDL_GetDisplayMode( displayIndex, modeIndex, &mode );
        
        if( SDL_BITSPERPIXEL( mode.format ) < 15 || mode.w < 640 || mode.h < 480 ) continue;
                
        // SDL reports high DPI resolutions here as well, which are
        // identified by being larger than the dimensions reported by
        // the display bounds function. We don't support High DPI in
        // Darwinia, so let's remove them from the list.
        if( mode.w > bounds.w || mode.h > bounds.h ) continue;
        
        WindowResolution *res = new WindowResolution( mode.w, mode.h );
        if (GetResolutionId( mode.w, mode.h ) == -1)
            m_resolutions.PutData( res );
    }
}


void WindowManagerSDL::SaveDesktop()
{
    int displayIndex = GetDefaultDisplayIndex();

    SDL_DisplayMode desktopMode;
    SDL_GetDesktopDisplayMode( displayIndex, &desktopMode );

    m_desktopColourDepth = SDL_BITSPERPIXEL( desktopMode.format );
    m_desktopRefresh = 60;

#ifdef TARGET_OS_MACOSX
	CGRect rect = CGDisplayBounds( CGMainDisplayID() );
    m_desktopScreenW = rect.size.width;
    m_desktopScreenH = rect.size.height;
#else
	#warning "Need to do code for linux to determine default WindowResolution"

    m_desktopScreenW = 1024;
    m_desktopScreenH = 768;
#endif
}


void WindowManagerSDL::RestoreDesktop()
{
}


void WindowManagerSDL::CalculateHighDPIScaleFactors()
{
    // In the case of high dpi, we're going to actually get a bigger
    // resolution by 2x. The approach we take with Defcon is to
    // tell the application the screen dimensions as if it was low dpi
    // and let the application adjust the glViewport, clipping rectangles
    // and screenshotting code accordingly.
    
    int clientW, clientH;
    SDL_GetWindowSize(m_window, &clientW, &clientH);
    int drawableW, drawableH;
    SDL_GL_GetDrawableSize(m_window, &drawableW, &drawableH);
    
    m_highDPIScaleX = (float) drawableW / clientW;
    m_highDPIScaleY = (float) drawableH / clientH;
}


void WindowManagerSDL::WindowHasMoved()
{
    CalculateHighDPIScaleFactors();
    
    m_windowDisplayIndex = SDL_GetWindowDisplayIndex(m_window);
    ListAllDisplayModes(m_windowDisplayIndex);
}


bool WindowManagerSDL::CreateWin(int _width, int _height, bool _windowed, int _colourDepth, int _refreshRate, int _zDepth, int _antiAlias, const char *_title)
{
    int displayIndex = GetDefaultDisplayIndex();
    AppReleaseAssert(displayIndex >= 0, "Failed to get current SDL display index.\n");
    
    SDL_DisplayMode displayMode;
    SDL_GetCurrentDisplayMode( displayIndex, &displayMode );
    
    m_windowed = _windowed;

    int bpp = SDL_BITSPERPIXEL( displayMode.format );

    SDL_Rect displayBounds;
    SDL_GetDisplayBounds( displayIndex, &displayBounds );

    // Set the flags for creating the mode
    int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
    if (!_windowed)
    {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        
        m_screenW = displayMode.w;
        m_screenH = displayMode.h;
        
    }
    else {
        SDL_Rect usableBounds;
        SDL_GetDisplayUsableBounds( displayIndex, &usableBounds );

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
        
        // Usually any combination is OK for windowed mode.
        m_screenW = std::min(usableBounds.w, _width);
        m_screenH = std::min(usableBounds.h, _height);
        
        // Add it to the list of screen resolutions if need be
        WindowResolution *found = NULL;
        for (int i = 0; i < m_resolutions.Size(); i++) {
            WindowResolution *res = m_resolutions.GetData(i);
            if (res->m_width == _width && res->m_height == _height) {
                found = res;
                break;
            }
        }
        
        if (!found) {
            WindowResolution *res = new WindowResolution(_width, _height);
            m_resolutions.PutData(res);
        }
    }
    
    switch (bpp) {
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
    
#if defined(TARGET_EMSCRIPTEN)
    // Request WebGL 2.0 context for modern OpenGL ES 3.0 features
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
#elif defined(TARGET_OS_MACOSX)
    // Request OpenGL 3.3 Core Profile for desktop
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE); 
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 3 );
#else
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 3 );
#endif
    
    if ( _antiAlias )
    {
         SDL_GL_SetAttribute ( SDL_GL_MULTISAMPLEBUFFERS, 2 );
         SDL_GL_SetAttribute ( SDL_GL_MULTISAMPLESAMPLES, 4 );
    }
    else
    {
        SDL_GL_SetAttribute ( SDL_GL_MULTISAMPLEBUFFERS, 0 );
        SDL_GL_SetAttribute ( SDL_GL_MULTISAMPLESAMPLES, 0 );
    }

    bool tryingToCreateWindow = true;
    while( tryingToCreateWindow )
    {
        tryingToCreateWindow = false;

        // Destroy window from a previous try if, for example, the
        // GL context failed to be created.
        if( m_window )
        {
            SDL_DestroyWindow(m_window);
            m_window = NULL;
        }
        
        m_glContext = 0;

        m_window = SDL_CreateWindow( _title,
                                     displayBounds.x,
                                     displayBounds.y,
                                     m_screenW,
                                     m_screenH,
                                     flags );

        if( m_window )
        {
            m_glContext = SDL_GL_CreateContext( m_window );
            
#ifdef TARGET_EMSCRIPTEN
            // Debug: Check what WebGL context we got
            if( m_glContext )
            {
                int major, minor, profile;
                SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
                SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
                SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);
                printf("WebGL Context Created: %d.%d, Profile: %d\n", major, minor, profile);
            }
            else
            {
                printf("Failed to create WebGL context: %s\n", SDL_GetError());
            }
#endif
        }
        
        if( !m_window || !m_glContext )
        {
            if( _antiAlias )
            {
                printf( "Falling back to not anti-aliased.\n" );
                _antiAlias = 0;
                SDL_GL_SetAttribute ( SDL_GL_MULTISAMPLEBUFFERS, 0 );
                SDL_GL_SetAttribute ( SDL_GL_MULTISAMPLESAMPLES, 0 );
                tryingToCreateWindow = true;
            }
            else if( _zDepth != 16 )
            {
                printf( "Falling back to z-depth of 16.\n" );
                _zDepth = 16;
                tryingToCreateWindow = true;
            }
        }
    }
    
    if( !m_window )
    {
        printf( "SDL failed to open %d x %d window", m_screenW, m_screenH );
        return false;
    }
    
    if( !m_glContext )
    {
        printf("Could not create SDL OpenGL context: %s\n", SDL_GetError ());
        
        SDL_DestroyWindow( m_window );
        m_window = NULL;
        return false;
    }

    if( SDL_GL_MakeCurrent( m_window, m_glContext ) < 0 )
    {
        printf("Could not make SDL OpenGL context current: %s\n", SDL_GetError ());
     
        SDL_GL_DeleteContext( m_glContext );
        m_glContext = 0;
        
        SDL_DestroyWindow( m_window );
        m_window = NULL;
        
        return false;
    }
    
#if defined(TARGET_OS_LINUX) || defined(TARGET_OS_MACOSX)
    // we need to initialise GLEW now that we are using compatibility profile
    // instead of OpenGL ES 3.0
    printf("Initializing GLEW...\n");
    fflush(stdout);
    
    GLenum glewResult = glewInit();
    if (glewResult != GLEW_OK) {
        printf("GLEW initialization failed: %s\n", glewGetErrorString(glewResult));
        
        SDL_GL_DeleteContext( m_glContext );
        m_glContext = 0;
        
        SDL_DestroyWindow( m_window );
        m_window = NULL;
        
        return false;
    }
    
    printf("GLEW initialized successfully\n");
#endif
    
    m_windowDisplayIndex = displayIndex;
    
    CalculateHighDPIScaleFactors();
         
    // Pass back the actual values to the Renderer
    _width = m_screenW;
    _height = m_screenH;
    _colourDepth = bpp;

    // Hide the mouse pointer again
    if( !m_mousePointerVisible )
        HideMousePointer();

    if( m_mouseCaptured )
        CaptureMouse();

    return true;
}


void WindowManagerSDL::DestroyWin()
{
}


void WindowManagerSDL::Flip()
{
	PollForMessages();
	
	if (m_tryingToCaptureMouse) 
		g_windowManager->CaptureMouse();
	
    SDL_GL_SwapWindow( m_window );
}


void WindowManagerSDL::PollForMessages()
{
	SDL_Event sdlEvent;

	while (SDL_PollEvent(&sdlEvent))
	{
        if( sdlEvent.type == SDL_WINDOWEVENT )
        {
            if( sdlEvent.window.event == SDL_WINDOWEVENT_MOVED )
            {
                WindowHasMoved();
            }
        }
        
		int result = g_inputManager->EventHandler(sdlEvent.type, (long long)&sdlEvent, 0);
		
		if( result == -1 && m_secondaryMessageHandler )
			m_secondaryMessageHandler( sdlEvent.type, (long long)&sdlEvent, 0 );
	}
	
	if( m_tryingToCaptureMouse )
		CaptureMouse();
}


void WindowManagerSDL::CaptureMouse()
{
    #ifdef VERBOSE_DEBUG
        printf("Capturing mouse.\n");
    #endif
    
    // Don't grab if we don't have focus
    if (!g_inputManager->m_windowHasFocus)
        return;

    // Important not to grab the mouse
    // until it's in the window proper. Otherwise
    // we might end up doing some strange things on MAC OS X
    
    int windowFlags = SDL_GetWindowFlags( m_window );
    if( !(windowFlags & SDL_WINDOW_MOUSE_FOCUS) ) {
        m_tryingToCaptureMouse = true;
        return;
    }
    
    SDL_SetRelativeMouseMode(SDL_TRUE);
    m_mouseCaptured = true;
    m_tryingToCaptureMouse = false;
}


void WindowManagerSDL::UncaptureMouse()
{
#ifdef VERBOSE_DEBUG
    printf("Uncapturing mouse.\n");
#endif
    SDL_SetRelativeMouseMode(SDL_FALSE);
    m_mouseCaptured = false;
    m_tryingToCaptureMouse = false;
}


void WindowManagerSDL::HideMousePointer()
{
	SDL_ShowCursor(false);
	m_mousePointerVisible = false;
}


void WindowManagerSDL::UnhideMousePointer()
{
	SDL_ShowCursor(true);
	m_mousePointerVisible = true;
}


void WindowManagerSDL::SetMousePos(int x, int y)
{
    // Not implemented.
}


void WindowManagerSDL::OpenWebsite( const char *_url )
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
#endif
}

void WindowManagerSDL::HideWin()
{
#ifdef TARGET_OS_MACOSX
	ProcessSerialNumber me;
	
	GetCurrentProcess(&me);
	ShowHideProcess(&me, false);
#endif
}

#if defined(TARGET_OS_LINUX) or defined(TARGET_OS_MACOSX)
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
	g_origWorkingDir = new char [PATH_MAX];
    if( !getcwd(g_origWorkingDir, PATH_MAX) )
    {
        strcpy( g_origWorkingDir, "/" );
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
    if (!br_init (&error)) {
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
    if(argc > 0)
    {
        printf("%s\n", argv[0]);
        ChangeToProgramDir(argv[0]);
        chdir("../Resources");
    }
#endif

	if (argc > 1) {
		if (strncmp(argv[1], "-v", 2) == 0) {
			puts(APP_VERSION);
			return 0;
		}
	}
	
    
    SDL_version linkedVersion, compiledVersion;
    SDL_VERSION(&compiledVersion);
    SDL_GetVersion(&linkedVersion);
        
    AppDebugOut("SDL Version: Compiled against %d.%d.%d, running with %d.%d.%d\n",
        compiledVersion.major, compiledVersion.minor, compiledVersion.patch,
        linkedVersion.major, linkedVersion.minor, linkedVersion.patch);

#if TARGET_OS_LINUX
	// Setup illegal memory access handler
	// See debug_utils_gcc.cpp
	SetupMemoryAccessHandlers();
#endif

    g_argc = argc;
    g_argv = argv;
    
	AppMain();
	
	return 0;
}

