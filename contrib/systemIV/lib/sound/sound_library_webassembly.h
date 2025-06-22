#ifndef INCLUDED_SOUND_LIBRARY_WEBASSEMBLY_H
#define INCLUDED_SOUND_LIBRARY_WEBASSEMBLY_H

#ifdef TARGET_EMSCRIPTEN

#include "sound_library_3d.h"
#include "lib/tosser/hash_table.h"
#include "lib/math/vector3.h"

//*****************************************************************************
// WebAssembly Sound System
// Purpose: Web Audio API implementation for browser compatibility
// Replaces SDL audio system which doesn't work properly in WebAssembly
//*****************************************************************************

// DSP Filter Constants (matching desktop implementations)
#define MAX_DSP_FILTERS 8

// DSP Filter Types (matching sound_filter.h and DirectSound)
enum DspFilterType {
    DSP_RESONANTLOWPASS = 10,
    DSP_BITCRUSHER = 11,
    DSP_GARGLE = 12,
    DSP_ECHO = 13,
    DSP_SIMPLE_REVERB = 14,
    DSP_SMOOTHER = 15,
    
    // DirectSound equivalents (for compatibility)
    DSP_DSOUND_CHORUS = 0,
    DSP_DSOUND_COMPRESSOR = 1,
    DSP_DSOUND_DISTORTION = 2,
    DSP_DSOUND_ECHO = 3,
    DSP_DSOUND_FLANGER = 4,
    DSP_DSOUND_GARGLE = 5,
    DSP_DSOUND_I3DL2REVERB = 6,
    DSP_DSOUND_PARAMEQ = 7,
    DSP_DSOUND_WAVESREVERB = 8
};

//*****************************************************************************
// Class SoundLibraryWebAssembly
//*****************************************************************************

class SoundLibraryWebAssembly : public SoundLibrary3d
{
protected:
    // Web Audio API Core Components
    void*                   m_audioContext;        // AudioContext*
    void*                   m_masterGainNode;      // GainNode* (master volume control)
    void*                   m_listenerNode;        // AudioListener* (3D positioning)
    
    // Channel Management System (32-64 channels like desktop)
    struct WebAudioChannel {
        // Web Audio API Nodes
        void*               sourceNode;             // AudioBufferSourceNode*
        void*               pannerNode;             // PannerNode* (for 3D audio)
        void*               gainNode;               // GainNode* (for volume control)
        void*               audioBuffer;            // AudioBuffer* (decoded audio data)
        
        // DSP Effects Chain (NEW)
        void*               dspChainInput;          // First node in DSP chain
        void*               dspChainOutput;         // Last node in DSP chain
        int                 numDspFilters;          // Number of active DSP filters
        int                 dspFilterTypes[MAX_DSP_FILTERS];  // Types of active filters
        void*               dspNodes[MAX_DSP_FILTERS];        // Web Audio API filter nodes
        
        // Channel State
        bool                active;
        bool                is3D;
        bool                isMusic;
        bool                looping;
        
        // Audio Properties
        float               volume;
        float               frequency;
        Vector3<float>      position;
        Vector3<float>      velocity;
        float               minDistance;
        
        // Playback State
        char                currentSample[256];     // Currently playing sample name
        double              startTime;              // When playback started
        bool                stopping;               // Channel is being stopped
        
        WebAudioChannel() {
            sourceNode = nullptr;
            pannerNode = nullptr;
            gainNode = nullptr;
            audioBuffer = nullptr;
            dspChainInput = nullptr;
            dspChainOutput = nullptr;
            numDspFilters = 0;
            for (int i = 0; i < MAX_DSP_FILTERS; i++) {
                dspFilterTypes[i] = -1;
                dspNodes[i] = nullptr;
            }
            active = false;
            is3D = true;
            isMusic = false;
            looping = false;
            volume = 1.0f;
            frequency = 1.0f;
            position = Vector3<float>(0,0,0);
            velocity = Vector3<float>(0,0,0);
            minDistance = 100.0f;
            currentSample[0] = '\0';
            startTime = 0.0;
            stopping = false;
        }
    };
    WebAudioChannel*        m_channels;
    
