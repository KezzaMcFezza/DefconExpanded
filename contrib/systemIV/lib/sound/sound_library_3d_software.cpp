#include "lib/universal_include.h"

#include "lib/debug_utils.h"
#include "lib/hi_res_time.h"
#include "lib/math/math_utils.h"
#include "lib/profiler.h"
#include "lib/gucci/input.h"

#include "sound_filter.h"
#include "sound_library_3d_software.h"
#include "sound_library_2d.h"
#include "lib/sound/sound_library_2d_sdl.h"
#include "soundsystem.h"

#include <algorithm>

static inline signed short FloatToPcmClamp(float value)
{
    if (value > 32767.0f) value = 32767.0f;
    if (value < -32768.0f) value = -32768.0f;
    return (signed short)Round(value);
}



//*****************************************************************************
// Class SoundLibFilterSoftware
//*****************************************************************************

class SoundLibFilterSoftware
{
public:
	int					m_chainIndex;	// Each channel has an 'effects chain'. 
										// This val is the index for this filter in that chain
										// A value of -1 means the filter is NOT active
	DspEffect			*m_userFilter;

	SoundLibFilterSoftware()
	:	m_chainIndex(-1), 
		m_userFilter(NULL) 
	{
	}
};


//*****************************************************************************
// Class SoftwareChannel
//*****************************************************************************

class SoftwareChannel
{
public:
    SoundLibFilterSoftware		m_dspFX[SoundLibrary3d::NUM_FILTERS];

	signed short		*m_buffer;
    float               *m_bufferFloat;
	unsigned int		m_samplesInBuffer;
	bool				m_containsSilence;		// Updated in callback

	unsigned int		m_freq;					// Value recorded on previous call of SetChannelFrequency
	float				m_volume;				// Value recorded on previous call of SetChannelVolume
	float				m_minDist;				// Value recorded on previous call of SetChannelMinDist
	Vector3<float>		m_pos;					// Value recorded on previous call of SetChannelPosition
	int					m_3DMode;				// Value recorded on previous call of SetChannel3DMode

	float				m_oldVolLeft;			// Volumes used last time this channel was mixed into 
	float				m_oldVolRight;			// the output buffer
	bool				m_forceVolumeJump;		// True when a channel has just had a new sound effect assigned to it - this prevents the volume ramping code from being used when a new sound first starts

public:
    SoftwareChannel();
	~SoftwareChannel();
    void Initialise( bool _stereo );
};


SoftwareChannel::SoftwareChannel()
:	m_buffer(NULL),
    m_bufferFloat(NULL),
	m_freq(1),
	m_volume(0.0f),
	m_oldVolLeft(0.0f),
	m_oldVolRight(0.0f),
	m_forceVolumeJump(true),
	m_containsSilence(true),
	m_minDist(0.1f),
	m_pos(0,0,0),
	m_3DMode(0)
{
}

SoftwareChannel::~SoftwareChannel()
{
    delete [] m_buffer;
    m_buffer = NULL;
    delete [] m_bufferFloat;
    m_bufferFloat = NULL;
}

void SoftwareChannel::Initialise( bool _stereo )
{
	// The 3 on the next line is there because we limit the frequency of a 
	// channel to 3 times that of the mix frequency
	m_samplesInBuffer = g_soundLibrary2d->GetSamplesPerBuffer() * 3;
	if( _stereo ) m_samplesInBuffer *= 5;
	m_buffer = new signed short[m_samplesInBuffer];
	memset(m_buffer, 0, sizeof(signed short) * m_samplesInBuffer);
    m_bufferFloat = new float[m_samplesInBuffer];
    memset(m_bufferFloat, 0, sizeof(float) * m_samplesInBuffer);

	for (int i = 0; i < SoundLibrary3dSoftware::NUM_FILTERS; ++i)
	{
		m_dspFX[i].m_userFilter = NULL;
	}
}



//*****************************************************************************
// Class SoundLibrary3dSoftware
//*****************************************************************************

void SoundLib3dSoftwareCallbackWrapper(StereoSample *_buf, unsigned int _numSamples)
{
	AppDebugAssert(g_soundLibrary3d);
	SoundLibrary3dSoftware *lib = (SoundLibrary3dSoftware*)g_soundLibrary3d;
	if (lib)
		lib->Callback(_buf, _numSamples);
}


