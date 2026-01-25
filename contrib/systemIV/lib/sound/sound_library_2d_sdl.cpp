#include "systemiv.h"

#include "lib/debug/debug_utils.h"
#include "lib/preferences.h"

#include "lib/sound/sound_library_2d_sdl.h"
#include "lib/sound/sound_library_2d_mode.h"
#include "lib/sound/sound_library_3d_software.h"

#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdio> // snprintf when caching device/driver name
#include <atomic> // For memory fences in lock-free callback access

//
// Platform-specific headers for thread priority

#ifdef TARGET_OS_LINUX
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <errno.h>
#include <time.h> // For nanosleep
#endif

#ifdef TARGET_OS_MACOSX
#include <pthread.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#include <time.h> // For nanosleep
#endif

#ifdef TARGET_MSVC
#include <windows.h>
#endif

static SDL_AudioSpec s_audioSpec;
static SDL_AudioStream *s_audioStream = NULL;
static std::atomic<int> s_audioStarted{ 0 };
static bool s_audioSubsystemInitialisedByLibrary = false;

#include "lib/sound/soundsystem.h"
#include "lib/hi_res_time.h"

#define G_SL2D static_cast<SoundLibrary2dSDL *>( g_soundLibrary2d )

static int FeederThreadEntry( void *userdata )
{
	SoundLibrary2dSDL *self = static_cast<SoundLibrary2dSDL *>( userdata );
	return self ? self->FeederLoop() : 0;
}


static void sdlAudioStreamCallback( void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount )
{
	SoundLibrary2dSDL *self = static_cast<SoundLibrary2dSDL *>( userdata );

	if ( s_audioStarted.load( std::memory_order_acquire ) == 0 || !self || additional_amount <= 0 )
	{
		return;
	}

	unsigned bytesPerFrame = self->GetBytesPerFrame();

	if ( bytesPerFrame == 0 )
	{
		return;
	}

	unsigned frames = (unsigned)( additional_amount / bytesPerFrame );

	if ( frames == 0 )
	{
		return;
	}

	static std::vector<float> s_callbackScratch;
	if ( s_callbackScratch.size() < (size_t)frames * 2 )
	{
		s_callbackScratch.resize( (size_t)frames * 2, 0.0f );
	}

	self->AudioCallback( s_callbackScratch.data(), frames );
	SDL_PutAudioStreamData( stream, s_callbackScratch.data(), (int)( frames * bytesPerFrame ) );
}


void SoundLibrary2dSDL::AudioCallback( float *stream, unsigned numFrames )
{
	double now = GetHighResTime();

	if ( stream && numFrames > 0 )
	{
		RenderFloatBlock( stream, numFrames );
	}

	m_statsLock.Lock();

	if ( m_stats.audioCallbacks > 0 )
	{
		double interval = now - m_stats.lastCallbackTimestamp;
		if ( interval >= 0.0 )
		{
			if ( m_stats.avgCallbackInterval <= 0.0 )
			{
				m_stats.avgCallbackInterval = interval;
			}
			else
			{
				const double smoothing = 0.9;
				m_stats.avgCallbackInterval = ( m_stats.avgCallbackInterval * smoothing ) + ( interval * ( 1.0 - smoothing ) );
			}
			if ( interval > m_stats.maxCallbackInterval )
			{
				m_stats.maxCallbackInterval = interval;
			}
		}
	}

	m_stats.audioCallbacks++;
	m_stats.callbacksDirect++;
	m_stats.totalSamplesMixed += numFrames;
	m_stats.lastCallbackTimestamp = now;
	m_stats.lastCallbackSamples = numFrames;
	m_stats.bufferIsThirsty = 0;
	m_stats.bufferedSamples[0] = 0;
	m_stats.bufferedSamples[1] = 0;

	m_statsLock.Unlock();
}


