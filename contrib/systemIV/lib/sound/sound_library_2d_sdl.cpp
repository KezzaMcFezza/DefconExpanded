#include "lib/universal_include.h"

#include "lib/debug_utils.h"
#include "lib/preferences.h"

#include "lib/sound/sound_library_2d_sdl.h"
#include "lib/sound/sound_library_2d_mode.h"

#include <SDL2/SDL.h>

static SDL_AudioSpec s_audioSpec;
static SDL_AudioDeviceID s_audioDevice = 0;
static int s_audioStarted = 0;
static bool s_audioSubsystemInitialisedByLibrary = false;

#include "app/app.h"
#include "lib/sound/soundsystem.h"
#include "lib/hi_res_time.h"

#define G_SL2D static_cast<SoundLibrary2dSDL *>(g_soundLibrary2d)

static void sdlAudioCallback(void *userdata, Uint8 *stream, int len)
{
#ifdef TOGGLE_SOUND_TESTBED	
	AppDebugOut("sdlAudioCallback START: len=%d, started=%d, g_soundLibrary2d=%p\n", len, s_audioStarted, g_soundLibrary2d);
#endif
	
	if (!s_audioStarted || !g_soundLibrary2d) 
	{
#ifdef TOGGLE_SOUND_TESTBED	
		AppDebugOut("sdlAudioCallback: aborting - not started or no library\n");
#endif
		return;
	}
#ifdef TOGGLE_SOUND_TESTBED	
	AppDebugOut("sdlAudioCallback: calling AudioCallback with %d samples\n", len / sizeof(StereoSample));
#endif
	G_SL2D->AudioCallback( (StereoSample *) stream, len / sizeof(StereoSample) );
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
	bool usedDirectCallback = false;
	uint64_t samplesMixed = 0;
	int bufferIsThirsty;
	unsigned bufferedLengths[2];
	
	m_callbackLock.Lock();
#ifdef TOGGLE_SOUND_TESTBED	
	AppDebugOut("SoundLibrary2dSDL::AudioCallback: lock acquired\n");
#endif
	
	if (!m_callback)
	{
#ifdef TOGGLE_SOUND_TESTBED	
		AppDebugOut("SoundLibrary2dSDL::AudioCallback: no callback set, unlocking and returning\n");
#endif
		m_callbackLock.Unlock();
		return;
	}

	bufferIsThirsty = m_bufferIsThirsty;
	bufferedLengths[0] = m_buffer[0].len;
	bufferedLengths[1] = m_buffer[1].len;

#ifdef INVOKE_CALLBACK_FROM_SOUND_THREAD
#ifdef TOGGLE_SOUND_TESTBED	
	AppDebugOut("SoundLibrary2dSDL::AudioCallback: invoking callback directly (INVOKE_CALLBACK_FROM_SOUND_THREAD)\n");
#endif
	usedDirectCallback = true;
	samplesMixed = numSamples;
	m_callback(stream, numSamples);
#else
#ifdef TOGGLE_SOUND_TESTBED	
	AppDebugOut("SoundLibrary2dSDL::AudioCallback: buffering callback (not INVOKE_CALLBACK_FROM_SOUND_THREAD)\n");
#endif
	m_buffer[1] = m_buffer[0];
	m_buffer[0].stream = stream;
	m_buffer[0].len = numSamples;
	m_bufferIsThirsty++;
	
	if (m_bufferIsThirsty > 2)
		m_bufferIsThirsty = 2;

	bufferIsThirsty = m_bufferIsThirsty;
	bufferedLengths[0] = m_buffer[0].len;
	bufferedLengths[1] = m_buffer[1].len;
#ifdef TOGGLE_SOUND_TESTBED
	AppDebugOut("SoundLibrary2dSDL::AudioCallback: bufferIsThirsty=%d\n", m_bufferIsThirsty);
#endif
#endif

	m_callbackLock.Unlock();

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
	if (usedDirectCallback)
	{
		m_stats.callbacksDirect++;
	}
	else
	{
		m_stats.callbacksQueued++;
	}
	m_stats.totalSamplesMixed += samplesMixed;
	m_stats.lastCallbackTimestamp = now;
	m_stats.lastCallbackSamples = numSamples;
	m_stats.bufferIsThirsty = bufferIsThirsty;
	m_stats.bufferedSamples[0] = bufferedLengths[0];
	m_stats.bufferedSamples[1] = bufferedLengths[1];

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
	uint64_t callbacksProcessed = 0;
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
			if (m_callback) {
				G_SL2D->m_callback(buf, samplesPerUpdate);
				processedSamples += samplesPerUpdate;
				lastSamplesProcessed = samplesPerUpdate;
				wavCallback = true;
			}
			fwrite(buf, samplesPerUpdate, sizeof(StereoSample), m_wavOutput);
			nextOutputTime += 1.0 / 20.0;
		}
	}
	else {
#ifndef INVOKE_CALLBACK_FROM_SOUND_THREAD
#ifdef TOGGLE_SOUND_TESTBED		
		AppDebugOut("SoundLibrary2dSDL::TopupBuffer: processing buffered callbacks, bufferIsThirsty=%d\n", m_bufferIsThirsty);
#endif
		StereoSample *streams[2] = { m_buffer[0].stream, m_buffer[1].stream };
		unsigned lengths[2] = { (unsigned)m_buffer[0].len, (unsigned)m_buffer[1].len };
		int callbacksToProcess = m_bufferIsThirsty;
		if (callbacksToProcess > 2) callbacksToProcess = 2;

		if (s_audioDevice != 0) {
			SDL_LockAudioDevice(s_audioDevice);
		}
		for (int i = 0; i < callbacksToProcess; i++) {
#ifdef TOGGLE_SOUND_TESTBED	
			AppDebugOut("SoundLibrary2dSDL::TopupBuffer: invoking callback %d/%d with %d samples\n", i+1, callbacksToProcess, lengths[i]);
#endif
			if (m_callback) {
				m_callback( streams[i], lengths[i] );
				processedSamples += lengths[i];
				lastSamplesProcessed = lengths[i];
				callbacksProcessed++;
			}
		}
		m_bufferIsThirsty = 0;
		if (s_audioDevice != 0) {
			SDL_UnlockAudioDevice(s_audioDevice);
		}
#endif
	}

	m_statsLock.Lock();
	m_stats.topupCalls++;
	if (callbacksProcessed > 0)
	{
		m_stats.topupCallbacksProcessed += callbacksProcessed;
	}
	if (wavCallback)
	{
		m_stats.wavCallbacks++;
	}
	if (processedSamples > 0)
	{
		m_stats.totalSamplesMixed += processedSamples;
		m_stats.lastCallbackSamples = lastSamplesProcessed;
		m_stats.lastCallbackTimestamp = now;
	}
	m_stats.bufferIsThirsty = m_bufferIsThirsty;
	m_stats.bufferedSamples[0] = m_buffer[0].len;
	m_stats.bufferedSamples[1] = m_buffer[1].len;
	m_statsLock.Unlock();

