#include "lib/universal_include.h"

#include <limits.h>
#include <shellapi.h>

#include "lib/debug_utils.h"

#include "input.h"
#include "window_manager_win32.h"

#ifdef TARGET_MSVC
#include <GL/wglew.h>
#endif

static HINSTANCE g_hInstance;

#define WH_KEYBOARD_LL 13

#ifndef LLKHF_ALTDOWN
#define LLKHF_ALTDOWN 0x20
#endif


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    SecondaryEventHandler secondaryHandler = g_windowManager->GetSecondaryMessageHandler();

	if (!g_inputManager ||
  		 g_inputManager->EventHandler(message, wParam, lParam) == -1 )
	{
        if( !secondaryHandler ||
            secondaryHandler(message, wParam, lParam) == -1 )
        {
			if( message == WM_CLOSE )
			{
				AppShutdown();
				PostQuitMessage(0);
				return 0;
			}

		    return DefWindowProc( hWnd, message, wParam, lParam );
        }
        return 0;
	}

	return 0;
}


WindowManagerWin32::WindowManagerWin32()
:	m_hWnd(NULL),
	m_hDC(NULL),
	m_hRC(NULL),
	m_dpiAwareness(false)

{
	AppAssert(g_windowManager == NULL);

	ListAllDisplayModes();

    SaveDesktop();
}


void WindowManagerWin32::SaveDesktop()
{
    DEVMODE mode;
    ZeroMemory(&mode, sizeof(mode));
    mode.dmSize = sizeof(mode);
    bool success = EnumDisplaySettings ( NULL, ENUM_CURRENT_SETTINGS, &mode );
            
    m_desktopScreenW = mode.dmPelsWidth;
    m_desktopScreenH = mode.dmPelsHeight;
    m_desktopColourDepth = mode.dmBitsPerPel;
    m_desktopRefresh = mode.dmDisplayFrequency;
}


void WindowManagerWin32::RestoreDesktop()
{
    //
    // Get current settings

    DEVMODE mode;
    ZeroMemory(&mode, sizeof(mode));
    mode.dmSize = sizeof(mode);
    bool success = EnumDisplaySettings ( NULL, ENUM_CURRENT_SETTINGS, &mode );

    //
    // has anything changed?

    bool changed = ( m_desktopScreenW != mode.dmPelsWidth ||
                     m_desktopScreenH != mode.dmPelsHeight ||
                     m_desktopColourDepth != mode.dmBitsPerPel ||
                     m_desktopRefresh != mode.dmDisplayFrequency );


    //
    // Restore if required

    if( changed )
    {
	    DEVMODE devmode;
	    devmode.dmSize = sizeof(DEVMODE);
	    devmode.dmBitsPerPel = m_desktopColourDepth;
	    devmode.dmPelsWidth = m_desktopScreenW;
	    devmode.dmPelsHeight = m_desktopScreenH;
	    devmode.dmDisplayFrequency = m_desktopRefresh;
	    devmode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY | DM_BITSPERPEL;
	    long result = ChangeDisplaySettings(&devmode, CDS_FULLSCREEN);
    }
}