void SoundLibrary2dSDL::RenderFloatBlock( float *dest, unsigned int numFrames )
{
	if ( !dest || numFrames == 0 )
	{
		return;
	}
	double t0 = GetHighResTime();

	SoundLibrary3dSoftware *software3d = nullptr;
	if ( m_usingFloatDevice && g_soundLibrary3d )
	{
		//
		// Only render via the 3D software mixer once it's fully initialised

		if ( g_soundLibrary3d->GetNumChannels() > 0 )
		{
			software3d = dynamic_cast<SoundLibrary3dSoftware *>( g_soundLibrary3d );
		}
	}

	if ( software3d )
	{
		software3d->RenderToInterleavedFloat( dest, numFrames );

		//
		// Record render timing

		double dtMs = ( GetHighResTime() - t0 ) * 1000.0;
		m_statsLock.Lock();
		m_stats.lastRenderMs = dtMs;
		if ( m_stats.avgRenderMs <= 0.0 )
			m_stats.avgRenderMs = dtMs;
		else
			m_stats.avgRenderMs = m_stats.avgRenderMs * 0.9 + dtMs * 0.1;
		if ( dtMs > m_stats.maxRenderMs )
			m_stats.maxRenderMs = dtMs;
		m_statsLock.Unlock();
		return;
	}

	//
	// No lock needed - callback pointer is effectively immutable after initialization
	// Using memory fence to ensure we see the initialized value

	std::atomic_thread_fence( std::memory_order_acquire );
	void ( *callback )( StereoSample *, unsigned int ) = m_callback;
	std::atomic_thread_fence( std::memory_order_acquire );

	if ( !callback )
	{
		memset( dest, 0, sizeof( float ) * numFrames * 2 );
		double dtMs = ( GetHighResTime() - t0 ) * 1000.0;
		m_statsLock.Lock();
		m_stats.lastRenderMs = dtMs;
		if ( m_stats.avgRenderMs <= 0.0 )
			m_stats.avgRenderMs = dtMs;
		else
			m_stats.avgRenderMs = m_stats.avgRenderMs * 0.9 + dtMs * 0.1;
		if ( dtMs > m_stats.maxRenderMs )
			m_stats.maxRenderMs = dtMs;
		m_statsLock.Unlock();
		return;
	}

	if ( m_tempShort.size() < numFrames )
	{
		m_tempShort.resize( numFrames );
	}

	callback( m_tempShort.data(), numFrames );
	ConvertShortBlockToFloat( m_tempShort.data(), dest, numFrames );

	double dtMs = ( GetHighResTime() - t0 ) * 1000.0;
	m_statsLock.Lock();
	m_stats.lastRenderMs = dtMs;
	if ( m_stats.avgRenderMs <= 0.0 )
		m_stats.avgRenderMs = dtMs;
	else
		m_stats.avgRenderMs = m_stats.avgRenderMs * 0.9 + dtMs * 0.1;
	if ( dtMs > m_stats.maxRenderMs )
		m_stats.maxRenderMs = dtMs;
	m_statsLock.Unlock();
}


void SoundLibrary2dSDL::ConvertShortBlockToFloat( const StereoSample *src, float *dst, unsigned int numFrames )
{
	if ( !src || !dst || numFrames == 0 )
	{
		return;
	}

	const float scale = 1.0f / 32767.0f;
	for ( unsigned int i = 0; i < numFrames; ++i )
	{
		dst[i * 2] = std::max( -1.0f, std::min( 1.0f, src[i].m_left * scale ) );
		dst[i * 2 + 1] = std::max( -1.0f, std::min( 1.0f, src[i].m_right * scale ) );
	}
}


void SoundLibrary2dSDL::ConvertFloatBlockToShort( const float *src, StereoSample *dst, unsigned int numFrames )
{
	if ( !src || !dst || numFrames == 0 )
	{
		return;
	}

	for ( unsigned int i = 0; i < numFrames; ++i )
	{
		float l = src[i * 2] * 32767.0f;
		float r = src[i * 2 + 1] * 32767.0f;
		if ( l > 32767.0f )
			l = 32767.0f;
		if ( l < -32768.0f )
			l = -32768.0f;
		if ( r > 32767.0f )
			r = 32767.0f;
		if ( r < -32768.0f )
			r = -32768.0f;
		dst[i].m_left = Round( l );
		dst[i].m_right = Round( r );
	}
}


void SoundLibrary2dSDL::TopupBuffer()
{
	uint64_t processedSamples = 0;
	unsigned lastSamplesProcessed = 0;
	bool wavCallback = false;
	double now = GetHighResTime();

	if ( m_wavOutput )
	{
		static double nextOutputTime = -1.0;
		if ( nextOutputTime < 0.0 )
			nextOutputTime = GetHighResTime();

		if ( GetHighResTime() > nextOutputTime )
		{
			int samplesPerSecond = 44100;
			int samplesPerUpdate = (int)( (double)samplesPerSecond / 20.0 );
			if ( samplesPerUpdate > 0 )
			{
				if ( m_sliceScratch.size() < (size_t)samplesPerUpdate * 2 )
				{
					m_sliceScratch.resize( (size_t)samplesPerUpdate * 2, 0.0f );
				}
				RenderFloatBlock( m_sliceScratch.data(), samplesPerUpdate );
				if ( m_tempShort.size() < (size_t)samplesPerUpdate )
				{
					m_tempShort.resize( samplesPerUpdate );
				}
				ConvertFloatBlockToShort( m_sliceScratch.data(), m_tempShort.data(), samplesPerUpdate );
				fwrite( m_tempShort.data(), sizeof( StereoSample ), samplesPerUpdate, m_wavOutput );
				processedSamples += samplesPerUpdate;
				lastSamplesProcessed = samplesPerUpdate;
				wavCallback = true;
			}
			nextOutputTime += 1.0 / 20.0;
		}
	}

	m_statsLock.Lock();
	m_stats.topupCalls++;
	if ( wavCallback )
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
}


