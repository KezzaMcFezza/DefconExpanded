#include "lib/universal_include.h"

#include <algorithm>
#include <limits>
#include <stdio.h>
#include <unordered_set>
#include <deque>

#include "lib/sound/sound_debug_overlay.h"

#include "lib/gucci/window_manager.h"
#include "lib/hi_res_time.h"
#include "lib/preferences.h"
#include "lib/render2d/renderer.h"
#include "lib/sound/sound_library_2d_sdl.h"
#include "lib/sound/sound_library_3d.h"
#include "lib/sound/sound_instance.h"
#include "lib/sound/sound_sample_bank.h"
#include "lib/sound/soundsystem.h"

SoundDebugOverlay::SoundDebugOverlay()
:   m_cachedStats(),
    m_previousStats(),
    m_hasStats(false),
    m_lastStatsSampleTime(GetHighResTime()),
    m_audioCallbacksPerSec(0.0),
    m_topupCallsPerSec(0.0),
    m_queuedCallbacksPerSec(0.0),
    m_directCallbacksPerSec(0.0),
    m_wavCallbacksPerSec(0.0),
    m_topupProcessedPerSec(0.0)
#if !defined(SOUND_USE_DSOUND_FREQUENCY_STUFF)
    ,m_resampleInstanceCount(0),
    m_resampleWaitingForLoop(0),
    m_resampleInvalidInstances(0),
    m_resampleStepMin(0.0),
    m_resampleStepMax(0.0),
    m_resampleStepAvg(0.0),
    m_resampleCursorFracMin(0.0),
    m_resampleCursorFracMax(0.0),
    m_resampleCursorFracAvg(0.0),
    m_resampleSfxQuality(SoundResampler::Quality::Linear),
    m_resampleMusicQuality(SoundResampler::Quality::Linear),
    m_resampleLinearInstances(0),
    m_resampleBankUsageSfx(),
    m_resampleBankUsageMusic()
#endif
{
}