SoundLibrary3dSoftware::SoundLibrary3dSoftware()
:   SoundLibrary3d(),
	m_channels(NULL),
	m_allocatedSamples(0),
    m_interleaved(NULL),
	m_listenerFront(1,0,0),
	m_listenerUp(0,1,0),
	m_lastVolumeSet(GetMasterVolume())
{
	AppReleaseAssert(g_soundLibrary2d, "SoundLibrary2d must be initialised before SoundLibrary3d");
	
	EnableCallback(true);

	m_allocatedSamples = g_soundLibrary2d->GetSamplesPerBuffer();

    m_left = new float[m_allocatedSamples];
    m_right = new float[m_allocatedSamples];
    m_interleaved = new float[m_allocatedSamples * 2];
}


SoundLibrary3dSoftware::~SoundLibrary3dSoftware()
{
	EnableCallback(false);

	delete [] m_channels;		m_channels = NULL;
	m_numChannels = 0;
	delete [] m_left;			m_left = NULL;
	delete [] m_right;			m_right = NULL;
    delete [] m_interleaved;    m_interleaved = NULL;
}


void SoundLibrary3dSoftware::Initialise(int _mixFreq, int _numChannels, int _numMusicChannels, bool /* hw3d */)
{
	m_sampleRate = _mixFreq;

	int log2NumChannels = ceilf(Log2(_numChannels));
	m_numChannels = 1 << log2NumChannels;
	AppReleaseAssert(m_numChannels == _numChannels, "Num channels must be a power of 2");
	
	m_channels = new SoftwareChannel[m_numChannels];

    m_numMusicChannels = _numMusicChannels;

    for( int i = 0; i < m_numChannels; ++i )
    {
        bool stereo = i >= m_numChannels - m_numMusicChannels;
        m_channels[i].Initialise( stereo );
    }
}


void SoundLibrary3dSoftware::SetMasterVolume( int _volume )
{
	m_lastVolumeSet = _volume;
	SoundLibrary3d::SetMasterVolume(_volume);
}

void SoundLibrary3dSoftware::Advance()
{
	//
	// Mute everything when in background (the Windows DirectSound version seems to do this
	// automatically)
	if (!g_inputManager->m_windowHasFocus)
	{
		if (GetMasterVolume() != 0)
		{
			m_lastVolumeSet = GetMasterVolume();
			SoundLibrary3d::SetMasterVolume(0);
		}
	}
	else if (m_lastVolumeSet != GetMasterVolume())
		SoundLibrary3d::SetMasterVolume(m_lastVolumeSet);
		
	g_soundLibrary2d->TopupBuffer();
	
}


void SoundLibrary3dSoftware::GetChannelData(float _duration, unsigned int _numSamples)
{
	(void)_duration;
	// START_PROFILE2(m_profiler, "GetChannelData");

		int numActiveChannels = 0;
		
        for (int i = 0; i < m_numChannels; ++i)
        {
            int silenceRemaining = -1;
            unsigned int samplesNeeded = _numSamples;
            if( i >= m_numChannels - m_numMusicChannels ) samplesNeeded *= 2;

            bool hasData = false;
#if !defined(SOUND_USE_DSOUND_FREQUENCY_STUFF)
            if (g_soundSystem)
            {
                // Guard against a race where per-channel buffers are not yet
                // allocated during a sound library restart. In that case we
                // simply report silence for this channel.
                float *dst = m_channels[i].m_bufferFloat;
                if (dst)
                {
                    hasData = g_soundSystem->GenerateChannelSamplesFloat(i, dst, samplesNeeded, &silenceRemaining);
                }
                else
                {
                    hasData = false;
                }
            }
            else
#endif
            if (m_mainCallback)
            {
                hasData = m_mainCallback(i, m_channels[i].m_buffer, samplesNeeded, &silenceRemaining);
                if (hasData)
                {
                    unsigned int copySamples = samplesNeeded;
                    signed short *src = m_channels[i].m_buffer;
                    float *dst = m_channels[i].m_bufferFloat;
                    for (unsigned int s = 0; s < copySamples; ++s)
                    {
                        dst[s] = (float)src[s];
                    }
                }
            }
            m_channels[i].m_containsSilence = !hasData;
				
			if (!m_channels[i].m_containsSilence)
			{
				numActiveChannels++;
			}
		}
	// END_PROFILE2(m_profiler, "GetChannelData");
}