SoundLibrary2dSDL::SoundLibrary2dSDL()
	: m_callback( NULL ),
	  m_wavOutput( NULL ),
	  m_actualFreq( 0 ),
	  m_actualSamplesPerBuffer( 0 ),
	  m_actualChannels( 0 ),
	  m_actualFormat( 0 ),
	  m_currentOutputDevice(),
	  m_lastQueueCritical( false ),
	  m_stats(),
	  m_deviceId( 0 )
{
	AppReleaseAssert( !g_soundLibrary2d, "SoundLibrary2dSDL already exists" );

	m_freq = 44100;
	g_preferences->SetInt( PREFS_SOUND_MIXFREQ, m_freq );
	m_samplesPerBuffer = g_preferences->GetInt( "SoundBufferSize", 512 );

	AppDebugOut( "Buffer size: %u\n", m_samplesPerBuffer );

	//
	// Initialise the output device

	SDL_AudioSpec desired;
	SDL_zero( desired );
	desired.freq = m_freq;
	desired.format = SDL_AUDIO_F32;
	desired.channels = 2;

#ifdef TARGET_MSVC
	int audioDriverPref = g_preferences->GetInt( PREFS_SOUND_AUDIODRIVER, 0 );
#endif

#ifndef TARGET_EMSCRIPTEN
	m_usePushMode = g_preferences->GetInt( "SoundUsePushMode", 1 );
#ifdef TARGET_MSVC

	//
	// Force pull mode when using DirectSound to avoid
	// ungodly earrape and screeching, DSound hates ring buffers.

	if ( audioDriverPref == 1 )
	{
		m_usePushMode = 0;
	}

#endif
#else

	//
	// Emscripten just flat out refuses to work with push mode.

	m_usePushMode = 0;

#endif
	int periodPref = g_preferences->GetInt( "SoundPeriodFrames", 128 );
	m_targetLatencyMs = g_preferences->GetInt( "SoundTargetLatencyMs", 180 );
	m_ringMs = g_preferences->GetInt( "SoundRingMs", 150 );
	m_deviceQueueLowMs = g_preferences->GetInt( "SoundDeviceQueueLowMs", 100 );
	m_deviceQueueHighMs = g_preferences->GetInt( "SoundDeviceQueueHighMs", 150 );
	m_timedScheduling = g_preferences->GetInt( "SoundTimedScheduling", 1 );
	m_audioClockedADSR = g_preferences->GetInt( "SoundAudioClockedADSR", 1 );

	AppDebugOut( "Initialising SDL Audio\n" );

#ifdef TARGET_MSVC
	if ( audioDriverPref == 1 )
	{
		SDL_setenv_unsafe( "SDL_AUDIODRIVER", "directsound", 1 );
		AppDebugOut( "Using DirectSound audio driver\n" );
	}
	else
	{
		SDL_setenv_unsafe( "SDL_AUDIODRIVER", "wasapi", 1 );
		AppDebugOut( "Using WASAPI audio driver\n" );
	}
#endif

	bool audioInitialised = ( SDL_WasInit( SDL_INIT_AUDIO ) & SDL_INIT_AUDIO ) != 0;
	if ( !audioInitialised )
	{
		if ( !SDL_InitSubSystem( SDL_INIT_AUDIO ) )
		{
			const char *errString = SDL_GetError();
			AppReleaseAssert( false, "Failed to initialise SDL audio subsystem: \"%s\"", errString ? errString : "unknown error" );
		}
		s_audioSubsystemInitialisedByLibrary = true;
	}

	SDL_AudioStreamCallback streamCallback = m_usePushMode ? NULL : sdlAudioStreamCallback;
	s_audioStream = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, streamCallback, this );

	if ( !s_audioStream )
	{
		const char *errString = SDL_GetError();
		if ( s_audioSubsystemInitialisedByLibrary )
		{
			SDL_QuitSubSystem( SDL_INIT_AUDIO );
			s_audioSubsystemInitialisedByLibrary = false;
		}
		AppReleaseAssert( false, "Failed to open audio output device: \"%s\"", errString ? errString : "unknown error" );
	}

	SDL_AudioDeviceID devid = SDL_GetAudioStreamDevice( s_audioStream );

	int periodFrames = 0;

	SDL_AudioSpec devSpec;
	SDL_zero( devSpec );
	SDL_GetAudioDeviceFormat( devid, &devSpec, &periodFrames );

	if ( periodFrames <= 0 )
	{
		periodFrames = m_usePushMode && periodPref > 0 ? periodPref : (int)m_samplesPerBuffer;
	}

	s_audioSpec.format = SDL_AUDIO_F32;
	s_audioSpec.channels = 2;
	s_audioSpec.freq = m_freq;

	const char *driverName = SDL_GetCurrentAudioDriver();
	if ( driverName )
	{
		char buffer[128];
		snprintf( buffer, sizeof( buffer ), "%s (%d Hz, %d ch)", driverName, s_audioSpec.freq, s_audioSpec.channels );
		m_currentOutputDevice = buffer;
	}
	else
	{
		m_currentOutputDevice.clear();
	}

	m_deviceId = devid;
	m_actualFreq = s_audioSpec.freq;
	m_actualSamplesPerBuffer = (unsigned)periodFrames;
	m_actualChannels = s_audioSpec.channels;
	m_actualFormat = (unsigned)s_audioSpec.format;
	m_periodFrames = (unsigned)periodFrames;
	m_usingFloatDevice = true;
	m_bytesPerFrame = ( SDL_AUDIO_BITSIZE( s_audioSpec.format ) / 8 ) * s_audioSpec.channels;
	m_totalQueuedFrames = 0;
	m_lastSliceStartSample = 0;

	auto nextPow2 = []( uint32_t v ) -> uint32_t
	{
		if ( v < 2 )
			return 2;
		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v++;
		return v;
	};

	uint32_t ringFramesTarget = MsToFrames( m_ringMs );
	if ( ringFramesTarget < GetPeriodFrames() * 4 )
		ringFramesTarget = GetPeriodFrames() * 4;
	m_ringFrames = nextPow2( ringFramesTarget );
	m_ringMask = m_ringFrames - 1;
	m_ring.assign( static_cast<size_t>( m_ringFrames ) * (unsigned)s_audioSpec.channels, 0.0f );

	m_copyIndex = 0;
	m_fillIndex = 0;
	s_audioStarted.store( 1, std::memory_order_release );
	m_samplesPerBuffer = (unsigned)periodFrames;
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
	return s_audioStarted.load( std::memory_order_acquire ) != 0;
}


