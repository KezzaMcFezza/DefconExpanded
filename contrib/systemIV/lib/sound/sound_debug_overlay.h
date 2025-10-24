#ifndef INCLUDED_SOUND_DEBUG_OVERLAY_H
#define INCLUDED_SOUND_DEBUG_OVERLAY_H

#include "lib/sound/sound_library_2d_sdl.h"

class SoundDebugOverlay
{
public:
    SoundDebugOverlay();

    void Update(double frameTime);
    void Render();

private:
    SoundLibrary2dSDL::RuntimeStats m_cachedStats;
    SoundLibrary2dSDL::RuntimeStats m_previousStats;
    bool        m_hasStats;
    double      m_lastStatsSampleTime;
    double      m_audioCallbacksPerSec;
    double      m_topupCallsPerSec;
    double      m_queuedCallbacksPerSec;
};

#endif