void WindowManagerWin32::EnableOpenGL(int _colourDepth, int _zDepth)
{
	PIXELFORMATDESCRIPTOR pfd;
	int format;
	
	// Get the device context (DC)
	m_hDC = GetDC( m_hWnd );
	
	// Set the pixel format for the DC
	ZeroMemory( &pfd, sizeof( pfd ) );
	pfd.nSize = sizeof( pfd );
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = _colourDepth;
	pfd.cDepthBits = _zDepth;
	pfd.iLayerType = PFD_MAIN_PLANE;
	
	format = ChoosePixelFormat( m_hDC, &pfd );
	
	// verify we actually got hardware acceleration
	PIXELFORMATDESCRIPTOR actualPfd;
	DescribePixelFormat(m_hDC, format, sizeof(actualPfd), &actualPfd);
	
	if (actualPfd.dwFlags & PFD_GENERIC_FORMAT && !(actualPfd.dwFlags & PFD_GENERIC_ACCELERATED)) {
		
		int numFormats = DescribePixelFormat(m_hDC, 1, sizeof(PIXELFORMATDESCRIPTOR), NULL);
		bool foundHardware = false;
		
		for (int i = 1; i <= numFormats; i++) {
			DescribePixelFormat(m_hDC, i, sizeof(actualPfd), &actualPfd);
			
			// check if this format is hardware accelerated
			if (!(actualPfd.dwFlags & PFD_GENERIC_FORMAT) || 
			    (actualPfd.dwFlags & PFD_GENERIC_ACCELERATED)) {
				if ((actualPfd.dwFlags & PFD_DRAW_TO_WINDOW) &&
				    (actualPfd.dwFlags & PFD_SUPPORT_OPENGL) &&
				    (actualPfd.dwFlags & PFD_DOUBLEBUFFER) &&
				    actualPfd.iPixelType == PFD_TYPE_RGBA &&
				    actualPfd.cColorBits >= _colourDepth &&
				    actualPfd.cDepthBits >= _zDepth) {
					
					format = i;
					foundHardware = true;
					AppDebugOut("SUCCESS: Found hardware accelerated format: %d\n", i);
					break;
				}
			}
		}
		
		if (!foundHardware) {
			AppDebugOut("FATAL: Could not find hardware accelerated OpenGL format!\n");
		}
	}
	
	SetPixelFormat( m_hDC, format, &pfd );
	
	// Create a temporary context to initialize GLEW
	HGLRC tempContext = wglCreateContext( m_hDC );
	wglMakeCurrent( m_hDC, tempContext );
	
	// Initialize GLEW
	GLenum glewResult = glewInit();
	if (glewResult != GLEW_OK) {
		// GLEW failed, fall back to legacy OpenGL
		AppDebugOut("GLEW initialization failed: %s\n", glewGetErrorString(glewResult));
		m_hRC = tempContext;
		glClear(GL_COLOR_BUFFER_BIT);
		return;
	}
	
	//
	// print OpenGL information for debugging

	const GLubyte* renderer = glGetString(GL_RENDERER);
	
	// check if we got software rendering
	if (strstr((const char*)renderer, "GDI Generic") || 
	    strstr((const char*)renderer, "Software") ||
	    strstr((const char*)renderer, "Microsoft")) {
		AppDebugOut("FATAL: Using software rendering :(\n");
	}
	
	// check if we can create a modern OpenGL 3.3 Core context using WGL extensions
	if (WGLEW_ARB_create_context && WGLEW_ARB_create_context_profile) {
		if (WGLEW_ARB_pixel_format) {
			AppDebugOut("Using wglChoosePixelFormatARB for hardware-accelerated pixel format...\n");
			
			const int pixelAttribs[] = {
				WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
				WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
				WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
				WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,  // require hardware acceleration
				WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
				WGL_COLOR_BITS_ARB, _colourDepth,
				WGL_DEPTH_BITS_ARB, _zDepth,
				0
			};
			
			int pixelFormat;
			UINT numFormats;
			if (wglChoosePixelFormatARB(m_hDC, pixelAttribs, NULL, 1, &pixelFormat, &numFormats) && numFormats > 0) {
				AppDebugOut("Found hardware-accelerated pixel format via WGL: %d\n", pixelFormat);
				
				// we would need to recreate the window to use this new pixel format
				// for now well continue with what we have but this is noted for future
			}
		}
		
		// define OpenGL 3.3 compatibility crofile attributes
		int contextAttribs[] = {
			WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
			WGL_CONTEXT_MINOR_VERSION_ARB, 3,
			WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB, 
			WGL_CONTEXT_FLAGS_ARB, 0,
			0
		};
		
		m_hRC = wglCreateContextAttribsARB(m_hDC, 0, contextAttribs);
		
		if (m_hRC) {
			// Success! Clean up temporary context and use the new one
			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(tempContext);
			wglMakeCurrent(m_hDC, m_hRC);
			
			// Re-initialize GLEW with the new context
			glewResult = glewInit();
			if (glewResult != GLEW_OK) {
				AppDebugOut("GLEW re-initialization failed: %s\n", glewGetErrorString(glewResult));
				wglMakeCurrent(NULL, NULL);
				wglDeleteContext(m_hRC);
				m_hRC = wglCreateContext( m_hDC );
				wglMakeCurrent( m_hDC, m_hRC );
			} else {
				AppDebugOut("Successfully created OpenGL 3.3 Compatibility context\n");
			}
		} else {
			AppDebugOut("OpenGL 3.3 context creation failed, using legacy context\n");
			// OpenGL 3.3 context creation failed, use the temporary context
			m_hRC = tempContext;
		}
	} else {
		// WGL extensions not available, use the temporary context
		m_hRC = tempContext;
	}

	glClear(GL_COLOR_BUFFER_BIT);
}