#ifdef TOGGLE_SOUND_TESTBED		
	AppDebugOut("SoundLibrary2dSDL::TopupBuffer END\n");
#endif
}

SoundLibrary2dSDL::SoundLibrary2dSDL()
:	m_bufferIsThirsty(0), 
	m_callback(NULL),
	m_wavOutput(NULL),
	m_actualFreq(0),
	m_actualSamplesPerBuffer(0),
	m_actualChannels(0),
	m_actualFormat(0),
	m_currentOutputDevice(),
	m_usedFallbackDevice(false),
	m_stats()
{
	AppReleaseAssert(!g_soundLibrary2d, "SoundLibrary2dSDL already exists");

	m_freq = 44100;
	g_preferences->SetInt(PREFS_SOUND_MIXFREQ, m_freq);
	m_samplesPerBuffer = g_preferences->GetInt("SoundBufferSize", 512);

	AppDebugOut("Buffer size: %u\n", m_samplesPerBuffer);

	// Initialise the output device

	SDL_AudioSpec desired;
	
	desired.freq = m_freq;
	desired.format = AUDIO_S16SYS;
	desired.samples = m_samplesPerBuffer;
	desired.channels = 2;
	desired.userdata = 0;
	desired.callback = sdlAudioCallback;
	
	AppDebugOut("Initialising SDL Audio\n");
	
#ifdef WINDOWS_SDL
	SDL_setenv("SDL_AUDIODRIVER", "directsound", 1);
#endif

		bool audioInitialised = (SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0;
		if (!audioInitialised) {
			if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
				const char *errString = SDL_GetError();
				AppReleaseAssert(false, "Failed to initialise SDL audio subsystem: \"%s\"", errString ? errString : "unknown error");
			}
			s_audioSubsystemInitialisedByLibrary = true;
		}

		const char *requestedDevice = g_preferences->GetString(PREFS_SOUND_OUTPUT_DEVICE, "");
	const char *deviceToUse = (requestedDevice && requestedDevice[0]) ? requestedDevice : NULL;
	SDL_AudioSpec obtainedSpec;
	SDL_AudioSpec *obtainedPtr = &obtainedSpec;

#ifdef TARGET_EMSCRIPTEN
	// Request exact parameters by disallowing automatic changes.
	obtainedPtr = NULL;
#endif

	s_audioDevice = SDL_OpenAudioDevice(deviceToUse, 0, &desired, obtainedPtr, 0);

	if (s_audioDevice == 0 && deviceToUse) {
		const char *errString = SDL_GetError();
		AppDebugOut("Failed to open audio output device \"%s\": \"%s\". Falling back to system default.\n",
		            deviceToUse, errString ? errString : "unknown error");
		deviceToUse = NULL;
		s_audioDevice = SDL_OpenAudioDevice(NULL, 0, &desired, obtainedPtr, 0);
	}

	if (s_audioDevice == 0) {
		const char *errString = SDL_GetError();
		AppReleaseAssert(false, "Failed to open audio output device: \"%s\"", errString ? errString : "unknown error");
	}

	bool requestedSpecificDevice = (requestedDevice && requestedDevice[0]);
	m_usedFallbackDevice = requestedSpecificDevice && (deviceToUse == NULL);
	if (deviceToUse && deviceToUse[0]) {
		m_currentOutputDevice = deviceToUse;
	}
	else {
		m_currentOutputDevice.clear();
	}

#ifdef TARGET_EMSCRIPTEN
	s_audioSpec = desired;
#else
	s_audioSpec = obtainedSpec;

	// Verify that SDL is actually using the requested number of samples
	AppDebugOut("Audio samples verification: requested=%d, SDL is using=%d\n",
		desired.samples, s_audioSpec.samples);

	if (desired.samples != s_audioSpec.samples) {
		AppDebugOut("WARNING: SDL changed samples per buffer from %d to %d\n",
			desired.samples, s_audioSpec.samples);
	}
#endif

	m_actualFreq = s_audioSpec.freq;
	m_actualSamplesPerBuffer = s_audioSpec.samples;
	m_actualChannels = s_audioSpec.channels;
	m_actualFormat = s_audioSpec.format;

#ifdef TOGGLE_SOUND_TESTBED	
	AppDebugOut("Frequency: %d\nFormat: %d\nChannels: %d\nSamples: %d\n", 
		s_audioSpec.freq, s_audioSpec.format, s_audioSpec.channels, s_audioSpec.samples);
	AppDebugOut("Size of Stereo Sample: %u\n", sizeof(StereoSample));
#endif
	
	s_audioStarted = 1;
	
	m_samplesPerBuffer = s_audioSpec.samples;
	
	// Start the callback function
	if (g_preferences->GetInt("Sound", 1)) {
		SDL_PauseAudioDevice(s_audioDevice, 0);
	}
}

