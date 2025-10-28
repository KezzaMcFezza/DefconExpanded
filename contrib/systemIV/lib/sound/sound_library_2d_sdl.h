#ifndef INCLUDED_SOUND_LIBRARY_2D_SDL
#define INCLUDED_SOUND_LIBRARY_2D_SDL

#include "sound_library_2d.h"
#include "lib/netlib/net_mutex.h"

#include <string>

#include <stdint.h>
#include <vector>

// Forward declarations for SDL types used in member pointers
struct SDL_Thread;
struct SDL_mutex;

//*****************************************************************************
// Class SoundLibrary2dSDL
//*****************************************************************************

class SoundLibrary2dSDL : public SoundLibrary2d
{
private:
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
    int              m_usePushMode = 0;
    // Ring buffer fields (push mode)
    std::vector<StereoSample> m_ring;
    uint32_t        m_ringFrames = 0;     // power-of-two length
    uint32_t        m_ringMask = 0;
    uint64_t        m_copyIndex = 0;      // next frame to copy into SDL device
    uint64_t        m_fillIndex = 0;      // ring mixed up to this frame (exclusive)
    unsigned        m_ringMs = 0;         // target ring horizon in ms
    unsigned        m_deviceQueueLowMs = 0;   // device queue low water mark
    unsigned        m_deviceQueueHighMs = 0;  // device queue high water mark
    int             m_timedScheduling = 1;    // enable timeline scheduling
    int             m_audioClockedADSR = 0;   // enable ADSR against audio clock
    NetMutex        m_deviceStateLock;

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
    void            MixWindowToRing(uint64_t startFrame, unsigned frames);
    void            EnsureMixedThrough(uint64_t endFrame);
    unsigned        CopyFromRingToSDL(unsigned framesToCopy);
    
    void			Stop();

    // Timeline tracking (frames)
    uint64_t        m_totalQueuedFrames = 0;     // cumulative frames ever queued to SDL
    uint64_t        m_lastSliceStartSample = 0;  // start sample index of the most recent slice we mixed
	
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

    // Push/timeline helpers
    inline bool     UsingPushMode() const { return m_usePushMode != 0; }
    inline bool     UsingTimedScheduling() const { return m_usePushMode != 0 && m_timedScheduling != 0; }
    inline bool     UsingAudioClockedADSR() const { return m_audioClockedADSR != 0; }
    inline unsigned GetPeriodFrames() const { return m_periodFrames ? m_periodFrames : GetActualSamplesPerBuffer(); }
    inline unsigned GetBytesPerFrame() const { return m_bytesPerFrame; }
    inline unsigned GetTargetLatencyMs() const { return m_targetLatencyMs; }
    unsigned        GetQueuedFrames() const;
    inline uint64_t GetTotalQueuedFrames() const { return m_totalQueuedFrames; }
    uint64_t        GetPlaybackSampleIndex() const;
    inline uint64_t GetCurrentSliceStartSample() const { return m_lastSliceStartSample; }
    unsigned        MsToFrames(unsigned ms) const;
    uint64_t        SuggestScheduledStartFrames(unsigned safetyMs = 5) const;
    // Ring/device accessors for debug/logic
    inline uint64_t GetRingFillFrames() const { return (m_fillIndex > m_copyIndex) ? (m_fillIndex - m_copyIndex) : 0; }
    inline unsigned GetRingMs() const { return m_ringMs; }
    inline unsigned GetDeviceQueueLowMs() const { return m_deviceQueueLowMs; }
    inline unsigned GetDeviceQueueHighMs() const { return m_deviceQueueHighMs; }

};


extern SoundLibrary2d *g_soundLibrary2d;


#endif
