#ifndef INCLUDED_SOUND_LIBRARY_3D_SOFTWARE_H
#define INCLUDED_SOUND_LIBRARY_3D_SOFTWARE_H

#include "sound_library_3d.h"


class SoftwareChannel;
class StereoSample;
class Profiler;


//*****************************************************************************
// Class SoundLibrary3dSoftware
//*****************************************************************************

class SoundLibrary3dSoftware: public SoundLibrary3d
{
protected:
	SoftwareChannel			*m_channels;	
	float					*m_left;						// Temp buffers used by mixer
	float					*m_right;						
	unsigned int			m_allocatedSamples;				// Actual allocated buffer size

    float                  *m_interleaved;                 // Scratch interleaved output (L,R)

	Vector3<float>			m_listenerFront;
	Vector3<float>			m_listenerUp;
	Vector3<float>			m_listenerRight;
	
	int						m_lastVolumeSet;				// consulted when un-muting

    // Mix-bus headroom (software path only)
    float                   m_headroomDb;       // fixed bus headroom in dB (e.g., 6.0)
    bool                    m_headroomEnabled;  // preference: apply fixed headroom

protected:
	void GetChannelData		(float _duration, unsigned int _numSamples);
	void ApplyDspFX			(float _duration, unsigned int _numSamples);
    
    void MixStereo          (float *in, unsigned int numSamples, float volL, float volR);
	void MixSameFreqFixedVol(float *in, unsigned int numSamples, float volL, float volR);
	void MixSameFreqRampVol (float *in, unsigned int num, float volL1, float volR1, float volL2, float volR2);
	void CalcChannelVolumes	(int _channelIndex, float *_left, float *_right);

public:
    SoundLibrary3dSoftware	();
	~SoundLibrary3dSoftware ();

    void Initialise         (int _mixFreq, int _numChannels, int _numMusicChannels, bool hw3d);

    void Advance            ();
	void SetMasterVolume    ( int _volume );
	void EnableCallback     (bool _enabled);
    void Callback			(StereoSample *_buf, unsigned int _numSamples); // Called from SoundLibrary2d
    void RenderToInterleavedFloat(float *_outInterleaved, unsigned int _numSamples);

    bool Hardware3DSupport	();
    int  GetMaxChannels		();
    int  GetCPUOverhead		();
    float GetChannelHealth  (int _channel);					// 0.0 = BAD, 1.0 = GOOD
    int GetChannelBufSize	(int _channel) const;
    float GetChannelRelFreq (int _channel) const;
    float GetChannelVolume  (int _channel) const;
            
    void ResetChannel       (int _channel);					// Refills entire channel with data immediately

    void SetChannel3DMode   (int _channel, int _mode);
    void SetChannelPosition (int _channel, Vector3<float> const &_pos, Vector3<float> const &_vel);    
    void SetChannelFrequency(int _channel, int _frequency);
    void SetChannelMinDistance( int _channel, float _minDistance);
    void SetChannelVolume   (int _channel, float _volume);	// logarithmic, 0.0f - 10.0f

    void EnableDspFX        (int _channel, int _numFilters, int const *_filterTypes);
    void UpdateDspFX        (int _channel, int _filterType, int _numParams, float const *_params);
    void DisableDspFX       (int _channel);
    
    void SetListenerPosition(Vector3<float> const &_pos, Vector3<float> const &_front,
                             Vector3<float> const &_up, Vector3<float> const &_vel);

	void StartRecordToFile	(char const *_filename);
    void EndRecordToFile	();

    // Runtime inspection (debug/overlay)
    inline float GetHeadroomDb() const { return m_headroomDb; }
    inline bool  GetHeadroomEnabled() const { return m_headroomEnabled; }
};


#endif
