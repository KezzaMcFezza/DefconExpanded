#include "lib/universal_include.h"

#include "lib/debug_utils.h"
#include "lib/preferences.h"

#include "lib/sound/sound_library_2d_sdl.h"
#include "lib/sound/sound_library_2d_mode.h"

#include <SDL2/SDL.h>
#include <vector>
#include <cstring>
#include <cstdio>   // snprintf when caching device/driver name

static SDL_AudioSpec s_audioSpec;
static SDL_AudioDeviceID s_audioDevice = 0;
static int s_audioStarted = 0;
static bool s_audioSubsystemInitialisedByLibrary = false;

#include "app/app.h"
#include "lib/sound/soundsystem.h"
#include "lib/hi_res_time.h"

#define G_SL2D static_cast<SoundLibrary2dSDL *>(g_soundLibrary2d)

static int FeederThreadEntry(void *userdata)
{
    SoundLibrary2dSDL *self = static_cast<SoundLibrary2dSDL *>(userdata);
    return self ? self->FeederLoop() : 0;
}

static void sdlAudioCallback(void *userdata, Uint8 *stream, int len)
{
#ifdef TOGGLE_SOUND_TESTBED	
	AppDebugOut("sdlAudioCallback START: len=%d, started=%d, g_soundLibrary2d=%p\n", len, s_audioStarted, g_soundLibrary2d);
#endif
	
	SoundLibrary2dSDL *self = static_cast<SoundLibrary2dSDL *>(userdata);
	if (!s_audioStarted || !self) 
	{
#ifdef TOGGLE_SOUND_TESTBED	
		AppDebugOut("sdlAudioCallback: aborting - not started or no library\n");
#endif
		return;
	}
#ifdef TOGGLE_SOUND_TESTBED	
	AppDebugOut("sdlAudioCallback: calling AudioCallback with %d samples\n", len / sizeof(StereoSample));
#endif
	self->AudioCallback( (StereoSample *) stream, len / sizeof(StereoSample) );
#ifdef TOGGLE_SOUND_TESTBED	
	AppDebugOut("sdlAudioCallback END\n");
#endif
}

void SoundLibrary2dSDL::AudioCallback(StereoSample *stream, unsigned numSamples)
{
#ifdef TOGGLE_SOUND_TESTBED	
	AppDebugOut("SoundLibrary2dSDL::AudioCallback START: stream=%p, numSamples=%u\n", stream, numSamples);
#endif

	double now = GetHighResTime();

	void (*callback)(StereoSample *, unsigned int) = NULL;
	m_callbackLock.Lock();
	callback = m_callback;
	m_callbackLock.Unlock();

	if (!callback)
	{
		memset(stream, 0, numSamples * sizeof(StereoSample));
#ifdef TOGGLE_SOUND_TESTBED
		AppDebugOut("SoundLibrary2dSDL::AudioCallback: no callback set, wrote silence\n");
#endif
		return;
	}

	callback(stream, numSamples);

	m_statsLock.Lock();

	if (m_stats.audioCallbacks > 0)
	{
		double interval = now - m_stats.lastCallbackTimestamp;
		if (interval >= 0.0)
		{
			if (m_stats.avgCallbackInterval <= 0.0)
			{
				m_stats.avgCallbackInterval = interval;
			}
			else
			{
				const double smoothing = 0.9;
				m_stats.avgCallbackInterval = (m_stats.avgCallbackInterval * smoothing) + (interval * (1.0 - smoothing));
			}
			if (interval > m_stats.maxCallbackInterval)
			{
				m_stats.maxCallbackInterval = interval;
			}
		}
	}

	m_stats.audioCallbacks++;
	m_stats.callbacksDirect++;
	m_stats.totalSamplesMixed += numSamples;
	m_stats.lastCallbackTimestamp = now;
	m_stats.lastCallbackSamples = numSamples;
	m_stats.bufferIsThirsty = 0;
	m_stats.bufferedSamples[0] = 0;
	m_stats.bufferedSamples[1] = 0;

	m_statsLock.Unlock();

#ifdef TOGGLE_SOUND_TESTBED	
	AppDebugOut("SoundLibrary2dSDL::AudioCallback END\n");
#endif
}

