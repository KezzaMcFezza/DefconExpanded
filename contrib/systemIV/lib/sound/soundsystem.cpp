#include "lib/universal_include.h"
#include "lib/tosser/btree.h"
#include "lib/profiler.h"
#include <string.h>
#include <algorithm>
#include "lib/debug_utils.h"
#include "lib/preferences.h"
#include "lib/hi_res_time.h"
#include "lib/math/random_number.h"
#include "lib/netlib/net_mutex.h"

#include "soundsystem.h"
#include "soundsystem_interface.h"
#include "sound_sample_bank.h"
#include "sound_library_2d.h"
#include "sound_library_3d.h"
#include "sound_sample_decoder.h"
#include "sound_blueprint_manager.h"
#include "resampler_polyphase.h"

#if defined(WINDOWS_SDL) || defined(TARGET_OS_MACOSX) || defined(TARGET_OS_LINUX)
    #include "sound_library_2d_sdl.h"
    #include "sound_library_3d_software.h"
#elif defined(TARGET_MSVC)
    #include "sound_library_3d_dsound.h"
#endif

#define SOUNDSYSTEM_UPDATEPERIOD    0.05f

//#define SOUNDSYSTEM_VERIFY                        // Define this to make the sound system
                                                    // Perform a series of (very slow)
                                                    // tests every frame for correctness

SoundSystem *g_soundSystem = NULL;

static inline signed short FloatToPcmSample(float _value)
{
    if (_value > 32767.0f) _value = 32767.0f;
    if (_value < -32768.0f) _value = -32768.0f;
    return (signed short)_value;
}


//*****************************************************************************
// Class SoundSystem
//*****************************************************************************

SoundSystem::SoundSystem()
:   m_timeSync(0.0f),
    m_channels(NULL),
    m_channelPacked(NULL),
    m_numChannels(0),
    m_numMusicChannels(0),
    m_interface(NULL),
    m_propagateBlueprints(false)
{
    m_soundInstanceMutex = new NetMutex();
    AppSeedRandom( (unsigned int) GetHighResTime() );
    m_invalidChannelIdReads.store(0, std::memory_order_relaxed);
}


SoundSystem::~SoundSystem()
{
    m_sounds.EmptyAndDelete();

    delete[] m_channels;
    m_channels = NULL;
    delete[] m_channelPacked;
    m_channelPacked = NULL;
    delete[] m_channelPacked;
    m_channelPacked = NULL;

    delete g_soundSampleBank;
    g_soundSampleBank = NULL;

    delete g_soundLibrary3d;
    g_soundLibrary3d = NULL;
#if !defined TARGET_MSVC || defined WINDOWS_SDL
	delete g_soundLibrary2d;
	g_soundLibrary2d = NULL;
#endif

    delete m_soundInstanceMutex;
}


void SoundSystem::Initialise( SoundSystemInterface *_interface )
{
    m_interface = _interface;

    g_soundSampleBank = new SoundSampleBank();

    m_blueprints.LoadEffects();
    m_blueprints.LoadBlueprints();

#if !defined TARGET_MSVC || defined WINDOWS_SDL
	g_soundLibrary2d = NULL;
#endif
    RestartSoundLibrary();
}


void SoundSystem::RestartSoundLibrary()
{
    //
    // Shut down existing sound library safely
    //

    //
    // Disable callback quickly to reduce activity in callback mode

    if (g_soundLibrary3d)
    {
        g_soundLibrary3d->EnableCallback(false);
    }

#if !defined TARGET_MSVC || defined WINDOWS_SDL

    //
    // If using the SDL 2D backend, stop the device and feeder thread first

    if (g_soundLibrary2d)
    {
        SoundLibrary2dSDL *sdl2d = dynamic_cast<SoundLibrary2dSDL *>(g_soundLibrary2d);
        if (sdl2d)
        {
            sdl2d->Stop();
        }
    }
#endif

    //
    // Make sure to stop all currently playing sounds before we clear caches

    m_soundInstanceMutex->Lock();
    for (int i = 0; i < m_sounds.Size(); ++i)
    {
        if (m_sounds.ValidIndex(i))
        {
            SoundInstance *instance = m_sounds[i];
            instance->StopPlaying();

            delete instance->m_soundSampleHandle;
            instance->m_soundSampleHandle = NULL;
        }
    }
    m_soundInstanceMutex->Unlock();

    //
    // Now clear the sample cache

    g_soundSampleBank->EmptyCache();

    //
    // Delete the 3D and 2D libraries (3D first as its dtor references 2D)

    delete g_soundLibrary3d;
    g_soundLibrary3d = NULL;
#if !defined TARGET_MSVC || defined WINDOWS_SDL
    delete g_soundLibrary2d;
    g_soundLibrary2d = NULL;
#endif

    //
    // Finally, free the channel map used by the mixer

    delete[] m_channels;
    m_channels = NULL;

    //
    // Start up a new sound library

    SoundResampler::Initialise(g_preferences);

    g_preferences->SetInt(PREFS_SOUND_MIXFREQ, 44100);
    int mixrate = 44100;
    int volume = g_preferences->GetInt("SoundMasterVolume", 255);
    int requestedChannels = g_preferences->GetInt("SoundChannels", 32);
    m_numMusicChannels = g_preferences->GetInt("SoundMusicChannels", 12);
    int hw3d = g_preferences->GetInt("SoundHW3D", 0);
    const char *libName = g_preferences->GetString("SoundLibrary", "dsound");

#if defined TARGET_MSVC && !defined WINDOWS_SDL
    m_numChannels = requestedChannels;
    g_soundLibrary3d = new SoundLibrary3dDirectSound();
#else
    g_soundLibrary2d = new SoundLibrary2dSDL();
    g_soundLibrary3d = new SoundLibrary3dSoftware();

    //
    // Ensure preferences reflect the actual backend used by SDL builds

    g_preferences->SetString(PREFS_SOUND_LIBRARY, "software");
    m_numChannels = 64;

    if (requestedChannels != m_numChannels)
    {
        g_preferences->SetInt("SoundChannels", m_numChannels);
    }

#endif

    g_soundLibrary3d->SetMasterVolume(volume);
    g_soundLibrary3d->Initialise(mixrate, m_numChannels, m_numMusicChannels, hw3d);
    g_soundLibrary3d->SetDopplerFactor(0.0f);

    m_numChannels = g_soundLibrary3d->GetNumChannels();
    m_numMusicChannels = g_soundLibrary3d->GetNumMusicChannels();
    m_channels = new SoundInstanceId[m_numChannels];
    m_channelPacked = new std::atomic<uint64_t>[m_numChannels];
    for (int i = 0; i < m_numChannels; ++i)
    {
        m_channels[i].SetInvalid();
        m_channelPacked[i].store(PackChannelId(InvalidChannelId()), std::memory_order_relaxed);
    }

    g_soundLibrary3d->SetMainCallback(&SoundLibraryMainCallback);

    //
    // Now that 3D mixer is ready, start the 2D backend playback/feeder
    
#if !defined TARGET_MSVC || defined WINDOWS_SDL
    if (g_soundLibrary2d)
    {
        SoundLibrary2dSDL *sdl2d = dynamic_cast<SoundLibrary2dSDL *>(g_soundLibrary2d);
        if (sdl2d && !sdl2d->Started())
        {
            sdl2d->Start();
        }
    }
#endif
}


