# Audio Preferences

The audio subsystem exposes several tunables for balancing latency, quality, and stability across SDL push/pull and DirectSound backends.

Use the sections below as a quick reference when adjusting `localisation/data/prefs_default.txt`, per-user preference files, or in-game configuration panels.

## Software Resampler Modes (SDL Software Mixer)

Per-voice resampling uses a windowed-sinc FIR when enabled. Two preferences control the quality tier:

- `SoundResamplerSfx` (default `linear`)
  - Applies to non-music channels (typically many short SFX voices).
- `SoundResamplerMusic` (default `linear`)
  - Applies to music/ambience channels (higher quality, fewer voices).

Accepted values:

| Value     | Kernel        | Phases | Notes                                                                                           |
| --------- | ------------- | ------ | ----------------------------------------------------------------------------------------------- |
| `linear`  | 2 taps (lerp) | 1      | Baseline for minimum CPU and maximum compatibility. Expect audible aliasing on bright material. |
| `sinc64`  | 64 taps       | 256    | Blackman–Harris window, ≈90 dB stop-band. Balanced for dense SFX mixes.                         |
| `sinc96`  | 96 taps       | 256    | Better transition band and stop-band. Good compromise if SFX aliasing is still audible.         |
| `sinc128` | 128 taps      | 512    | Highest quality / lowest aliasing. Intended for music, ambience or other exposed content.       |

Implementation notes:

- Polyphase LUT with 256 or 512 fractional phases, symmetric kernel halves to minimise multiplies.
- Three cutoff banks (0.90 / 0.70 / 0.50 Nyquist) cover down-sampling cases; the runtime picks the closest cutoff based on the current pitch ratio.
- Preferences can be edited in `prefs_default.txt`, `prefs_testbed.txt`, or via the in-game config UI once exposed.
- The Sound Debug Overlay (F3) lists the active SFX/Music resampler tier, taps/phases, current bank usage, and any voices that fell back to linear interpolation if using polyphase.

## SDL Push/Queue Mode Highlights

Push mode renders small slices in software and feeds them to SDL via `SDL_QueueAudio`, decoupling change granularity from the total safety horizon. Key preferences (see `docs/sdl_push_mode_guide.md` for deep dives):

- `SoundUsePushMode` (default `1`): 1 = push/queue, 0 = SDL callback.
- `SoundPeriodFrames` (default `128`): render quantum per slice. Smaller = crisper response, higher CPU overhead.
- `SoundTargetLatencyMs`, `SoundDeviceQueueLowMs`, `SoundDeviceQueueHighMs`: tune audible latency vs. underrun safety.
- `SoundRingMs`: software ring horizon premixed ahead of the device queue.
- `SoundTimedScheduling` / `SoundAudioClockedADSR`: enable precise onset placement and audio-clocked envelopes.
- `SoundUpdatePeriod` (default `0.02`): cadence for `SoundSystem::Advance`. Lower values let the main thread start/stop sounds and reposition the listener more frequently but increase channel-management overhead.

Keep `SoundUpdatePeriod` comfortably below the total output latency while keeping it longer than a single push slice. As a rule of thumb, aim for an update period 3–8× the slice duration (`SoundPeriodFrames / sampleRate`). With 128-frame slices at 44.1 kHz (~2.9 ms each), a 20 ms update period means the mixer sees new channel assignments within roughly seven slices. Dropping to 64 frames halves the slice length, so trimming the update period toward ~10 ms preserves the benefit of the shorter quantum. Smaller periods improve responsiveness but raise CPU usage and contention with the audio threads.

Keep the period small (64–128) for tight ADSR and adjust the device queue/ring horizons upward when underruns occur on slower CPUs.

## Debug Overlay (F3)

- **Sound Debug Overlay**
  - Displays runtime stats (callbacks, buffering, queue fill, active resampler tiers, etc.). Useful for timing/throughput verification and spotting underruns.