void SoundDebugOverlay::Update(double /*frameTime*/)
{
    SoundLibrary2dSDL *sdl = g_soundLibrary2d ? dynamic_cast<SoundLibrary2dSDL *>(g_soundLibrary2d) : NULL;

    if (!sdl)
    {
        m_cachedStats = SoundLibrary2dSDL::RuntimeStats();
        m_previousStats = SoundLibrary2dSDL::RuntimeStats();
        m_hasStats = false;
        m_audioCallbacksPerSec = 0.0;
        m_topupCallsPerSec = 0.0;
        m_queuedCallbacksPerSec = 0.0;
        m_lastStatsSampleTime = GetHighResTime();
#if !defined(SOUND_USE_DSOUND_FREQUENCY_STUFF)
        m_prevInvalidChannelReadsTotal = 0ULL;
        m_invalidChannelReadsPerSec = 0.0;
        m_invalidChannelReadsDelta = 0ULL;
#endif
#if !defined(SOUND_USE_DSOUND_FREQUENCY_STUFF)
        m_resampleInstanceCount = 0;
        m_resampleWaitingForLoop = 0;
        m_resampleInvalidInstances = 0;
        m_resampleStepMin = 0.0;
        m_resampleStepMax = 0.0;
        m_resampleStepAvg = 0.0;
        m_resampleCursorFracMin = 0.0;
        m_resampleCursorFracMax = 0.0;
        m_resampleCursorFracAvg = 0.0;
        m_resampleSfxQuality = SoundResampler::Quality::Linear;
        m_resampleMusicQuality = SoundResampler::Quality::Linear;
        m_resampleLinearInstances = 0;
        m_resampleBankUsageSfx.clear();
        m_resampleBankUsageMusic.clear();
#endif
        // Clear rolling windows when audio system is not available
        m_sliceMixWindow.clear();
        m_renderTimeWindow.clear();
        m_overrunEvents.clear();
        return;
    }

    SoundLibrary2dSDL::RuntimeStats stats;
    sdl->GetRuntimeStats(stats);
    m_cachedStats = stats;

    double now = GetHighResTime();

    // If SDL audio is not started, reset rolling windows so stale data doesn't linger
    if (!sdl->IsAudioStarted())
    {
        m_sliceMixWindow.clear();
        m_renderTimeWindow.clear();
        m_overrunEvents.clear();
    }

    if (!m_hasStats)
    {
        m_previousStats = stats;
        m_lastStatsSampleTime = now;
        m_audioCallbacksPerSec = 0.0;
        m_topupCallsPerSec = 0.0;
        m_queuedCallbacksPerSec = 0.0;
        m_hasStats = true;
#if !defined(SOUND_USE_DSOUND_FREQUENCY_STUFF)
        m_prevInvalidChannelReadsTotal = g_soundSystem ? g_soundSystem->GetInvalidChannelIdReadTotal() : 0ULL;
        m_invalidChannelReadsPerSec = 0.0;
        m_invalidChannelReadsDelta = 0ULL;
#endif
#if !defined(SOUND_USE_DSOUND_FREQUENCY_STUFF)
        m_resampleSfxQuality = SoundResampler::GetSfxQuality();
        m_resampleMusicQuality = SoundResampler::GetMusicQuality();
        unsigned int bankCount = SoundResampler::GetKernelBankCount();
        m_resampleBankUsageSfx.assign(bankCount, 0);
        m_resampleBankUsageMusic.assign(bankCount, 0);
        m_resampleLinearInstances = 0;
#endif
        return;
    }

    double elapsed = now - m_lastStatsSampleTime;
    if (elapsed <= 0.0001)
    {
        return;
    }

    auto diff = [](uint64_t current, uint64_t previous) -> uint64_t {
        if (current >= previous) return current - previous;
        return current;
    };

    m_audioCallbacksPerSec = static_cast<double>(diff(stats.audioCallbacks, m_previousStats.audioCallbacks)) / elapsed;
    m_topupCallsPerSec = static_cast<double>(diff(stats.topupCalls, m_previousStats.topupCalls)) / elapsed;
    m_queuedCallbacksPerSec = static_cast<double>(diff(stats.callbacksQueued, m_previousStats.callbacksQueued)) / elapsed;
    m_directCallbacksPerSec = static_cast<double>(diff(stats.callbacksDirect, m_previousStats.callbacksDirect)) / elapsed;
    m_wavCallbacksPerSec = static_cast<double>(diff(stats.wavCallbacks, m_previousStats.wavCallbacks)) / elapsed;
    m_topupProcessedPerSec = static_cast<double>(diff(stats.topupCallbacksProcessed, m_previousStats.topupCallbacksProcessed)) / elapsed;

#if !defined(SOUND_USE_DSOUND_FREQUENCY_STUFF)
    if (g_soundSystem)
    {
        unsigned long long total = g_soundSystem->GetInvalidChannelIdReadTotal();
        unsigned long long d = diff(total, m_prevInvalidChannelReadsTotal);
        m_invalidChannelReadsDelta = d;
        m_invalidChannelReadsPerSec = static_cast<double>(d) / elapsed;
        m_prevInvalidChannelReadsTotal = total;
    }
#endif

#if !defined(SOUND_USE_DSOUND_FREQUENCY_STUFF)
    m_resampleInstanceCount = 0;
    m_resampleWaitingForLoop = 0;
    m_resampleInvalidInstances = 0;
    m_resampleStepMin = 0.0;
    m_resampleStepMax = 0.0;
    m_resampleStepAvg = 0.0;
    m_resampleCursorFracMin = 0.0;
    m_resampleCursorFracMax = 0.0;
    m_resampleCursorFracAvg = 0.0;
    m_resampleSfxQuality = SoundResampler::GetSfxQuality();
    m_resampleMusicQuality = SoundResampler::GetMusicQuality();
    unsigned int bankCount = SoundResampler::GetKernelBankCount();
    m_resampleBankUsageSfx.assign(bankCount, 0);
    m_resampleBankUsageMusic.assign(bankCount, 0);
    m_resampleLinearInstances = 0;

    if (g_soundSystem && g_soundSystem->m_numChannels > 0)
    {
        double stepMin = std::numeric_limits<double>::max();
        double stepMax = 0.0;
        double stepSum = 0.0;
        int stepCount = 0;

        double fracMin = std::numeric_limits<double>::max();
        double fracMax = 0.0;
        double fracSum = 0.0;
        int fracCount = 0;

        std::unordered_set<SoundInstance *> processed;
        processed.reserve(static_cast<size_t>(g_soundSystem->m_numChannels));

        for (int i = 0; i < g_soundSystem->m_numChannels; ++i)
        {
            SoundInstanceId instanceId = g_soundSystem->GetChannelId(i);
            if (instanceId.m_index < 0)
            {
                continue;
            }

            SoundInstance *instance = g_soundSystem->LockSoundInstance(instanceId);
            if (!instance)
            {
                continue;
            }

            bool inserted = processed.insert(instance).second;
            if (!inserted)
            {
                g_soundSystem->UnlockSoundInstance(instance);
                continue;
            }

            SoundSampleHandle *handle = instance->m_soundSampleHandle;
            if (handle && handle->m_soundSample)
            {
                ++m_resampleInstanceCount;

                double step = instance->m_resampleStep;
                if (step < stepMin) stepMin = step;
                if (step > stepMax) stepMax = step;
                stepSum += step;
                ++stepCount;

                unsigned int totalFrames = handle->GetFrameCount();
                if (totalFrames > 0)
                {
                    double cursor = instance->m_resampleCursor;
                    double fraction = cursor / static_cast<double>(totalFrames);
                    if (fraction < 0.0) fraction = 0.0;
                    if (fraction < fracMin) fracMin = fraction;
                    if (fraction > fracMax) fracMax = fraction;
                    fracSum += fraction;
                    ++fracCount;
                    if (fraction >= 1.0)
                    {
                        ++m_resampleWaitingForLoop;
                    }
                }
                else
                {
                    ++m_resampleInvalidInstances;
                }

                const int musicThreshold = g_soundSystem->m_numChannels - g_soundSystem->m_numMusicChannels;
                const bool isMusicChannel = (g_soundSystem->m_numMusicChannels > 0) && (i >= musicThreshold);
                SoundResampler::Quality quality = isMusicChannel ? m_resampleMusicQuality : m_resampleSfxQuality;
                if (quality == SoundResampler::Quality::Linear || step <= 0.0)
                {
                    ++m_resampleLinearInstances;
                }
                else
                {
                    SoundResampler::Selection selection = SoundResampler::SelectKernel(quality, step);
                    if (selection.kernel && selection.bankIndex < bankCount)
                    {
                        std::vector<int> &usage = isMusicChannel ? m_resampleBankUsageMusic : m_resampleBankUsageSfx;
                        if (selection.bankIndex < usage.size())
                        {
                            usage[selection.bankIndex]++;
                        }
                    }
                    else
                    {
                        ++m_resampleLinearInstances;
                    }
                }
            }
            else
            {
                ++m_resampleInvalidInstances;
            }

            g_soundSystem->UnlockSoundInstance(instance);
        }

        if (stepCount > 0)
        {
            m_resampleStepMin = stepMin;
            m_resampleStepMax = stepMax;
            m_resampleStepAvg = stepSum / static_cast<double>(stepCount);
        }

        if (fracCount > 0)
        {
            m_resampleCursorFracMin = fracMin;
            m_resampleCursorFracMax = fracMax;
            m_resampleCursorFracAvg = fracSum / static_cast<double>(fracCount);
        }
    }
#else
    // When hardware resampling is enabled, still track invalid channel-read diagnostics
    if (g_soundSystem)
    {
        // maintain last total on first pass
        if (!m_hasStats) m_prevInvalidChannelReadsTotal = g_soundSystem->GetInvalidChannelIdReadTotal();
    }
#endif

    // Maintain rolling windows for last 10 seconds
    const double windowSec = m_windowSeconds;

    // Track overrun deltas (push mode stat, but safe to diff regardless)
    {
        uint64_t prev = m_previousStats.sliceMixOverruns;
        uint64_t curr = stats.sliceMixOverruns;
        uint64_t delta = (curr >= prev) ? (curr - prev) : 0ULL;
        if (delta > 0ULL)
        {
            m_overrunEvents.emplace_back(now, delta);
        }
        // prune old overrun events
        while (!m_overrunEvents.empty() && (now - m_overrunEvents.front().first) > windowSec)
        {
            m_overrunEvents.pop_front();
        }
    }

    // Track sample durations for last 10s
    if (stats.usingPushMode)
    {
        // Slice mix timing sample (only if audio running)
        if (sdl->IsAudioStarted())
        {
            m_sliceMixWindow.emplace_back(now, stats.lastSliceMs);
        }
    }
    else
    {
        // Render timing sample (only if audio running)
        if (sdl->IsAudioStarted())
        {
            m_renderTimeWindow.emplace_back(now, stats.lastRenderMs);
        }
    }

    // Prune timing windows
    while (!m_sliceMixWindow.empty() && (now - m_sliceMixWindow.front().first) > windowSec)
    {
        m_sliceMixWindow.pop_front();
    }
    while (!m_renderTimeWindow.empty() && (now - m_renderTimeWindow.front().first) > windowSec)
    {
        m_renderTimeWindow.pop_front();
    }

    m_previousStats = stats;
    m_lastStatsSampleTime = now;
}

