#include "lib/universal_include.h"

#include "lib/debug_utils.h"
#include "lib/preferences.h"

#include "lib/sound/sound_library_2d_sdl.h"
#include "lib/sound/sound_library_2d_mode.h"

#include <SDL2/SDL.h>

static SDL_AudioSpec s_audioSpec;
static int s_audioStarted = 0;

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
			int samplesPerSecond = g_preferences->GetInt("SoundMixFreq", 44100);
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

		SDL_LockAudio();
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
		SDL_UnlockAudio();
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
	m_stats()
{
	AppReleaseAssert(!g_soundLibrary2d, "SoundLibrary2dSDL already exists");

	m_freq = g_preferences->GetInt("SoundMixFreq", 44100);
	m_samplesPerBuffer = g_preferences->GetInt("SoundBufferSize", 4000);

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

#ifdef TARGET_EMSCRIPTEN
	// now this is the real fix
	// we pass NULL as second parameter to force SDL to honor our exact frequency request
	// this prevents the browser from overriding our 44100 Hz with 48000 Hz
	if (SDL_OpenAudio(&desired, NULL) < 0) {
		const char *errString = SDL_GetError();
		AppReleaseAssert(false, "Failed to open audio output device with exact frequency: \"%s\"", errString);
	}
	else {
		// since we passed NULL, SDL should give us exactly what we requested
		// if SDL_OpenAudio succeeded, we can assume we got exactly what we wanted
		// but we know this works so who cares
		// if sdl denies our request, i will have to punish it
		s_audioSpec = desired;
	}
#else
	if (SDL_OpenAudio(&desired, &s_audioSpec) < 0) {
		const char *errString = SDL_GetError();
		AppReleaseAssert(false, "Failed to open audio output device: \"%s\"", errString);
	}
	else {
		// Verify that SDL is actually using the requested number of samples
		// s_audioSpec is filled by SDL_OpenAudio with the actual values being used
		AppDebugOut("Audio samples verification: requested=%d, SDL is using=%d\n", 
			desired.samples, s_audioSpec.samples);
		
		if (desired.samples != s_audioSpec.samples) {
			AppDebugOut("WARNING: SDL changed samples per buffer from %d to %d\n", 
				desired.samples, s_audioSpec.samples);
		}
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
	if (g_preferences->GetInt("Sound", 1)) 
		SDL_PauseAudio(0);
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

SoundLibrary2dSDL::~SoundLibrary2dSDL()
{
	AppDebugOut ( "Destructing SoundLibrary2dSDL class... " );
	Stop();
	AppDebugOut ( "done.\n" );
}


void SoundLibrary2dSDL::Stop()
{
	SDL_CloseAudio();
	s_audioStarted = 0;

	m_callbackLock.Lock();
	m_bufferIsThirsty = 0;
	m_callbackLock.Unlock();

	m_statsLock.Lock();
	m_stats = RuntimeStats();
	m_stats.bufferedSamples[0] = 0;
	m_stats.bufferedSamples[1] = 0;
	m_statsLock.Unlock();
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
