#ifndef INCLUDED_WINDOW_MANAGER_SDL_H
#define INCLUDED_WINDOW_MANAGER_SDL_H

#include <SDL2/SDL.h>
#include "window_manager.h"

class WindowManagerSDL : public WindowManager
{
public:
	WindowManagerSDL();
	~WindowManagerSDL();

	bool        CreateWin           (int _width, int _height,		       // Set _colourDepth, _refreshRate and/or 
			                         bool _windowed, int _colourDepth,	   // _zDepth to -1 to get default values
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
    void        OpenWebsite( const char *_url );

private:
	void        ListAllDisplayModes ( int displayIndex );
    void        CalculateHighDPIScaleFactors();
    void        WindowHasMoved();
    
	bool		m_tryingToCaptureMouse;
    
    int         m_windowDisplayIndex;
    SDL_Window   *m_window;
    SDL_GLContext m_glContext;
};

#endif
