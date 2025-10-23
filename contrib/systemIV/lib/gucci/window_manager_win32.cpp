#include "lib/universal_include.h"

#include <limits.h>
#include <shellapi.h>
#include <thread>
#include <algorithm>

#include "lib/debug_utils.h"

#include "input.h"
#include "window_manager_win32.h"

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
    EnumDisplaySettings ( NULL, ENUM_CURRENT_SETTINGS, &mode );
            
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
    EnumDisplaySettings ( NULL, ENUM_CURRENT_SETTINGS, &mode );

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
	    ChangeDisplaySettings(&devmode, CDS_FULLSCREEN);
    }
}

//
// add proper logging for opengl context creation errors

static void LogLastError(const char* where)
{
    DWORD err = GetLastError();
    if (err != 0) {
        LPVOID lpMsgBuf = nullptr;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPSTR)&lpMsgBuf, 0, nullptr);
        if (lpMsgBuf) {
            AppDebugOut("%s failed. GetLastError=%lu: %s\n", where, err, (LPSTR)lpMsgBuf);
            LocalFree(lpMsgBuf);
        } else {
            AppDebugOut("%s failed. GetLastError=%lu\n", where, err);
        }
    }
}

//
// create a hidden dummy window for loading WGL extensions

static HWND CreateDummyWindow(HINSTANCE hInst)
{
    WNDCLASSA wc = {};
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = DefWindowProcA;
    wc.hInstance     = hInst;
    wc.lpszClassName = "GLDummyWindow";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowA("GLDummyWindow", "dummy", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 32, 32,
                              nullptr, nullptr, hInst, nullptr);
    return hwnd;
}

//
// choose a legacy pixelformat + legacy RC for the dummy

static bool MakeDummyContext(HWND dummy, HDC& hdc, HGLRC& hglrc)
{
    hdc = GetDC(dummy);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize      = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.cStencilBits= 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(hdc, &pfd);
    if (pf == 0) { AppDebugOut("Dummy ChoosePixelFormat failed\n"); LogLastError("ChoosePixelFormat"); return false; }
    if (!SetPixelFormat(hdc, pf, &pfd)) { AppDebugOut("Dummy SetPixelFormat failed\n"); LogLastError("SetPixelFormat"); return false; }

    hglrc = wglCreateContext(hdc);
    if (!hglrc) { AppDebugOut("Dummy wglCreateContext failed\n"); LogLastError("wglCreateContext"); return false; }
    if (!wglMakeCurrent(hdc, hglrc)) { AppDebugOut("Dummy wglMakeCurrent failed\n"); LogLastError("wglMakeCurrent"); return false; }

	//
    // load only the WGL procs we need

    _wglChoosePixelFormatARB    = (PFNWGLCHOOSEPIXELFORMATARBPROC) wglGetProcAddress("wglChoosePixelFormatARB");
    _wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC) wglGetProcAddress("wglCreateContextAttribsARB");
    _wglSwapIntervalEXT         = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");
    _wglGetSwapIntervalEXT      = (PFNWGLGETSWAPINTERVALEXTPROC) wglGetProcAddress("wglGetSwapIntervalEXT");

    return true;
}

static void DestroyDummy(HWND dummy, HDC hdc, HGLRC hglrc)
{
    wglMakeCurrent(nullptr, nullptr);
    if (hglrc) wglDeleteContext(hglrc);
    if (hdc)   ReleaseDC(dummy, hdc);
    DestroyWindow(dummy);
}

//
// create the OpenGL context

