#include "lib/universal_include.h"

#ifdef TARGET_EMSCRIPTEN

#include <emscripten.h>
#include <emscripten/html5.h>
#include <string.h>
#include <stdio.h>

#include "sound_library_webassembly.h"
#include "sound_sample_bank.h"
#include "lib/debug_utils.h"
#include "lib/preferences.h"
#include "lib/hi_res_time.h"
#include "lib/string_utils.h"

//*****************************************************************************
// WebAssembly Sound Library Implementation
//*****************************************************************************

SoundLibraryWebAssembly::SoundLibraryWebAssembly()
{
    m_audioContext = nullptr;
    m_masterGainNode = nullptr;
    m_listenerNode = nullptr;
    m_channels = nullptr;
    m_initialized = false;
    m_contextSuspended = false;
    m_requiresUserInteraction = false;
    m_masterVolume = 1.0f;
    m_freq = 44100;  // Default sample rate
    m_listenerPosition = Vector3<float>(0, 0, 0);
    m_listenerFront = Vector3<float>(0, 0, -1);
    m_listenerUp = Vector3<float>(0, 1, 0);
    
#ifdef EMSCRIPTEN_SOUND_TESTBED
    AppDebugOut("SoundLibraryWebAssembly: Constructor called\n");
#endif
}

SoundLibraryWebAssembly::~SoundLibraryWebAssembly()
{
#ifdef EMSCRIPTEN_SOUND_TESTBED
    AppDebugOut("SoundLibraryWebAssembly: Destructor called\n");
#endif
    
    // Cleanup all channels
    if (m_channels)
    {
        for (int i = 0; i < m_numChannels; i++)
        {
            CleanupChannel(i);
        }
        delete[] m_channels;
        m_channels = nullptr;
    }
    
    // Cleanup audio assets
    for (int i = 0; i < m_audioAssets.Size(); i++)
    {
        if (m_audioAssets.ValidIndex(i))
        {
            AudioAsset* asset = m_audioAssets[i];
            if (asset)
            {
                jsUnloadAsset(asset->filename);
                delete asset;
            }
        }
    }
    m_audioAssets.Empty();
    
    // Cleanup Web Audio API context
    CleanupAudioContext();
}

void SoundLibraryWebAssembly::Initialise(int _mixFreq, int _numChannels, int _numMusicChannels, bool hw3d)
{
#ifdef EMSCRIPTEN_SOUND_TESTBED
    AppDebugOut("SoundLibraryWebAssembly: Initializing with %d channels, %d music channels, %d Hz\n", 
                _numChannels, _numMusicChannels, _mixFreq);
#endif
    
    m_freq = _mixFreq;
    m_numChannels = _numChannels;
    m_numMusicChannels = _numMusicChannels;
    
    // Create channel management array
    m_channels = new WebAudioChannel[m_numChannels];
    
    // Initialize Web Audio API context
    if (InitializeWebAudioContext())
    {
        m_initialized = true;
#ifdef EMSCRIPTEN_SOUND_TESTBED
        AppDebugOut("SoundLibraryWebAssembly: Successfully initialized Web Audio API\n");
#endif
        
        // Set initial listener position
        SetListenerPosition(Vector3<float>(0, 0, 0), 
                          Vector3<float>(0, 0, -1), 
                          Vector3<float>(0, 1, 0), 
                          Vector3<float>(0, 0, 0));
    }
    else
    {
#ifdef EMSCRIPTEN_SOUND_TESTBED
        AppDebugOut("SoundLibraryWebAssembly: Failed to initialize Web Audio API\n");
#endif
        m_initialized = false;
    }
}

bool SoundLibraryWebAssembly::InitializeWebAudioContext()
{
#ifdef EMSCRIPTEN_SOUND_TESTBED
    AppDebugOut("SoundLibraryWebAssembly: Creating Web Audio API context...\n");
#endif
    
    // Create the Web Audio API context via JavaScript
    jsCreateAudioContext();
    
    // Check if context was created successfully
    if (!jsIsAudioContextAvailable())
    {
#ifdef EMSCRIPTEN_SOUND_TESTBED
        AppDebugOut("SoundLibraryWebAssembly: Failed to create AudioContext\n");
#endif
        return false;
    }
    
    // Check if user interaction is required (mobile browsers)
    m_requiresUserInteraction = jsRequiresUserInteraction();
    if (m_requiresUserInteraction)
    {
#ifdef EMSCRIPTEN_SOUND_TESTBED
        AppDebugOut("SoundLibraryWebAssembly: User interaction required to unlock audio\n");
#endif
    }
    
    // Check if context is suspended
    m_contextSuspended = jsIsAudioContextSuspended();
    if (m_contextSuspended)
    {
#ifdef EMSCRIPTEN_SOUND_TESTBED
        AppDebugOut("SoundLibraryWebAssembly: AudioContext is suspended, will need user interaction\n");
#endif
    }
    
    // Set initial master volume
    jsSetMasterVolume(m_masterVolume);
    
#ifdef EMSCRIPTEN_SOUND_TESTBED
    AppDebugOut("SoundLibraryWebAssembly: Web Audio API context created successfully\n");
#endif
    return true;
}

void SoundLibraryWebAssembly::CleanupAudioContext()
{
    // JavaScript will handle AudioContext cleanup automatically
    m_audioContext = nullptr;
    m_masterGainNode = nullptr;
    m_listenerNode = nullptr;
}

void SoundLibraryWebAssembly::SetMasterVolume(int _volume)
{
    // Convert from 0-255 range to 0.0-1.0 range
    m_masterVolume = (float)_volume / 255.0f;
    
    if (m_initialized)
    {
        jsSetMasterVolume(m_masterVolume);
    }
    
#ifdef EMSCRIPTEN_SOUND_TESTBED
    AppDebugOut("SoundLibraryWebAssembly: Master volume set to %d (%.2f)\n", _volume, m_masterVolume);
#endif
}

void SoundLibraryWebAssembly::SetChannelVolume(int _channel, float _volume)
{
    if (_channel < 0 || _channel >= m_numChannels)
    {
        return;
    }
    
    m_channels[_channel].volume = _volume;
    
    if (m_initialized && m_channels[_channel].active)
    {
        jsSetChannelVolume(_channel, _volume);
    }
}

void SoundLibraryWebAssembly::SetChannelPosition(int _channel, Vector3<float> const &_pos, Vector3<float> const &_vel)
{
    if (_channel < 0 || _channel >= m_numChannels)
    {
        return;
    }
    
    m_channels[_channel].position = _pos;
    m_channels[_channel].velocity = _vel;
    
    if (m_initialized && m_channels[_channel].active && m_channels[_channel].is3D)
    {
        jsSetChannelPosition(_channel, _pos.x, _pos.y, _pos.z);
        jsSetChannelVelocity(_channel, _vel.x, _vel.y, _vel.z);
    }
}

void SoundLibraryWebAssembly::SetListenerPosition(Vector3<float> const &_pos, Vector3<float> const &_front, 
                                                   Vector3<float> const &_up, Vector3<float> const &_vel)
{
    m_listenerPosition = _pos;
    m_listenerFront = _front;
    m_listenerUp = _up;
    
    if (m_initialized)
    {
        jsSetListenerPosition(_pos.x, _pos.y, _pos.z);
        jsSetListenerOrientation(_front.x, _front.y, _front.z, _up.x, _up.y, _up.z);
    }
}