void SoundDebugOverlay::Render()
{
    if (!g_renderer)
    {
        return;
    }

    float baseXLeft = 25.0f;
    if (g_windowManager)
    {
        baseXLeft = std::max(25.0f, static_cast<float>(g_windowManager->WindowW()) - 570.0f);
    }

    const float line = 16.0f;
    const float sectionGap = 6.0f;
    char buffer[256];

    const float baseY = 55.0f;
    float leftY = baseY;
    float baseXRight = baseXLeft + 220.0f;

    g_renderer->TextSimple(baseXLeft, leftY, Colour(255, 220, 80, 255), 14.0f, "Sound Debug Overlay (F3)");
    leftY += line;

    Colour sectionColour(180, 220, 255, 255);
    Colour textColour(255, 255, 255, 255);
    Colour warnColour(255, 80, 80, 255);

    g_renderer->TextSimple(baseXLeft, leftY, sectionColour, 12.0f, "Preferences");
    leftY += line;

    if (g_preferences)
    {
        snprintf(buffer, sizeof(buffer), "Sound enabled       : %s", g_preferences->GetInt("Sound", 1) ? "yes" : "no");
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;

        // (SDL device callback state shown in SDL 2D section)

        snprintf(buffer, sizeof(buffer), "Master volume       : %d", g_preferences->GetInt("SoundMasterVolume", 255));
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;

        // Clarify callback-vs-push parameters in the Preferences view
        const int usePush = g_preferences->GetInt("SoundUsePushMode", 1);

        // In push mode, period frames are the relevant granularity; hide callback buffer size
        if (!usePush)
        {
            snprintf(buffer, sizeof(buffer), "Mix freq / buffer   : %d Hz / %d samples (callback mode)",
                     g_preferences->GetInt("SoundMixFreq", 44100),
                     g_preferences->GetInt("SoundBufferSize", 512));
            g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
            leftY += line;
        }

        snprintf(buffer, sizeof(buffer), "Channels (total/music): %d / %d",
                 g_preferences->GetInt("SoundChannels", 32),
                 g_preferences->GetInt("SoundMusicChannels", 12));
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;

        const char *libName = g_preferences->GetString("SoundLibrary", "sdl");
        snprintf(buffer, sizeof(buffer), "Sound library       : %s", libName ? libName : "unknown");
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;

        snprintf(buffer, sizeof(buffer), "Hardware 3D target  : %s", g_preferences->GetInt("SoundHW3D", 0) ? "enabled" : "disabled");
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;

        // New push/queue and scheduling prefs
        snprintf(buffer, sizeof(buffer), "Use push mode       : %s", usePush ? "yes" : "no");
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;

        // For push-only prefs, de-emphasize when push mode is off
        Colour pushPrefColour = usePush ? textColour : Colour(160,160,160,220);

        snprintf(buffer, sizeof(buffer), usePush ?
                 "Period frames       : %d" :
                 "Period frames       : %d (push-mode only)",
                 g_preferences->GetInt("SoundPeriodFrames", 128));
        g_renderer->TextSimple(baseXLeft, leftY, pushPrefColour, 11.0f, buffer);
        leftY += line;

        snprintf(buffer, sizeof(buffer), usePush ?
                 "Target latency      : %d ms" :
                 "Target latency      : %d ms (push-mode only)",
                 g_preferences->GetInt("SoundTargetLatencyMs", 180));
        g_renderer->TextSimple(baseXLeft, leftY, pushPrefColour, 11.0f, buffer);
        leftY += line;

        // Device queue bounds and ring horizon
        snprintf(buffer, sizeof(buffer), usePush ?
                 "Device low/high     : %d / %d ms" :
                 "Device low/high     : %d / %d ms (push-mode only)",
                 g_preferences->GetInt("SoundDeviceQueueLowMs", 100),
                 g_preferences->GetInt("SoundDeviceQueueHighMs", 150));
        g_renderer->TextSimple(baseXLeft, leftY, pushPrefColour, 11.0f, buffer);
        leftY += line;

        snprintf(buffer, sizeof(buffer), usePush ?
                 "Ring horizon        : %d ms" :
                 "Ring horizon        : %d ms (push-mode only)",
                 g_preferences->GetInt("SoundRingMs", 150));
        g_renderer->TextSimple(baseXLeft, leftY, pushPrefColour, 11.0f, buffer);
        leftY += line;

        snprintf(buffer, sizeof(buffer), usePush ?
                 "Timed scheduling    : %s" :
                 "Timed scheduling    : %s (push-mode only)",
                 g_preferences->GetInt("SoundTimedScheduling", 1) ? "yes" : "no");
        g_renderer->TextSimple(baseXLeft, leftY, pushPrefColour, 11.0f, buffer);
        leftY += line;

        snprintf(buffer, sizeof(buffer), usePush ?
                 "Audio-clocked ADSR  : %s" :
                 "Audio-clocked ADSR  : %s (push-mode only)",
                 g_preferences->GetInt("SoundAudioClockedADSR", 1) ? "on" : "off");
        g_renderer->TextSimple(baseXLeft, leftY, pushPrefColour, 11.0f, buffer);
        leftY += line;

    }
    else
    {
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, "Preferences unavailable");
        leftY += line;
    }

    leftY += sectionGap;

    g_renderer->TextSimple(baseXLeft, leftY, sectionColour, 12.0f, "Sound System");
    leftY += line;

    if (g_soundSystem)
    {
        snprintf(buffer, sizeof(buffer), "Instances active    : %d", g_soundSystem->NumSoundInstances());
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;

        snprintf(buffer, sizeof(buffer), "Channels used       : %d / %d (music %d)",
                 g_soundSystem->NumChannelsUsed(),
                 g_soundSystem->m_numChannels,
                 g_soundSystem->m_numMusicChannels);
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;

        snprintf(buffer, sizeof(buffer), "Sounds discarded    : %d", g_soundSystem->NumSoundsDiscarded());
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;

        double timeUntilSync = g_soundSystem->m_timeSync - GetHighResTime();
        snprintf(buffer, sizeof(buffer), "Time to next sync   : %.1f ms", timeUntilSync * 1000.0);
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;

        snprintf(buffer, sizeof(buffer), "Blueprint propagate : %s", g_soundSystem->m_propagateBlueprints ? "yes" : "no");
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;
    }
    else
    {
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, "Sound system not initialised");
        leftY += line;
    }

    leftY += sectionGap;

    g_renderer->TextSimple(baseXLeft, leftY, sectionColour, 12.0f, "3D Library");
    leftY += line;

    if (g_soundLibrary3d)
    {
        snprintf(buffer, sizeof(buffer), "Sample rate         : %d Hz", g_soundLibrary3d->GetSampleRate());
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;

        snprintf(buffer, sizeof(buffer), "Master volume       : %d", g_soundLibrary3d->GetMasterVolume());
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;

        snprintf(buffer, sizeof(buffer), "Channels (total/music): %d / %d",
                 g_soundLibrary3d->GetNumChannels(),
                 g_soundLibrary3d->GetNumMusicChannels());
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;

        snprintf(buffer, sizeof(buffer), "Max channels        : %d", g_soundLibrary3d->GetMaxChannels());
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;

        snprintf(buffer, sizeof(buffer), "CPU overhead        : %d%%", g_soundLibrary3d->GetCPUOverhead());
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;
    }
    else
    {
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, "3D library not initialised");
        leftY += line;
    }

    if (g_soundSystem)
    {
        float applied = g_soundSystem->m_updatePeriod;
        float pref = g_preferences ? g_preferences->GetFloat(PREFS_SOUND_UPDATEPERIOD, applied) : applied;
        snprintf(buffer, sizeof(buffer), "Update period       : %.2f ms (pref %.2f ms)", applied * 1000.0f, pref * 1000.0f);
        g_renderer->TextSimple(baseXLeft, leftY, textColour, 11.0f, buffer);
        leftY += line;
    }

    leftY += sectionGap;

    float rightY = baseY;

    g_renderer->TextSimple(baseXRight, rightY, sectionColour, 12.0f, "SDL 2D Renderer");
    rightY += line;

    SoundLibrary2dSDL *sdl = g_soundLibrary2d ? dynamic_cast<SoundLibrary2dSDL *>(g_soundLibrary2d) : NULL;
    if (sdl)
    {
        unsigned actualFreq = sdl->GetActualFreq();
        unsigned actualBuffer = sdl->GetActualSamplesPerBuffer();
        double expectedCallbackMs = 0.0;
        snprintf(buffer, sizeof(buffer), "Audio started       : %s  Library mix callback: %s",
                 sdl->IsAudioStarted() ? "yes" : "no",
                 sdl->HasCallback() ? "set" : "unset");
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;

        // Explicitly show whether SDL is using a device callback (off in push mode)
        snprintf(buffer, sizeof(buffer), "SDL device callback : %s", sdl->UsingPushMode() ? "off" : "on");
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;

        snprintf(buffer, sizeof(buffer), "Actual freq/buffer  : %u Hz / %u samples", actualFreq, actualBuffer);
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;

        snprintf(buffer, sizeof(buffer), "Channels / format   : %u / 0x%04x",
                 sdl->GetActualChannels(),
                 sdl->GetActualFormat());
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;

        // Push mode details
        if (m_cachedStats.usingPushMode) {
            double periodMs = (actualFreq > 0) ? (1000.0 * (double)m_cachedStats.periodFrames / (double)actualFreq) : 0.0;
            expectedCallbackMs = periodMs;
            snprintf(buffer, sizeof(buffer), "Push mode           : yes");
            g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
            rightY += line;

            snprintf(buffer, sizeof(buffer), "Device period       : %u frames (%.2f ms)",
                     m_cachedStats.periodFrames, periodMs);
            g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
            rightY += line;

            uint32_t queuedBytes = m_cachedStats.queuedBytes;
            double queuedMs = m_cachedStats.queuedMs;
            bool queuedCritical = (queuedBytes == 0) || (queuedMs <= 2.0);
            // Also flag if queued latency is far above the configured high-water mark (excess latency)
            bool queuedTooHigh = false;
            if (sdl) {
                unsigned highMs = sdl->GetDeviceQueueHighMs();
                if (highMs > 0 && queuedMs > (double)highMs * 1.25) {
                    queuedTooHigh = true;
                }
            }
            snprintf(buffer, sizeof(buffer), "Queued latency      : %u bytes (%.2f ms)",
                     queuedBytes, queuedMs);
            g_renderer->TextSimple(baseXRight, rightY, (queuedCritical || queuedTooHigh) ? warnColour : textColour, 11.0f, buffer);
            rightY += line;

            snprintf(buffer, sizeof(buffer), "Slices generated    : %llu",
                     (unsigned long long)m_cachedStats.slicesGenerated);
            g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
            rightY += line;

            // Extra ring/device queue stats
            if (sdl) {
                unsigned qFrames = sdl->GetQueuedFrames();
                double qMs = (actualFreq > 0) ? (1000.0 * (double)qFrames / (double)actualFreq) : 0.0;
                unsigned periodFrames = sdl->GetPeriodFrames();
                unsigned lowFrames = sdl->MsToFrames(sdl->GetDeviceQueueLowMs());
                unsigned criticalFrames = periodFrames / 2;
                if (criticalFrames == 0) criticalFrames = 1;
                unsigned lowFramesSafe = std::max(1u, lowFrames);
                criticalFrames = std::min(criticalFrames, lowFramesSafe);
                bool deviceQueueCritical = (qFrames <= criticalFrames);
                snprintf(buffer, sizeof(buffer), "Device queue        : %u frames (%.2f ms)", qFrames, qMs);
                g_renderer->TextSimple(baseXRight, rightY, deviceQueueCritical ? warnColour : textColour, 11.0f, buffer);
                rightY += line;

                uint64_t ringFillFrames = sdl->GetRingFillFrames();
                double ringFillMs = (actualFreq > 0) ? (1000.0 * (double)ringFillFrames / (double)actualFreq) : 0.0;
                uint64_t criticalRingFrames = std::max<uint64_t>(periodFrames, (uint64_t)std::max(lowFramesSafe, criticalFrames));
                bool ringCritical = ringFillFrames <= criticalRingFrames;
                snprintf(buffer, sizeof(buffer), "Ring fill/horizon   : %llu frames (%.2f ms) / %u ms",
                         (unsigned long long)ringFillFrames, ringFillMs, sdl->GetRingMs());
                g_renderer->TextSimple(baseXRight, rightY, ringCritical ? warnColour : textColour, 11.0f, buffer);
                rightY += line;

                snprintf(buffer, sizeof(buffer), "Device low/high     : %u / %u ms",
                         sdl->GetDeviceQueueLowMs(), sdl->GetDeviceQueueHighMs());
                g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
                rightY += line;

                // Slice mix timing (push mode) — avg/warn based on last 10s
                double lastSlice = m_cachedStats.lastSliceMs;
                double avgSlice10 = 0.0;
                double maxSlice10 = 0.0;
                if (!m_sliceMixWindow.empty()) {
                    double sum = 0.0; size_t n = 0;
                    double nowTs = GetHighResTime();
                    // prune to be safe in render as well
                    while (!m_sliceMixWindow.empty() && (nowTs - m_sliceMixWindow.front().first) > m_windowSeconds) {
                        m_sliceMixWindow.pop_front();
                    }
                    for (const auto &p : m_sliceMixWindow) { sum += p.second; ++n; if (p.second > maxSlice10) maxSlice10 = p.second; }
                    if (n > 0) avgSlice10 = sum / (double)n;
                }
                bool sliceWarn = false;
                if (expectedCallbackMs > 0.0) {
                    // Warn if any sample in the last 10s exceeded the period, or avg10 high
                    bool anyExceeded = false;
                    for (const auto &p : m_sliceMixWindow) { if (p.second > expectedCallbackMs) { anyExceeded = true; break; } }
                    if (anyExceeded) sliceWarn = true;
                    if (avgSlice10 > expectedCallbackMs * 0.8) sliceWarn = true;
                }
                snprintf(buffer, sizeof(buffer), "Slice mix time (10s) : last %.2f ms  avg %.2f ms  max %.2f ms",
                         lastSlice, avgSlice10, maxSlice10);
                g_renderer->TextSimple(baseXRight, rightY, sliceWarn ? warnColour : textColour, 11.0f, buffer);
                rightY += line;

                // Slice overruns — always visible, show 10s rolling avg and warn if any in last 10s
                {
                    // prune in render using current time
                    double nowTs = GetHighResTime();
                    while (!m_overrunEvents.empty() && (nowTs - m_overrunEvents.front().first) > m_windowSeconds) {
                        m_overrunEvents.pop_front();
                    }
                    bool recentOverrun = !m_overrunEvents.empty();
                    unsigned long long windowOverruns = 0ULL;
                    for (const auto &ev : m_overrunEvents) windowOverruns += ev.second;
                    double avgOverruns = (m_windowSeconds > 0.0) ? (double)windowOverruns / m_windowSeconds : 0.0;
                    snprintf(buffer, sizeof(buffer), "Slice overruns (10s) : avg %.2f/s  total %llu",
                             avgOverruns,
                             (unsigned long long)m_cachedStats.sliceMixOverruns);
                    g_renderer->TextSimple(baseXRight, rightY, recentOverrun ? warnColour : textColour, 11.0f, buffer);
                    rightY += line;
                }

                // Target latency and scheduling/ADSR flags
                snprintf(buffer, sizeof(buffer), "Target latency      : %u ms", sdl->GetTargetLatencyMs());
                g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
                rightY += line;

                snprintf(buffer, sizeof(buffer), "Timed scheduling    : %s", sdl->UsingTimedScheduling() ? "yes" : "no");
                g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
                rightY += line;

                snprintf(buffer, sizeof(buffer), "Audio-clocked ADSR  : %s", sdl->UsingAudioClockedADSR() ? "on" : "off");
                g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
                rightY += line;

                // Playback cursor and total queued frames since start
                uint64_t playbackFrames = sdl->GetPlaybackSampleIndex();
                double playbackMs = (actualFreq > 0) ? (1000.0 * (double)playbackFrames / (double)actualFreq) : 0.0;
                snprintf(buffer, sizeof(buffer), "Playback cursor     : %llu frames (%.2f ms)",
                         (unsigned long long)playbackFrames, playbackMs);
                g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
                rightY += line;

                uint64_t totalQueued = sdl->GetTotalQueuedFrames();
                double totalQueuedSec = (actualFreq > 0) ? ((double)totalQueued / (double)actualFreq) : 0.0;
                snprintf(buffer, sizeof(buffer), "Total queued audio  : %llu frames (%.2f s)",
                         (unsigned long long)totalQueued, totalQueuedSec);
                g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
                rightY += line;

                snprintf(buffer, sizeof(buffer), "Device underruns    : %llu",
                         (unsigned long long)m_cachedStats.deviceUnderruns);
                g_renderer->TextSimple(baseXRight, rightY,
                                       (m_cachedStats.deviceUnderruns > 0) ? warnColour : textColour,
                                       11.0f, buffer);
                rightY += line;

                snprintf(buffer, sizeof(buffer), "Device low hits     : %llu",
                         (unsigned long long)m_cachedStats.deviceLowHits);
                g_renderer->TextSimple(baseXRight, rightY,
                                       (m_cachedStats.deviceLowHits > 0) ? warnColour : textColour,
                                       11.0f, buffer);
                rightY += line;

                snprintf(buffer, sizeof(buffer), "Ring starvation     : %llu",
                         (unsigned long long)m_cachedStats.ringStarvations);
                g_renderer->TextSimple(baseXRight, rightY,
                                       (m_cachedStats.ringStarvations > 0) ? warnColour : textColour,
                                       11.0f, buffer);
                rightY += line;
            }
        } else {
            snprintf(buffer, sizeof(buffer), "Push mode           : no");
            g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
            rightY += line;
            if (actualFreq > 0 && actualBuffer > 0) {
                expectedCallbackMs = (1000.0 * (double)actualBuffer / (double)actualFreq);
            }
            // Render timing (callback mode) — avg/warn based on last 10s
            double lastRender = m_cachedStats.lastRenderMs;
            double avgRender10 = 0.0;
            double maxRender10 = 0.0;
            if (!m_renderTimeWindow.empty()) {
                double sum = 0.0; size_t n = 0;
                double nowTs = GetHighResTime();
                while (!m_renderTimeWindow.empty() && (nowTs - m_renderTimeWindow.front().first) > m_windowSeconds) {
                    m_renderTimeWindow.pop_front();
                }
                for (const auto &p : m_renderTimeWindow) { sum += p.second; ++n; if (p.second > maxRender10) maxRender10 = p.second; }
                if (n > 0) avgRender10 = sum / (double)n;
            }
            bool renderWarn = false;
            if (expectedCallbackMs > 0.0) {
                bool anyExceeded = false;
                for (const auto &p : m_renderTimeWindow) { if (p.second > expectedCallbackMs) { anyExceeded = true; break; } }
                if (anyExceeded) renderWarn = true;
                if (avgRender10 > expectedCallbackMs * 0.8) renderWarn = true;
            }
            snprintf(buffer, sizeof(buffer), "Render time (10s)    : last %.2f ms  avg %.2f ms  max %.2f ms",
                     lastRender, avgRender10, maxRender10);
            g_renderer->TextSimple(baseXRight, rightY, renderWarn ? warnColour : textColour, 11.0f, buffer);
            rightY += line;
        }

        double audioSecondsMixed = (actualFreq > 0)
                                    ? static_cast<double>(m_cachedStats.totalSamplesMixed) / static_cast<double>(actualFreq)
                                    : 0.0;

        snprintf(buffer, sizeof(buffer), "Audio callbacks     : %llu (%.1f/sec)",
                 static_cast<unsigned long long>(m_cachedStats.audioCallbacks),
                 m_audioCallbacksPerSec);
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;

        snprintf(buffer, sizeof(buffer), "Direct callbacks    : %llu (%.1f/sec)",
                 static_cast<unsigned long long>(m_cachedStats.callbacksDirect),
                 m_directCallbacksPerSec);
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;

        snprintf(buffer, sizeof(buffer), "Queued callbacks    : %llu (%.1f/sec) processed %llu (%.1f/sec)",
                 static_cast<unsigned long long>(m_cachedStats.callbacksQueued),
                 m_queuedCallbacksPerSec,
                 static_cast<unsigned long long>(m_cachedStats.topupCallbacksProcessed),
                 m_topupProcessedPerSec);
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;

        snprintf(buffer, sizeof(buffer), "WAV callbacks       : %llu (%.1f/sec)",
                 static_cast<unsigned long long>(m_cachedStats.wavCallbacks),
                 m_wavCallbacksPerSec);
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;

        double avgMs = m_cachedStats.avgCallbackInterval * 1000.0;
        double maxMs = m_cachedStats.maxCallbackInterval * 1000.0;
        double sinceLast = (m_cachedStats.lastCallbackTimestamp > 0.0)
                               ? (GetHighResTime() - m_cachedStats.lastCallbackTimestamp) * 1000.0
                               : 0.0;
        double expectedMsThreshold = expectedCallbackMs > 0.0 ? expectedCallbackMs : 0.0;
        bool intervalsCritical = false;
        if (expectedMsThreshold > 0.0)
        {
            if (maxMs > expectedMsThreshold * 1.5) intervalsCritical = true;
            if (sinceLast > expectedMsThreshold * 2.0) intervalsCritical = true;
        }
        snprintf(buffer, sizeof(buffer), "Callback intervals  : avg %.2f ms  max %.2f ms  idle %.2f ms",
                 avgMs, maxMs, sinceLast);
        g_renderer->TextSimple(baseXRight, rightY, intervalsCritical ? warnColour : textColour, 11.0f, buffer);
        rightY += line;

        snprintf(buffer, sizeof(buffer), "Topup calls         : %llu (%.1f/sec)",
                 static_cast<unsigned long long>(m_cachedStats.topupCalls),
                 m_topupCallsPerSec);
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;

        snprintf(buffer, sizeof(buffer), "Buffer thirst       : %d  pending [%u, %u]",
                 m_cachedStats.bufferIsThirsty,
                 m_cachedStats.bufferedSamples[0],
                 m_cachedStats.bufferedSamples[1]);
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;

        snprintf(buffer, sizeof(buffer), "Last callback size  : %u samples", m_cachedStats.lastCallbackSamples);
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;

        snprintf(buffer, sizeof(buffer), "Total mixed audio   : %llu samples (%.2f s)",
                 static_cast<unsigned long long>(m_cachedStats.totalSamplesMixed),
                 audioSecondsMixed);
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;
    }
    else
    {
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, "SDL library inactive");
        rightY += line;
    }

    rightY += sectionGap;