bool SoundLibrary2dSDL::HasCallback() const
{
	return m_callback != NULL;
}


void SoundLibrary2dSDL::Start()
{
	if ( m_started )
		return;

	//
	// Start audio device if sound is enabled

	if ( s_audioStream && g_preferences->GetInt( "Sound", 1 ) )
	{
		SDL_ResumeAudioStreamDevice( s_audioStream );
	}

	//
	// Start feeder in push mode

	if ( m_usePushMode )
	{
		m_feederMutex = SDL_CreateMutex();
		m_feederRun = 1;
		m_feederThread = SDL_CreateThread( FeederThreadEntry, "SDLFeeder", this );

		if ( !m_feederThread )
		{
#ifdef TOGGLE_SOUND_TESTBED
			AppDebugOut( "Failed to create SDL feeder thread: %s\n", SDL_GetError() );
#endif
			m_feederRun = 0;
		}

		//
		// Note: Thread priority is boosted from within FeederLoop()
	}

	m_started = true;
}


bool SoundLibrary2dSDL::IsRecording() const
{
	return m_wavOutput != NULL;
}


void SoundLibrary2dSDL::GetRuntimeStats( RuntimeStats &_outStats )
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

	SDL_AudioStream *statsStream = NULL;

	m_deviceStateLock.Lock();
	statsStream = s_audioStream;
	m_deviceStateLock.Unlock();

	if ( m_usePushMode && statsStream )
	{
		int q = SDL_GetAudioStreamQueued( statsStream );
		unsigned bpf = m_bytesPerFrame ? m_bytesPerFrame : (unsigned)( ( SDL_AUDIO_BITSIZE( s_audioSpec.format ) / 8 ) * s_audioSpec.channels );
		unsigned freq = GetActualFreq();

		_outStats.queuedBytes = (uint32_t)( q > 0 ? q : 0 );

		if ( bpf > 0 && freq > 0 )
		{
			_outStats.queuedMs = (double)( q > 0 ? q : 0 ) / (double)bpf / (double)freq * 1000.0;
		}
		else
		{
			_outStats.queuedMs = 0.0;
		}
	}
	else if ( !m_usePushMode )
	{
		_outStats.deviceUnderruns = 0;
		_outStats.deviceLowHits = 0;
		_outStats.ringStarvations = 0;
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
	if ( m_feederThread )
	{
		m_feederRun = 0;
		SDL_WaitThread( m_feederThread, NULL );
		m_feederThread = NULL;
	}
	m_lastQueueCritical = false;

	SDL_AudioStream *streamToClose = NULL;

	m_deviceStateLock.Lock();
	streamToClose = s_audioStream;
	s_audioStream = NULL;
	m_deviceId = 0;
	s_audioStarted.store( 0, std::memory_order_release );
	m_deviceStateLock.Unlock();

	if ( streamToClose )
	{
		SDL_ClearAudioStream( streamToClose );
		SDL_PauseAudioStreamDevice( streamToClose );
		SDL_DestroyAudioStream( streamToClose );
	}

	if ( m_feederMutex )
	{
		SDL_DestroyMutex( m_feederMutex );
		m_feederMutex = NULL;
	}

	if ( s_audioSubsystemInitialisedByLibrary )
	{
		SDL_QuitSubSystem( SDL_INIT_AUDIO );
		s_audioSubsystemInitialisedByLibrary = false;
	}

	m_statsLock.Lock();
	m_stats = RuntimeStats();
	m_statsLock.Unlock();

	m_currentOutputDevice.clear();
}