void SoundLibraryWebAssembly::SetChannel3DMode(int _channel, int _mode)
{
    if (_channel < 0 || _channel >= m_numChannels)
    {
        return;
    }
    
    m_channels[_channel].is3D = (_mode != 0);
    
    if (m_initialized && m_channels[_channel].active)
    {
        jsSetChannel3DMode(_channel, m_channels[_channel].is3D);
    }
}

void SoundLibraryWebAssembly::SetChannelFrequency(int _channel, int _frequency)
{
    if (_channel < 0 || _channel >= m_numChannels)
    {
        return;
    }
    
    m_channels[_channel].frequency = (float)_frequency;  // Convert int to float for internal use
    
    if (m_initialized && m_channels[_channel].active)
    {
        jsSetChannelFrequency(_channel, (float)_frequency);
    }
}

void SoundLibraryWebAssembly::SetChannelMinDistance(int _channel, float _minDistance)
{
    if (_channel < 0 || _channel >= m_numChannels)
    {
        return;
    }
    
    m_channels[_channel].minDistance = _minDistance;
    
    if (m_initialized && m_channels[_channel].active)
    {
        jsSetChannelMinDistance(_channel, _minDistance);
    }
}

void SoundLibraryWebAssembly::ResetChannel(int _channel)
{
    if (_channel < 0 || _channel >= m_numChannels)
    {
        return;
    }

    // Stop any existing playback on this channel
    StopChannel(_channel);
    CleanupChannel(_channel);
    
    // This is the key function that DEFCON calls to start playing sounds!
    // We need to set up continuous audio streaming for this channel
    if (m_initialized && m_mainCallback)
    {
        m_channels[_channel].active = true;
        m_channels[_channel].startTime = GetHighResTime();
        
        // Start continuous audio streaming for this channel using Web Audio API ScriptProcessorNode
        jsStartContinuousAudioStream(_channel);
    }
}

void SoundLibraryWebAssembly::StopChannel(int _channel)
{
    if (_channel < 0 || _channel >= m_numChannels)
    {
        return;
    }
    
    if (m_channels[_channel].active)
    {
        // Stop the continuous audio stream
        jsStopContinuousAudioStream(_channel);
        
        // Stop any JavaScript audio playback
        jsStopChannelPlayback(_channel);
        
        // Clean up the channel
        CleanupChannel(_channel);
        
        //AppDebugOut("SoundLibraryWebAssembly: Stopped channel %d\n", _channel);
    }
}

float SoundLibraryWebAssembly::GetChannelHealth(int _channel)
{
    if (_channel < 0 || _channel >= m_numChannels)
    {
        return 0.0f;
    }
    
    if (!m_channels[_channel].active)
    {
        return 0.0f;
    }
    
    // Return progress of playback (0.0 = just started, 1.0 = finished)
    if (m_initialized)
    {
        return jsGetChannelProgress(_channel);
    }
    
    return 0.5f; // Default middle value
}

int SoundLibraryWebAssembly::FindFreeChannel(bool _music)
{
    int startChannel = _music ? (m_numChannels - m_numMusicChannels) : 0;
    int endChannel = _music ? m_numChannels : (m_numChannels - m_numMusicChannels);
    
    // Find a completely free channel first
    for (int i = startChannel; i < endChannel; i++)
    {
        if (!m_channels[i].active)
        {
            return i;
        }
    }
    
    // If no free channel, find the oldest one
    int oldestChannel = startChannel;
    double oldestTime = m_channels[startChannel].startTime;
    
    for (int i = startChannel; i < endChannel; i++)
    {
        if (m_channels[i].startTime < oldestTime)
        {
            oldestTime = m_channels[i].startTime;
            oldestChannel = i;
        }
    }
    
    // Stop the oldest channel to make room
    StopChannel(oldestChannel);
    return oldestChannel;
}

void SoundLibraryWebAssembly::CleanupChannel(int _channel)
{
    if (_channel < 0 || _channel >= m_numChannels)
    {
        return;
    }
    
    WebAudioChannel& channel = m_channels[_channel];
    
    // Reset all channel properties to defaults
    channel.sourceNode = nullptr;
    channel.pannerNode = nullptr;
    channel.gainNode = nullptr;
    channel.audioBuffer = nullptr;
    channel.active = false;
    channel.is3D = true;
    channel.isMusic = false;
    channel.looping = false;
    channel.volume = 1.0f;
    channel.frequency = 1.0f;
    channel.position = Vector3<float>(0, 0, 0);
    channel.velocity = Vector3<float>(0, 0, 0);
    channel.minDistance = 100.0f;
    channel.currentSample[0] = '\0';
    channel.startTime = 0.0;
    channel.stopping = false;
}

bool SoundLibraryWebAssembly::LoadAudioAsset(const char* _filename)
{
    if (!_filename || strlen(_filename) == 0) {
        return false;
    }
    
    // Check if already loaded or loading
    if (IsAudioAssetLoaded(_filename) || jsIsAssetLoading(_filename)) {
        return true;
    }
    
    // Try to load the file from Emscripten's preloaded filesystem
    EM_ASM({
        var filename = UTF8ToString($0);
        
        try {
            var data = FS.readFile(filename);
            
            if (data && data.length > 0) {
                var dataPtr = _malloc(data.length);
                HEAPU8.set(data, dataPtr);
                Module._jsLoadAudioAsset(filename, dataPtr, data.length);
                _free(dataPtr);
                console.log("Loading audio file: " + filename + " (" + data.length + " bytes)");
            } else {
                console.warn("Audio file is empty: " + filename);
            }
        } catch (error) {
            console.warn("Could not load audio file: " + filename + " - " + error.message);
            
            // Try alternative paths
            var altPath1 = "data/" + filename;
            var altPath2 = "sounds/" + filename;
            var altPath3 = "data/sounds/" + filename;
            
            var loaded = false;
            
            if (!loaded) {
                try {
                    var altData1 = FS.readFile(altPath1);
                    if (altData1 && altData1.length > 0) {
                        var altDataPtr1 = _malloc(altData1.length);
                        HEAPU8.set(altData1, altDataPtr1);
                        Module._jsLoadAudioAsset(filename, altDataPtr1, altData1.length);
                        _free(altDataPtr1);
                        console.log("Loaded audio file from: " + altPath1 + " (" + altData1.length + " bytes)");
                        loaded = true;
                    }
                } catch (altError1) {
                    // Continue to next path
                }
            }
            
            if (!loaded) {
                try {
                    var altData2 = FS.readFile(altPath2);
                    if (altData2 && altData2.length > 0) {
                        var altDataPtr2 = _malloc(altData2.length);
                        HEAPU8.set(altData2, altDataPtr2);
                        Module._jsLoadAudioAsset(filename, altDataPtr2, altData2.length);
                        _free(altDataPtr2);
                        console.log("Loaded audio file from: " + altPath2 + " (" + altData2.length + " bytes)");
                        loaded = true;
                    }
                } catch (altError2) {
                    // Continue to next path
                }
            }
            
            if (!loaded) {
                try {
                    var altData3 = FS.readFile(altPath3);
                    if (altData3 && altData3.length > 0) {
                        var altDataPtr3 = _malloc(altData3.length);
                        HEAPU8.set(altData3, altDataPtr3);
                        Module._jsLoadAudioAsset(filename, altDataPtr3, altData3.length);
                        _free(altDataPtr3);
                        console.log("Loaded audio file from: " + altPath3 + " (" + altData3.length + " bytes)");
                        loaded = true;
                    }
                } catch (altError3) {
                    // All paths failed
                }
            }
            
            if (!loaded) {
                console.error("Failed to load audio file from any path: " + filename);
            }
        }
    }, _filename);
    
    return true;
}