void SoundLibrary2dSDL::TopupBuffer()
{
#ifdef TOGGLE_SOUND_TESTBED	
	AppDebugOut("SoundLibrary2dSDL::TopupBuffer START\n");
#endif
	uint64_t processedSamples = 0;
	unsigned lastSamplesProcessed = 0;
	bool wavCallback = false;
	double now = GetHighResTime();

	if (m_wavOutput)
	{
#ifdef TOGGLE_SOUND_TESTBED	
		AppDebugOut("SoundLibrary2dSDL::TopupBuffer: handling WAV output\n");
#endif
		static double nextOutputTime = -1.0;
		if (nextOutputTime < 0.0) nextOutputTime = GetHighResTime();
		
		if (GetHighResTime() > nextOutputTime)
		{
			StereoSample buf[5000];
			int samplesPerSecond = 44100;
			int samplesPerUpdate = (int)((double)samplesPerSecond / 20.0);
			void (*callback)(StereoSample *, unsigned int) = NULL;
			m_callbackLock.Lock();
			callback = m_callback;
			m_callbackLock.Unlock();
			if (callback) {
				callback(buf, samplesPerUpdate);
				processedSamples += samplesPerUpdate;
				lastSamplesProcessed = samplesPerUpdate;
				wavCallback = true;
			}
			else {
				memset(buf, 0, samplesPerUpdate * sizeof(StereoSample));
			}
			fwrite(buf, sizeof(StereoSample), samplesPerUpdate, m_wavOutput);
			nextOutputTime += 1.0 / 20.0;
		}
	}

	m_statsLock.Lock();
	m_stats.topupCalls++;
	if (wavCallback)
	{
		m_stats.wavCallbacks++;
		m_stats.totalSamplesMixed += processedSamples;
		m_stats.lastCallbackSamples = lastSamplesProcessed;
		m_stats.lastCallbackTimestamp = now;
	}
	m_stats.bufferIsThirsty = 0;
	m_stats.bufferedSamples[0] = 0;
	m_stats.bufferedSamples[1] = 0;
	m_statsLock.Unlock();

#ifdef TOGGLE_SOUND_TESTBED	
	AppDebugOut("SoundLibrary2dSDL::TopupBuffer END\n");
#endif
}
SoundLibrary2dSDL::SoundLibrary2dSDL()
:	m_callback(NULL),
	m_wavOutput(NULL),
	m_actualFreq(0),
	m_actualSamplesPerBuffer(0),
	m_actualChannels(0),
	m_actualFormat(0),
	m_currentOutputDevice(),
	m_stats(),
	m_deviceId(0)
{
	AppReleaseAssert(!g_soundLibrary2d, "SoundLibrary2dSDL already exists");

	m_freq = 44100;
	g_preferences->SetInt(PREFS_SOUND_MIXFREQ, m_freq);
	m_samplesPerBuffer = g_preferences->GetInt("SoundBufferSize", 512);

	AppDebugOut("Buffer size: %u\n", m_samplesPerBuffer);

	// Initialise the output device

	SDL_AudioSpec desired;
	SDL_zero(desired);
	
    desired.freq = m_freq;
    // Use signed 16-bit output for both modes to keep device format
    // aligned with our internal ring buffer and avoid extra conversions
    desired.format = AUDIO_S16SYS;
	desired.samples = m_samplesPerBuffer;
	desired.channels = 2;

    m_usePushMode = g_preferences->GetInt("SoundUsePushMode", 1);
    int periodPref = g_preferences->GetInt("SoundPeriodFrames", 128);
    m_targetLatencyMs = g_preferences->GetInt("SoundTargetLatencyMs", 80);
    m_ringMs = g_preferences->GetInt("SoundRingMs", 160);
    m_deviceQueueLowMs = g_preferences->GetInt("SoundDeviceQueueLowMs", 20);
    m_deviceQueueHighMs = g_preferences->GetInt("SoundDeviceQueueHighMs", 35);
    m_timedScheduling = g_preferences->GetInt("SoundTimedScheduling", 1);
    m_audioClockedADSR = g_preferences->GetInt("SoundAudioClockedADSR", 1);

    desired.userdata = this;
    // In push/queue mode, we disable the SDL callback and feed via SDL_QueueAudio.
    // We keep AUDIO_S16SYS to match the ring buffer format.
    desired.callback = m_usePushMode ? NULL : sdlAudioCallback;
	
	AppDebugOut("Initialising SDL Audio\n");
	
#ifdef WINDOWS_SDL
	int audioDriverPref = g_preferences->GetInt(PREFS_SOUND_AUDIODRIVER, 0);
	if (audioDriverPref == 1) {
		SDL_setenv("SDL_AUDIODRIVER", "directsound", 1);
		AppDebugOut("Using DirectSound audio driver\n");
	} else {
		SDL_setenv("SDL_AUDIODRIVER", "wasapi", 1);
		AppDebugOut("Using WASAPI audio driver\n");
	}
#endif

		bool audioInitialised = (SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0;
		if (!audioInitialised) {
			if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
				const char *errString = SDL_GetError();
				AppReleaseAssert(false, "Failed to initialise SDL audio subsystem: \"%s\"", errString ? errString : "unknown error");
			}
			s_audioSubsystemInitialisedByLibrary = true;
		}

    SDL_AudioSpec obtainedSpec;
    SDL_AudioSpec *obtainedPtr = &obtainedSpec;

#ifdef TARGET_EMSCRIPTEN
	// Request exact parameters by disallowing automatic changes.
	obtainedPtr = NULL;
#endif

    //
	// Set period based on mode: in push mode, request the preferred small period

	if (m_usePushMode && periodPref > 0) {
		desired.samples = static_cast<Uint16>(periodPref);
	}

	s_audioDevice = SDL_OpenAudioDevice(NULL, 0, &desired, obtainedPtr, 0);

	if (s_audioDevice == 0) {
		const char *errString = SDL_GetError();
		AppReleaseAssert(false, "Failed to open audio output device: \"%s\"", errString ? errString : "unknown error");
	}


#ifdef TARGET_EMSCRIPTEN
	s_audioSpec = desired;
#else
	s_audioSpec = obtainedSpec;

#ifdef TOGGLE_SOUND_TESTBED	

    //
	// Verify that SDL is actually using the requested number of samples

	AppDebugOut("Audio samples verification: requested=%d, SDL is using=%d\n",
		desired.samples, s_audioSpec.samples);

	if (desired.samples != s_audioSpec.samples) {
		AppDebugOut("WARNING: SDL changed samples per buffer from %d to %d\n",
			desired.samples, s_audioSpec.samples);
	}

#endif
#endif

    // Enforce the signed 16-bit stereo contract expected by the mixer
    if (s_audioSpec.format != AUDIO_S16SYS || s_audioSpec.channels != 2)
    {
        SDL_AudioDeviceID badDevice = s_audioDevice;
        s_audioDevice = 0;
        if (badDevice != 0)
        {
            SDL_CloseAudioDevice(badDevice);
        }
        if (s_audioSubsystemInitialisedByLibrary)
        {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            s_audioSubsystemInitialisedByLibrary = false;
        }
        AppReleaseAssert(false,
            "SDL audio device returned unsupported format (format=0x%x, channels=%d). Signed 16-bit stereo is required.",
            s_audioSpec.format, s_audioSpec.channels);
    }

    const char *driverName = SDL_GetCurrentAudioDriver();
    if (driverName)
    {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "%s (%d Hz, %d ch)", driverName, s_audioSpec.freq, s_audioSpec.channels);
        m_currentOutputDevice = buffer;
    }
    else
    {
        m_currentOutputDevice.clear();
    }

    //
    // Snapshot the device id for tagging buffers

    m_deviceId = s_audioDevice;

    m_actualFreq = s_audioSpec.freq;
	m_actualSamplesPerBuffer = s_audioSpec.samples;
	m_actualChannels = s_audioSpec.channels;
	m_actualFormat = s_audioSpec.format;

    m_periodFrames = s_audioSpec.samples;
    m_bytesPerFrame = (SDL_AUDIO_BITSIZE(s_audioSpec.format)/8) * s_audioSpec.channels;
    m_totalQueuedFrames = 0;
    m_lastSliceStartSample = 0;

    //
    // Allocate ring (power of two frames)

    auto nextPow2 = [](uint32_t v)->uint32_t {
        if (v < 2) return 2;
        v--; v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16; v++;
        return v;
    };

    uint32_t ringFramesTarget = MsToFrames(m_ringMs);
    if (ringFramesTarget < GetPeriodFrames()*4) ringFramesTarget = GetPeriodFrames()*4;
    m_ringFrames = nextPow2(ringFramesTarget);
    m_ringMask = m_ringFrames - 1;
    m_ring.assign(m_ringFrames, StereoSample());
    m_copyIndex = 0;
    m_fillIndex = 0;

