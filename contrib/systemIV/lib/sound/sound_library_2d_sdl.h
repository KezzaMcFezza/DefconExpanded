#ifndef INCLUDED_SOUND_LIBRARY_2D_SDL
#define INCLUDED_SOUND_LIBRARY_2D_SDL

#include "sound_library_2d.h"
#include "lib/netlib/net_mutex.h"

#include <string>
#include <vector>

#include <stdint.h>

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
    bool            m_usedFallbackDevice;

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
			bufferIsThirsty(0)
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
    
    void			Stop();
	
public:
	SoundLibrary2dSDL();
	~SoundLibrary2dSDL();

	void			SetCallback(void (*_callback) (StereoSample *, unsigned int));
	void			TopupBuffer();
	
	void            StartRecordToFile(char const *_filename);
	void            EndRecordToFile();

    static void     EnumerateOutputDevices(std::vector<std::string> &_outDevices);
	
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
    bool            UsedFallbackDevice() const;
	
};


extern SoundLibrary2d *g_soundLibrary2d;


#endif
