#ifndef INCLUDED_SOUND_LIBRARY_2D_SDL
#define INCLUDED_SOUND_LIBRARY_2D_SDL

#include "sound_library_2d.h"
#include "lib/netlib/net_mutex.h"

#include <string>

#include <stdint.h>
#include <vector>

struct SDL_Thread;
struct SDL_Mutex;

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
    struct SDL_Mutex * m_feederMutex = NULL;
    volatile int     m_feederRun = 0;
    unsigned         m_periodFrames = 0;           // device period in frames
    unsigned         m_bytesPerFrame = 0;          // channels * bytes/sample
    unsigned         m_targetLatencyMs = 0;
    int              m_usePushMode = 0;
    bool             m_usingFloatDevice = false;

    std::vector<float> m_ring;                     // interleaved float stereo frames
    std::vector<float> m_sliceScratch;
    std::vector<StereoSample> m_tempShort;
    uint32_t        m_ringFrames = 0;              // power-of-two length
    uint32_t        m_ringMask = 0;
    uint64_t        m_copyIndex = 0;               // next frame to copy into SDL device
    uint64_t        m_fillIndex = 0;               // ring mixed up to this frame (exclusive)
    unsigned        m_ringMs = 0;                  // target ring horizon in ms
    unsigned        m_deviceQueueLowMs = 0;        // device queue low water mark
    unsigned        m_deviceQueueHighMs = 0;       // device queue high water mark
    int             m_timedScheduling = 1;         // enable timeline scheduling
    int             m_audioClockedADSR = 0;        // enable ADSR against audio clock
    bool            m_lastQueueCritical = false;
    mutable NetMutex m_deviceStateLock;

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
            usingPushMode(0),
            deviceUnderruns(0),
            deviceLowHits(0),
            ringStarvations(0),
            lastRenderMs(0.0),
            avgRenderMs(0.0),
            maxRenderMs(0.0),
            lastSliceMs(0.0),
            avgSliceMs(0.0),
            maxSliceMs(0.0),
            sliceMixOverruns(0)
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
        uint64_t    slicesGenerated;
        uint32_t    periodFrames;
        uint32_t    queuedBytes;
        double      queuedMs;
        uint8_t     usingPushMode;
        uint64_t    deviceUnderruns;
        uint64_t    deviceLowHits;
        uint64_t    ringStarvations;
        double      lastRenderMs;      // RenderFloatBlock duration of last call
        double      avgRenderMs;       // EMA of RenderFloatBlock duration
        double      maxRenderMs;
        double      lastSliceMs;       // Per-slice RenderFloatBlock duration (push mode)
        double      avgSliceMs;        // EMA of per-slice duration
        double      maxSliceMs;
        uint64_t    sliceMixOverruns;  // Count of slices exceeding expected period time
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

    void			AudioCallback(float *buf, unsigned int numFrames);
    void            RenderFloatBlock(float *dest, unsigned int numFrames);
    void            ConvertShortBlockToFloat(const StereoSample *src, float *dst, unsigned int numFrames);
    void            ConvertFloatBlockToShort(const float *src, StereoSample *dst, unsigned int numFrames);
    int             FeederLoop();
    void            MixWindowToRing(uint64_t startFrame, unsigned frames);
    void            EnsureMixedThrough(uint64_t endFrame);
    unsigned        CopyFromRingToSDL(unsigned framesToCopy);
    void			Stop();
    void            SetAudioThreadPriority();             // Boost audio thread priority
    void            PrecisionSleep(double milliseconds);  // Platform-specific high-precision sleep

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
    inline uint64_t GetRingFillFrames() const { return (m_fillIndex > m_copyIndex) ? (m_fillIndex - m_copyIndex) : 0; }
    inline unsigned GetRingMs() const { return m_ringMs; }
    inline unsigned GetDeviceQueueLowMs() const { return m_deviceQueueLowMs; }
    inline unsigned GetDeviceQueueHighMs() const { return m_deviceQueueHighMs; }

    void            Start();
    inline bool     Started() const { return m_started; }

private:
    bool            m_started = false;

};


extern SoundLibrary2d *g_soundLibrary2d;


#endif