void SoundSystem::StopAllDSPEffects()
{
    for( int i = 0; i < m_sounds.Size(); ++i )
    {
        if( m_sounds.ValidIndex(i) )
        {
            SoundInstance *instance = m_sounds[i];
            while( instance->m_dspFX.ValidIndex(0) )
            {
                DspHandle *handle = instance->m_dspFX[0];
                instance->m_dspFX.RemoveData(0);
                delete handle;
            }
        }
    }
}


bool SoundSystem::IsMusicChannel( int _channelId )
{
    return( _channelId >= m_numChannels - m_numMusicChannels );
}


bool SoundSystem::SoundLibraryMainCallback( unsigned int _channel, signed short *_data, 
											unsigned int _numSamples, int *_silenceRemaining )
{
    if (!g_soundSystem)
    {
        if (g_soundLibrary3d)
        {
            g_soundLibrary3d->WriteSilence(_data, _numSamples);
        }
        else
        {
            memset(_data, 0, sizeof(signed short) * _numSamples);
        }
        return false;
    }

    return g_soundSystem->GenerateChannelSamplesShort(_channel, _data, _numSamples, _silenceRemaining);
}

bool SoundSystem::GenerateChannelSamplesShort(unsigned int channel, signed short *dst, unsigned int numSamples, int *silenceRemaining)
{
    // Defensive guard: mixer may be reinitialising
    if (!m_channelPacked || channel >= (unsigned)m_numChannels)
    {
        if (g_soundLibrary3d)
        {
            g_soundLibrary3d->WriteSilence(dst, numSamples);
        }
        else
        {
            memset(dst, 0, sizeof(signed short) * numSamples);
        }
        return false;
    }

    SoundInstanceId soundId = LoadChannelId((int)channel);
    SoundInstance *instance = LockSoundInstance(soundId);
    bool stereo = IsMusicChannel(channel);

    if (instance && instance->m_soundSampleHandle)
    {
#if defined(SOUND_USE_DSOUND_FREQUENCY_STUFF)
        float relFreq = g_soundLibrary3d->GetChannelRelFreq(channel);
        int numSamplesWritten = instance->m_soundSampleHandle->Read(dst, numSamples, stereo, relFreq);

        if (numSamplesWritten < (int)numSamples)
        {
            signed short *loopStart = dst + numSamplesWritten;
            unsigned int numSamplesRemaining = numSamples - numSamplesWritten;

            if (instance->m_loopType == SoundInstance::Looped ||
                instance->m_loopType == SoundInstance::LoopedADSR)
            {
                while (numSamplesRemaining > 0)
                {
                    bool looped = instance->AdvanceLoop();
                    if (looped)
                    {
                        unsigned int numWritten = instance->m_soundSampleHandle->Read(loopStart, numSamplesRemaining, stereo, relFreq);
                        loopStart += numWritten;
                        numSamplesRemaining -= numWritten;
                    }
                    else
                    {
                        g_soundLibrary3d->WriteSilence(loopStart, numSamplesRemaining);
                        numSamplesRemaining = 0;
                    }
                }
            }
            else if (instance->m_loopType == SoundInstance::SinglePlay)
            {
                if (numSamplesWritten > 0)
                {
                    g_soundLibrary3d->WriteSilence(loopStart, numSamplesRemaining);
                    *silenceRemaining = g_soundLibrary3d->GetChannelBufSize(channel) - numSamplesRemaining;
                }
                else
                {
                    g_soundLibrary3d->WriteSilence(loopStart, numSamplesRemaining);
                    *silenceRemaining -= numSamplesRemaining;
                    if (*silenceRemaining <= 0)
                    {
                        instance->BeginRelease(false);
                    }
                }
            }
        }
#else
        instance->RecalculateResampleStep();

        unsigned int requestedSamples = numSamples;
        unsigned int framesRequested = stereo ? requestedSamples / 2 : requestedSamples;

        if (stereo)
        {
            AppDebugAssert((requestedSamples % 2) == 0);
        }

        SoundSampleHandle *handle = instance->m_soundSampleHandle;
        SoundSampleDecoder *decoder = handle ? handle->m_soundSample : NULL;
        unsigned int totalFrames = handle ? handle->GetFrameCount() : 0;

        double cursor = instance->m_resampleCursor;
        double step = instance->m_resampleStep;
        unsigned int framesWritten = 0;
        bool waitingForLoop = false;
        SoundResampler::Quality resampleQuality = stereo ? SoundResampler::GetMusicQuality() : SoundResampler::GetSfxQuality();

#if !defined TARGET_MSVC || defined WINDOWS_SDL
        do {
            SoundLibrary2dSDL *sdl2d = g_soundLibrary2d ? dynamic_cast<SoundLibrary2dSDL *>(g_soundLibrary2d) : NULL;
            uint64_t scheduled = instance->GetScheduledStartFrames();
            if (sdl2d && sdl2d->UsingPushMode() && scheduled > 0)
            {
                uint64_t mixStart = sdl2d->GetCurrentSliceStartSample();
                if (scheduled > mixStart)
                {
                    uint64_t df = scheduled - mixStart;
                    unsigned int preFrames = (df > (uint64_t)framesRequested) ? framesRequested : (unsigned int)df;
                    if (preFrames > 0)
                    {
                        if (stereo)
                        {
                            unsigned int preSamples = preFrames * 2u;
                            memset(dst, 0, preSamples * sizeof(signed short));
                        }
                        else
                        {
                            memset(dst, 0, preFrames * sizeof(signed short));
                        }
                        framesWritten = preFrames;
                    }
                }
            }
        } while (0);
#endif

        while (framesWritten < framesRequested)
        {
            if (!decoder || totalFrames == 0)
            {
                break;
            }

            if (cursor >= (double)totalFrames)
            {
                double leftover = cursor - (double)totalFrames;

                if (instance->m_loopType == SoundInstance::Looped ||
                    instance->m_loopType == SoundInstance::LoopedADSR)
                {
                    bool looped = instance->AdvanceLoop();
                    if (looped)
                    {
                        handle = instance->m_soundSampleHandle;
                        decoder = handle ? handle->m_soundSample : NULL;
                        totalFrames = handle ? handle->GetFrameCount() : 0;

                        cursor = leftover;
                        if (cursor < 0.0) cursor = 0.0;
                        instance->ResetResamplerCursor(cursor);
                        instance->RecalculateResampleStep();
                        step = instance->m_resampleStep;
                        continue;
                    }
                    else
                    {
                        cursor = (double)totalFrames;
                        waitingForLoop = true;
                        break;
                    }
                }
                else
                {
                    cursor = (double)totalFrames;
                    break;
                }
            }

            float outLeft = 0.0f;
            float outRight = 0.0f;
            handle->GetFrame(cursor, stereo, resampleQuality, step, outLeft, outRight);

            if (stereo)
            {
                unsigned int sampleIndex = framesWritten * 2;
                if (sampleIndex + 1 < requestedSamples)
                {
                    dst[sampleIndex] = FloatToPcmSample(outLeft);
                    dst[sampleIndex + 1] = FloatToPcmSample(outRight);
                }
            }
            else
            {
                dst[framesWritten] = FloatToPcmSample(outLeft);
            }

            cursor += step;
            framesWritten++;
        }

        instance->ResetResamplerCursor(cursor);

        unsigned int samplesWritten = stereo ? framesWritten * 2 : framesWritten;

        if (samplesWritten < requestedSamples)
        {
            signed short *loopStart = dst + samplesWritten;
            unsigned int numSamplesRemaining = requestedSamples - samplesWritten;

            if (numSamplesRemaining > 0)
            {
                g_soundLibrary3d->WriteSilence(loopStart, numSamplesRemaining);
            }

            if (!waitingForLoop && instance->m_loopType == SoundInstance::SinglePlay)
            {
                if (samplesWritten > 0)
                {
                    *silenceRemaining = g_soundLibrary3d->GetChannelBufSize(channel) - numSamplesRemaining;
                }
                else
                {
                    *silenceRemaining -= numSamplesRemaining;
                    if (*silenceRemaining <= 0)
                    {
                        instance->BeginRelease(false);
                    }
                }
            }
        }
#endif

        UnlockSoundInstance(instance);
        return true;
    }
    else
    {
        if (g_soundLibrary3d)
        {
            g_soundLibrary3d->WriteSilence(dst, numSamples);
        }
        else
        {
            memset(dst, 0, sizeof(signed short) * numSamples);
        }
        if (instance)
        {
            UnlockSoundInstance(instance);
        }
        return false;
    }
}

