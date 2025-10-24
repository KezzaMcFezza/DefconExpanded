#ifndef INCLUDED_WINDOW_MANAGER_WIN32_H
#define INCLUDED_WINDOW_MANAGER_WIN32_H

#include "window_manager.h"

#define NOMINMAX
#include <windows.h>

typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int *);
typedef BOOL  (WINAPI *PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC, const int *, const FLOAT *, UINT, int *, UINT *);
typedef BOOL  (WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int);
typedef int   (WINAPI *PFNWGLGETSWAPINTERVALEXTPROC)(void);

static PFNWGLCREATECONTEXTATTRIBSARBPROC  _wglCreateContextAttribsARB  = nullptr;
static PFNWGLCHOOSEPIXELFORMATARBPROC     _wglChoosePixelFormatARB     = nullptr;
static PFNWGLSWAPINTERVALEXTPROC          _wglSwapIntervalEXT          = nullptr;
static PFNWGLGETSWAPINTERVALEXTPROC       _wglGetSwapIntervalEXT       = nullptr;

class WindowManagerWin32 : public WindowManager
{
public:
	WindowManagerWin32();

	bool        CreateWin           (int _width, int _height,		                        // Set _colourDepth, _refreshRate and/or 
			                         bool _windowed, int _colourDepth,		                // _zDepth to -1 to get default values
			                         int _refreshRate, int _zDepth,
									 int _antiAlias, bool _borderless,
		                             const char *_title );
    
	void		HideWin				();
	void        DestroyWin          ();
	void        Flip                ();
	void        PollForMessages     ();
	void        SetMousePos         (int x, int y);
    
	void        CaptureMouse        ();
	void        UncaptureMouse      ();
    
	void        HideMousePointer    ();
	void        UnhideMousePointer  ();
    
    void        SaveDesktop         ();
    void        RestoreDesktop      ();
    void        OpenWebsite			( const char *_url );
    HINSTANCE   GethInstance();

public:
	HWND		m_hWnd;
	HDC			m_hDC;
	HGLRC		m_hRC;

protected:
	void        ListAllDisplayModes ();
	void        EnableOpenGL        (int _colourDepth, int _zDepth);
	void        DisableOpenGL       ();
	
private:
	bool        m_dpiAwareness;
};

inline WindowManagerWin32 *GetWindowManagerWin32()
{
    return static_cast<WindowManagerWin32 *>(g_windowManager);
}

#endif