bool SoundLibraryWebAssembly::IsAudioAssetLoaded(const char* _filename)
{
    return jsIsAssetLoaded(_filename);
}

bool SoundLibraryWebAssembly::StartChannelPlayback(int _channel, const char* _sampleName, bool _looped)
{
    if (_channel < 0 || _channel >= m_numChannels || !_sampleName) {
        return false;
    }
    
    // First, try to load the asset if it's not already loaded
    if (!IsAudioAssetLoaded(_sampleName)) {
        LoadAudioAsset(_sampleName);
    }
    
    // If the asset is loaded, use asset-based playback
    if (IsAudioAssetLoaded(_sampleName)) {
        jsStartChannelPlaybackWithAsset(_channel, _sampleName, _looped, 
                                       m_channels[_channel].volume, 
                                       m_channels[_channel].frequency);
        
        m_channels[_channel].active = true;
        m_channels[_channel].looping = _looped;
        strcpy(m_channels[_channel].currentSample, _sampleName);
        m_channels[_channel].startTime = GetHighResTime();
        
        return true;
    }
    
    return false;
}

SoundLibraryWebAssembly::AudioAsset* SoundLibraryWebAssembly::GetAudioAsset(const char* _filename)
{
    int index = m_audioAssets.GetIndex(_filename);
    if (m_audioAssets.ValidIndex(index))
    {
        return m_audioAssets[index];
    }
    return nullptr;
}

bool SoundLibraryWebAssembly::RequiresUserInteraction()
{
    return m_requiresUserInteraction || m_contextSuspended;
}

void SoundLibraryWebAssembly::TriggerAudioUnlock()
{
    if (m_initialized && RequiresUserInteraction())
    {
#ifdef EMSCRIPTEN_SOUND_TESTBED
        AppDebugOut("SoundLibraryWebAssembly: Triggering audio unlock for mobile browsers\n");
#endif
        jsTriggerAudioUnlock();
        
        // Update state
        m_contextSuspended = jsIsAudioContextSuspended();
        m_requiresUserInteraction = jsRequiresUserInteraction();
    }
}

bool SoundLibraryWebAssembly::IsContextSuspended()
{
    if (m_initialized)
    {
        m_contextSuspended = jsIsAudioContextSuspended();
    }
    return m_contextSuspended;
}

void SoundLibraryWebAssembly::ResumeAudioContext()
{
    if (m_initialized && m_contextSuspended)
    {
#ifdef EMSCRIPTEN_SOUND_TESTBED
        AppDebugOut("SoundLibraryWebAssembly: Resuming audio context\n");
#endif
        jsResumeAudioContext();
        m_contextSuspended = jsIsAudioContextSuspended();
    }
}

// Debug and Statistics Methods
void SoundLibraryWebAssembly::LogChannelStatus(int _channel)
{
    if (_channel < 0 || _channel >= m_numChannels)
    {
        return;
    }
    
    WebAudioChannel& channel = m_channels[_channel];
#ifdef EMSCRIPTEN_SOUND_TESTBED
    AppDebugOut("Channel %d: Active=%d, Sample='%s', Volume=%.2f, 3D=%d, Music=%d\n",
                _channel, channel.active, channel.currentSample, 
                channel.volume, channel.is3D, channel.isMusic);
#endif
}

void SoundLibraryWebAssembly::LogSystemStatus()
{
#ifdef EMSCRIPTEN_SOUND_TESTBED
    AppDebugOut("=== WebAssembly Sound System Status ===\n");
    AppDebugOut("Initialized: %d\n", m_initialized);
    AppDebugOut("Context Suspended: %d\n", m_contextSuspended);
    AppDebugOut("Requires User Interaction: %d\n", m_requiresUserInteraction);
    AppDebugOut("Master Volume: %.2f\n", m_masterVolume);
    AppDebugOut("Total Channels: %d\n", m_numChannels);
    AppDebugOut("Music Channels: %d\n", m_numMusicChannels);
    AppDebugOut("Active Channels: %d\n", GetActiveChannelCount());
    AppDebugOut("Loaded Assets: %d\n", GetLoadedAssetCount());
#endif
    
    if (m_initialized)
    {
        jsLogAudioStats();
    }
}

int SoundLibraryWebAssembly::GetActiveChannelCount()
{
    int count = 0;
    for (int i = 0; i < m_numChannels; i++)
    {
        if (m_channels[i].active)
        {
            count++;
        }
    }
    return count;
}

int SoundLibraryWebAssembly::GetLoadedAssetCount()
{
    int count = 0;
    for (int i = 0; i < m_audioAssets.Size(); i++)
    {
        if (m_audioAssets.ValidIndex(i))
        {
            AudioAsset* asset = m_audioAssets[i];
            if (asset && asset->loaded)
            {
                count++;
            }
        }
    }
    return count;
}

float SoundLibraryWebAssembly::GetTotalMemoryUsage()
{
    if (m_initialized)
    {
        return jsGetAudioMemoryUsage();
    }
    return 0.0f;
}

// Required Pure Virtual Function Implementations
int SoundLibraryWebAssembly::GetChannelBufSize(int _channel) const
{
    if (_channel < 0 || _channel >= m_numChannels)
    {
        return 0;
    }
    
    // Web Audio API doesn't use traditional buffer sizes like DirectSound
    // Return a reasonable default for compatibility
    return 4096;
}

float SoundLibraryWebAssembly::GetChannelVolume(int _channel) const
{
    if (_channel < 0 || _channel >= m_numChannels)
    {
        return 0.0f;
    }
    
    return m_channels[_channel].volume;
}

float SoundLibraryWebAssembly::GetChannelRelFreq(int _channel) const
{
    if (_channel < 0 || _channel >= m_numChannels)
    {
        return 1.0f;
    }
    
    return m_channels[_channel].frequency;
}

void SoundLibraryWebAssembly::EnableDspFX(int _channel, int _numFilters, int const *_filterTypes)
{
    if (!m_initialized || _channel < 0 || _channel >= m_numChannels)
        return;
        
    #ifdef EMSCRIPTEN_SOUND_TESTBED
        AppDebugOut("SoundLibraryWebAssembly: Enabling DSP Effects for channel %d (%d filters)\n",
                    _channel, _numFilters);
#endif
    
    // Store filter configuration for this channel
    m_channels[_channel].numDspFilters = _numFilters;
    for (int i = 0; i < _numFilters && i < MAX_DSP_FILTERS; i++)
    {
        m_channels[_channel].dspFilterTypes[i] = _filterTypes[i];
    }
    
    // Create Web Audio API DSP chain for this channel
    jsCreateDspChain(_channel, _numFilters, _filterTypes);
}

