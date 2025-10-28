#include "lib/universal_include.h"

#include <algorithm>
#include <limits>
#include <stdio.h>
#include <unordered_set>

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
    m_resampleCursorFracAvg(0.0)
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
        return;
    }

    SoundLibrary2dSDL::RuntimeStats stats;
    sdl->GetRuntimeStats(stats);
    m_cachedStats = stats;

    double now = GetHighResTime();

    if (!m_hasStats)
    {
        m_previousStats = stats;
        m_lastStatsSampleTime = now;
        m_audioCallbacksPerSec = 0.0;
        m_topupCallsPerSec = 0.0;
        m_queuedCallbacksPerSec = 0.0;
        m_hasStats = true;
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
    m_resampleInstanceCount = 0;
    m_resampleWaitingForLoop = 0;
    m_resampleInvalidInstances = 0;
    m_resampleStepMin = 0.0;
    m_resampleStepMax = 0.0;
    m_resampleStepAvg = 0.0;
    m_resampleCursorFracMin = 0.0;
    m_resampleCursorFracMax = 0.0;
    m_resampleCursorFracAvg = 0.0;

    if (g_soundSystem && g_soundSystem->m_channels && g_soundSystem->m_numChannels > 0)
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
            SoundInstanceId instanceId = g_soundSystem->m_channels[i];
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
#endif

    m_previousStats = stats;
    m_lastStatsSampleTime = now;
}

