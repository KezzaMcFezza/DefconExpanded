# Sound Debug Overlay Reference

When you toggle the overlay with `F3`, it prints a right-aligned table of the sound renderer’s current configuration and live telemetry. Each line below describes the corresponding readout.

## Header

- **Sound Debug Overlay (F3)**  
  Friendly reminder of the toggle key and confirms the overlay is active.

## Preferences Section

- **Sound enabled : yes/no**  
  `Sound` preference flag; shows if audio is globally enabled.
- **Master volume : <value>**  
  `SoundMasterVolume` from preferences (0–255 scale).
- **Mix freq / buffer : <Hz> / <samples>**  
  Configured mix rate (`SoundMixFreq`) and SDL buffer size (`SoundBufferSize`) requested by preferences.
- **Channels (total/music): <n> / <n>**  
  Preferred total channels and the subset reserved for music (`SoundChannels`, `SoundMusicChannels`).
- **Sound library : <name>**  
  Library chosen via `SoundLibrary` preference (e.g., `sdl`).
- **Hardware 3D Sound : Enabled/Disabled/Unavailable**  
  Whether 3D hardware acceleration was requested and is available (`SoundHW3D` combined with runtime capability checks).

## Sound System Section

- **Instances active : <count>**  
  Total `SoundInstance` objects currently tracked by the sound system (`NumSoundInstances()`).
- **Channels used : <used> / <total> (music <music_total>)**  
  Active channel assignments, total allocated channels, and how many are music channels.
- **Sounds discarded : <count>**  
  Cumulative count of instances dropped because channels weren’t available (`NumSoundsDiscarded()`).
- **Time to next sync : <ms>**  
  Remaining time until the sound system’s periodic update (`m_timeSync`) runs again.
- **Blueprint propagate : yes/no**  
  Shows whether blueprint propagation (loop sync across instances) is currently enabled (`m_propagateBlueprints`).

## 3D Library Section

- **Sample rate : <Hz>**  
  Effective sample rate of the 3D mixer (`GetSampleRate()`).
- **Master volume : <value>**  
  Current master volume set on the 3D library (`GetMasterVolume()`).
- **Channels (total/music): <total> / <music>**  
  How many 3D channels exist and how many are dedicated to music.
- **Max channels : <value>**  
  Maximum channels reported by the 3D backend (`GetMaxChannels()`).
- **CPU overhead : <percent>%**  
  Library-estimated CPU usage of the 3D mixer (`GetCPUOverhead()`).

## SDL 2D Renderer Section

- **Audio started : yes/no Callback: set/unset**  
  Indicates SDL audio device state and whether the callback function is currently assigned.
- **Recording to WAV : yes/no**  
  Reports if SDL is writing audio to a WAV file via the library’s recorder.
- **Actual freq/buffer : <Hz> / <samples>**  
  Real values SDL granted for the device (`SDL_AudioSpec`), which may differ from the preference request.
- **Channels / format : <channels> / 0x<format>**  
  Channel count and audio format returned by SDL (format is the raw SDL constant).
- **Audio callbacks : <total> (<per-sec>/sec)**  
  Total number of SDL audio thread callbacks observed and their per-second rate since the last sample.
- **Callback intervals : avg <ms> max <ms> idle <ms>**  
  Rolling average and maximum callback spacing, plus milliseconds since the last callback (helps spot stalls/underruns).
- **Topup calls : <total> (<per-sec>/sec)**  
  Main-thread `TopupBuffer()` invocations and the rate at which they occur.
- **Buffered callbacks : queued <total> (<per-sec>/sec) processed <count>**  
  Number of callbacks queued for main-thread processing (when not serviced on the audio thread) and how many were actually handled.
- **Buffer thirst : <level> pending [<samples0>, <samples1>]**  
  `bufferIsThirsty` backlog counter and the sample lengths still waiting to be consumed from the double-buffer slots.
- **Last callback size : <samples>**  
  Sample count passed in the most recent audio callback.
- **Total mixed audio : <total samples> (<seconds> s)**  
  Accumulated samples mixed since startup and their duration in seconds, based on the actual device frequency.