#if !defined(SOUND_USE_DSOUND_FREQUENCY_STUFF)
bool SoundSystem::GenerateChannelSamplesFloat(unsigned int channel, float *dst, unsigned int numSamples, int *silenceRemaining)
{
    // Defensive guard: mixer may be reinitialising
    if (!m_channelPacked || channel >= (unsigned)m_numChannels)
    {
        if (dst && numSamples > 0)
        {
            std::fill(dst, dst + numSamples, 0.0f);
        }
        return false;
    }

    // Safety: if destination buffer is not available (e.g. during device/library
    // restart races) return silence without touching memory.
    if (!dst || numSamples == 0)
    {
        return false;
    }

    SoundInstanceId soundId = LoadChannelId((int)channel);
    SoundInstance *instance = LockSoundInstance(soundId);
    bool stereo = IsMusicChannel(channel);

    if (instance && instance->m_soundSampleHandle)
    {
        instance->RecalculateResampleStep();

        unsigned int requestedSamples = numSamples;
        unsigned int framesRequested = stereo ? requestedSamples / 2 : requestedSamples;

        if (stereo)
        {
            AppDebugAssert((requestedSamples % 2) == 0);
        }

        SoundSampleHandle *handle = instance->m_soundSampleHandle;
        SoundSampleDecoder *decoder = handle ? handle->m_soundSample : NULL;
        unsigned int totalFrames = handle ? handle->GetFrameCount() : 0;

        double cursor = instance->m_resampleCursor;
        double step = instance->m_resampleStep;
        unsigned int framesWritten = 0;
        bool waitingForLoop = false;
        SoundResampler::Quality resampleQuality = stereo ? SoundResampler::GetMusicQuality() : SoundResampler::GetSfxQuality();

#if !defined TARGET_MSVC || defined WINDOWS_SDL
        do {
            SoundLibrary2dSDL *sdl2d = g_soundLibrary2d ? dynamic_cast<SoundLibrary2dSDL *>(g_soundLibrary2d) : NULL;
            uint64_t scheduled = instance->GetScheduledStartFrames();
            if (sdl2d && sdl2d->UsingPushMode() && scheduled > 0)
            {
                uint64_t mixStart = sdl2d->GetCurrentSliceStartSample();
                if (scheduled > mixStart)
                {
                    uint64_t df = scheduled - mixStart;
                    unsigned int preFrames = (df > (uint64_t)framesRequested) ? framesRequested : (unsigned int)df;
                    if (preFrames > 0)
                    {
                        unsigned int preSamples = stereo ? preFrames * 2u : preFrames;
                        std::fill(dst, dst + preSamples, 0.0f);
                        framesWritten = preFrames;
                    }
                }
            }
        } while (0);
#endif

        while (framesWritten < framesRequested)
        {
            if (!decoder || totalFrames == 0)
            {
                break;
            }

            if (cursor >= (double)totalFrames)
            {
                double leftover = cursor - (double)totalFrames;

                if (instance->m_loopType == SoundInstance::Looped ||
                    instance->m_loopType == SoundInstance::LoopedADSR)
                {
                    bool looped = instance->AdvanceLoop();
                    if (looped)
                    {
                        handle = instance->m_soundSampleHandle;
                        decoder = handle ? handle->m_soundSample : NULL;
                        totalFrames = handle ? handle->GetFrameCount() : 0;

                        cursor = leftover;
                        if (cursor < 0.0) cursor = 0.0;
                        instance->ResetResamplerCursor(cursor);
                        instance->RecalculateResampleStep();
                        step = instance->m_resampleStep;
                        continue;
                    }
                    else
                    {
                        cursor = (double)totalFrames;
                        waitingForLoop = true;
                        break;
                    }
                }
                else
                {
                    cursor = (double)totalFrames;
                    break;
                }
            }

            float outLeft = 0.0f;
            float outRight = 0.0f;
            handle->GetFrame(cursor, stereo, resampleQuality, step, outLeft, outRight);

            if (stereo)
            {
                unsigned int sampleIndex = framesWritten * 2;
                if (sampleIndex + 1 < requestedSamples)
                {
                    dst[sampleIndex] = outLeft;
                    dst[sampleIndex + 1] = outRight;
                }
            }
            else
            {
                dst[framesWritten] = outLeft;
            }

            cursor += step;
            framesWritten++;
        }

        instance->ResetResamplerCursor(cursor);

        unsigned int samplesWritten = stereo ? framesWritten * 2 : framesWritten;

        if (samplesWritten < requestedSamples)
        {
            unsigned int numSamplesRemaining = requestedSamples - samplesWritten;
            std::fill(dst + samplesWritten, dst + requestedSamples, 0.0f);

            if (!waitingForLoop && instance->m_loopType == SoundInstance::SinglePlay)
            {
                if (samplesWritten > 0)
                {
                    *silenceRemaining = g_soundLibrary3d->GetChannelBufSize(channel) - numSamplesRemaining;
                }
                else
                {
                    *silenceRemaining -= numSamplesRemaining;
                    if (*silenceRemaining <= 0)
                    {
                        instance->BeginRelease(false);
                    }
                }
            }
        }

        UnlockSoundInstance(instance);
        return true;
    }
    else
    {
        // Destination may be null if the output buffers haven't been allocated yet.
        if (dst && numSamples > 0)
        {
            std::fill(dst, dst + numSamples, 0.0f);
        }
        if (soundId.m_index >= 0)
        {
            m_invalidChannelIdReads.fetch_add(1, std::memory_order_relaxed);
        }
        if (instance)
        {
            UnlockSoundInstance(instance);
        }
        return false;
    }
}
#endif


