#ifndef _included_soundsystem_h
#define _included_soundsystem_h

#include "lib/tosser/fast_darray.h"
#include "lib/tosser/llist.h"
#include "lib/tosser/hash_table.h"
#include "lib/math/vector3.h"
#include "lib/netlib/net_mutex.h"

#include "sound_instance.h"
#include "sound_parameter.h"
#include "soundsystem_interface.h"
#include "sound_blueprint_manager.h"

#include <atomic>


#if defined(TARGET_MSVC) && !defined(WINDOWS_SDL)
#define SOUND_USE_DSOUND_FREQUENCY_STUFF                // Set DirectSound buffer frequencies directly
                                                         // If not set, our sound library will do the Frequency adjustments in software
#endif

class TextReader;
class SoundInstance;
class TextFileWriter;
class SoundSystemInterface;
class SoundEventBlueprint;
class SampleGroup;
class DspBlueprint;
class SoundBlueprintManager;
class NetMutex;


//*****************************************************************************
// Class SoundSystem
//*****************************************************************************

class SoundSystem
{
public:    
    SoundBlueprintManager   m_blueprints;
    SoundSystemInterface    *m_interface;
    
    float                   m_timeSync;   
    float                   m_updatePeriod;
    bool                    m_propagateBlueprints;                          // If true, looping sounds will sync with blueprints
    Vector3<float>          m_editorPos;
    SoundInstanceId         m_editorInstanceId;
    
    FastDArray              <SoundInstance *> m_sounds;						// All the sounds that want to play

    // Deprecated direct channel map; left allocated to avoid breaking external code.
    // Do not read this from audio threads. Use atomic packed map helpers instead.
    SoundInstanceId         *m_channels;
    // Atomic channel map: each slot packs (index, uniqueId) into a 64-bit word
    std::atomic<uint64_t>   *m_channelPacked;
    int                     m_numChannels;
    int                     m_numMusicChannels;
    
protected:    
    int FindBestAvailableChannel( bool _music );
    
    bool IsMusicChannel         ( int _channelId );

    static bool SoundLibraryMainCallback( unsigned int _channel, signed short *_data, 
                                          unsigned int _numSamples, int *_silenceRemaining );

    NetMutex                *m_soundInstanceMutex;
    LList<SoundInstance *>   m_soundsPendingDelete;

    // Mixer safety diagnostic: counts times a valid-looking channel id was unreadable
    // (index >= 0) but GetSoundInstance returned NULL in the mixer thread.
    std::atomic<uint64_t>    m_invalidChannelIdReads;

public:
    SoundSystem();
    ~SoundSystem();

	void Initialise				( SoundSystemInterface *_interface );
    void RestartSoundLibrary    ();
    
    void Advance();

    bool InitialiseSound        ( SoundInstance *_instance );                   // Sets up sound, adds to instance list
    void ShutdownSound          ( SoundInstance *_instance );                   // Stops / deletes sound + removes refs

    void TriggerEvent           ( SoundObjectId _id, const char *_eventName );
    void TriggerEvent           ( const char *_type, const char *_eventName );
    void TriggerEvent           ( const char *_type, const char *_eventName, Vector3<float> const &_pos );

    void StopAllSounds          ( SoundObjectId _id, const char *_eventName=NULL );        // Pass in NULL to stop every event.
                                                                                    // Full event name required, eg "Darwinian SeenThreat"
	void EnableCallback         ( bool _enabled );

    bool GenerateChannelSamplesShort(unsigned int channel, signed short *dst, unsigned int numSamples, int *silenceRemaining);
#if !defined(SOUND_USE_DSOUND_FREQUENCY_STUFF)
    bool GenerateChannelSamplesFloat(unsigned int channel, float *dst, unsigned int numSamples, int *silenceRemaining);
#endif

    void StopAllDSPEffects      ();

    void TriggerDuplicateSound  ( SoundInstance *_instance );

    
    int  IsSoundPlaying         ( SoundInstanceId _id );
    int  NumInstancesPlaying    ( const char *_sampleName );
    int  NumInstancesPlaying    ( SoundObjectId _id, const char *_eventName );
    int  NumInstances           ( SoundObjectId _id, const char *_eventName );

    int  NumSoundInstances      ();
    int  NumChannelsUsed        ();
    int  NumSoundsDiscarded     ();
    
    void PropagateBlueprints    (bool _forceRestart=false);                     // Call this to update all looping sounds
    void RuntimeVerify          ();												// Verifies that the sound system has screwed it's own datastructures
	bool IsSampleUsed           (char const *_soundName);                       // Looks to see if that sound name is used in any blueprints 

    SoundInstance *GetSoundInstance( SoundInstanceId _id );
    SoundInstance *LockSoundInstance( SoundInstanceId _id );
    void          UnlockSoundInstance( SoundInstance *_instance );

    // Atomic channel map helpers
    inline static uint64_t PackChannelId(const SoundInstanceId &id) {
        // Pack two 32-bit signed ints into a 64-bit word: low=index, high=unique
        uint64_t lo = (uint32_t)id.m_index;
        uint64_t hi = (uint32_t)id.m_uniqueId;
        return (hi << 32) | lo;
    }
    inline static SoundInstanceId UnpackChannelId(uint64_t v) {
        SoundInstanceId id;
        id.m_index = (int32_t)(v & 0xFFFFFFFFu);
        id.m_uniqueId = (int32_t)((v >> 32) & 0xFFFFFFFFu);
        return id;
    }
    inline static SoundInstanceId InvalidChannelId() {
        SoundInstanceId id; id.m_index = -1; id.m_uniqueId = -1; return id;
    }
    inline SoundInstanceId LoadChannelId(int index) const {
        if (!m_channelPacked || index < 0 || index >= m_numChannels) return InvalidChannelId();
        uint64_t v = m_channelPacked[index].load(std::memory_order_acquire);
        return UnpackChannelId(v);
    }
    inline void StoreChannelId(int index, const SoundInstanceId &id) {
        if (!m_channelPacked || index < 0 || index >= m_numChannels) return;
        m_channelPacked[index].store(PackChannelId(id), std::memory_order_release);
        // Keep deprecated mirror updated for non-critical readers on main thread
        if (m_channels) m_channels[index] = id;
    }
    inline void InvalidateChannelId(int index) {
        StoreChannelId(index, InvalidChannelId());
    }
    inline bool ClearChannelIfMatches(int index, const SoundInstanceId &expected) {
        if (!m_channelPacked || index < 0 || index >= m_numChannels) return false;
        uint64_t exp = PackChannelId(expected);
        uint64_t inv = PackChannelId(InvalidChannelId());
        bool swapped = m_channelPacked[index].compare_exchange_strong(exp, inv, std::memory_order_acq_rel);
        if (swapped && m_channels) m_channels[index].SetInvalid();
        return swapped;
    }
    inline SoundInstanceId GetChannelId(int index) const { return LoadChannelId(index); }

    inline uint64_t GetInvalidChannelIdReadTotal() const { return m_invalidChannelIdReads.load(std::memory_order_relaxed); }
};



extern SoundSystem *g_soundSystem;

#endif