void SoundLibraryWebAssembly::UpdateDspFX(int _channel, int _filterType, int _numParams, float const *_params)
{
    if (!m_initialized || _channel < 0 || _channel >= m_numChannels)
        return;
        
    #ifdef EMSCRIPTEN_SOUND_TESTBED
        AppDebugOut("SoundLibraryWebAssembly: Updating DSP Effect for channel %d, filter %d\n",
                    _channel, _filterType);
#endif
    
    // Update Web Audio API DSP parameters
    jsUpdateDspFilter(_channel, _filterType, _numParams, _params);
}

void SoundLibraryWebAssembly::DisableDspFX(int _channel)
{
    if (!m_initialized || _channel < 0 || _channel >= m_numChannels)
        return;
        
    #ifdef EMSCRIPTEN_SOUND_TESTBED
        AppDebugOut("SoundLibraryWebAssembly: Disabling DSP Effects for channel %d\n", _channel);
#endif
    
    // Clear filter configuration
    m_channels[_channel].numDspFilters = 0;
    
    // Remove Web Audio API DSP chain
    jsRemoveDspChain(_channel);
}

void SoundLibraryWebAssembly::Advance()
{
    if (!m_initialized)
    {
        return;
    }
    
    // Check if audio context needs to be resumed (mobile browsers)
    if (m_contextSuspended)
    {
        m_contextSuspended = jsIsAudioContextSuspended();
    }
    
    // Update channel states and cleanup finished sounds
    for (int i = 0; i < m_numChannels; i++)
    {
        if (m_channels[i].active)
        {
            // Check if channel is still playing
            if (m_initialized && !jsIsChannelPlaying(i))
            {
                // Channel finished playing, clean it up
                CleanupChannel(i);
            }
        }
    }
}

//*****************************************************************************
// JavaScript Bridge Function Stubs
// These will be implemented with actual JavaScript code in the next phase
//*****************************************************************************