bool SoundSystem::InitialiseSound( SoundInstance *_instance )
{
    bool createNewSound = true;

    if( _instance->m_instanceType != SoundInstance::Polyphonic )
    {
        // This is a monophonic sound, so look for an exisiting
        // instance of the same sound
        for( int i = 0; i < m_sounds.Size(); ++i )
        {
            if( m_sounds.ValidIndex(i) )
            {
                SoundInstance *thisInstance = m_sounds[i];
                if( thisInstance->m_instanceType != SoundInstance::Polyphonic &&
                    stricmp( thisInstance->m_eventName, _instance->m_eventName ) == 0 )
                {
                    for( int j = 0; j < _instance->m_objIds.Size(); ++j )
                    {
                        SoundObjectId id = *_instance->m_objIds.GetPointer(j);
                        thisInstance->m_objIds.PutData( id );
                    }
                    createNewSound = false;
                    break;
                }
            }
        }
    }   

    if( createNewSound )
    {
        m_soundInstanceMutex->Lock();
        _instance->m_id.m_index = m_sounds.PutData( _instance );
        _instance->m_id.m_uniqueId = SoundInstanceId::GenerateUniqueId();
        _instance->m_restartAttempts = 3;
        
        // Memory fence to ensure instance is fully visible to audio/feeder threads
        std::atomic_thread_fence(std::memory_order_release);
        
        m_soundInstanceMutex->Unlock();
        return true;
    }
    else
    {
        return false;
    }
}


