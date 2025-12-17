
#ifndef _included_soundoptionswindow_h
#define _included_soundoptionswindow_h

#include "systemiv.h"
#include "lib/eclipse/components/core.h"

//#define SOUNDOPTIONSWINDOW_USEDSPEFFECTS


class SoundOptionsWindow : public InterfaceWindow
{
public:
	int     m_mixFreq;
	int     m_swapStereo;
	int     m_dspEffects;
	int     m_memoryUsage;
	int     m_masterVolume;
	int     m_sfxQuality;
	int     m_musicQuality;
	int     m_channelCount;
#ifdef WINDOWS_SDL
	int     m_audioDriver;
#endif

public:
    SoundOptionsWindow();

    void Create();
    void Render( bool _hasFocus );
};


#endif