void SoundDebugOverlay::Render()
{
    if (!g_renderer)
    {
        return;
    }

    float baseX = 25.0f;
    if (g_windowManager)
    {
        baseX = std::max(25.0f, static_cast<float>(g_windowManager->WindowW()) - 420.0f);
    }

    float y = 55.0f;
    const float line = 16.0f;
    const float sectionGap = 6.0f;
    char buffer[256];

    g_renderer->TextSimple(baseX, y, Colour(255, 220, 80, 255), 14.0f, "Sound Debug Overlay (F3)");
    y += line;

    Colour sectionColour(180, 220, 255, 255);
    Colour textColour(255, 255, 255, 255);

    g_renderer->TextSimple(baseX, y, sectionColour, 12.0f, "Preferences");
    y += line;

    if (g_preferences)
    {
        snprintf(buffer, sizeof(buffer), "Sound enabled       : %s", g_preferences->GetInt("Sound", 1) ? "yes" : "no");
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        // (SDL device callback state shown in SDL 2D section)

        snprintf(buffer, sizeof(buffer), "Master volume       : %d", g_preferences->GetInt("SoundMasterVolume", 255));
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Mix freq / buffer   : %d Hz / %d samples",
                 g_preferences->GetInt("SoundMixFreq", 44100),
                 g_preferences->GetInt("SoundBufferSize", 512));
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Channels (total/music): %d / %d",
                 g_preferences->GetInt("SoundChannels", 32),
                 g_preferences->GetInt("SoundMusicChannels", 12));
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        const char *libName = g_preferences->GetString("SoundLibrary", "sdl");
        snprintf(buffer, sizeof(buffer), "Sound library       : %s", libName ? libName : "unknown");
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Hardware 3D target  : %s", g_preferences->GetInt("SoundHW3D", 0) ? "enabled" : "disabled");
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        // New push/queue and scheduling prefs
        snprintf(buffer, sizeof(buffer), "Use push mode       : %s", g_preferences->GetInt("SoundUsePushMode", 1) ? "yes" : "no");
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Period frames       : %d", g_preferences->GetInt("SoundPeriodFrames", 128));
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Target latency      : %d ms", g_preferences->GetInt("SoundTargetLatencyMs", 80));
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        // Device queue bounds and ring horizon
        snprintf(buffer, sizeof(buffer), "Device low/high     : %d / %d ms",
                 g_preferences->GetInt("SoundDeviceQueueLowMs", g_preferences->GetInt("SoundQueueLowWaterMs", 20)),
                 g_preferences->GetInt("SoundDeviceQueueHighMs", g_preferences->GetInt("SoundQueueHighWaterMs", 35)));
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Ring horizon        : %d ms", g_preferences->GetInt("SoundRingMs", 160));
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Timed scheduling    : %s", g_preferences->GetInt("SoundTimedScheduling", 1) ? "yes" : "no");
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Audio-clocked ADSR  : %s", g_preferences->GetInt("SoundAudioClockedADSR", 1) ? "on" : "off");
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;
    }
    else
    {
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, "Preferences unavailable");
        y += line;
    }

    y += sectionGap;

    g_renderer->TextSimple(baseX, y, sectionColour, 12.0f, "Sound System");
    y += line;

    if (g_soundSystem)
    {
        snprintf(buffer, sizeof(buffer), "Instances active    : %d", g_soundSystem->NumSoundInstances());
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Channels used       : %d / %d (music %d)",
                 g_soundSystem->NumChannelsUsed(),
                 g_soundSystem->m_numChannels,
                 g_soundSystem->m_numMusicChannels);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Sounds discarded    : %d", g_soundSystem->NumSoundsDiscarded());
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        double timeUntilSync = g_soundSystem->m_timeSync - GetHighResTime();
        snprintf(buffer, sizeof(buffer), "Time to next sync   : %.1f ms", timeUntilSync * 1000.0);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Blueprint propagate : %s", g_soundSystem->m_propagateBlueprints ? "yes" : "no");
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;
    }
    else
    {
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, "Sound system not initialised");
        y += line;
    }

    y += sectionGap;

    g_renderer->TextSimple(baseX, y, sectionColour, 12.0f, "3D Library");
    y += line;

    if (g_soundLibrary3d)
    {
        snprintf(buffer, sizeof(buffer), "Sample rate         : %d Hz", g_soundLibrary3d->GetSampleRate());
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Master volume       : %d", g_soundLibrary3d->GetMasterVolume());
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Channels (total/music): %d / %d",
                 g_soundLibrary3d->GetNumChannels(),
                 g_soundLibrary3d->GetNumMusicChannels());
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Max channels        : %d", g_soundLibrary3d->GetMaxChannels());
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "CPU overhead        : %d%%", g_soundLibrary3d->GetCPUOverhead());
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;
    }
    else
    {
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, "3D library not initialised");
        y += line;
    }

    y += sectionGap;

    g_renderer->TextSimple(baseX, y, sectionColour, 12.0f, "SDL 2D Renderer");
    y += line;

    SoundLibrary2dSDL *sdl = g_soundLibrary2d ? dynamic_cast<SoundLibrary2dSDL *>(g_soundLibrary2d) : NULL;
    if (sdl)
    {
        unsigned actualFreq = sdl->GetActualFreq();
        unsigned actualBuffer = sdl->GetActualSamplesPerBuffer();
        snprintf(buffer, sizeof(buffer), "Audio started       : %s  Library mix callback: %s",
                 sdl->IsAudioStarted() ? "yes" : "no",
                 sdl->HasCallback() ? "set" : "unset");
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        // Explicitly show whether SDL is using a device callback (off in push mode)
        snprintf(buffer, sizeof(buffer), "SDL device callback : %s", sdl->UsingPushMode() ? "off" : "on");
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Actual freq/buffer  : %u Hz / %u samples", actualFreq, actualBuffer);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Channels / format   : %u / 0x%04x",
                 sdl->GetActualChannels(),
                 sdl->GetActualFormat());
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        // Push mode details
        if (m_cachedStats.usingPushMode) {
            double periodMs = (actualFreq > 0) ? (1000.0 * (double)m_cachedStats.periodFrames / (double)actualFreq) : 0.0;
            snprintf(buffer, sizeof(buffer), "Push mode           : yes");
            g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
            y += line;

            snprintf(buffer, sizeof(buffer), "Device period       : %u frames (%.2f ms)",
                     m_cachedStats.periodFrames, periodMs);
            g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
            y += line;

            snprintf(buffer, sizeof(buffer), "Queued latency      : %u bytes (%.2f ms)",
                     m_cachedStats.queuedBytes, m_cachedStats.queuedMs);
            g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
            y += line;

            snprintf(buffer, sizeof(buffer), "Slices generated    : %llu",
                     (unsigned long long)m_cachedStats.slicesGenerated);
            g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
            y += line;

            // Extra ring/device queue stats
            SoundLibrary2dSDL *sdlInst = g_soundLibrary2d ? dynamic_cast<SoundLibrary2dSDL *>(g_soundLibrary2d) : NULL;
            if (sdlInst) {
                unsigned qFrames = sdlInst->GetQueuedFrames();
                double qMs = (actualFreq > 0) ? (1000.0 * (double)qFrames / (double)actualFreq) : 0.0;
                snprintf(buffer, sizeof(buffer), "Device queue        : %u frames (%.2f ms)", qFrames, qMs);
                g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
                y += line;

                uint64_t ringFillFrames = sdlInst->GetRingFillFrames();
                double ringFillMs = (actualFreq > 0) ? (1000.0 * (double)ringFillFrames / (double)actualFreq) : 0.0;
                snprintf(buffer, sizeof(buffer), "Ring fill/horizon   : %llu frames (%.2f ms) / %u ms",
                         (unsigned long long)ringFillFrames, ringFillMs, sdlInst->GetRingMs());
                g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
                y += line;

                snprintf(buffer, sizeof(buffer), "Device low/high     : %u / %u ms",
                         sdlInst->GetDeviceQueueLowMs(), sdlInst->GetDeviceQueueHighMs());
                g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
                y += line;

                // Target latency and scheduling/ADSR flags
                snprintf(buffer, sizeof(buffer), "Target latency      : %u ms", sdlInst->GetTargetLatencyMs());
                g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
                y += line;

                snprintf(buffer, sizeof(buffer), "Timed scheduling    : %s", sdlInst->UsingTimedScheduling() ? "yes" : "no");
                g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
                y += line;

                snprintf(buffer, sizeof(buffer), "Audio-clocked ADSR  : %s", sdlInst->UsingAudioClockedADSR() ? "on" : "off");
                g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
                y += line;

                // Playback cursor and total queued frames since start
                uint64_t playbackFrames = sdlInst->GetPlaybackSampleIndex();
                double playbackMs = (actualFreq > 0) ? (1000.0 * (double)playbackFrames / (double)actualFreq) : 0.0;
                snprintf(buffer, sizeof(buffer), "Playback cursor     : %llu frames (%.2f ms)",
                         (unsigned long long)playbackFrames, playbackMs);
                g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
                y += line;

                uint64_t totalQueued = sdlInst->GetTotalQueuedFrames();
                double totalQueuedSec = (actualFreq > 0) ? ((double)totalQueued / (double)actualFreq) : 0.0;
                snprintf(buffer, sizeof(buffer), "Total queued audio  : %llu frames (%.2f s)",
                         (unsigned long long)totalQueued, totalQueuedSec);
                g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
                y += line;
            }
        } else {
            snprintf(buffer, sizeof(buffer), "Push mode           : no");
            g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
            y += line;
        }

        double audioSecondsMixed = (actualFreq > 0)
                                    ? static_cast<double>(m_cachedStats.totalSamplesMixed) / static_cast<double>(actualFreq)
                                    : 0.0;

        snprintf(buffer, sizeof(buffer), "Audio callbacks     : %llu (%.1f/sec)",
                 static_cast<unsigned long long>(m_cachedStats.audioCallbacks),
                 m_audioCallbacksPerSec);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Direct callbacks    : %llu (%.1f/sec)",
                 static_cast<unsigned long long>(m_cachedStats.callbacksDirect),
                 m_directCallbacksPerSec);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Queued callbacks    : %llu (%.1f/sec) processed %llu (%.1f/sec)",
                 static_cast<unsigned long long>(m_cachedStats.callbacksQueued),
                 m_queuedCallbacksPerSec,
                 static_cast<unsigned long long>(m_cachedStats.topupCallbacksProcessed),
                 m_topupProcessedPerSec);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "WAV callbacks       : %llu (%.1f/sec)",
                 static_cast<unsigned long long>(m_cachedStats.wavCallbacks),
                 m_wavCallbacksPerSec);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        double avgMs = m_cachedStats.avgCallbackInterval * 1000.0;
        double maxMs = m_cachedStats.maxCallbackInterval * 1000.0;
        double sinceLast = (m_cachedStats.lastCallbackTimestamp > 0.0)
                               ? (GetHighResTime() - m_cachedStats.lastCallbackTimestamp) * 1000.0
                               : 0.0;
        snprintf(buffer, sizeof(buffer), "Callback intervals  : avg %.2f ms  max %.2f ms  idle %.2f ms",
                 avgMs, maxMs, sinceLast);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Topup calls         : %llu (%.1f/sec)",
                 static_cast<unsigned long long>(m_cachedStats.topupCalls),
                 m_topupCallsPerSec);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Buffer thirst       : %d  pending [%u, %u]",
                 m_cachedStats.bufferIsThirsty,
                 m_cachedStats.bufferedSamples[0],
                 m_cachedStats.bufferedSamples[1]);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Last callback size  : %u samples", m_cachedStats.lastCallbackSamples);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Total mixed audio   : %llu samples (%.2f s)",
                 static_cast<unsigned long long>(m_cachedStats.totalSamplesMixed),
                 audioSecondsMixed);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;
    }
    else
    {
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, "SDL library inactive");
        y += line;
    }

    y += sectionGap;