int SoundSystem::FindBestAvailableChannel( bool _music )
{
    int emptyChannelIndex = -1;                                         // Channel without a current sound or requested sound
    int lowestPriorityChannelIndex = -1;                                // Channel with very low priority
    float lowestPriorityChannel = 999999.9f;

    int lowestChannel = 0;
    int highestChannel = m_numChannels - m_numMusicChannels;

    if( _music )
    {
        lowestChannel = m_numChannels - m_numMusicChannels;
        highestChannel = m_numChannels;
    }

    for( int i = lowestChannel; i < highestChannel; ++i )
    {
        SoundInstanceId soundId = LoadChannelId(i);
        SoundInstance *currentSound = GetSoundInstance( soundId );

        float channelPriority = 0.0f;
        if( currentSound ) 
        {
            channelPriority = currentSound->m_calculatedPriority;
        }
        else
        {
            emptyChannelIndex = i;
            break;
        }

        if( channelPriority < lowestPriorityChannel )
        {
            lowestPriorityChannel = channelPriority;
            lowestPriorityChannelIndex = i;
        }
    }
    

    //
    // Did we find any empty channels?
	
    if( emptyChannelIndex != -1 )
    {
        return emptyChannelIndex;
    }
    else
    {
        return lowestPriorityChannelIndex;
    }
}


void SoundSystem::ShutdownSound( SoundInstance *_instance )
{
    m_soundInstanceMutex->Lock();

    if( m_sounds.ValidIndex( _instance->m_id.m_index ) )
    {
        m_sounds.RemoveData( _instance->m_id.m_index );
    }

    bool locked = _instance->IsLocked();
    m_soundInstanceMutex->Unlock();

    _instance->StopPlaying();

    if( locked )
    {
        // The audio thread is playing the instance right now, schedule this for deletion
        // when it's done with it.
        m_soundsPendingDelete.PutData( _instance );
    }
    else
    {
        delete _instance;
    }
}


SoundInstance *SoundSystem::GetSoundInstance( SoundInstanceId _id )
{
    // Lock-free read with memory barrier for thread safety
    // Called from both main thread and feeder/audio threads
    
    if( !m_sounds.ValidIndex(_id.m_index) )
    {
        return NULL;
    }

    // Memory fence to ensure we see up-to-date pointer from any writes
    std::atomic_thread_fence(std::memory_order_acquire);
    
    SoundInstance *found = m_sounds[_id.m_index];
    
    // Validate pointer is non-null and ID matches before use
    // The unique ID check prevents ABA problem if slot was reused
    if( found && found->m_id == _id )
    {
        return found;
    }

    return NULL;
}


SoundInstance *SoundSystem::LockSoundInstance( SoundInstanceId _id )
{
    // Use per-instance lock instead of global mutex
    SoundInstance *instance = GetSoundInstance( _id );
    if( instance )
    {
        instance->LockState();  // Fast spinlock, only held for microseconds
        instance->Lock();       // Legacy lock flag
    }
    return instance;
}


void SoundSystem::UnlockSoundInstance( SoundInstance *instance )
{
    // Use per-instance lock instead of global mutex
    if( instance )
    {
        instance->Unlock();         // Legacy lock flag
        instance->UnlockState();    // Fast spinlock
    }
}


void SoundSystem::TriggerEvent( SoundObjectId _objId, const char *_eventName )
{
    if( !m_channelPacked ) return;

	START_PROFILE("TriggerEvent");
    
    char *objectType = m_interface->GetObjectType(_objId);
    if( objectType )
    {
        SoundEventBlueprint *seb = m_blueprints.GetBlueprint(objectType);
        if( seb )
        {
            for( int i = 0; i < seb->m_events.Size(); ++i )
            {
                SoundInstanceBlueprint *sib = seb->m_events[i];
                if( stricmp( sib->m_eventName, _eventName ) == 0 )
                {
                    Vector3<float> pos, vel;
                    m_interface->GetObjectPosition( _objId, pos, vel );

                    SoundInstance *instance = new SoundInstance();
                    instance->Copy( sib->m_instance );
                    instance->m_objIds.PutData( _objId );
                    instance->m_pos = pos;
                    instance->m_vel = vel;
                    bool success = InitialiseSound  ( instance );                
                    if( !success ) ShutdownSound    ( instance );                
                }
            }
        }
    }

	END_PROFILE("TriggerEvent");
}