    // Audio Asset Management
    struct AudioAsset {
        void*               audioBuffer;            // AudioBuffer* (decoded OGG data)
        bool                loaded;
        bool                loading;
        int                 refCount;
        char                filename[256];
        int                 numChannels;
        int                 sampleRate;
        float               duration;
        
        AudioAsset() {
            audioBuffer = nullptr;
            loaded = false;
            loading = false;
            refCount = 0;
            filename[0] = '\0';
            numChannels = 0;
            sampleRate = 0;
            duration = 0.0f;
        }
    };
    HashTable<AudioAsset*>  m_audioAssets;
    
    // WebAssembly State
    bool                    m_initialized;
    bool                    m_contextSuspended;
    bool                    m_requiresUserInteraction;
    float                   m_masterVolume;
    int                     m_freq;                     // Sample rate (inherited from base class)
    Vector3<float>          m_listenerPosition;
    Vector3<float>          m_listenerFront;
    Vector3<float>          m_listenerUp;
    
    // Internal Helper Methods
    bool                    CreateAudioContext();
    void                    CleanupAudioContext();
    int                     FindFreeChannel(bool _music);
    void                    CleanupChannel(int _channel);
    bool                    LoadAudioAsset(const char* _filename);
    AudioAsset*             GetAudioAsset(const char* _filename);
    void                    UpdateChannelProperties(int _channel);
    
public:
    SoundLibraryWebAssembly();
    virtual ~SoundLibraryWebAssembly();
    
    // Core SoundLibrary3d Interface Implementation
    void    Initialise                 (int _mixFreq, int _numChannels, int _numMusicChannels, bool hw3d) override;
    void    SetMasterVolume            (int _volume) override;
    void    SetChannelVolume           (int _channel, float _volume) override;
    void    SetChannelPosition         (int _channel, Vector3<float> const &_pos, Vector3<float> const &_vel) override;
    void    SetListenerPosition        (Vector3<float> const &_pos, Vector3<float> const &_front, 
                                        Vector3<float> const &_up, Vector3<float> const &_vel) override;
    
    // Channel Management
    void    SetChannel3DMode           (int _channel, int _mode) override;
    void    SetChannelFrequency        (int _channel, int _frequency) override;  // Note: base class uses int, we'll convert internally
    void    SetChannelMinDistance      (int _channel, float _minDistance) override;
    void    ResetChannel               (int _channel) override;
    void    StopChannel                (int _channel);  // Not virtual in base class, so no override
    
    // Hardware Support Query (always true for Web Audio API)
    bool    Hardware3DSupport          () override { return true; }
    int     GetMaxChannels             () override { return m_numChannels; }
    int     GetCPUOverhead             () override { return 1; }  // Low CPU overhead for Web Audio API
    int     GetChannelBufSize          (int _channel) const override;
    float   GetChannelVolume           (int _channel) const override;
    float   GetChannelRelFreq          (int _channel) const override;
    float   GetChannelHealth           (int _channel) override;
    
    // DSP Effects (required pure virtual functions)
    void    EnableDspFX                (int _channel, int _numFilters, int const *_filterTypes) override;
    void    UpdateDspFX                (int _channel, int _filterType, int _numParams, float const *_params) override;
    void    DisableDspFX               (int _channel) override;
    
    // System Management
    void    Advance                    () override;
    
    // WebAssembly-Specific Implementation
    bool    InitializeWebAudioContext  ();
    void    TriggerAudioUnlock         ();
    bool    RequiresUserInteraction    ();
    bool    IsContextSuspended         ();
    void    ResumeAudioContext         ();
    
    // Audio Streaming (NEW - Phase 8.3.1)
    void    RequestAudioDataForChannel (int channelId);
    signed short* RequestAudioDataForProcessor(int channelId, int bufferSize);
    
