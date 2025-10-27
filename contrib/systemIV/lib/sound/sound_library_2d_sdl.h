#ifndef INCLUDED_SOUND_LIBRARY_2D_SDL
#define INCLUDED_SOUND_LIBRARY_2D_SDL

#include "sound_library_2d.h"
#include "lib/netlib/net_mutex.h"

#include <string>

#include <stdint.h>

// Forward declarations for SDL types used in member pointers
struct SDL_Thread;
struct SDL_mutex;

//*****************************************************************************
// Class SoundLibrary2dSDL
//*****************************************************************************

class SoundLibrary2dSDL : public SoundLibrary2d
{
private:
    class Buffer {
    public:
        Buffer()
            : stream(NULL), len(0), deviceId(0)
        {
        }
        
    StereoSample *stream;
        int len;
        uint32_t deviceId; // SDL_AudioDeviceID snapshot when the callback provided this buffer
    };
	
	int				m_bufferIsThirsty;
	Buffer			m_buffer[2];
    void			(*m_callback) (StereoSample *buf, unsigned int numSamples);
	FILE            *m_wavOutput;
    std::string     m_currentOutputDevice;

    // Push/queue mode state
    SDL_Thread *     m_feederThread = NULL;
    SDL_mutex *      m_feederMutex = NULL;
    volatile int     m_feederRun = 0;
    unsigned         m_periodFrames = 0;   // device period in frames
    unsigned         m_bytesPerFrame = 0;  // channels * bytes/sample
    unsigned         m_targetLatencyMs = 0;
    unsigned         m_lowWaterMs = 0;
    unsigned         m_highWaterMs = 0;
    int              m_usePushMode = 0;

public:
	struct RuntimeStats {
        RuntimeStats()
        :	audioCallbacks(0),
            callbacksQueued(0),
            callbacksDirect(0),
            topupCalls(0),
            topupCallbacksProcessed(0),
            wavCallbacks(0),
            totalSamplesMixed(0),
            lastCallbackTimestamp(0.0),
            avgCallbackInterval(0.0),
            maxCallbackInterval(0.0),
            lastCallbackSamples(0),
            bufferIsThirsty(0),
            slicesGenerated(0),
            periodFrames(0),
            queuedBytes(0),
            queuedMs(0.0),
            usingPushMode(0)
        {
            bufferedSamples[0] = 0;
            bufferedSamples[1] = 0;
        }

		uint64_t	audioCallbacks;
		uint64_t	callbacksQueued;
		uint64_t	callbacksDirect;
		uint64_t	topupCalls;
		uint64_t	topupCallbacksProcessed;
		uint64_t	wavCallbacks;
		uint64_t	totalSamplesMixed;
		double		lastCallbackTimestamp;
		double		avgCallbackInterval;
		double		maxCallbackInterval;
        unsigned	lastCallbackSamples;
        int			bufferIsThirsty;
        unsigned	bufferedSamples[2];
        // Push mode runtime
        uint64_t    slicesGenerated;
        uint32_t    periodFrames;
        uint32_t    queuedBytes;
        double      queuedMs;
        uint8_t     usingPushMode;
    };

public:

	NetMutex		m_callbackLock;
	NetMutex		m_statsLock;
	unsigned int	m_freq;
	unsigned int	m_samplesPerBuffer;
	unsigned int	m_actualFreq;
	unsigned int	m_actualSamplesPerBuffer;
	unsigned int	m_actualChannels;
    unsigned int	m_actualFormat;
    RuntimeStats	m_stats;
    uint32_t        m_deviceId; // current SDL audio device id for this instance

    void			AudioCallback(StereoSample *buf, unsigned int numSamples);
    int             FeederLoop();
    
    void			Stop();
	
public:
	SoundLibrary2dSDL();
	~SoundLibrary2dSDL();

	void			SetCallback(void (*_callback) (StereoSample *, unsigned int));
	void			TopupBuffer();
	
	void            StartRecordToFile(char const *_filename);
	void            EndRecordToFile();

	unsigned		GetSamplesPerBuffer();
	unsigned		GetFreq();
	unsigned		GetActualFreq() const;
	unsigned		GetActualSamplesPerBuffer() const;
	unsigned		GetActualChannels() const;
	unsigned		GetActualFormat() const;
	bool			IsAudioStarted() const;
	bool			HasCallback() const;
	bool			IsRecording() const;
    void			GetRuntimeStats(RuntimeStats &_outStats);
    const char     *GetCurrentOutputDeviceName() const;
	
};


extern SoundLibrary2d *g_soundLibrary2d;


#endif