#if !defined(SOUND_USE_DSOUND_FREQUENCY_STUFF)
    g_renderer->TextSimple(baseX, y, sectionColour, 12.0f, "Software Resampler");
    y += line;

    if (m_resampleInstanceCount > 0)
    {
        snprintf(buffer, sizeof(buffer), "Instances active    : %d  waiting loop %d  missing data %d",
                 m_resampleInstanceCount,
                 m_resampleWaitingForLoop,
                 m_resampleInvalidInstances);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Resample step       : min %.4f  avg %.4f  max %.4f",
                 m_resampleStepMin,
                 m_resampleStepAvg,
                 m_resampleStepMax);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;

        snprintf(buffer, sizeof(buffer), "Cursor progress     : min %.1f%%  avg %.1f%%  max %.1f%%",
                 m_resampleCursorFracMin * 100.0,
                 m_resampleCursorFracAvg * 100.0,
                 m_resampleCursorFracMax * 100.0);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "Instances active    : 0  missing data %d",
                 m_resampleInvalidInstances);
        g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
        y += line;
    }
#else
    g_renderer->TextSimple(baseX, y, sectionColour, 12.0f, "Software Resampler");
    y += line;
    g_renderer->TextSimple(baseX, y, textColour, 11.0f, "Disabled (hardware handles pitch)");
    y += line;
#endif
}
