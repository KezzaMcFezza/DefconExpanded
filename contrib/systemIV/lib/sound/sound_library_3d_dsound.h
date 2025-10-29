#ifndef INCLUDED_SOUND_LIBRARY_3D_DSOUND_H
#define INCLUDED_SOUND_LIBRARY_3D_DSOUND_H


#include "sound_library_3d.h"


class DirectSoundChannel;
class DirectSoundData;
struct IDirectSoundBuffer;


//*****************************************************************************
// Class SoundLibrary3dDirectSound
//*****************************************************************************

class SoundLibrary3dDirectSound: public SoundLibrary3d
{
protected:    
    DirectSoundChannel          *m_channels;
	DirectSoundData		        *m_directSound;
    bool                         m_dynEnabled;        // preference: enable dynamic bus attenuation

    // Fixed and dynamic anti-clip attenuation (centi-dB)
    float                        m_fixedHeadroomCentiDb; // fixed headroom in centi-dB
    float                        m_dynamicBusAtten;    // current smoothed attenuation
    float                        m_dynamicTarget;      // target attenuation
    float                        m_dynAttack;          // smoothing towards more attenuation
    float                        m_dynRelease;         // smoothing back towards 0
    // Dynamic attenuation heuristics (tunable)
    float                        m_dynLoudVolThresh;   // channel volume >= threshold considered "loud" (0..10)
    int                          m_dynStartCount;      // number of loud channels before attenuation starts
    float                        m_dynDbPerExtra;      // dB attenuation per extra loud channel beyond start
    float                        m_dynMaxDb;           // maximum dynamic attenuation in dB
            
protected:
	IDirectSoundBuffer          *CreateSecondaryBuffer(int _numSamples, int _numChannels);
    void RefreshCapabilities    ();
	long CalcWrappedDelta	    (long a, long b, unsigned long bufferSize);
    void PopulateBuffer		    (int _channel, int _fromSample, int _numSamples);
    void CommitChanges          ();					                                        // Commits all pos/or/vel etc changes
	void AdvanceChannel		    (int _channel, int _frameNum);
	int  GetNumFilters		    (int _channel);
	void Verify				    ();

public:
    SoundLibrary3dDirectSound   ();
    ~SoundLibrary3dDirectSound  ();

    void Initialise             (int _mixFreq, int _numChannels, int _numMusicChannels, bool hw3d);

    bool    Hardware3DSupport   ();
    int     GetMaxChannels		();
    int     GetCPUOverhead		();
    float   GetChannelHealth    (int _channel);					                            // 0.0 = BAD, 1.0 = GOOD
	int     GetChannelBufSize	(int _channel) const;
    float   GetChannelVolume    (int _channel) const;
    float   GetChannelRelFreq   (int _channel) const;

    void ResetChannel           (int _channel);					                            // Refills entire channel with data immediately

    void SetChannel3DMode       (int _channel, int _mode);		                            // 0 = 3d, 1 = head relative, 2 = disabled
    void SetChannelPosition     (int _channel, Vector3<float> const &_pos, Vector3<float> const &_vel);    
    void SetChannelFrequency    (int _channel, int _frequency);
    void SetChannelMinDistance  (int _channel, float _minDistance);
    void SetChannelVolume       (int _channel, float _volume);	                            // logarithmic, 0.0 - 10.0, 0=practially silent

    void EnableDspFX            (int _channel, int _numFilters, int const *_filterTypes);
    void UpdateDspFX            (int _channel, int _filterType, int _numParams, float const *_params);
    void DisableDspFX           (int _channel);
    
    void SetDopplerFactor       ( float _doppler );

    void SetListenerPosition    (Vector3<float> const &_pos, Vector3<float> const &_front,
                                 Vector3<float> const &_up, Vector3<float> const &_vel);

    void Advance                ();

    // Runtime inspection (debug/overlay)
    inline float GetFixedHeadroomDb() const { return m_fixedHeadroomCentiDb * 0.01f; }
    inline float GetDynamicBusAttenDb() const { return m_dynamicBusAtten * 0.01f; }
    inline float GetDynAttack() const { return m_dynAttack; }
    inline float GetDynRelease() const { return m_dynRelease; }
    inline float GetDynLoudVolThresh() const { return m_dynLoudVolThresh; }
    inline int   GetDynStartCount() const { return m_dynStartCount; }
    inline float GetDynDbPerExtra() const { return m_dynDbPerExtra; }
    inline float GetDynMaxDb() const { return m_dynMaxDb; }
};


#endif
