 
#ifndef _included_prefsscreenwindow_h
#define _included_prefsscreenwindow_h

#include "interface/components/core.h"


class ScreenOptionsWindow : public InterfaceWindow
{
public:
    int     m_resId;
    int     m_screenMode;
    int     m_colourDepth;
    int     m_refreshRate;
    int     m_zDepth;
    int		m_antiAlias;
    int     m_uiScale;
#if !defined(TARGET_OS_LINUX) || !defined(TARGET_EMSCRIPTEN)
    int     m_fpsLimit;
#endif
	
public:
    ScreenOptionsWindow();

    void Create();
    void Render( bool _hasFocus );


    virtual void RestartWindowManager();                        // override me
};




#endif