#ifdef TOGGLE_SOUND_TESTBED	
	AppDebugOut("Frequency: %d\nFormat: %d\nChannels: %d\nSamples: %d\n", 
		s_audioSpec.freq, s_audioSpec.format, s_audioSpec.channels, s_audioSpec.samples);
	AppDebugOut("Size of Stereo Sample: %u\n", sizeof(StereoSample));
#endif
	
	s_audioStarted = 1;
	
	m_samplesPerBuffer = s_audioSpec.samples;
	
    //
    // Start audio device

    if (g_preferences->GetInt("Sound", 1)) {
        SDL_PauseAudioDevice(s_audioDevice, 0);
    }

    //
    // Start feeder thread if in push mode

    if (m_usePushMode) {
        m_feederMutex = SDL_CreateMutex();
        m_feederRun = 1;
        m_feederThread = SDL_CreateThread(FeederThreadEntry, "SDLFeeder", this);

        if (!m_feederThread) {
#ifdef TOGGLE_SOUND_TESTBED	
            AppDebugOut("Failed to create SDL feeder thread: %s\n", SDL_GetError());
#endif
            m_feederRun = 0;
        }
    } else {

        //
        // In callback mode, ensure callback is set

        desired.callback = sdlAudioCallback;
    }
}

unsigned SoundLibrary2dSDL::GetSamplesPerBuffer()
{
	return m_samplesPerBuffer;
}

