#ifndef INCLUDED_WINDOW_MANAGER_H
#define INCLUDED_WINDOW_MANAGER_H

#include "systemiv.h"
#include "lib/tosser/llist.h"

class WindowResolution;

typedef long (*SecondaryEventHandler )(unsigned int, long, long long);
typedef void (*WindowResizeHandler)(int newWidth, int newHeight, int oldWidth, int oldHeight);

// ****************************************************************************
// Class WindowManager
// ****************************************************************************

class WindowManager
{
public:
	static WindowManager *Create();

	WindowManager();	
	virtual ~WindowManager();

	virtual bool        CreateWin           (int _width, int _height,			// Set _colourDepth, _refreshRate and/or 
				                             bool _windowed, int _colourDepth,	// _zDepth to -1 to get default values
				                             int _refreshRate, int _zDepth,
											 int _antiAlias, bool _borderless,
		                                     const char *_title ) = 0;

	virtual void		HideWin				();
	virtual void        DestroyWin          () = 0;
	void                Flip                ();
	virtual void        PollForMessages     ();
	virtual void        SetMousePos         (int x, int y);
	virtual void        HandleResize        (int newWidth, int newHeight) = 0;
	virtual void        HandleWindowFocusGained() = 0;

	virtual void        CaptureMouse        ();
	virtual void        UncaptureMouse      ();

	virtual void        HideMousePointer    ();
	virtual void        UnhideMousePointer  ();

    virtual void        OpenWebsite			(const char *_url);
    
	const int           WindowW             (); // Kept for backwards compatibility
	const int           WindowH             (); // ^^^^^^^^^^^^^^

    const int         	GetPhysicalWidth	(); // Physical window dimensions (actual pixels)
    const int         	GetPhysicalHeight   (); // ^^^^^^^^^^^^^^
    
    const int           GetLogicalWidth		(); // Logical window dimensions (what the game thinks the resolution is)
    const int         	GetLogicalHeight	();	// ^^^^^^^^^^^^^^

    bool        		Windowed            ();
	bool        		Captured            ();
	bool        		MouseVisible        ();

    void        		SuggestDefaultRes   ( int *_width, int *_height, int *_refresh, int *_depth );
	int                 GetCurrentRefreshRate ();
	
	int                 GetResolutionId (int _width, int _height);      // Returns -1 if resolution doesn't exist
    WindowResolution    *GetResolution  (int _id);

    void        		RegisterMessageHandler (SecondaryEventHandler _messageHandler );
    SecondaryEventHandler GetSecondaryMessageHandler();
    
    void                RegisterWindowResizeHandler(WindowResizeHandler _handler);
    WindowResizeHandler GetWindowResizeHandler();
    
    void        SaveDesktop               ();
    void        RestoreDesktop            ();
    void        ListAllDisplayModes       (int displayIndex);
    void        WindowHasMoved            ();

    int         GetDefaultDisplayIndex();
    int         GetNumDisplays        ();
    const char* GetDisplayName        (int displayIndex);
    int         GetCurrentDisplayIndex();
    void        GetDisplayUsableSize  (int displayIndex, int *outW, int *outH);

	static SDL_DisplayID GetDisplayIDFromIndex( int displayIndex );
	static int GetDisplayIndexFromID          ( SDL_DisplayID id );

    SDL_Window* GetSDLWindow          () const { return m_sdlWindow; }
    LList		<WindowResolution *> m_resolutions;

protected:
	virtual void DoFlip              () = 0;

	void        InitializeSDL       ();
	void        CleanupResolutions  ();

	int			m_screenW;	
	int			m_screenH;	
	bool        m_windowed; 
	bool        m_mouseCaptured;
	bool		m_mousePointerVisible;

	int			m_mouseOffsetX;
	int			m_mouseOffsetY;

	int			m_borderWidth;
	int			m_titleHeight;

    int         m_desktopScreenW;           // Original starting values
    int         m_desktopScreenH;           // Original starting values
    int         m_desktopColourDepth;       // Original starting values
    int         m_desktopRefresh;           // Original starting values

	static int  GetDisplayIndexForPoint(int x, int y);

    SecondaryEventHandler m_secondaryMessageHandler;
    WindowResizeHandler m_windowResizeHandler;
    
    SDL_Window* m_sdlWindow;
    int         m_windowDisplayIndex;
    bool        m_tryingToCaptureMouse;
};


// ****************************************************************************
// Class WindowResolution
// ****************************************************************************

class WindowResolution
{
public:
	int			m_width;
	int			m_height;
	LList <int>	m_refreshRates;

	WindowResolution(int _width, int _height): 
                        m_width(_width), 
                        m_height(_height) {}
};


void AppMain();

void AppShutdown();

extern WindowManager *g_windowManager;
extern int     g_argc;
extern char**  g_argv;

#endif