void SoundSystem::TriggerEvent( const char *_type, const char *_eventName )
{
    TriggerEvent( _type, _eventName, Vector3<float>::ZeroVector() );
}


void SoundSystem::TriggerEvent( const char *_type, const char *_eventName, Vector3<float> const &_pos )
{
    if( !m_channelPacked ) return;

	START_PROFILE("TriggerEvent");
    
    SoundEventBlueprint *seb = m_blueprints.GetBlueprint(_type);
    if( seb )
    {
        for( int i = 0; i < seb->m_events.Size(); ++i )
        {
            SoundInstanceBlueprint *sib = seb->m_events[i];
            if( stricmp( sib->m_eventName, _eventName ) == 0 )
            {
                SoundInstance *instance = new SoundInstance();
                instance->Copy( sib->m_instance );
                instance->m_pos = _pos;
                bool success = InitialiseSound  ( instance );                
                if( !success ) ShutdownSound    ( instance );                
            }
        }
    }

	END_PROFILE("TriggerEvent");
}


void SoundSystem::TriggerDuplicateSound ( SoundInstance *_instance )
{
    SoundInstance *newInstance = new SoundInstance();
    newInstance->Copy( _instance );
    newInstance->m_parent = _instance->m_parent;
    newInstance->m_pos = _instance->m_pos;
    newInstance->m_vel = _instance->m_vel;

    for( int i = 0; i < _instance->m_objIds.Size(); ++i )
    {
        SoundObjectId id = *_instance->m_objIds.GetPointer(i);
        newInstance->m_objIds.PutData( id );
    }

    bool success = InitialiseSound( newInstance );
    if( success && newInstance->m_positionType == SoundInstance::TypeInEditor )
    {
        m_editorInstanceId = newInstance->m_id;        
    }
    else if( !success )
    {
        ShutdownSound( newInstance);
    }
}


void SoundSystem::StopAllSounds( SoundObjectId _id, const char *_eventName )
{
    for( int i = 0; i < m_sounds.Size(); ++i )
    {
        if( m_sounds.ValidIndex(i) )
        {
            SoundInstance *instance = m_sounds[i];
            
            if( instance->m_objId == _id )
            {                    
                if( !_eventName || stricmp(instance->m_eventName, _eventName) == 0 )
                {
                    if( instance->IsPlaying() )
                    {
                        instance->BeginRelease(true);
                    }
                    else
                    {
                        ShutdownSound( instance );
                    }
                }
            }
        }
    }
}


int SoundSystem::IsSoundPlaying( SoundInstanceId _id )
{
    for( int i = 0; i < m_numChannels; ++i )
    {            
        SoundInstanceId soundId = LoadChannelId(i);            
        if( _id == soundId )
        {
            return i;
        }
    }

    return -1;               
}


int SoundSystem::NumInstancesPlaying( const char *_sampleName )
{
    int result = 0;

    for( int i = 0; i < m_numChannels; ++i )
    {            
        SoundInstanceId soundId = LoadChannelId(i);            
        SoundInstance *instance = GetSoundInstance(soundId);
        if( instance && 
            stricmp(instance->m_sampleName, _sampleName) == 0 )
        {
            ++result;
        }
    }   

    return result;
}


int SoundSystem::NumInstancesPlaying( SoundObjectId _id, const char *_eventName )
{
    int result = 0;

    for( int i = 0; i < m_numChannels; ++i )
    {            
        SoundInstanceId soundId = LoadChannelId(i);            
        SoundInstance *instance = GetSoundInstance( soundId );
        bool instanceMatch = !_id.IsValid() || instance->m_objId == _id;

        if( instance && 
            instanceMatch &&
            stricmp( instance->m_eventName, _eventName ) == 0 )
        {
            ++result;
        }                
    }
    
    return result;
}


int SoundSystem::NumInstances( SoundObjectId _id, const char *_eventName )
{
    int result = 0;

    for( int i = 0; i < m_sounds.Size(); ++i )
    {            
        if( m_sounds.ValidIndex(i) )
        {
            SoundInstance *instance = m_sounds[i];
            bool instanceMatch = !_id.IsValid() || instance->m_objId == _id;

            if( instance && 
                instanceMatch &&
                stricmp( instance->m_eventName, _eventName ) == 0 )
            {
                ++result;
            }                
        }
    }
    
    return result;
  
}


int SoundSystem::NumSoundInstances()
{
    return m_sounds.NumUsed();
}


int SoundSystem::NumChannelsUsed()
{
    int numUsed = 0;
    for( int i = 0; i < m_numChannels; ++i )
    {
        SoundInstanceId id = LoadChannelId(i);
        if( GetSoundInstance( id ) != NULL )
        {
            numUsed++;
        }
    }

    return numUsed;
}


int SoundSystem::NumSoundsDiscarded()
{
    return NumSoundInstances() - NumChannelsUsed();
}