void WindowManagerWin32::DisableOpenGL()
{
	wglMakeCurrent( NULL, NULL );
	wglDeleteContext( m_hRC );
	ReleaseDC( m_hWnd, m_hDC );
}


void WindowManagerWin32::ListAllDisplayModes()
{
	int i = 0;
	DEVMODE devMode;
	while (EnumDisplaySettings(NULL, i, &devMode) != 0)
	{
		if (devMode.dmBitsPerPel >= 15 &&
			devMode.dmPelsWidth >= 640 &&
			devMode.dmPelsHeight >= 480)
		{
			int resId = GetResolutionId(devMode.dmPelsWidth, devMode.dmPelsHeight);
			WindowResolution *res;
			if (resId == -1)
			{
				res = new WindowResolution(devMode.dmPelsWidth, devMode.dmPelsHeight);
				
                bool added = false;
                for( int i = 0; i < m_resolutions.Size(); ++i )
                {
                    if( devMode.dmPelsWidth < m_resolutions[i]->m_width )
                    {
                        m_resolutions.PutDataAtIndex( res, i );
                        added = true;
                        break;
                    }
                }

                if( !added )
                {
                    m_resolutions.PutDataAtEnd( res );
                }
			}
			else
			{
				res = m_resolutions[resId];
			}

			if (res->m_refreshRates.FindData(devMode.dmDisplayFrequency) == -1)
			{
				res->m_refreshRates.PutDataAtEnd(devMode.dmDisplayFrequency);
			}
		}
		++i;
	}
}


bool WindowManagerWin32::CreateWin(int _width, int _height, bool _windowed, 
	           				     int _colourDepth, int _refreshRate, int _zDepth,
                                 int antiAlias, const char *_title )
{
	if (!m_dpiAwareness) {
		bool SetDPIAware = false;
		
		if (_windowed) {
			// Always set DPI awareness for windowed mode
			SetDPIAware = true;
		} else {

			//
			// For fullscreen mode, only set DPI awareness if resolution is >= desktop resolution
			//
			// This still is buggy as SetProcessDPIAware cannot be changed during runtime so a
			// restart is required right now until i can be assed making a proper fix
			// and who the fuck wants to run at a lower resolution in fullscreen anyway

			if (_width >= m_desktopScreenW && _height >= m_desktopScreenH) {
				SetDPIAware = true;
			}
		}
		
		if (SetDPIAware) {
			SetProcessDPIAware();
		}
		
		m_dpiAwareness = true;
	}

	m_screenW = _width;
	m_screenH = _height;
    m_windowed = _windowed;

	// Register window class
	WNDCLASS wc;
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = g_hInstance;
	wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
	wc.hCursor = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
	wc.lpszMenuName = NULL;
	wc.lpszClassName = strdup( _title );
	RegisterClass( &wc );

	int posX, posY;
	unsigned int windowStyle = WS_POPUPWINDOW | WS_VISIBLE;

	if (_windowed)
	{
        RestoreDesktop();

        windowStyle |= WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
		RECT windowRect = { 0, 0, _width, _height };
		AdjustWindowRect(&windowRect, windowStyle, false);
		m_borderWidth = ((windowRect.right - windowRect.left) - _width) / 2;
		m_titleHeight = ((windowRect.bottom - windowRect.top) - _height) - m_borderWidth * 2;
		_width += m_borderWidth * 2;
		_height += m_borderWidth * 2 + m_titleHeight;

		HWND desktopWindow = GetDesktopWindow();
		RECT desktopRect;
		GetWindowRect(desktopWindow, &desktopRect);
		int desktopWidth = desktopRect.right - desktopRect.left;
		int desktopHeight = desktopRect.bottom - desktopRect.top;

		posX = (desktopRect.right - _width) / 2;
		posY = (desktopRect.bottom - _height) / 2;
	}
	else
	{
        windowStyle |= WS_MAXIMIZE;
        
		DEVMODE devmode;
		devmode.dmSize = sizeof(DEVMODE);
		devmode.dmBitsPerPel = _colourDepth;
		devmode.dmPelsWidth = _width;
		devmode.dmPelsHeight = _height;
		devmode.dmDisplayFrequency = _refreshRate;
		devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
		long result = ChangeDisplaySettings(&devmode, CDS_FULLSCREEN);
        if( result != DISP_CHANGE_SUCCESSFUL ) return false;
        
        posX = 0;
		posY = 0;
		m_borderWidth = 1;
		m_titleHeight = 0;
	}

	// Create main window
	m_hWnd = CreateWindow( 
		wc.lpszClassName, wc.lpszClassName, 
		windowStyle,
		posX, posY, _width, _height,
		NULL, NULL, g_hInstance, NULL );

    if( !m_hWnd ) return false;

	m_mouseOffsetX = INT_MAX;

	// Enable OpenGL for the window
	EnableOpenGL(_colourDepth, _zDepth);

    return true;
}