//
// Callback pointer is set once during initialization and never changed
// No lock needed - just memory fences to ensure visibility

void SoundLibrary2dSDL::SetCallback( void ( *_callback )( StereoSample *, unsigned int ) )
{

	//
	// Memory fence ensures the callback pointer write is visible to all threads

	std::atomic_thread_fence( std::memory_order_release );
	m_callback = _callback;
	std::atomic_thread_fence( std::memory_order_release );
}


void SoundLibrary2dSDL::StartRecordToFile( char const *_filename )
{
	m_wavOutput = fopen( _filename, "wb" );
	AppReleaseAssert( m_wavOutput != NULL, "Couldn't create wave outout file %s", _filename );
}


void SoundLibrary2dSDL::EndRecordToFile()
{
	fclose( m_wavOutput );
	m_wavOutput = NULL;
}


void SoundLibrary2dSDL::SetAudioThreadPriority()
{
#ifdef TARGET_OS_LINUX
	if ( !m_feederThread )
		return;

	//
	// Try real-time FIFO scheduling first (requires CAP_SYS_NICE or root)

	struct sched_param param;
	param.sched_priority = 80; // High priority (1-99, higher = more priority)

	pthread_t current = pthread_self();

	if ( pthread_setschedparam( current, SCHED_FIFO, &param ) != 0 )
	{

		//
		// Fallback to nice priority (always works without special privileges)

		param.sched_priority = 0;
		pthread_setschedparam( current, SCHED_OTHER, &param );

		//
		// Set nice value to high priority (-20 is highest, but -15 is safer)

		if ( setpriority( PRIO_PROCESS, 0, -15 ) != 0 )
		{
			printf( "[AUDIO THREAD] ✗ Failed to set priority (errno: %d)\n", errno );
			fflush( stdout );
		}
	}
#elif defined( TARGET_OS_MACOSX )
	if ( !m_feederThread )
		return;

	pthread_t current = pthread_self();

	//
	// Set time constraint policy for real-time audio

	thread_time_constraint_policy_data_t policy;
	policy.period = 2902;	   // ~2.9ms at 1MHz (approximate audio period)
	policy.computation = 1451; // 50% of period
	policy.constraint = 2902;
	policy.preemptible = 1;

	mach_port_t thread_port = pthread_mach_thread_np( current );
	if ( thread_policy_set( thread_port, THREAD_TIME_CONSTRAINT_POLICY,
							(thread_policy_t)&policy,
							THREAD_TIME_CONSTRAINT_POLICY_COUNT ) != KERN_SUCCESS )
	{
		printf( "[AUDIO THREAD] ✗ Failed to set time constraint policy\n" );
		fflush( stdout );
	}
#elif defined( TARGET_MSVC )
	if ( !m_feederThread )
		return;

	//
	// Get the native Windows handle for the current thread

	HANDLE hThread = GetCurrentThread();

	//
	// Set to THREAD_PRIORITY_TIME_CRITICAL for real-time audio performance
	// This is the highest priority level available for normal threads

	if ( !SetThreadPriority( hThread, THREAD_PRIORITY_TIME_CRITICAL ) )
	{
		DWORD error = GetLastError();
		printf( "[AUDIO THREAD] ✗ Failed to set thread priority (error: %lu)\n", error );
		fflush( stdout );
	}
#else
	// Other platforms - SDL's audio callback already runs at high priority
#endif
}


