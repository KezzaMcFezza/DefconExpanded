#ifndef INCLUDED_WINDOW_MANAGER_OPENGL_H
#define INCLUDED_WINDOW_MANAGER_OPENGL_H

#ifdef RENDERER_OPENGL

#include <SDL2/SDL.h>
#include "window_manager.h"

class WindowManagerOpenGL : public WindowManager
{
public:
	WindowManagerOpenGL();
	~WindowManagerOpenGL();

	bool        CreateWin           (int _width, int _height,		       // Set _colourDepth, _refreshRate and/or 
			                         bool _windowed, int _colourDepth,	   // _zDepth to -1 to get default values
			                         int _refreshRate, int _zDepth,
									 int _antiAlias, bool _borderless,
		                             const char *_title ) override;
    
	void        DestroyWin          () override;
	void        DoFlip              () override;
	void        HandleResize        (int newWidth, int newHeight) override;
	void        HandleWindowFocusGained() override;

private:
    SDL_GLContext m_glContext;
};

#endif // INCLUDED_WINDOW_MANAGER_OPENGL_H
#endif // RENDERER_OPENGL