extern "C" {

// Placeholder implementations - these will call actual JavaScript in Phase 8.2
void jsCreateAudioContext() 
{
    EM_ASM({
        if (typeof window.defconAudioContext === 'undefined') {
            try {
                window.defconAudioContext = new (window.AudioContext || window.webkitAudioContext)();
                window.defconMasterGain = window.defconAudioContext.createGain();
                window.defconMasterGain.connect(window.defconAudioContext.destination);
                window.defconChannels = [];
                for (var i = 0; i < 64; i++) {
                    var channel = {};
                    channel.source = null;
                    channel.gainNode = null;
                    channel.pannerNode = null;
                    channel.active = false;
                    window.defconChannels[i] = channel;
                }
                window.defconAudioBuffers = {};
                
                // Auto-resume audio context when suspended
                if (window.defconAudioContext.state === 'suspended') {
                    console.log("Audio context suspended - attempting auto-resume");
                    window.defconAudioContext.resume();
                }
                
                // Set up automatic audio context resumption on user interaction
                window.defconAutoResumeAudio = true;
                
            } catch (e) {
                console.error("Failed to create Web Audio API context:", e);
                window.defconAudioContext = null;
            }
        }
    });
}

bool jsIsAudioContextAvailable() 
{
    return EM_ASM_INT({
        return (window.defconAudioContext !== null && window.defconAudioContext !== undefined) ? 1 : 0;
    });
}

bool jsIsAudioContextSuspended() 
{
    return EM_ASM_INT({
        if (window.defconAudioContext) {
            return (window.defconAudioContext.state === 'suspended') ? 1 : 0;
        }
        return 1;
    });
}

void jsResumeAudioContext() 
{
    EM_ASM({
        if (window.defconAudioContext && window.defconAudioContext.state === 'suspended') {
            window.defconAudioContext.resume().then(function() {
                console.log("Audio context resumed successfully");
            }).catch(function(e) {
                console.error("Failed to resume audio context:", e);
            });
        }
    });
}

void jsSetMasterVolume(float volume) 
{
    EM_ASM({
        if (window.defconMasterGain) {
            window.defconMasterGain.gain.setValueAtTime($0, window.defconAudioContext.currentTime);
        }
    }, volume);
}

void jsSetListenerPosition(float x, float y, float z) 
{
    EM_ASM({
        // Set 3D listener position (silent operation)
    }, x, y, z);
}

void jsSetListenerOrientation(float fx, float fy, float fz, float ux, float uy, float uz) 
{
    EM_ASM({
        // Set 3D listener orientation (silent operation)
    });
}

void jsLoadAudioAsset(const char* filename, const char* data, int dataSize) 
{
    EM_ASM({
        var filename = UTF8ToString($0);
        var dataPtr = $1;
        var dataSize = $2;
        
        if (!window.defconAudioAssets) {
            window.defconAudioAssets = {};
        }
        
        // Copy the OGG file data from WebAssembly memory
        var oggData = new Uint8Array(dataSize);
        for (var i = 0; i < dataSize; i++) {
            oggData[i] = HEAPU8[dataPtr + i];
        }
        
        // Decode OGG Vorbis using Web Audio API
        if (window.defconAudioContext) {
            window.defconAudioContext.decodeAudioData(oggData.buffer)
                .then(function(audioBuffer) {
                    window.defconAudioAssets[filename] = {
                        buffer: audioBuffer,
                        loaded: true,
                        duration: audioBuffer.duration,
                        sampleRate: audioBuffer.sampleRate,
                        channels: audioBuffer.numberOfChannels
                    };
                    console.log("Loaded audio asset: " + filename + " (" + audioBuffer.duration.toFixed(2) + "s)");
                })
                .catch(function(error) {
                    console.error("Failed to decode audio asset: " + filename, error);
                    window.defconAudioAssets[filename] = {
                        buffer: null,
                        loaded: false,
                        error: error.message
                    };
                });
        }
    }, filename, data, dataSize);
}

bool jsIsAssetLoaded(const char* filename) 
{
    return EM_ASM_INT({
        var filename = UTF8ToString($0);
        if (window.defconAudioAssets && window.defconAudioAssets[filename]) {
            return window.defconAudioAssets[filename].loaded ? 1 : 0;
        }
        return 0;
    }, filename);
}

bool jsIsAssetLoading(const char* filename) 
{
    return EM_ASM_INT({
        var filename = UTF8ToString($0);
        if (window.defconAudioAssets && window.defconAudioAssets[filename]) {
            return (!window.defconAudioAssets[filename].loaded && !window.defconAudioAssets[filename].error) ? 1 : 0;
        }
        return 0;
    }, filename);
}

void jsUnloadAsset(const char* filename) 
{
    EM_ASM({
        // Unload audio asset (placeholder for future implementation)
    }, filename);
}

void jsStartChannelPlayback(int channelId, const char* filename, bool loop, float volume, float frequency) 
{
    EM_ASM({
        if (!window.defconAudioContext) {
            return;
        }
        if (window.defconChannels[$0] && window.defconChannels[$0].source) {
            try {
                window.defconChannels[$0].source.stop();
            } catch (e) {
            }
        }
        try {
            var oscillator = window.defconAudioContext.createOscillator();
            var gainNode = window.defconAudioContext.createGain();
            var freq = 440;
            var filename = UTF8ToString($1);
            
            // Check if this is real game audio data
            if (filename === 'realdata') {
                // We have actual game audio data! Use a different frequency to indicate this
                freq = 1000; // Higher frequency to distinguish from test beeps
            } else if (filename.indexOf('explosion') !== -1) {
                freq = 200;
            } else if (filename.indexOf('click') !== -1) {
                freq = 800;
            } else if (filename.indexOf('beep') !== -1) {
                freq = 600;
            }
            
            oscillator.frequency.setValueAtTime(freq, window.defconAudioContext.currentTime);
            oscillator.type = 'sine';
            gainNode.gain.setValueAtTime($3, window.defconAudioContext.currentTime);
            oscillator.connect(gainNode);
            gainNode.connect(window.defconMasterGain);
            var channel = {};
            channel.source = oscillator;
            channel.gainNode = gainNode;
            channel.pannerNode = null;
            channel.active = true;
            window.defconChannels[$0] = channel;
            oscillator.start();
            
            // Real game audio gets longer duration
            var duration = filename === 'realdata' ? 0.5 : 0.2;
            oscillator.stop(window.defconAudioContext.currentTime + duration);
            
            var channelId = $0;
            oscillator.onended = function() {
                window.defconChannels[channelId].active = false;
                window.defconChannels[channelId].source = null;
            };
        } catch (e) {
            console.error("Failed to start playback:", e);
        }
    }, channelId, filename, loop, volume, frequency);
}

void jsStopChannelPlayback(int channelId) 
{
    EM_ASM({
        // Stop channel playback (silent operation)
    }, channelId);
}

bool jsIsChannelPlaying(int channelId) 
{
    return EM_ASM_INT({
        if (window.defconChannels && window.defconChannels[$0]) {
            return window.defconChannels[$0].active ? 1 : 0;
        }
        return 0;
    }, channelId);
}

float jsGetChannelProgress(int channelId) 
{
    return EM_ASM_DOUBLE({
        return 0.5; // Default progress value
    }, channelId);
}

void jsSetChannelVolume(int channelId, float volume) 
{
    EM_ASM({
        // Set individual channel volume (placeholder for future implementation)
    }, channelId, volume);
}

void jsSetChannelFrequency(int channelId, float frequency) 
{
    EM_ASM({
        // Set channel frequency (placeholder for future implementation)
    }, channelId, frequency);
}

void jsSetChannelPosition(int channelId, float x, float y, float z) 
{
    EM_ASM({
        // Set 3D channel position (placeholder for future implementation)
    }, channelId, x, y, z);
}

void jsSetChannelVelocity(int channelId, float vx, float vy, float vz) 
{
    EM_ASM({
        // Set 3D channel velocity (placeholder for future implementation)
    }, channelId, vx, vy, vz);
}

void jsSetChannelMinDistance(int channelId, float minDistance) 
{
    EM_ASM({
        // Set 3D channel minimum distance (placeholder for future implementation)
    }, channelId, minDistance);
}

void jsSetChannel3DMode(int channelId, bool enabled) 
{
    EM_ASM({
        // Set channel 3D mode (placeholder for future implementation)
    }, channelId, enabled);
}

bool jsRequiresUserInteraction() 
{
    return EM_ASM_INT({
        if (window.defconAudioContext) {
            return (window.defconAudioContext.state === 'suspended') ? 1 : 0;
        }
        return 1;
    });
}

void jsTriggerAudioUnlock() 
{
    EM_ASM({
        if (window.defconAudioContext && window.defconAudioContext.state === 'suspended') {
            var buffer = window.defconAudioContext.createBuffer(1, 1, 22050);
            var source = window.defconAudioContext.createBufferSource();
            source.buffer = buffer;
            // Connect through master gain instead of directly to destination
            if (window.defconMasterGain) {
                source.connect(window.defconMasterGain);
            } else {
                source.connect(window.defconAudioContext.destination);
            }
            source.start();
            window.defconAudioContext.resume().then(function() {
                console.log("Audio unlocked successfully");
            }).catch(function(e) {
                console.error("Failed to unlock audio:", e);
            });
        }
    });
}

void jsLogAudioStats() 
{
    EM_ASM({
        console.log("=== WebAssembly Audio Statistics ===");
        console.log("Audio context available: " + (typeof AudioContext !== 'undefined'));
    });
}

int jsGetActiveSourceCount() 
{
    return 0;
}

float jsGetAudioMemoryUsage() 
{
    return 0.0f;
}

void jsStartChannelPlaybackWithPCMData(int channelId, signed short* pcmData, int numSamples, bool loop, float volume, float frequency) 
{
    EM_ASM({
        if (!window.defconAudioContext) {
            return;
        }
        
        // Auto-resume audio context if suspended
        if (window.defconAudioContext.state === 'suspended') {
            window.defconAudioContext.resume();
        }
        
        var channelId = $0;
        var pcmDataPtr = $1;
        var numSamples = $2;
        var loop = $3;
        var volume = $4;
        var frequency = $5;
        
        // Stop any existing playback on this channel
        if (window.defconChannels[channelId] && window.defconChannels[channelId].source) {
            try {
                window.defconChannels[channelId].source.stop();
            } catch (e) {}
        }
        
        try {
            // Create AudioBuffer from PCM data
            var audioBuffer = window.defconAudioContext.createBuffer(1, numSamples, frequency);
            var channelData = audioBuffer.getChannelData(0);
            
            // Convert signed short PCM to float32 (-1.0 to 1.0)
            for (var i = 0; i < numSamples; i++) {
                var sample = HEAP16[(pcmDataPtr >> 1) + i]; // Read signed short
                channelData[i] = sample / 32768.0; // Convert to float32
            }
            
            // Create audio source
            var source = window.defconAudioContext.createBufferSource();
            source.buffer = audioBuffer;
            source.loop = loop;
            
            // Create gain node for volume control
            var gainNode = window.defconAudioContext.createGain();
            gainNode.gain.setValueAtTime(volume, window.defconAudioContext.currentTime);
            
            // Connect audio graph
            source.connect(gainNode);
            gainNode.connect(window.defconMasterGain);
            
            // Start playback
            source.start();
            
            // Store channel info
            var channel = {};
            channel.source = source;
            channel.gainNode = gainNode;
            channel.pannerNode = null;
            channel.active = true;
            window.defconChannels[channelId] = channel;
            
        } catch (error) {
            console.error("Error playing PCM audio on channel " + channelId + ":", error);
        }
        
    }, channelId, pcmData, numSamples, loop, volume, frequency);
}

void jsStartChannelPlaybackWithAsset(int channelId, const char* filename, bool loop, float volume, float frequency) 
{
    EM_ASM({
        if (!window.defconAudioContext) {
            return;
        }
        
        // Auto-resume audio context if suspended
        if (window.defconAudioContext.state === 'suspended') {
            window.defconAudioContext.resume();
        }
        
        var channelId = $0;
        var filename = UTF8ToString($1);
        var loop = $2;
        var volume = $3;
        var frequency = $4;
        
        // Check if asset is loaded
        if (!window.defconAudioAssets || !window.defconAudioAssets[filename] || !window.defconAudioAssets[filename].loaded) {
            console.warn("Audio asset not loaded: " + filename);
            return;
        }
        
        // Stop any existing playback on this channel
        if (window.defconChannels[channelId] && window.defconChannels[channelId].source) {
            try {
                window.defconChannels[channelId].source.stop();
            } catch (e) {}
        }
        
        try {
            var audioBuffer = window.defconAudioAssets[filename].buffer;
            
            // Create audio source
            var source = window.defconAudioContext.createBufferSource();
            source.buffer = audioBuffer;
            source.loop = loop;
            
            // Apply frequency adjustment if needed
            if (frequency !== audioBuffer.sampleRate) {
                source.playbackRate.setValueAtTime(frequency / audioBuffer.sampleRate, window.defconAudioContext.currentTime);
            }
            
            // Create gain node for volume control
            var gainNode = window.defconAudioContext.createGain();
            gainNode.gain.setValueAtTime(volume, window.defconAudioContext.currentTime);
            
            // Connect audio graph
            source.connect(gainNode);
            gainNode.connect(window.defconMasterGain);
            
            // Start playback
            source.start();
            
            // Store channel info
            var channel = {};
            channel.source = source;
            channel.gainNode = gainNode;
            channel.pannerNode = null;
            channel.active = true;
            window.defconChannels[channelId] = channel;
            
            console.log("Playing audio asset: " + filename + " on channel " + channelId);
            
        } catch (error) {
            console.error("Error playing audio asset " + filename + " on channel " + channelId + ":", error);
        }
        
    }, channelId, filename, loop, volume, frequency);
}

void jsStartContinuousAudioStream(int channelId) 
{
    EM_ASM({
        if (!window.defconAudioContext) {
            return;
        }
        
        // Auto-resume audio context if suspended
        if (window.defconAudioContext.state === 'suspended') {
            window.defconAudioContext.resume();
        }
        
        var channelId = $0;
        
        // Stop any existing stream on this channel
        if (window.defconChannels[channelId] && window.defconChannels[channelId].processor) {
            window.defconChannels[channelId].processor.disconnect();
            window.defconChannels[channelId].processor = null;
        }
        
        // Initialize channel if needed
        if (!window.defconChannels[channelId]) {
            var channel = {};
            channel.source = null;
            channel.gainNode = null;
            channel.pannerNode = null;
            channel.active = false;
            channel.processor = null;
            window.defconChannels[channelId] = channel;
        }
        
        try {
            // Create a ScriptProcessorNode for continuous audio generation
            // Buffer size: 4096 samples = ~93ms at 44.1kHz (good balance of latency vs performance)
            var bufferSize = 4096;
            var processor = window.defconAudioContext.createScriptProcessor(bufferSize, 0, 2); // 0 inputs, 2 outputs (stereo)
            
            // Create gain node for volume control
            var gainNode = window.defconAudioContext.createGain();
            gainNode.gain.setValueAtTime(1.0, window.defconAudioContext.currentTime);
            
            // Set up the audio processing callback
            processor.onaudioprocess = function(audioProcessingEvent) {
                if (!window.defconChannels[channelId] || !window.defconChannels[channelId].active) {
                    // Channel is inactive - fill with silence
                    var outputBuffer = audioProcessingEvent.outputBuffer;
                    var leftChannel = outputBuffer.getChannelData(0);
                    var rightChannel = outputBuffer.getChannelData(1);
                    
                    for (var i = 0; i < bufferSize; i++) {
                        leftChannel[i] = 0;
                        rightChannel[i] = 0;
                    }
                    return;
                }
                
                // Get audio data from DEFCON's sound system
                var audioDataPtr = Module._jsRequestAudioDataForProcessor(channelId, bufferSize);
                
                if (audioDataPtr !== 0) {
                    // We got audio data! Convert and copy to output buffer
                    var outputBuffer = audioProcessingEvent.outputBuffer;
                    var leftChannel = outputBuffer.getChannelData(0);
                    var rightChannel = outputBuffer.getChannelData(1);
                    
                    // Convert signed short PCM to float32 and write to Web Audio API buffer
                    for (var i = 0; i < bufferSize; i++) {
                        // Read stereo samples (left, right, left, right, ...)
                        var leftSample = HEAP16[(audioDataPtr >> 1) + (i * 2)];     // Left channel
                        var rightSample = HEAP16[(audioDataPtr >> 1) + (i * 2) + 1]; // Right channel
                        
                        // Convert from signed short (-32768 to 32767) to float (-1.0 to 1.0)
                        leftChannel[i] = leftSample / 32768.0;
                        rightChannel[i] = rightSample / 32768.0;
                    }
                    
                    // Free the audio data memory
                    Module._jsFreeAudioData(audioDataPtr);
                } else {
                    // No audio data - fill with silence
                    var outputBuffer = audioProcessingEvent.outputBuffer;
                    var leftChannel = outputBuffer.getChannelData(0);
                    var rightChannel = outputBuffer.getChannelData(1);
                    
                    for (var i = 0; i < bufferSize; i++) {
                        leftChannel[i] = 0;
                        rightChannel[i] = 0;
                    }
                }
            };
            
            // Store references first
            window.defconChannels[channelId].processor = processor;
            window.defconChannels[channelId].gainNode = gainNode;
            window.defconChannels[channelId].source = processor; // For DSP chain compatibility
            window.defconChannels[channelId].active = true;
            
            // Connect the audio graph with DSP chain integration
            // This will handle: processor -> [DSP chain] -> gain -> master gain -> destination
            Module._jsConnectDspChain(channelId);
            
            //console.log("Started continuous audio stream for channel " + channelId + " using ScriptProcessorNode");
            
        } catch (error) {
            console.error("Error starting continuous audio stream for channel " + channelId + ":", error);
        }
        
    }, channelId);
}

void jsStopContinuousAudioStream(int channelId) 
{
    EM_ASM({
        var channelId = $0;
        
        if (window.defconChannels[channelId]) {
            // Stop the audio processor
            if (window.defconChannels[channelId].processor) {
                window.defconChannels[channelId].processor.disconnect();
                window.defconChannels[channelId].processor = null;
            }
            
            // Clean up gain node
            if (window.defconChannels[channelId].gainNode) {
                window.defconChannels[channelId].gainNode.disconnect();
                window.defconChannels[channelId].gainNode = null;
            }
            
            window.defconChannels[channelId].active = false;
            //console.log("Stopped continuous audio stream for channel " + channelId);
        }
        
    }, channelId);
}

// New function called by JavaScript ScriptProcessorNode to get audio data
extern "C" EMSCRIPTEN_KEEPALIVE signed short* jsRequestAudioDataForProcessor(int channelId, int bufferSize)
{
    if (!g_soundLibrary3d) {
        return nullptr;
    }
    
    SoundLibraryWebAssembly* webAudioLib = static_cast<SoundLibraryWebAssembly*>(g_soundLibrary3d);
    if (webAudioLib) {
        return webAudioLib->RequestAudioDataForProcessor(channelId, bufferSize);
    }
    
    return nullptr;
}

signed short* SoundLibraryWebAssembly::RequestAudioDataForProcessor(int channelId, int bufferSize)
{
    if (channelId < 0 || channelId >= m_numChannels) {
        return nullptr;
    }
    
    if (!m_mainCallback) {
        return nullptr;
    }
    
    // Allocate audio buffer (stereo)
    signed short* audioBuffer = new signed short[bufferSize * 2];
    int silenceRemaining = 0;
    
    // Call DEFCON's audio callback to fill the buffer
    bool hasAudio = m_mainCallback(channelId, audioBuffer, bufferSize * 2, &silenceRemaining);
    
    if (hasAudio) {
        // Audio files are pre-converted to correct sample rates - no real-time resampling needed!
        // This greatly improves performance and audio quality
        return audioBuffer;
    } else {
        // No audio data - check if channel should stop
        if (silenceRemaining >= bufferSize) {
            // Channel is done playing
            StopChannel(channelId);
            jsStopContinuousAudioStream(channelId);
        }
        
        // Clean up and return null
        delete[] audioBuffer;
        return nullptr;
    }
}

// Function to free audio data allocated by C++
extern "C" EMSCRIPTEN_KEEPALIVE void jsFreeAudioData(signed short* audioData)
{
    if (audioData) {
        free(audioData);
    }
}

//*****************************************************************************
// DSP Effects JavaScript Bridge Functions (NEW)
//*****************************************************************************

extern "C" EMSCRIPTEN_KEEPALIVE void jsCreateDspChain(int channelId, int numFilters, const int* filterTypes) 
{
    EM_ASM({
        var channelId = $0;
        var numFilters = $1;
        var filterTypesPtr = $2;
        
        if (!window.defconChannels || !window.defconChannels[channelId]) {
            console.error("Cannot create DSP chain - channel not initialized:", channelId);
            return;
        }
        
        var channel = window.defconChannels[channelId];
        
        // Create DSP chain array if it doesn't exist
        if (!channel.dspChain) {
            channel.dspChain = [];
        }
        
        // Clear existing DSP chain
        channel.dspChain.forEach(function(node) {
            if (node && node.disconnect) {
                node.disconnect();
            }
        });
        channel.dspChain = [];
        
        // Create new DSP filters based on types
        for (var i = 0; i < numFilters; i++) {
            var filterType = HEAP32[(filterTypesPtr >> 2) + i];
            var dspNode = null;
            
            switch (filterType) {
                case 10: // DSP_RESONANTLOWPASS
                    dspNode = window.defconAudioContext.createBiquadFilter();
                    dspNode.type = 'lowpass';
                    dspNode.frequency.setValueAtTime(2500, window.defconAudioContext.currentTime);
                    dspNode.Q.setValueAtTime(7, window.defconAudioContext.currentTime);
                    break;
                    
                case 11: // DSP_BITCRUSHER
                    // Web Audio API doesn't have native bit crusher, use waveshaper
                    dspNode = window.defconAudioContext.createWaveShaper();
                    var samples = 4096;
                    var curve = new Float32Array(samples);
                    var bitRate = 16;
                    var step = Math.pow(2, bitRate);
                    for (var j = 0; j < samples; j++) {
                        var x = (j * 2) / samples - 1;
                        curve[j] = Math.round(x * step) / step;
                    }
                    dspNode.curve = curve;
                    break;
                    
                case 12: // DSP_GARGLE
                case 5:  // DSP_DSOUND_GARGLE
                    // Create tremolo effect using gain modulation
                    dspNode = window.defconAudioContext.createGain();
                    var lfo = window.defconAudioContext.createOscillator();
                    var lfoGain = window.defconAudioContext.createGain();
                    lfo.frequency.setValueAtTime(20, window.defconAudioContext.currentTime);
                    lfoGain.gain.setValueAtTime(0.5, window.defconAudioContext.currentTime);
                    lfo.connect(lfoGain);
                    lfoGain.connect(dspNode.gain);
                    lfo.start();
                    dspNode._lfo = lfo; // Keep reference
                    break;
                    
                case 13: // DSP_ECHO
                case 3:  // DSP_DSOUND_ECHO
                    dspNode = window.defconAudioContext.createDelay(3.0);
                    dspNode.delayTime.setValueAtTime(0.2, window.defconAudioContext.currentTime);
                    var feedback = window.defconAudioContext.createGain();
                    var wetGain = window.defconAudioContext.createGain();
                    feedback.gain.setValueAtTime(0.5, window.defconAudioContext.currentTime);
                    wetGain.gain.setValueAtTime(0.5, window.defconAudioContext.currentTime);
                    dspNode.connect(feedback);
                    feedback.connect(dspNode);
                    dspNode.connect(wetGain);
                    dspNode._feedback = feedback;
                    dspNode._wetGain = wetGain;
                    break;
                    
                case 14: // DSP_SIMPLE_REVERB
                case 6:  // DSP_DSOUND_I3DL2REVERB
                case 8:  // DSP_DSOUND_WAVESREVERB
                    if (window.defconAudioContext.createConvolver) {
                        dspNode = window.defconAudioContext.createConvolver();
                        // Create simple impulse response for reverb
                        var length = window.defconAudioContext.sampleRate * 2;
                        var impulse = window.defconAudioContext.createBuffer(2, length, window.defconAudioContext.sampleRate);
                        for (var channel = 0; channel < 2; channel++) {
                            var channelData = impulse.getChannelData(channel);
                            for (var j = 0; j < length; j++) {
                                channelData[j] = (Math.random() * 2 - 1) * Math.pow(1 - j / length, 2);
                            }
                        }
                        dspNode.buffer = impulse;
                    } else {
                        // Fallback to delay-based reverb
                        dspNode = window.defconAudioContext.createDelay(0.1);
                        dspNode.delayTime.setValueAtTime(0.03, window.defconAudioContext.currentTime);
                    }
                    break;
                    
                case 0: // DSP_DSOUND_CHORUS
                case 4: // DSP_DSOUND_FLANGER
                    // Create chorus/flanger using delay + LFO
                    dspNode = window.defconAudioContext.createDelay(0.02);
                    var chorusLfo = window.defconAudioContext.createOscillator();
                    var chorusLfoGain = window.defconAudioContext.createGain();
                    chorusLfo.frequency.setValueAtTime(1.1, window.defconAudioContext.currentTime);
                    chorusLfoGain.gain.setValueAtTime(0.005, window.defconAudioContext.currentTime);
                    chorusLfo.connect(chorusLfoGain);
                    chorusLfoGain.connect(dspNode.delayTime);
                    chorusLfo.start();
                    dspNode._chorusLfo = chorusLfo;
                    break;
                    
                case 1: // DSP_DSOUND_COMPRESSOR
                    if (window.defconAudioContext.createDynamicsCompressor) {
                        dspNode = window.defconAudioContext.createDynamicsCompressor();
                        dspNode.threshold.setValueAtTime(-20, window.defconAudioContext.currentTime);
                        dspNode.knee.setValueAtTime(40, window.defconAudioContext.currentTime);
                        dspNode.ratio.setValueAtTime(3, window.defconAudioContext.currentTime);
                        dspNode.attack.setValueAtTime(0.01, window.defconAudioContext.currentTime);
                        dspNode.release.setValueAtTime(0.2, window.defconAudioContext.currentTime);
                    }
                    break;
                    
                case 2: // DSP_DSOUND_DISTORTION
                    dspNode = window.defconAudioContext.createWaveShaper();
                    var distortionCurve = new Float32Array(4096);
                    var gain = -18; // dB
                    var edge = 15;
                    for (var j = 0; j < 4096; j++) {
                        var x = (j * 2) / 4096 - 1;
                        distortionCurve[j] = Math.sign(x) * Math.pow(Math.abs(x), edge / 100) * Math.pow(10, gain / 20);
                    }
                    dspNode.curve = distortionCurve;
                    break;
                    
                case 7: // DSP_DSOUND_PARAMEQ
                    dspNode = window.defconAudioContext.createBiquadFilter();
                    dspNode.type = 'peaking';
                    dspNode.frequency.setValueAtTime(8000, window.defconAudioContext.currentTime);
                    dspNode.Q.setValueAtTime(12, window.defconAudioContext.currentTime);
                    dspNode.gain.setValueAtTime(0, window.defconAudioContext.currentTime);
                    break;
                    
                default:
                    console.warn("Unknown DSP filter type:", filterType);
                    continue;
            }
            
            if (dspNode) {
                channel.dspChain.push(dspNode);
                console.log("Created DSP filter type", filterType, "for channel", channelId);
            }
        }
        
        console.log("Created DSP chain with", channel.dspChain.length, "filters for channel", channelId);
        
    }, channelId, numFilters, filterTypes);
}

extern "C" EMSCRIPTEN_KEEPALIVE void jsUpdateDspFilter(int channelId, int filterType, int numParams, const float* params) 
{
    EM_ASM({
        var channelId = $0;
        var filterType = $1;
        var numParams = $2;
        var paramsPtr = $3;
        
        if (!window.defconChannels || !window.defconChannels[channelId] || !window.defconChannels[channelId].dspChain) {
            return;
        }
        
        var channel = window.defconChannels[channelId];
        var params = [];
        for (var i = 0; i < numParams; i++) {
            params[i] = HEAPF32[(paramsPtr >> 2) + i];
        }
        
        // Find the DSP node of the specified type and update its parameters
        channel.dspChain.forEach(function(node, index) {
            if (!node) return;
            
            switch (filterType) {
                case 10: // DSP_RESONANTLOWPASS
                    if (node.frequency && params.length >= 3) {
                        var frequency = Math.exp(params[0]/3850.0 + 4.7);
                        var resonance = Math.exp(params[1]/4.0) - 1.0;
                        var gain = params[2];
                        node.frequency.setValueAtTime(Math.min(frequency, 20000), window.defconAudioContext.currentTime);
                        node.Q.setValueAtTime(Math.max(0.1, resonance), window.defconAudioContext.currentTime);
                        if (node.gain) {
                            node.gain.setValueAtTime(gain, window.defconAudioContext.currentTime);
                        }
                    }
                    break;
                    
                case 11: // DSP_BITCRUSHER
                    if (params.length >= 1) {
                        var bitRate = Math.max(1, Math.min(16, params[0]));
                        var samples = 4096;
                        var curve = new Float32Array(samples);
                        var step = Math.pow(2, bitRate);
                        for (var j = 0; j < samples; j++) {
                            var x = (j * 2) / samples - 1;
                            curve[j] = Math.round(x * step) / step;
                        }
                        node.curve = curve;
                    }
                    break;
                    
                case 12: // DSP_GARGLE
                case 5:  // DSP_DSOUND_GARGLE
                    if (node._lfo && params.length >= 2) {
                        var wetDryMix = params[0] / 100.0;
                        var rateHz = Math.max(0.01, Math.min(100, params[1]));
                        node._lfo.frequency.setValueAtTime(rateHz, window.defconAudioContext.currentTime);
                        node.gain.setValueAtTime(1.0 - wetDryMix * 0.5, window.defconAudioContext.currentTime);
                    }
                    break;
                    
                case 13: // DSP_ECHO
                case 3:  // DSP_DSOUND_ECHO
                    if (params.length >= 3) {
                        var wetDryMix = params[0] / 100.0;
                        var delay = Math.max(1, Math.min(3000, params[1])) / 1000.0;
                        var attenuation = params[2] / 100.0;
                        node.delayTime.setValueAtTime(delay, window.defconAudioContext.currentTime);
                        if (node._feedback) {
                            node._feedback.gain.setValueAtTime(attenuation, window.defconAudioContext.currentTime);
                        }
                        if (node._wetGain) {
                            node._wetGain.gain.setValueAtTime(wetDryMix, window.defconAudioContext.currentTime);
                        }
                    }
                    break;
                    
                case 14: // DSP_SIMPLE_REVERB
                    if (params.length >= 1) {
                        var wetDryMix = params[0] / 100.0;
                        // For convolver reverb, we'd need to create a new impulse response
                        // For now, just adjust the overall effect level
                        if (node.gain) {
                            node.gain.setValueAtTime(wetDryMix, window.defconAudioContext.currentTime);
                        }
                    }
                    break;
                    
                // Add more parameter updates for other filter types as needed
            }
        });
        
    }, channelId, filterType, numParams, params);
}

extern "C" EMSCRIPTEN_KEEPALIVE void jsRemoveDspChain(int channelId) 
{
    EM_ASM({
        var channelId = $0;
        
        if (!window.defconChannels || !window.defconChannels[channelId]) {
            return;
        }
        
        var channel = window.defconChannels[channelId];
        
        if (channel.dspChain) {
            // Disconnect and clean up all DSP nodes
            channel.dspChain.forEach(function(node) {
                if (node) {
                    if (node._lfo) {
                        node._lfo.stop();
                        node._lfo.disconnect();
                    }
                    if (node._chorusLfo) {
                        node._chorusLfo.stop();
                        node._chorusLfo.disconnect();
                    }
                    if (node._feedback) {
                        node._feedback.disconnect();
                    }
                    if (node._wetGain) {
                        node._wetGain.disconnect();
                    }
                    node.disconnect();
                }
            });
            
            channel.dspChain = [];
            console.log("Removed DSP chain for channel", channelId);
        }
        
    }, channelId);
}

extern "C" EMSCRIPTEN_KEEPALIVE void jsConnectDspChain(int channelId) 
{
    EM_ASM({
        var channelId = $0;
        
        if (!window.defconChannels || !window.defconChannels[channelId]) {
            return;
        }
        
        var channel = window.defconChannels[channelId];
        
        if (!channel.dspChain || channel.dspChain.length === 0) {
            // No DSP chain, connect source directly to gain/panner
            if (channel.source && channel.gainNode) {
                if (channel.pannerNode) {
                    channel.source.connect(channel.pannerNode);
                    channel.pannerNode.connect(channel.gainNode);
                } else {
                    channel.source.connect(channel.gainNode);
                }
                channel.gainNode.connect(window.defconMasterGain);
            }
            return;
        }
        
        // Connect DSP chain: source -> dsp1 -> dsp2 -> ... -> gain/panner -> master
        var currentNode = channel.source;
        
        for (var i = 0; i < channel.dspChain.length; i++) {
            var dspNode = channel.dspChain[i];
            if (currentNode && dspNode) {
                currentNode.connect(dspNode);
                currentNode = dspNode;
            }
        }
        
        // Connect final DSP node to gain/panner chain
        if (currentNode && channel.gainNode) {
            if (channel.pannerNode) {
                currentNode.connect(channel.pannerNode);
                channel.pannerNode.connect(channel.gainNode);
            } else {
                currentNode.connect(channel.gainNode);
            }
            channel.gainNode.connect(window.defconMasterGain);
        }
        
        console.log("Connected DSP chain for channel", channelId, "with", channel.dspChain.length, "filters");
        
    }, channelId);
}

} // extern "C"

#endif // TARGET_EMSCRIPTEN 