void SoundLibrary2dSDL::PrecisionSleep( double milliseconds )
{
	if ( milliseconds <= 0.0 )
		return;

#ifdef TARGET_MSVC

	//
	// Windows: hybrid sleep + busy-wait for sub-millisecond precision
	// This matches the approach used in app.cpp for frame limiting

	if ( milliseconds > 1.0 )
	{
		Sleep( (DWORD)( milliseconds - 0.5 ) );
		double endTime = GetHighResTime() + 0.0005; // 0.5ms
		while ( GetHighResTime() < endTime )
		{
			// Precision spin - we have THREAD_PRIORITY_TIME_CRITICAL so this is safe
		}
	}
	else
	{

		//
		// For very short sleeps, just busy-wait with high-res timer

		double endTime = GetHighResTime() + ( milliseconds / 1000.0 );
		while ( GetHighResTime() < endTime )
		{
			// Spin
		}
	}
#else

	///
	// Unix/Linux/macOS: use nanosleep for sub-millisecond precision (like app.cpp)
	// nanosleep has much better precision than SDL_Delay on Linux (no 4ms granularity issue)

	struct timespec ts;
	ts.tv_sec = (time_t)( milliseconds / 1000.0 );
	ts.tv_nsec = (long)( fmod( milliseconds, 1000.0 ) * 1000000.0 );
	nanosleep( &ts, NULL );
#endif
}


int SoundLibrary2dSDL::FeederLoop()
{
	SetAudioThreadPriority();

	//
	// Maintain device queue short; ring horizon deep

	const unsigned freq = GetActualFreq();
	const unsigned bytesPerFrame = m_bytesPerFrame ? m_bytesPerFrame : (unsigned)( ( SDL_AUDIO_BITSIZE( s_audioSpec.format ) / 8 ) * s_audioSpec.channels );
	const unsigned periodFrames = m_periodFrames ? m_periodFrames : m_actualSamplesPerBuffer;

	if ( bytesPerFrame == 0 || periodFrames == 0 || freq == 0 )
	{
		return 0;
	}

	unsigned deviceLowFrames = MsToFrames( GetDeviceQueueLowMs() );
	unsigned deviceHighFrames = MsToFrames( GetDeviceQueueHighMs() );
	unsigned ringHorizonFrames = MsToFrames( m_ringMs );
	unsigned criticalLowFrames = periodFrames / 2;

	if ( criticalLowFrames == 0 )
	{
		criticalLowFrames = 1;
	}

	unsigned deviceLowFramesSafe = std::max( 1u, deviceLowFrames );

	if ( deviceLowFramesSafe > 0 )
	{
		criticalLowFrames = std::min( criticalLowFrames, deviceLowFramesSafe );
	}

	//
	// Initial ring fill and device prefill

	EnsureMixedThrough( m_copyIndex + ringHorizonFrames );
	if ( GetQueuedFrames() < deviceHighFrames )
	{
		unsigned need = deviceHighFrames - GetQueuedFrames();
		bool ringStarvedPrefill = false;

		//
		// Ensure ring covers what we'll copy

		EnsureMixedThrough( m_copyIndex + need );
		while ( need > 0 && m_feederRun )
		{
			unsigned copied = CopyFromRingToSDL( need );
			if ( copied == 0 )
			{
				ringStarvedPrefill = true;
				PrecisionSleep( 0.3 ); // Sub-millisecond precision sleep
				break;
			}
			need -= copied;
		}

		if ( ringStarvedPrefill )
		{
			m_statsLock.Lock();
			m_stats.ringStarvations++;
			m_statsLock.Unlock();
		}
	}

	//
	// Main loop

	m_lastQueueCritical = false;
	while ( m_feederRun )
	{
		SDL_AudioStream *stream = NULL;
		m_deviceStateLock.Lock();
		stream = s_audioStream;
		m_deviceStateLock.Unlock();

		if ( !stream )
		{
			break;
		}

		int qb = SDL_GetAudioStreamQueued( stream );
		Uint32 queuedBytes = (Uint32)( qb > 0 ? qb : 0 );
		unsigned queuedFrames = (unsigned)( queuedBytes / (Uint32)std::max( 1u, bytesPerFrame ) );
		double queuedMs = (double)queuedFrames / (double)freq * 1000.0;
		bool queueCritical = ( queuedFrames <= criticalLowFrames );
		bool queueWasCritical = m_lastQueueCritical;

		//
		// Update queued stats snapshot

		m_statsLock.Lock();
		m_stats.queuedBytes = queuedBytes;
		m_stats.queuedMs = queuedMs;
		m_stats.periodFrames = periodFrames;
		m_stats.usingPushMode = 1;

		if ( queuedFrames == 0 )
		{
			m_stats.deviceUnderruns++;
		}

		if ( queueCritical && !queueWasCritical )
		{
			m_stats.deviceLowHits++;
		}

		m_statsLock.Unlock();

		m_lastQueueCritical = queueCritical;

		if ( queuedFrames < deviceLowFrames )
		{
			unsigned target = deviceHighFrames;
			unsigned need = ( queuedFrames < target ) ? ( target - queuedFrames ) : 0;
			bool ringStarved = false;

			//
			// Ensure ring has enough mixed ahead for both need and ring horizon

			uint64_t ensureEnd = m_copyIndex + std::max<uint64_t>( need, ringHorizonFrames );
			EnsureMixedThrough( ensureEnd );
			while ( need > 0 && m_feederRun )
			{
				unsigned copied = CopyFromRingToSDL( need );
				if ( copied == 0 )
				{
					ringStarved = true;
					PrecisionSleep( 0.3 ); // Sub-millisecond precision sleep
					break;
				}
				need -= copied;
			}

			if ( ringStarved )
			{
				m_statsLock.Lock();
				m_stats.ringStarvations++;
				m_statsLock.Unlock();
			}
		}
		else
		{

			//
			// Keep ring horizon topped up in background

			EnsureMixedThrough( m_copyIndex + ringHorizonFrames );
			Uint32 sleepMs = (Uint32)std::max( 1.0, ( 1000.0 * (double)periodFrames / (double)freq ) * 0.25 );
			PrecisionSleep( (double)sleepMs ); // Use precision sleep instead of SDL_Delay
		}
	}

	return 0;
}