void SoundLibrary3dSoftware::ApplyDspFX(float _duration, unsigned int _numSamples)
{
    // Legacy DSP notice (currently disabled):
    // - Effects can be defined in effects.txt and referenced via EFFECT blocks in sounds.txt,
    //   but the current sounds.txt does not use them, so the old DSP chain is effectively idle.
    // - The legacy implementation processes 16-bit PCM in-place (signed short), while the rest of
    //   the mixer now runs fully in 32-bit float. The previous bridge converted float->int16,
    //   ran DSP, then converted back to float, adding quantization noise and CPU overhead.
    // - To avoid artifacts and simplify the realtime path, the DSP step is disabled for now.
    //
    // To migrate DSP to the 32-bit float pipeline, the following work is needed:
    // 1) Change the DSP interface to operate on float buffers:
    //    - Update DspEffect::Process to take float* (and length), using [-1.0, 1.0] range.
    //    - Decide mono/stereo handling (separate left/right, or interleaved with stride).
    // 2) Port each effect (ResonantLowPass, BitCrusher, Gargle, Echo, Reverb, Smoother):
    //    - Remove int16 clamps/conversions; keep internal state/buffers in float.
    //    - Make parameter math sample-rate aware (use current m_sampleRate).
    // 3) Integrate at the correct point in the float chain:
    //    - Run per-channel on m_channels[i].m_bufferFloat before mixing into m_left/m_right.
    //    - Handle music channels (stereo interleaved) vs SFX (mono) consistently.
    // 4) Re-enable here by iterating active channels and calling the new float Process.
    // 5) RT safety/perf: no allocations in audio thread, maintain effect instances per channel,
    //    and avoid locks in the hot path.
    //
    // Until that rewrite lands, effects remain disabled.
    (void)_duration;
    (void)_numSamples;
    return;

    // Original DSP code (16-bit) left below for reference
#if 0
	//START_PROFILE2(m_profiler, "ApplyDspFX");
	{
		for (int i = 0; i < m_numChannels; ++i)
		{
            bool hasFilters = false;
            for (int j = 0; j < NUM_FILTERS; ++j)
            {
                if (m_channels[i].m_dspFX[j].m_userFilter)
                {
                    hasFilters = true;
                    break;
                }
            }

            if (!hasFilters)
            {
                continue;
            }

            unsigned int samplesNeeded = _numSamples;
			if( i >= m_numChannels - m_numMusicChannels ) samplesNeeded *= 2;

            float *floatBuf = m_channels[i].m_bufferFloat;
            signed short *shortBuf = m_channels[i].m_buffer;

            for (unsigned int s = 0; s < samplesNeeded; ++s)
            {
                shortBuf[s] = FloatToPcmClamp(floatBuf[s]);
            }

			for( int j = 0; j < NUM_FILTERS; ++j )
		    {
				if (m_channels[i].m_dspFX[j].m_userFilter == NULL) continue;
				m_channels[i].m_dspFX[j].m_userFilter->Process(shortBuf, samplesNeeded);
			}

            for (unsigned int s = 0; s < samplesNeeded; ++s)
            {
                floatBuf[s] = (float)shortBuf[s];
            }
		}
	}
	// END_PROFILE2(m_profiler, "ApplyDspFX");
#endif
}

void SoundLibrary3dSoftware::CalcChannelVolumes(int _channelIndex, 
												float *_left, float *_right)
{
	SoftwareChannel *channel = &m_channels[_channelIndex];

    // Map channel volume (0..10) + master (centi-dB) to linear gain using 10^(dB/20)
    float channelDb = -(5.0f - channel->m_volume * 0.5f) * 10.0f; // -50dB .. 0dB
    float masterDb = m_masterVolume * 0.01f;                      // centi-dB -> dB (<= 0)
    float totalDb = channelDb + masterDb;
    // Clamp within reasonable range
    if (totalDb < -100.0f) totalDb = -100.0f;
    if (totalDb > 0.0f) totalDb = 0.0f;
    float calculatedVolume = powf(10.0f, totalDb / 20.0f);

	if ( _channelIndex < m_numChannels - m_numMusicChannels )
	{
		Vector3<float> to = channel->m_pos - m_listenerPos;
		float dist = to.Mag();
		float dotRight = 0.0f;
		if (dist > 1e-5f)
		{
			to /= dist;
			dotRight = to * m_listenerRight;
		}
		Clamp(dotRight, -1.0f, 1.0f);
		*_right = (dotRight + 1.0f) * 0.5f;
		*_left = 1.0f - *_right;

		if (dist > channel->m_minDist) 
		{
			float dropOff = channel->m_minDist / dist;
			*_left *= dropOff;
			*_right *= dropOff;
		}
	}
	
	if ( _channelIndex >= m_numChannels - m_numMusicChannels )
	{
		*_left = 0.9f;
		*_right = 0.9f;
	}

	*_left *= calculatedVolume; 
	*_right *= calculatedVolume; 
	Clamp(*_left, 0.0f, calculatedVolume);
	Clamp(*_right, 0.0f, calculatedVolume);
}