int SoundInstanceCompare(const void *elem1, const void *elem2 )
{
    SoundInstanceId id1 = *((SoundInstanceId *) elem1);
    SoundInstanceId id2 = *((SoundInstanceId *) elem2);
    
    SoundInstance *instance1 = g_soundSystem->GetSoundInstance( id1 );
    SoundInstance *instance2 = g_soundSystem->GetSoundInstance( id2 );

    AppDebugAssert( instance1 );
    AppDebugAssert( instance2 );

    
    if      ( instance1->m_perceivedVolume < instance2->m_perceivedVolume )     return +1;
    else if ( instance1->m_perceivedVolume > instance2->m_perceivedVolume )     return -1;
    else                                                                        return 0;
}


void SoundSystem::Advance()
{
    if( !m_channelPacked )
	{
		return;
    }

    // Delete the sound instances that are pending for deletion.
    m_soundInstanceMutex->Lock();
    for( int i = 0; i < m_soundsPendingDelete.Size(); ++i )
    {
        SoundInstance *instance = m_soundsPendingDelete[i];
        if( !instance->IsLocked() )
        {
            AppDebugOut( "Removed pending audio instance.\n" );
            m_soundsPendingDelete.RemoveData( i );
            delete instance;
            --i;
        }
        else
        {
            AppDebugOut( "Failed to remove pending audio instance.\n" );
        }
    }
    m_soundInstanceMutex->Unlock();

    float timeNow = GetHighResTime();
    if( timeNow >= m_timeSync )
    {
        m_timeSync = timeNow + SOUNDSYSTEM_UPDATEPERIOD;
        
        START_PROFILE("SoundSystem");        
		
        // NOTE: Removed m_callbackLock - no longer needed with per-instance locks
        // Audio thread can now mix sounds concurrently with game logic updates
		
        //
        // Resync with blueprints (changed by editor)

        START_PROFILE("Propagate Blueprints" );
        if( m_propagateBlueprints )
        {
            PropagateBlueprints();
        }
        END_PROFILE("Propagate Blueprints" );


        //
		// First pass : Recalculate all Perceived Sound Volumes
		// Throw away sounds that have had their chance
        // Build a list of instanceIDs for sorting

        START_PROFILE("Allocate Sorted Array" );
        static int sortedArraySize = 128;
        static SoundInstanceId *sortedIds = NULL;
        if( m_sounds.NumUsed() > sortedArraySize )
        {
            delete [] sortedIds;
            sortedIds = NULL;
            while( sortedArraySize < m_sounds.NumUsed() )
                sortedArraySize *= 2;
        }
        if( !sortedIds )
        {            
            sortedIds = new SoundInstanceId[ sortedArraySize ];
        }

        int numSortedIds = 0;
        END_PROFILE("Allocate Sorted Array" );

        START_PROFILE("Perceived Volumes" );
        for( int i = 0; i < m_sounds.Size(); ++i )
        {
            if( m_sounds.ValidIndex(i) )
            {
                SoundInstance *instance = m_sounds[i];
                if( !instance->IsPlaying() && !instance->m_loopType ) instance->m_restartAttempts--;
                if( instance->m_restartAttempts < 0 )
                {
                    ShutdownSound( instance );
                }
                else if( instance->m_positionType == SoundInstance::Type3DAttachedToObject &&
                         !instance->GetAttachedObject().IsValid() )                
                {
                    ShutdownSound( instance );
                }
                else
                {
                    instance->CalculatePerceivedVolume();
                    sortedIds[numSortedIds] = instance->m_id;
                    numSortedIds++;
                }
            }
        }
        END_PROFILE("Perceived Volumes" );


        //        
		// Sort sounds into perceived volume order
        // NOTE : There are exactly numSortedId elements in sortedIds.  
        // NOTE : It isn't safe to assume numSortedIds == m_sounds.NumUsed()
        
        START_PROFILE("Sort Samples" );
        qsort( sortedIds, numSortedIds, sizeof(SoundInstanceId), SoundInstanceCompare );
        END_PROFILE("Sort Samples" );


        //
		// Second pass : Recalculate all Sound Priorities starting with the nearest sounds
        // Reduce priorities as more of the same sounds are played

        BTree<float> existingInstances;
        
                
        //                
        // Also look out for the highest priority new sound to swap in

        START_PROFILE("Recalculate Priorities" );

        LList<SoundInstance *> newInstances;
        SoundInstance *newInstance = NULL;
        float highestInstancePriority = 0.0f;

        for( int i = 0; i < numSortedIds; ++i )
        {
            SoundInstanceId id = sortedIds[i];
            SoundInstance *instance = GetSoundInstance( id );
            AppDebugAssert( instance );

            instance->m_calculatedPriority = instance->m_perceivedVolume;

            BTree<float> *existingInstance = existingInstances.LookupTree( instance->m_eventName );
            if( existingInstance )
            {
                instance->m_calculatedPriority *= existingInstance->data;
                existingInstance->data *= 0.75f;
            }
            else
            {
                existingInstances.PutData( instance->m_eventName, 0.75f );
            }

            if( !instance->IsPlaying() )
            {
                if( instance->m_positionType == SoundInstance::TypeMusic )
                {
                    newInstances.PutData( instance );
                }
                else if( instance->m_calculatedPriority > highestInstancePriority )
                {
                    newInstance = instance;
                    highestInstancePriority = instance->m_calculatedPriority;
                }
            }
        }

        if( newInstance )
        {
            newInstances.PutData( newInstance );
        }

        END_PROFILE("Recalculate Priorities" );
        
     
        for( int i = 0; i < newInstances.Size(); ++i )
        {            
            SoundInstance *newInstance = newInstances[i];
            bool isMusic = (newInstance->m_positionType == SoundInstance::TypeMusic);

            // Find worst old sound to get rid of  
            START_PROFILE("Find best Channel" );
            int bestAvailableChannel = FindBestAvailableChannel( isMusic );
            END_PROFILE("Find best Channel" );

			// FindBestAvailableChannel can return -1, so let's not access an invalid index later on.
			if ( bestAvailableChannel < 0 )
				continue;

            START_PROFILE("Stop Old Sound" );
			// Stop the old sound
            SoundInstance *existingInstance = GetSoundInstance( LoadChannelId(bestAvailableChannel) );
            if( existingInstance && !existingInstance->m_loopType )
            {
                ShutdownSound( existingInstance );
            }
            else if( existingInstance )
            {
                existingInstance->StopPlaying();
            }            
            END_PROFILE("Stop Old Sound" );


            START_PROFILE( "Start New Sound" );
				// Start the new sound
            bool success = newInstance->StartPlaying( bestAvailableChannel );
            if( success )
            {
                StoreChannelId(bestAvailableChannel, newInstance->m_id);
#if !defined TARGET_MSVC || defined WINDOWS_SDL
                // In push mode with timed scheduling, schedule start on audio timeline
                SoundLibrary2dSDL *sdl2d = g_soundLibrary2d ? dynamic_cast<SoundLibrary2dSDL *>(g_soundLibrary2d) : NULL;
                if (sdl2d && sdl2d->UsingTimedScheduling())
                {
                    uint64_t startFrames = sdl2d->SuggestScheduledStartFrames(5);
                    newInstance->SetScheduledStartFrames(startFrames);
                }
#endif
            }
            else
            {
                // This is fairly bad, the sound failed to play
                // Which means it failed to load, or to go into a channel
                ShutdownSound( newInstance );
            }
            END_PROFILE("Start New Sound" );
            
            START_PROFILE("Reset Channel" );
            g_soundLibrary3d->ResetChannel( bestAvailableChannel);
            END_PROFILE("Reset Channel" );
        }


        //
        // Advance all sound channels

        START_PROFILE("Advance All Channels" );
        for( int i = 0; i < m_numChannels; ++i )
        {            
            SoundInstanceId soundId = LoadChannelId(i);            
            SoundInstance *currentSound = GetSoundInstance( soundId );                         
            if( currentSound )
            {
                bool amIDone = currentSound->Advance();
                if( amIDone )
                {                    
			        START_PROFILE("Shutdown Sound" );
                    ShutdownSound( currentSound );
			        END_PROFILE("Shutdown Sound" );
                }    
            }
        }
        END_PROFILE("Advance All Channels" );


		//
        // Update our listener position

        START_PROFILE("UpdateListener" );    

        Vector3<float> camPos, camVel, camUp, camFront;
        bool cameraDefined = m_interface->GetCameraPosition( camPos, camFront, camUp, camVel );

        if( cameraDefined )
        {
            if( g_preferences->GetInt("SoundSwapStereo",0) == 0 )
            {
                camUp *= -1.0f;
            }

            g_soundLibrary3d->SetListenerPosition( camPos, camFront, camUp, camVel );
        }
        
        END_PROFILE("UpdateListener" );    


        //
        // Advance our sound library

        START_PROFILE("SoundLibrary3d Advance" );
        g_soundLibrary3d->Advance();
        END_PROFILE("SoundLibrary3d Advance" );
      
#ifdef SOUNDSYSTEM_VERIFY
        RuntimeVerify();
#endif

        // NOTE: Removed m_callbackLock.Unlock() - no longer needed with per-instance locks

        END_PROFILE("SoundSystem");                    
    }
}