void SoundLibrary2dSDL::MixWindowToRing( uint64_t startFrame, unsigned frames )
{
	if ( frames == 0 )
		return;
	const unsigned period = GetPeriodFrames();
	const unsigned channels = s_audioSpec.channels ? s_audioSpec.channels : 2;
	if ( m_sliceScratch.size() < (size_t)period * channels )
	{
		m_sliceScratch.resize( (size_t)period * channels, 0.0f );
	}
	unsigned remaining = frames;
	uint64_t cursor = startFrame;

	while ( remaining > 0 )
	{
		unsigned chunk = std::min( period, remaining );
		m_lastSliceStartSample = cursor;
		double t0 = GetHighResTime();
		RenderFloatBlock( m_sliceScratch.data(), chunk );
		double dtMs = ( GetHighResTime() - t0 ) * 1000.0;
		// Update per-slice timing stats and detect overruns
		m_statsLock.Lock();
		m_stats.lastSliceMs = dtMs;
		if ( m_stats.avgSliceMs <= 0.0 )
			m_stats.avgSliceMs = dtMs;
		else
			m_stats.avgSliceMs = m_stats.avgSliceMs * 0.9 + dtMs * 0.1;
		if ( dtMs > m_stats.maxSliceMs )
			m_stats.maxSliceMs = dtMs;
		m_statsLock.Unlock();
		unsigned freq = GetActualFreq();
		if ( freq > 0 )
		{
			double expectedMs = 1000.0 * (double)chunk / (double)freq;
			if ( dtMs > expectedMs )
			{
				m_statsLock.Lock();
				m_stats.sliceMixOverruns++;
				m_statsLock.Unlock();
			}
		}
		if ( m_wavOutput )
		{
			if ( m_tempShort.size() < (size_t)chunk )
			{
				m_tempShort.resize( chunk );
			}
			ConvertFloatBlockToShort( m_sliceScratch.data(), m_tempShort.data(), chunk );
			fwrite( m_tempShort.data(), sizeof( StereoSample ), chunk, m_wavOutput );
		}

		//
		// Copy into ring (handle wrap)

		uint32_t ringPos = (uint32_t)( cursor & m_ringMask );
		unsigned first = std::min<unsigned>( chunk, m_ringFrames - ringPos );
		if ( !m_ring.empty() )
		{
			float *dst = &m_ring[(size_t)ringPos * channels];
			const float *src = m_sliceScratch.data();
			memcpy( dst, src, (size_t)first * channels * sizeof( float ) );
			if ( chunk > first )
			{
				memcpy( &m_ring[0], src + (size_t)first * channels, (size_t)( chunk - first ) * channels * sizeof( float ) );
			}
		}

		m_statsLock.Lock();
		m_stats.slicesGenerated++;
		m_statsLock.Unlock();
		cursor += chunk;
		remaining -= chunk;
	}
}