void SoundLibrary3dSoftware::MixStereo(float *_inBuf, unsigned int _numSamples,
                                       float _volLeft, float _volRight)
{
    float *left = m_left;
    float *right = m_right;

    for (unsigned int j = 0; j < _numSamples; ++j)
    {
        unsigned int sampleIndex = j * 2;
        float leftSample = _inBuf[sampleIndex];
        float rightSample = _inBuf[sampleIndex + 1];

        *left += leftSample * _volLeft;
        *right += rightSample * _volRight;
        left++;
        right++;
    }
}


void SoundLibrary3dSoftware::MixSameFreqFixedVol(float *_inBuf, unsigned int _numSamples,
											 	 float _volLeft, float _volRight)
{
	float *finalLeft = &m_left[_numSamples - 1];
	float *left = m_left;
	float *right = m_right;
	while (left <= finalLeft)
	{
		*left += *_inBuf * _volLeft;
		*right += *_inBuf * _volRight;
		left++;	right++; _inBuf++;
	}
}

void SoundLibrary3dSoftware::MixSameFreqRampVol(float *_inBuf, unsigned int _numSamples, 
												float _volL1, float _volR1, float _volL2, float _volR2)
{
	float volLeft = _volL1;
	float volRight = _volR1;
	float volLeftInc = (_volL2 - _volL1) / (float)_numSamples;
	float volRightInc = (_volR2 - _volR1) / (float)_numSamples;
	float *finalLeft = &m_left[_numSamples - 1];
	float *left = m_left;
	float *right = m_right;
	while (left <= finalLeft)
	{
		*left += *_inBuf * volLeft;
		*right += *_inBuf * volRight;
		left++;	right++; _inBuf++;
		volLeft += volLeftInc;
		volRight += volRightInc;
	}
}


void SoundLibrary3dSoftware::EnableCallback(bool _enabled)
{
	g_soundLibrary2d->SetCallback( _enabled ? SoundLib3dSoftwareCallbackWrapper : NULL );
}

void SoundLibrary3dSoftware::RenderToInterleavedFloat(float *_outInterleaved, unsigned int _numSamples)
{
    if (!_outInterleaved || _numSamples == 0)
    {
        return;
    }

    if (!m_channels)
    {
        memset(_outInterleaved, 0, sizeof(float) * _numSamples * 2);
        return;
    }

    // No Emscripten frequency snapshot needed in float push/pull pipeline

    unsigned actualRate = g_soundLibrary2d ? g_soundLibrary2d->GetFreq() : 44100;
    if (g_soundLibrary2d) {
        SoundLibrary2dSDL *sdl2d = dynamic_cast<SoundLibrary2dSDL *>(g_soundLibrary2d);
        if (sdl2d) {
            unsigned r = sdl2d->GetActualFreq();
            if (r > 0) actualRate = r;
        }
    }
    double duration = (double)_numSamples / (double)actualRate;

	GetChannelData(duration, _numSamples);
	ApplyDspFX(duration, _numSamples);

    // No Emscripten frequency restore needed

	if (_numSamples > m_allocatedSamples)
	{
		_numSamples = m_allocatedSamples;
	}

	if (!m_left || !m_right)
	{
        memset(_outInterleaved, 0, sizeof(float) * _numSamples * 2);
		return;
	}

	memset(m_left, 0, sizeof(float) * _numSamples);
	memset(m_right, 0, sizeof(float) * _numSamples);

	for (int i = 0; i < m_numChannels; ++i)
	{
		
        float volLeft, volRight;
        CalcChannelVolumes(i, &volLeft, &volRight);
        // Always compute deltas and ramp unless the change is negligible
        float deltaVolLeft  = volLeft  - m_channels[i].m_oldVolLeft;
        float deltaVolRight = volRight - m_channels[i].m_oldVolRight;
        m_channels[i].m_forceVolumeJump = false;
        float const maxDelta = 0.003f;

		if (!m_channels[i].m_containsSilence) 
		{
            float *inBuf = m_channels[i].m_bufferFloat;
            if (!inBuf)
            {
            	continue;
            }

            if( i >= m_numChannels - m_numMusicChannels )
            {
                // Music channels: keep simple mixing using current block volume
                // (we could ramp here as well, but it's typically unnecessary)
                MixStereo(inBuf, _numSamples, volLeft, volRight);
            }
            else
            {
                if (fabsf(deltaVolLeft) < maxDelta &&
                    fabsf(deltaVolRight) < maxDelta &&
                    fabsf(volLeft  - m_channels[i].m_oldVolLeft)  < maxDelta &&
                    fabsf(volRight - m_channels[i].m_oldVolRight) < maxDelta)
                {
                    MixSameFreqFixedVol(inBuf, _numSamples, volLeft, volRight);
                }
                else
                {
                    MixSameFreqRampVol(inBuf, _numSamples,
                                       m_channels[i].m_oldVolLeft, m_channels[i].m_oldVolRight,
                                       volLeft, volRight);
                }
            }
        }

		m_channels[i].m_oldVolLeft = volLeft;
		m_channels[i].m_oldVolRight = volRight;
	}

    // Output mapping to [-1,1] for SDL float
    const float invPcm = 1.0f / 32767.0f;
    for (unsigned int i = 0; i < _numSamples; ++i)
    {
        float l = m_left[i] * invPcm;
        float r = m_right[i] * invPcm;
        if (l < -1.0f) l = -1.0f; if (l > 1.0f) l = 1.0f;
        if (r < -1.0f) r = -1.0f; if (r > 1.0f) r = 1.0f;
        _outInterleaved[i * 2] = l;
        _outInterleaved[i * 2 + 1] = r;
    }
}