void SoundLibrary2dSDL::EnumerateOutputDevices(std::vector<std::string> &_outDevices)
{
	_outDevices.clear();

	bool temporaryInit = false;
	if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
			return;
		}
		temporaryInit = true;
	}

	int deviceCount = SDL_GetNumAudioDevices(0);
	for (int i = 0; i < deviceCount; ++i) {
		const char *deviceName = SDL_GetAudioDeviceName(i, 0);
		if (deviceName && deviceName[0]) {
			_outDevices.emplace_back(deviceName);
		}
	}

	if (temporaryInit) {
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
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
	m_callbackLock.Lock();
	int bufferIsThirsty = m_bufferIsThirsty;
	unsigned buffered0 = m_buffer[0].len;
	unsigned buffered1 = m_buffer[1].len;
	m_callbackLock.Unlock();

	m_statsLock.Lock();
	_outStats = m_stats;
	m_statsLock.Unlock();

	_outStats.bufferIsThirsty = bufferIsThirsty;
	_outStats.bufferedSamples[0] = buffered0;
	_outStats.bufferedSamples[1] = buffered1;
}

const char *SoundLibrary2dSDL::GetCurrentOutputDeviceName() const
{
	return m_currentOutputDevice.c_str();
}


bool SoundLibrary2dSDL::UsedFallbackDevice() const
{
	return m_usedFallbackDevice;
}

SoundLibrary2dSDL::~SoundLibrary2dSDL()
{
	AppDebugOut ( "Destructing SoundLibrary2dSDL class... " );
	Stop();
	AppDebugOut ( "done.\n" );
}


void SoundLibrary2dSDL::Stop()
{
	if (s_audioDevice != 0) {
		SDL_PauseAudioDevice(s_audioDevice, 1);
		SDL_CloseAudioDevice(s_audioDevice);
		s_audioDevice = 0;
	}
	if (s_audioSubsystemInitialisedByLibrary) {
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		s_audioSubsystemInitialisedByLibrary = false;
	}
	s_audioStarted = 0;

	m_callbackLock.Lock();
	m_bufferIsThirsty = 0;
	m_callbackLock.Unlock();

	m_statsLock.Lock();
	m_stats = RuntimeStats();
	m_stats.bufferedSamples[0] = 0;
	m_stats.bufferedSamples[1] = 0;
	m_statsLock.Unlock();
	m_currentOutputDevice.clear();
	m_usedFallbackDevice = false;
}

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