void WindowManagerWin32::EnableOpenGL(int _colourDepth, int _zDepth)
{

    //
    // create dummy window to load WGL extensions
    // we need a temporary OpenGL context to access wglGetProcAddress
    
    HINSTANCE hInst = GetModuleHandle(nullptr);
    HWND dummyWnd = CreateDummyWindow(hInst);
    HDC dummyDC = nullptr;
    HGLRC dummyRC = nullptr;
    
    int chosenPixelFormat = 0;
    PIXELFORMATDESCRIPTOR chosenPFD = {};
    PIXELFORMATDESCRIPTOR finalPFD = {};
    
    if (!dummyWnd || !MakeDummyContext(dummyWnd, dummyDC, dummyRC))
    {
        AppDebugOut("Failed to create dummy GL context. Falling back to legacy path.\n");
        // this should never happen but this is useful for debugging
    }


    //
    // get device context for our main window
    
    m_hDC = GetDC(m_hWnd);
    if (!m_hDC)
    {
        if (dummyWnd) DestroyDummy(dummyWnd, dummyDC, dummyRC);
        return;
    }


    //
    // choose pixel format with hardware acceleration
    // attempt modern WGL method first, fall back to legacy if not available
    
    BOOL setPixelFormatSuccess = FALSE;

    if (_wglChoosePixelFormatARB)
    {
        AppDebugOut("Using wglChoosePixelFormatARB for hardware-accelerated pixel format...\n");
        
		//
        // request the hardware-accelerated pixel format that we want

        int pixelAttribs[] = {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
            WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,  // explicitly request hardware acceleration
            WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB, _colourDepth,
            WGL_ALPHA_BITS_ARB, 8,
            WGL_DEPTH_BITS_ARB, _zDepth,
            WGL_STENCIL_BITS_ARB, 8,
            0, 0
        };
        
        UINT numFormats = 0;
        if (_wglChoosePixelFormatARB(m_hDC, pixelAttribs, nullptr, 1, &chosenPixelFormat, &numFormats) && numFormats > 0)
        {
            AppDebugOut("Found hardware-accelerated pixel format: %d\n", chosenPixelFormat);
            DescribePixelFormat(m_hDC, chosenPixelFormat, sizeof(chosenPFD), &chosenPFD);
            setPixelFormatSuccess = SetPixelFormat(m_hDC, chosenPixelFormat, &chosenPFD);
            
            if (!setPixelFormatSuccess)
            {
                LogLastError("SetPixelFormat(ARB)");
            }
        }
        else
        {
            AppDebugOut("wglChoosePixelFormatARB failed to find suitable format\n");
        }
    }

	//
    // fall back to legacy pixel format selection if modern method failed

    if (!setPixelFormatSuccess)
    {
        AppDebugOut("Falling back to legacy ChoosePixelFormat\n");
        
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = _colourDepth;
        pfd.cAlphaBits = 8;
        pfd.cDepthBits = _zDepth;
        pfd.cStencilBits = 8;
        pfd.iLayerType = PFD_MAIN_PLANE;

        chosenPixelFormat = ChoosePixelFormat(m_hDC, &pfd);
        if (chosenPixelFormat == 0)
        {
            LogLastError("ChoosePixelFormat");
            goto CLEANUP_AND_FAIL;
        }

        if (!SetPixelFormat(m_hDC, chosenPixelFormat, &pfd))
        {
            LogLastError("SetPixelFormat");
            goto CLEANUP_AND_FAIL;
        }
        
        chosenPFD = pfd;
    }


    //
    // destroy dummy window
    
    if (dummyWnd)
    {
        DestroyDummy(dummyWnd, dummyDC, dummyRC);
    }


    //
    // create OpenGL 3.3 context
    // try compatibility profile first, if that somehow fails we have core profile
    
    HGLRC shareContext = nullptr;
    
    if (_wglCreateContextAttribsARB)
    {
		//
        // compatibility Profile

        int contextAttribsCompat[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
            0
        };
        
        m_hRC = _wglCreateContextAttribsARB(m_hDC, shareContext, contextAttribsCompat);
        
        if (m_hRC)
        {
            AppDebugOut("Successfully created OpenGL 3.3 Compatibility context\n");
        }
        else
        {
			//
            // core Profile
			
            AppDebugOut("OpenGL 3.3 Compatibility failed, trying Core profile\n");
            
            int contextAttribsCore[] = {
                WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
                WGL_CONTEXT_MINOR_VERSION_ARB, 3,
                WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
                WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                0
            };
            
            m_hRC = _wglCreateContextAttribsARB(m_hDC, shareContext, contextAttribsCore);
            
            if (m_hRC)
            {
                AppDebugOut("Successfully created OpenGL 3.3 Core context\n");
            }
        }
    }

	//
    // fall back to legacy OpenGL context if modern creation failed

    if (!m_hRC)
    {
        AppDebugOut("Falling back to legacy wglCreateContext\n");
        m_hRC = wglCreateContext(m_hDC);
        
        if (!m_hRC)
        {
            LogLastError("wglCreateContext");
            goto CLEANUP_AND_FAIL;
        }
    }
    
    if (!wglMakeCurrent(m_hDC, m_hRC))
    {
        LogLastError("wglMakeCurrent");
        goto CLEANUP_AND_FAIL;
    }


    //
    // initialize GLAD
    
    if (!gladLoadGL())
    {
        AppDebugOut("gladLoadGL failed\n");
        return;
    }


    //
    // if we have to start looking at these debug logs
	// then something is wrong
    
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    const char* version = (const char*)glGetString(GL_VERSION);
    
    AppDebugOut("Reported GPU Vendor   : %s\n", vendor ? vendor : "(null)");
    AppDebugOut("Reported GPU Renderer : %s\n", renderer ? renderer : "(null)");
    AppDebugOut("Reported GPU Version  : %s\n", version ? version : "(null)");

	//
    // do we have software rendering?

    if ((vendor && strstr(vendor, "Microsoft")) || (renderer && strstr(renderer, "GDI Generic")))
    {
        AppDebugOut("WARNING: Software rendering detected via driver\n");
    }

    //
    // pixel formats are usually more accurate for detecting software rendering

    if ((finalPFD.dwFlags & PFD_GENERIC_FORMAT) || (finalPFD.dwFlags & PFD_SUPPORT_GDI))
    {
        AppDebugOut("WARNING: Software rendering detected via pixel format\n");
    }

	//
    // disable vsync, nobody likes it
    
    if (_wglSwapIntervalEXT)
    {
        _wglSwapIntervalEXT(0);
    }

    return;
    
CLEANUP_AND_FAIL:
    if (m_hRC)
    {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_hRC);
        m_hRC = nullptr;
    }
    
    if (m_hDC)
    {
        ReleaseDC(m_hWnd, m_hDC);
        m_hDC = nullptr;
    }
    
    if (dummyWnd)
    {
        DestroyDummy(dummyWnd, dummyDC, dummyRC);
    }
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
				//
                // insert in descending order (highest refresh rate first)

				bool added = false;
				for( int i = 0; i < res->m_refreshRates.Size(); ++i )
				{
					if( devMode.dmDisplayFrequency > res->m_refreshRates[i] )
					{
						res->m_refreshRates.PutDataAtIndex( devMode.dmDisplayFrequency, i );
						added = true;
						break;
					}
				}
				if( !added )
				{
					res->m_refreshRates.PutDataAtEnd( devMode.dmDisplayFrequency );
				}
			}
		}
		++i;
	}
}


bool WindowManagerWin32::CreateWin(int _width, int _height, bool _windowed, 
	           				     int _colourDepth, int _refreshRate, int _zDepth,
                                 int antiAlias, bool _borderless, const char *_title )
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

		posX = (desktopRect.right - _width) / 2;
		posY = (desktopRect.bottom - _height) / 2;
	}
	else if (_borderless)
	{
		//
		// borderless fullscreen: use desktop resolution and don't change display settings
		// creates a popup window that covers the entire screen

        RestoreDesktop();

        windowStyle = WS_POPUP | WS_VISIBLE | WS_MAXIMIZE;
		
		HWND desktopWindow = GetDesktopWindow();
		RECT desktopRect;
		GetWindowRect(desktopWindow, &desktopRect);
		
		posX = desktopRect.left;
		posY = desktopRect.top;
		_width = desktopRect.right - desktopRect.left;
		_height = desktopRect.bottom - desktopRect.top;
		
		m_borderWidth = 0;
		m_titleHeight = 0;
	}
	else
	{

		//
		// real fullscreen: change display mode with requested resolution and refresh rate
        
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