void SoundSystem::RuntimeVerify()
{
    //
    // Make sure there aren't any SoundInstances on more than one channel
    // Make sure all playing samples have sensible channel handles

/*
    for( int i = 0; i < m_numChannels; ++i )
    {
        SoundInstanceId id1 = m_channels[i];
        SoundInstance *currentSound = GetSoundInstance( id1 );
        AppDebugAssert( !currentSound || 
                     !(currentSound->IsPlaying() && currentSound->m_channelIndex == -1) );
        
        if( currentSound )
        {                
            for( int j = 0; j < m_numChannels; ++j )
            {
                if( i != j )
                {
                    SoundInstanceId id2 = m_channels[j];
                    if( GetSoundInstance(id2) )
                    {
                        AppDebugAssert( !(id1 == id2) );
                    }
                }
            }
        }
    }
*/

    //
    // Make sure all sounds that believe they are playing have an opened sound stream

    for( int i = 0; i < m_numChannels; ++i )
    {
        SoundInstanceId id1 = m_channels[i];
        SoundInstance *currentSound = GetSoundInstance( id1 );
    }
}


void SoundSystem::PropagateBlueprints( bool _forceRestart )
{
    for( int i = m_sounds.Size() - 1; i >= 0; --i )
    {
        if( m_sounds.ValidIndex(i) )
        {
            SoundInstance *instance = m_sounds[i];

			if( instance->IsPlayable() )
			{
				instance->PropagateBlueprints( _forceRestart );
			}
			else
			{
				ShutdownSound( instance );
			}
        }
    }
}
    

void SoundSystem::EnableCallback( bool _enabled )
{
	g_soundLibrary3d->EnableCallback(_enabled);
}