    // Audio Asset Management
    bool    PreloadAudioAsset          (const char* _filename);
    bool    IsAudioAssetLoaded         (const char* _filename);
    void    UnloadAudioAsset           (const char* _filename);
    
    // Playback Control
    bool    StartChannelPlayback       (int _channel, const char* _sampleName, bool _looped);
    void    StopChannelPlayback        (int _channel);
    bool    IsChannelPlaying           (int _channel);
    float   GetChannelProgress         (int _channel);
    
    // Debug and Statistics
    void    LogChannelStatus           (int _channel);
    void    LogSystemStatus            ();
    int     GetActiveChannelCount      ();
    int     GetLoadedAssetCount        ();
    float   GetTotalMemoryUsage        ();
};

//*****************************************************************************
// JavaScript Bridge Functions (C++ to Web Audio API)
//*****************************************************************************

extern "C" {
    // Core Audio Context Management
    void jsCreateAudioContext();
    bool jsIsAudioContextAvailable();
    bool jsIsAudioContextSuspended();
    void jsResumeAudioContext();
    void jsSetMasterVolume(float volume);
    
    // Listener (Camera) Positioning
    void jsSetListenerPosition(float x, float y, float z);
    void jsSetListenerOrientation(float fx, float fy, float fz, float ux, float uy, float uz);
    
    // Asset Loading and Management
    void jsLoadAudioAsset(const char* filename, const char* data, int dataSize);
    bool jsIsAssetLoaded(const char* filename);
    bool jsIsAssetLoading(const char* filename);
    void jsUnloadAsset(const char* filename);
    
    // Channel Playback Control
    void jsStartChannelPlayback(int channelId, const char* filename, bool loop, float volume, float frequency);
    void jsStopChannelPlayback(int channelId);
    bool jsIsChannelPlaying(int channelId);
    float jsGetChannelProgress(int channelId);
    
    // Real Audio Playback (NEW - Phase 8.3)
    void jsStartChannelPlaybackWithPCMData(int channelId, signed short* pcmData, int numSamples, bool loop, float volume, float frequency);
    void jsStartChannelPlaybackWithAsset(int channelId, const char* filename, bool loop, float volume, float frequency);
    
    // Continuous Audio Streaming (NEW - Phase 8.3.1)
    void jsStartContinuousAudioStream(int channelId);
    void jsStopContinuousAudioStream(int channelId);
    void jsPlayAudioChunk(int channelId, signed short* pcmData, int numSamples, float volume, float frequency);
    
    // C++ Callback for JavaScript (EMSCRIPTEN_KEEPALIVE)
    void jsRequestAudioData(int channelId);
    signed short* jsRequestAudioDataForProcessor(int channelId, int bufferSize);
    void jsFreeAudioData(signed short* audioData);
    
    // Channel Property Control
    void jsSetChannelVolume(int channelId, float volume);
    void jsSetChannelFrequency(int channelId, float frequency);
    void jsSetChannelPosition(int channelId, float x, float y, float z);
    void jsSetChannelVelocity(int channelId, float vx, float vy, float vz);
    void jsSetChannelMinDistance(int channelId, float minDistance);
    void jsSetChannel3DMode(int channelId, bool enabled);
    
    // Browser Compatibility
    bool jsRequiresUserInteraction();
    void jsTriggerAudioUnlock();
    
    // Debug and Statistics
    void jsLogAudioStats();
    int jsGetActiveSourceCount();
    float jsGetAudioMemoryUsage();
    
    // DSP Effects System (NEW)
    void jsCreateDspChain(int channelId, int numFilters, const int* filterTypes);
    void jsUpdateDspFilter(int channelId, int filterType, int numParams, const float* params);
    void jsRemoveDspChain(int channelId);
    void jsConnectDspChain(int channelId);
}

#endif // TARGET_EMSCRIPTEN

#endif // INCLUDED_SOUND_LIBRARY_WEBASSEMBLY_H 