void WindowManagerWin32::HideWin()
{
	ShowWindow(GetWindowManagerWin32()->m_hWnd, SW_MINIMIZE);
	ShowWindow(GetWindowManagerWin32()->m_hWnd, SW_HIDE);
}

void WindowManagerWin32::DestroyWin()
{
	DisableOpenGL();
	DestroyWindow(m_hWnd);
}


void WindowManagerWin32::Flip()
{
    PollForMessages();
	SwapBuffers(m_hDC);
}


void WindowManagerWin32::PollForMessages()
{
	MSG msg;
	while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) ) 
	{
		// handle or dispatch messages
		TranslateMessage( &msg );
		DispatchMessage( &msg );
	} 
}


void WindowManagerWin32::SetMousePos(int x, int y)
{
	if (m_mouseOffsetX == INT_MAX)
	{
		RECT rect1;
		GetWindowRect(m_hWnd, &rect1);

		RECT rect2;
		GetClientRect(m_hWnd, &rect2);

		//int borderWidth = (m_screenW - rect2.right) / 2;
		//int menuHeight = (m_screenH - rect2.bottom) - (borderWidth * 2);
		m_mouseOffsetX = rect1.left + m_borderWidth;
		m_mouseOffsetY = rect1.top + m_borderWidth + m_titleHeight;
	}

	// Convert logical coordinates to physical coordinates for cursor positioning
	float scaleX = (float)PhysicalWindowW() / (float)WindowW();
	float scaleY = (float)PhysicalWindowH() / (float)WindowH();
	
	int physicalX = (int)(x * scaleX);
	int physicalY = (int)(y * scaleY);

	SetCursorPos(physicalX + m_mouseOffsetX, physicalY + m_mouseOffsetY);
}

HINSTANCE WindowManagerWin32::GethInstance()
{
    return g_hInstance;
}

void WindowManagerWin32::CaptureMouse()
{
	SetCapture(m_hWnd);
	m_mouseCaptured = true;
}


void WindowManagerWin32::UncaptureMouse()
{
	ReleaseCapture();
	m_mouseCaptured = true;
}


void WindowManagerWin32::HideMousePointer()
{
	ShowCursor(false);
	m_mousePointerVisible = false;
}


void WindowManagerWin32::UnhideMousePointer()
{
	ShowCursor(true);
	m_mousePointerVisible = true;
}


void WindowManagerWin32::OpenWebsite( const char *_url )
{
    ShellExecute(NULL, "open", _url, NULL, NULL, SW_SHOWNORMAL); 
}


#ifdef USE_CRASHREPORTING
#include "contrib/XCrashReport/client/exception_handler.h"
#endif

int WINAPI WinMain(HINSTANCE _hInstance, HINSTANCE _hPrevInstance, 
				   LPSTR _cmdLine, int _iCmdShow)
{
	g_hInstance = _hInstance;

#ifdef USE_CRASHREPORTING
    __try
#endif
    {

        g_argc = __argc;
        g_argv = __argv;

		AppMain();
    }
#ifdef USE_CRASHREPORTING
	__except( RecordExceptionInfo( GetExceptionInformation(), "WinMain", APP_NAME, APP_VERSION ) ) {}
#endif
	
	return WM_QUIT;
}