void SoundLibrary3dSoftware::Callback(StereoSample *_buf, unsigned int _numSamples)
{
	if (!m_interleaved || !_buf)
	{
		return;
	}

    RenderToInterleavedFloat(m_interleaved, _numSamples);

    for (unsigned int i = 0; i < _numSamples; ++i)
    {
        float l = m_interleaved[i * 2];
        float r = m_interleaved[i * 2 + 1];
        float lPcm = l * 32767.0f;
        float rPcm = r * 32767.0f;
        if (lPcm > 32767.0f) lPcm = 32767.0f;
        if (lPcm < -32768.0f) lPcm = -32768.0f;
        if (rPcm > 32767.0f) rPcm = 32767.0f;
        if (rPcm < -32768.0f) rPcm = -32768.0f;
        _buf[i].m_left = Round(lPcm);
        _buf[i].m_right = Round(rPcm);
    }
}


bool SoundLibrary3dSoftware::Hardware3DSupport()
{
    return false;
}


int SoundLibrary3dSoftware::GetMaxChannels()
{           
    return 128;
}


int SoundLibrary3dSoftware::GetCPUOverhead()
{
    return 1;
}


float SoundLibrary3dSoftware::GetChannelHealth(int _channel)
{
	return 1.0f;
}


int SoundLibrary3dSoftware::GetChannelBufSize(int _channel) const
{
	return 0;
}

float SoundLibrary3dSoftware::GetChannelVolume(int _channel) const
{
	return m_channels[_channel].m_volume;
}

float SoundLibrary3dSoftware::GetChannelRelFreq (int _channel) const
{
   return 1.0f;
}

void SoundLibrary3dSoftware::SetChannel3DMode( int _channel, int _mode )
{
	AppAssert ( _channel >= 0 );
	m_channels[_channel].m_3DMode = _mode;
}


void SoundLibrary3dSoftware::SetChannelPosition( int _channel, Vector3<float> const &_pos, Vector3<float> const &_vel )
{
	AppAssert ( _channel >= 0 );
	m_channels[_channel].m_pos = _pos;
}


void SoundLibrary3dSoftware::SetChannelFrequency( int _channel, int _frequency )
{
	AppAssert ( _channel >= 0 );
	
	if( _channel < m_numChannels )
	{
		m_channels[_channel].m_freq = _frequency;
	}
	else
	{
		AppDebugOut("SOUND: channel %d out of range (numChannels=%d)\n", _channel, m_numChannels );
	}
}


void SoundLibrary3dSoftware::SetChannelMinDistance( int _channel, float _minDistance )
{
	AppAssert ( _channel >= 0 );
	m_channels[_channel].m_minDist = _minDistance;
}