#if !defined(SOUND_USE_DSOUND_FREQUENCY_STUFF)
    // Channel map integrity diagnostics
    g_renderer->TextSimple(baseXRight, rightY, sectionColour, 12.0f, "Channel Map Health");
    rightY += line;
    if (g_soundSystem)
    {
        unsigned long long totalInvalid = (unsigned long long)g_soundSystem->GetInvalidChannelIdReadTotal();
        snprintf(buffer, sizeof(buffer), "Invalid reads total : %llu  (%.2f/sec)",
                 totalInvalid,
                 m_invalidChannelReadsPerSec);
        g_renderer->TextSimple(baseXRight, rightY,
                               (m_invalidChannelReadsDelta > 0ULL) ? warnColour : textColour,
                               11.0f, buffer);
        rightY += line;
    }

    g_renderer->TextSimple(baseXRight, rightY, sectionColour, 12.0f, "Software Resampler");
    rightY += line;

    auto describeQuality = [&](SoundResampler::Quality quality, const char *label) {
        const char *name = SoundResampler::QualityToString(quality);
        if (quality == SoundResampler::Quality::Linear)
        {
            snprintf(buffer, sizeof(buffer), "%s resampler     : %s (linear interpolation)", label, name);
        }
        else
        {
            const SoundResampler::Kernel &kernel = SoundResampler::GetKernel(quality, 0);
            snprintf(buffer, sizeof(buffer), "%s resampler     : %s (%u taps, %u phases)",
                     label,
                     name,
                     kernel.taps,
                     kernel.phases);
        }
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;
    };

    auto emitQualityLines = [&]() {
        describeQuality(m_resampleSfxQuality, "SFX");
        describeQuality(m_resampleMusicQuality, "Music");
    };

    if (m_resampleInstanceCount > 0)
    {
        snprintf(buffer, sizeof(buffer), "Instances active    : %d  waiting loop %d  missing data %d",
                 m_resampleInstanceCount,
                 m_resampleWaitingForLoop,
                 m_resampleInvalidInstances);
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;

        snprintf(buffer, sizeof(buffer), "Resample step       : min %.4f  avg %.4f  max %.4f",
                 m_resampleStepMin,
                 m_resampleStepAvg,
                 m_resampleStepMax);
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;

        snprintf(buffer, sizeof(buffer), "Cursor progress     : min %.1f%%  avg %.1f%%  max %.1f%%",
                 m_resampleCursorFracMin * 100.0,
                 m_resampleCursorFracAvg * 100.0,
                 m_resampleCursorFracMax * 100.0);
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;

        emitQualityLines();

        if (m_resampleLinearInstances > 0)
        {
            snprintf(buffer, sizeof(buffer), "Linear fallback     : %u instances", m_resampleLinearInstances);
            g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
            rightY += line;
        }

        if (m_resampleSfxQuality != SoundResampler::Quality::Linear ||
            m_resampleMusicQuality != SoundResampler::Quality::Linear)
        {
            const size_t bankCount = std::min(m_resampleBankUsageSfx.size(), m_resampleBankUsageMusic.size());
            for (size_t bank = 0; bank < bankCount; ++bank)
            {
                int sfxCount = m_resampleBankUsageSfx[bank];
                int musicCount = m_resampleBankUsageMusic[bank];

                float cutoff = 0.0f;
                if (m_resampleSfxQuality != SoundResampler::Quality::Linear)
                {
                    const SoundResampler::Kernel &kernel = SoundResampler::GetKernel(m_resampleSfxQuality, static_cast<unsigned int>(bank));
                    cutoff = kernel.cutoff;
                }
                else if (m_resampleMusicQuality != SoundResampler::Quality::Linear)
                {
                    const SoundResampler::Kernel &kernel = SoundResampler::GetKernel(m_resampleMusicQuality, static_cast<unsigned int>(bank));
                    cutoff = kernel.cutoff;
                }

                snprintf(buffer, sizeof(buffer), "Bank %u cutoff %.2f  SFX %d  Music %d",
                         static_cast<unsigned int>(bank),
                         cutoff,
                         sfxCount,
                         musicCount);
                g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
                rightY += line;
            }
        }
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "Instances active    : 0  missing data %d",
                 m_resampleInvalidInstances);
        g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, buffer);
        rightY += line;

        emitQualityLines();
    }
#else
    g_renderer->TextSimple(baseXRight, rightY, sectionColour, 12.0f, "Software Resampler");
    rightY += line;
    g_renderer->TextSimple(baseXRight, rightY, textColour, 11.0f, "Disabled (hardware handles pitch)");
    rightY += line;
#endif
}