void SoundLibrary2dSDL::EnsureMixedThrough( uint64_t endFrame )
{
	if ( endFrame <= m_fillIndex )
		return;
	uint64_t toMix = endFrame - m_fillIndex;
	MixWindowToRing( m_fillIndex, (unsigned)std::min<uint64_t>( toMix, UINT32_MAX ) );
	m_fillIndex = endFrame;
}


unsigned SoundLibrary2dSDL::CopyFromRingToSDL( unsigned framesToCopy )
{
	if ( framesToCopy == 0 || m_bytesPerFrame == 0 )
		return 0;

	SDL_AudioStream *stream = NULL;
	m_deviceStateLock.Lock();
	stream = s_audioStream;
	m_deviceStateLock.Unlock();

	if ( !stream )
	{
		return 0;
	}

	//
	// Limit to available mixed frames

	uint64_t available = ( m_fillIndex > m_copyIndex ) ? ( m_fillIndex - m_copyIndex ) : 0;
	if ( available == 0 )
		return 0;
	unsigned frames = (unsigned)std::min<uint64_t>( available, framesToCopy );

	//
	// Copy in up to two segments to respect ring wrap

	uint32_t ringPos = (uint32_t)( m_copyIndex & m_ringMask );
	unsigned first = std::min<unsigned>( frames, m_ringFrames - ringPos );
	unsigned bytesFirst = first * m_bytesPerFrame;
	const unsigned channels = s_audioSpec.channels ? s_audioSpec.channels : 2;
	if ( first > 0 )
	{
		if ( !SDL_PutAudioStreamData( stream, &m_ring[(size_t)ringPos * channels], (int)bytesFirst ) )
		{
			return 0;
		}
	}

	unsigned second = frames - first;
	if ( second > 0 )
	{
		unsigned bytesSecond = second * m_bytesPerFrame;
		if ( !SDL_PutAudioStreamData( stream, &m_ring[0], (int)bytesSecond ) )
		{
			frames = first; // Only copied the first segment
		}
	}

	m_copyIndex += frames;
	m_totalQueuedFrames += frames;
	return frames;
}


unsigned SoundLibrary2dSDL::GetQueuedFrames() const
{
	if ( m_bytesPerFrame == 0 )
	{
		return 0;
	}

	SDL_AudioStream *stream = NULL;
	m_deviceStateLock.Lock();
	stream = s_audioStream;
	m_deviceStateLock.Unlock();

	if ( !stream )
	{
		return 0;
	}

	int q = SDL_GetAudioStreamQueued( stream );
	Uint32 bytesQueued = (Uint32)( q > 0 ? q : 0 );
	return (unsigned)( bytesQueued / (Uint32)std::max( 1u, m_bytesPerFrame ) );
}

//
// (Per-voice resampler load reporting removed)

uint64_t SoundLibrary2dSDL::GetPlaybackSampleIndex() const
{
	uint64_t queued = (uint64_t)GetQueuedFrames();
	if ( m_totalQueuedFrames >= queued )
		return m_totalQueuedFrames - queued;
	return 0;
}


unsigned SoundLibrary2dSDL::MsToFrames( unsigned ms ) const
{
	unsigned freq = GetActualFreq();
	double frames = (double)ms * (double)freq / 1000.0;
	if ( frames < 0.0 )
		frames = 0.0;
	if ( frames > (double)UINT32_MAX )
		frames = (double)UINT32_MAX;
	return (unsigned)( frames + 0.5 );
}


uint64_t SoundLibrary2dSDL::SuggestScheduledStartFrames( unsigned safetyMs ) const
{
	uint64_t playbackIndex = GetPlaybackSampleIndex();
	uint64_t targetFrames = (uint64_t)MsToFrames( m_targetLatencyMs );
	uint64_t safetyFrames = (uint64_t)MsToFrames( safetyMs );
	if ( safetyFrames < (uint64_t)GetPeriodFrames() )
		safetyFrames = GetPeriodFrames();
	uint64_t earliestWritable = m_totalQueuedFrames + safetyFrames;
	uint64_t desired = playbackIndex + targetFrames;
	return desired > earliestWritable ? desired : earliestWritable;
}