unsigned SoundLibrary2dSDL::GetFreq()
{
	return m_freq;
}

unsigned SoundLibrary2dSDL::GetActualFreq() const
{
	return m_actualFreq ? m_actualFreq : m_freq;
}

unsigned SoundLibrary2dSDL::GetActualSamplesPerBuffer() const
{
	return m_actualSamplesPerBuffer ? m_actualSamplesPerBuffer : m_samplesPerBuffer;
}

unsigned SoundLibrary2dSDL::GetActualChannels() const
{
	return m_actualChannels;
}

unsigned SoundLibrary2dSDL::GetActualFormat() const
{
	return m_actualFormat;
}

bool SoundLibrary2dSDL::IsAudioStarted() const
{
	return s_audioStarted != 0;
}

bool SoundLibrary2dSDL::HasCallback() const
{
	return m_callback != NULL;
}

bool SoundLibrary2dSDL::IsRecording() const
{
	return m_wavOutput != NULL;
}

void SoundLibrary2dSDL::GetRuntimeStats(RuntimeStats &_outStats)
{
	m_statsLock.Lock();
	_outStats = m_stats;
	m_statsLock.Unlock();

	_outStats.bufferIsThirsty = 0;
	_outStats.bufferedSamples[0] = 0;
	_outStats.bufferedSamples[1] = 0;

    //
    // Ensure some fields are updated on query

    _outStats.periodFrames = m_periodFrames;
    _outStats.usingPushMode = m_usePushMode ? 1 : 0;
    SDL_AudioDeviceID statsDevice = 0;
    m_deviceStateLock.Lock();
    statsDevice = s_audioDevice;
    m_deviceStateLock.Unlock();
    if (m_usePushMode && statsDevice != 0) {
        Uint32 q = SDL_GetQueuedAudioSize(statsDevice);
        unsigned bpf = m_bytesPerFrame ? m_bytesPerFrame : (unsigned)((SDL_AUDIO_BITSIZE(s_audioSpec.format)/8) * s_audioSpec.channels);
        unsigned freq = GetActualFreq();
        _outStats.queuedBytes = q;
        _outStats.queuedMs = (double)q / (double)bpf / (double)freq * 1000.0;
    }
}

