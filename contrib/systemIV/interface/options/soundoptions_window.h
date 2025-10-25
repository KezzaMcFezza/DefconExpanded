
#ifndef _included_soundoptionswindow_h
#define _included_soundoptionswindow_h

#include "lib/universal_include.h"
#include "interface/components/core.h"
#include <string>
#include <vector>

#if defined(HAVE_DSOUND) && !defined(WINDOWS_SDL)
#define SOUNDOPTIONSWINDOW_USEHARDWARE3D
#endif
//#define SOUNDOPTIONSWINDOW_USEDSPEFFECTS


class SoundOptionsWindow : public InterfaceWindow
{
public:
	int     m_soundLib;
	int     m_mixFreq;
	int     m_useHardware3D;
	int     m_swapStereo;
	int     m_dspEffects;
	int     m_memoryUsage;
	int     m_masterVolume;
#if !defined(TARGET_MSVC) || defined(WINDOWS_SDL)
	int     m_soundBufferSize;
#endif
#if defined(WINDOWS_SDL) || defined(TARGET_OS_MACOSX) || defined(TARGET_OS_LINUX)
	int                             m_selectedDeviceIndex;
	std::string                     m_preferredOutputDevice;
	std::vector<std::string>        m_outputDevices;
#endif

public:
    SoundOptionsWindow();

    void Create();
    void Render( bool _hasFocus );
#if defined(WINDOWS_SDL) || defined(TARGET_OS_MACOSX) || defined(TARGET_OS_LINUX)
	const char *GetSelectedOutputDeviceName() const;
	void PopulateOutputDevices();
#endif
};


#endif