void SoundLibrary3dSoftware::SetChannelVolume( int _channel, float _volume )
{
	AppAssert ( _channel >= 0 );
	m_channels[_channel].m_volume = _volume;
}

void SoundLibrary3dSoftware::SetListenerPosition( Vector3<float> const &_pos, 
                                        Vector3<float> const &_front,
                                        Vector3<float> const &_up,
                                        Vector3<float> const &_vel )
{   
	m_listenerPos = _pos;
	// Preserve handedness and ensure a stable, orthonormal basis for panning
	// Normalize inputs defensively in case caller passes non-unit vectors
	m_listenerFront = _front;
	m_listenerUp = _up;

	// Construct right = up x front; if degenerate, rebuild a safe basis
	m_listenerRight = m_listenerUp ^ m_listenerFront;
	float rightMag = m_listenerRight.Mag();

	if (rightMag < 1e-5f)
	{
		// Front and up nearly colinear; choose an auxiliary up vector
		Vector3<float> aux(0.0f, 1.0f, 0.0f);
		// If aux ~ front, pick a different axis
		if (fabsf(m_listenerFront.x) < 1e-3f && fabsf(m_listenerFront.z) < 1e-3f)
		{
			aux.Set(1.0f, 0.0f, 0.0f);
		}
		m_listenerRight = aux ^ m_listenerFront;
	}

	// Normalize the basis vectors
	float invRight = m_listenerRight.Mag();
	if (invRight > 1e-6f) m_listenerRight /= invRight;
	float invFront = m_listenerFront.Mag();
	if (invFront > 1e-6f) m_listenerFront /= invFront;
	// Recompute 'up' to ensure orthonormality
	m_listenerUp = m_listenerFront ^ m_listenerRight;
	float invUp = m_listenerUp.Mag();
	if (invUp > 1e-6f) m_listenerUp /= invUp;
}


void SoundLibrary3dSoftware::ResetChannel( int _channel )
{
	m_channels[_channel].m_forceVolumeJump = true;
}


void SoundLibrary3dSoftware::EnableDspFX(int _channel, int _numFilters, int const *_filterTypes)
{
    AppReleaseAssert( _numFilters > 0, "Bad argument passed to EnableFilters" );

	SoftwareChannel *channel = &m_channels[_channel];
    for( int i = 0; i < _numFilters; ++i )
    {
		AppDebugAssert(_filterTypes[i] >= 0 && _filterTypes[i] < NUM_FILTERS);
		
		channel->m_dspFX[_filterTypes[i]].m_chainIndex = i;

		switch (_filterTypes[i])
		{
			case DSP_RESONANTLOWPASS:
				channel->m_dspFX[_filterTypes[i]].m_userFilter = new DspResLowPass(m_sampleRate);
				break;
			case DSP_BITCRUSHER:
				channel->m_dspFX[_filterTypes[i]].m_userFilter = new DspBitCrusher(m_sampleRate);
				break;
			case DSP_GARGLE:
				channel->m_dspFX[_filterTypes[i]].m_userFilter = new DspGargle(m_sampleRate);
				break;
			case DSP_ECHO:
				channel->m_dspFX[_filterTypes[i]].m_userFilter = new DspEcho(m_sampleRate);
				break;
			case DSP_SIMPLE_REVERB:
				channel->m_dspFX[_filterTypes[i]].m_userFilter = new DspReverb(m_sampleRate);
				break;
		}
    }
}


void SoundLibrary3dSoftware::UpdateDspFX( int _channel, int _filterType, int _numParams, float const *_params )
{
	SoftwareChannel *channel = &m_channels[_channel];

	if (!channel->m_dspFX[_filterType].m_userFilter) return;

    channel->m_dspFX[_filterType].m_userFilter->SetParameters( _params );
}


void SoundLibrary3dSoftware::DisableDspFX( int _channel )
{
	SoftwareChannel *channel = &m_channels[_channel];
	for (int i = 0; i < NUM_FILTERS; ++i)
	{
		SoundLibFilterSoftware *filter = &channel->m_dspFX[i];
		if (filter->m_chainIndex != -1)
		{
			filter->m_chainIndex = -1;
			delete filter->m_userFilter; filter->m_userFilter = NULL;
		}
	}
}


void SoundLibrary3dSoftware::StartRecordToFile(char const *_filename)
{
	g_soundLibrary2d->StartRecordToFile(_filename);
}


void SoundLibrary3dSoftware::EndRecordToFile()
{
	g_soundLibrary2d->EndRecordToFile();
}