const char *SoundLibrary2dSDL::GetCurrentOutputDeviceName() const
{
	return m_currentOutputDevice.c_str();
}

SoundLibrary2dSDL::~SoundLibrary2dSDL()
{
	Stop();
}


void SoundLibrary2dSDL::Stop()
{
    if (m_feederThread) {
        m_feederRun = 0;
        SDL_WaitThread(m_feederThread, NULL);
        m_feederThread = NULL;
    }

    SDL_AudioDeviceID deviceToClose = 0;

    m_deviceStateLock.Lock();
    deviceToClose = s_audioDevice;
    if (deviceToClose != 0) {
        SDL_LockAudioDevice(deviceToClose);
    }
    m_deviceStateLock.Unlock();

    if (deviceToClose != 0) {
        SDL_ClearQueuedAudio(deviceToClose);
        SDL_PauseAudioDevice(deviceToClose, 1);
        SDL_UnlockAudioDevice(deviceToClose);
        SDL_CloseAudioDevice(deviceToClose);
    }

    m_deviceStateLock.Lock();
    if (s_audioDevice == deviceToClose) {
        s_audioDevice = 0;
    }
    m_deviceId = 0;
    s_audioStarted = 0;
    m_deviceStateLock.Unlock();

    if (m_feederMutex) {
        SDL_DestroyMutex(m_feederMutex);
        m_feederMutex = NULL;
    }

    if (s_audioSubsystemInitialisedByLibrary) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        s_audioSubsystemInitialisedByLibrary = false;
    }

    m_statsLock.Lock();
    m_stats = RuntimeStats();
    m_statsLock.Unlock();

    m_currentOutputDevice.clear();
}

//
// Lock ensures that old callback won't still be running once this method exits

void SoundLibrary2dSDL::SetCallback(void (*_callback)(StereoSample *, unsigned int))
{
	m_callbackLock.Lock();
	m_callback = _callback;
	m_callbackLock.Unlock();
}

void SoundLibrary2dSDL::StartRecordToFile(char const *_filename)
{
	m_wavOutput = fopen(_filename, "wb");
	AppReleaseAssert(m_wavOutput != NULL, "Couldn't create wave outout file %s", _filename);
}


void SoundLibrary2dSDL::EndRecordToFile()
{
	fclose(m_wavOutput);
	m_wavOutput = NULL;
}

