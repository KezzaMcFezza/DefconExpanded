#ifndef INCLUDED_SOUND_DEBUG_OVERLAY_H
#define INCLUDED_SOUND_DEBUG_OVERLAY_H

#include <vector>

#include "lib/sound/sound_library_2d_sdl.h"
#include "resampler_polyphase.h"

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
    double      m_directCallbacksPerSec;
    double      m_wavCallbacksPerSec;
    double      m_topupProcessedPerSec;
    // SoundSystem mixer safety diagnostics
    unsigned long long m_prevInvalidChannelReadsTotal = 0ULL;
    double      m_invalidChannelReadsPerSec = 0.0;
    unsigned long long m_invalidChannelReadsDelta = 0ULL;
#if !defined(SOUND_USE_DSOUND_FREQUENCY_STUFF)
    int         m_resampleInstanceCount;
    int         m_resampleWaitingForLoop;
    int         m_resampleInvalidInstances;
    double      m_resampleStepMin;
    double      m_resampleStepMax;
    double      m_resampleStepAvg;
    double      m_resampleCursorFracMin;
    double      m_resampleCursorFracMax;
    double      m_resampleCursorFracAvg;
    SoundResampler::Quality m_resampleSfxQuality;
    SoundResampler::Quality m_resampleMusicQuality;
    unsigned int m_resampleLinearInstances;
    std::vector<int> m_resampleBankUsageSfx;
    std::vector<int> m_resampleBankUsageMusic;
    // (Per-voice resampler load indicator removed)
#endif
};

#endif