int SoundLibrary2dSDL::FeederLoop()
{
    //
    // Maintain device queue short; ring horizon deep

    const unsigned freq = GetActualFreq();
    const unsigned bytesPerFrame = m_bytesPerFrame ? m_bytesPerFrame : (unsigned)((SDL_AUDIO_BITSIZE(s_audioSpec.format)/8) * s_audioSpec.channels);
    const unsigned periodFrames = m_periodFrames ? m_periodFrames : s_audioSpec.samples;
    if (bytesPerFrame == 0 || periodFrames == 0 || freq == 0) return 0;

    unsigned deviceLowFrames = MsToFrames(GetDeviceQueueLowMs());
    unsigned deviceHighFrames = MsToFrames(GetDeviceQueueHighMs());
    unsigned ringHorizonFrames = MsToFrames(m_ringMs);

    //
    // Initial ring fill and device prefill

    EnsureMixedThrough(m_copyIndex + ringHorizonFrames);
    if (GetQueuedFrames() < deviceHighFrames) {
        unsigned need = deviceHighFrames - GetQueuedFrames();

        //
        // Ensure ring covers what we'll copy

        EnsureMixedThrough(m_copyIndex + need);
        while (need > 0 && m_feederRun) {
            unsigned copied = CopyFromRingToSDL(need);
            if (copied == 0) { SDL_Delay(1); break; }
            need -= copied;
        }
    }

    //
    // Main loop

    while (m_feederRun) {
        Uint32 queuedBytes = SDL_GetQueuedAudioSize(s_audioDevice);
        unsigned queuedFrames = (unsigned)(queuedBytes / (Uint32)std::max(1u, bytesPerFrame));
        double queuedMs = (double)queuedFrames / (double)freq * 1000.0;

        //
        // Update queued stats snapshot

        m_statsLock.Lock();
        m_stats.queuedBytes = queuedBytes;
        m_stats.queuedMs = queuedMs;
        m_stats.periodFrames = periodFrames;
        m_stats.usingPushMode = 1;
        m_statsLock.Unlock();

        if (queuedFrames < deviceLowFrames) {
            unsigned target = deviceHighFrames;
            unsigned need = (queuedFrames < target) ? (target - queuedFrames) : 0;

            //
            // Ensure ring has enough mixed ahead for both need and ring horizon

            uint64_t ensureEnd = m_copyIndex + std::max<uint64_t>(need, ringHorizonFrames);
            EnsureMixedThrough(ensureEnd);
            while (need > 0 && m_feederRun) {
                unsigned copied = CopyFromRingToSDL(need);
                if (copied == 0) { SDL_Delay(1); break; }
                need -= copied;
            }
        } else {

            //
            // Keep ring horizon topped up in background

            EnsureMixedThrough(m_copyIndex + ringHorizonFrames);
            Uint32 sleepMs = (Uint32)std::max(1.0, (1000.0 * (double)periodFrames / (double)freq) * 0.25);
            SDL_Delay(sleepMs);
        }
    }

    return 0;
}

void SoundLibrary2dSDL::MixWindowToRing(uint64_t startFrame, unsigned frames)
{
    if (frames == 0) return;
    const unsigned period = GetPeriodFrames();
    std::vector<StereoSample> slice;
    slice.resize(period);
    unsigned remaining = frames;
    uint64_t cursor = startFrame;

    while (remaining > 0) {
        unsigned chunk = std::min(period, remaining);
        m_lastSliceStartSample = cursor;
        void (*callback)(StereoSample *, unsigned int) = NULL;
        m_callbackLock.Lock();
        callback = m_callback;
        m_callbackLock.Unlock();

        if (callback) {
            callback(slice.data(), chunk);
        } else {
            memset(slice.data(), 0, chunk * sizeof(StereoSample));
        }
        if (m_wavOutput) {
            fwrite(slice.data(), sizeof(StereoSample), chunk, m_wavOutput);
        }

        //
        // Copy into ring (handle wrap)

        uint32_t ringPos = (uint32_t)(cursor & m_ringMask);
        unsigned first = std::min<unsigned>(chunk, m_ringFrames - ringPos);
        if (!m_ring.empty()) {
            memcpy(&m_ring[ringPos], slice.data(), first * sizeof(StereoSample));
            if (chunk > first) {
                memcpy(&m_ring[0], slice.data() + first, (chunk - first) * sizeof(StereoSample));
            }
        }

        m_statsLock.Lock();
        m_stats.slicesGenerated++;
        m_statsLock.Unlock();
        cursor += chunk;
        remaining -= chunk;
    }
}

void SoundLibrary2dSDL::EnsureMixedThrough(uint64_t endFrame)
{
    if (endFrame <= m_fillIndex) return;
    uint64_t toMix = endFrame - m_fillIndex;
    MixWindowToRing(m_fillIndex, (unsigned)std::min<uint64_t>(toMix, UINT32_MAX));
    m_fillIndex = endFrame;
}

unsigned SoundLibrary2dSDL::CopyFromRingToSDL(unsigned framesToCopy)
{
    if (framesToCopy == 0 || m_bytesPerFrame == 0) return 0;

    //
    // Limit to available mixed frames

    uint64_t available = (m_fillIndex > m_copyIndex) ? (m_fillIndex - m_copyIndex) : 0;
    if (available == 0) return 0;
    unsigned frames = (unsigned)std::min<uint64_t>(available, framesToCopy);

    //
    // Copy in up to two segments to respect ring wrap

    uint32_t ringPos = (uint32_t)(m_copyIndex & m_ringMask);
    unsigned first = std::min<unsigned>(frames, m_ringFrames - ringPos);
    unsigned bytesFirst = first * m_bytesPerFrame;
    if (first > 0) {
        // Queue first contiguous block directly (S16 device format)
        if (SDL_QueueAudio(s_audioDevice, &m_ring[ringPos], bytesFirst) != 0) {
            return 0; // Failed, try later
        }
    }

    unsigned second = frames - first;
    if (second > 0) {
        unsigned bytesSecond = second * m_bytesPerFrame;
        if (SDL_QueueAudio(s_audioDevice, &m_ring[0], bytesSecond) != 0) {
            frames = first; // Only copied the first segment
        }
    }

    m_copyIndex += frames;
    m_totalQueuedFrames += frames;
    return frames;
}

unsigned SoundLibrary2dSDL::GetQueuedFrames() const
{
    if (!s_audioDevice || m_bytesPerFrame == 0) return 0;
    Uint32 bytesQueued = SDL_GetQueuedAudioSize(s_audioDevice);
    return (unsigned)(bytesQueued / (Uint32)std::max(1u, m_bytesPerFrame));
}

uint64_t SoundLibrary2dSDL::GetPlaybackSampleIndex() const
{
    uint64_t queued = (uint64_t)GetQueuedFrames();
    if (m_totalQueuedFrames >= queued) return m_totalQueuedFrames - queued;
    return 0;
}

unsigned SoundLibrary2dSDL::MsToFrames(unsigned ms) const
{
    unsigned freq = GetActualFreq();
    double frames = (double)ms * (double)freq / 1000.0;
    if (frames < 0.0) frames = 0.0;
    if (frames > (double)UINT32_MAX) frames = (double)UINT32_MAX;
    return (unsigned)(frames + 0.5);
}

uint64_t SoundLibrary2dSDL::SuggestScheduledStartFrames(unsigned safetyMs) const
{
    uint64_t playbackIndex = GetPlaybackSampleIndex();
    uint64_t targetFrames = (uint64_t)MsToFrames(m_targetLatencyMs);
    uint64_t safetyFrames = (uint64_t)MsToFrames(safetyMs);
    if (safetyFrames < (uint64_t)GetPeriodFrames()) safetyFrames = GetPeriodFrames();
    uint64_t earliestWritable = m_totalQueuedFrames + safetyFrames;
    uint64_t desired = playbackIndex + targetFrames;
    return desired > earliestWritable ? desired : earliestWritable;
}
