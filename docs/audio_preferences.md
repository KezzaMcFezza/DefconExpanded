# Audio Preferences

The audio subsystem exposes several tunables for balancing latency, quality, and stability across SDL push/pull and DirectSound backends. Channel and master volumes now feed the mixers directly, without an additional fixed headroom stage.

Use the sections below as a quick reference when adjusting `localisation/data/prefs_default.txt`, per-user preference files, or in-game configuration panels.

## Software Resampler Modes (SDL Software Mixer)

Per-voice resampling uses a windowed-sinc FIR by default. Two preferences control the quality tier:

- `SoundResamplerSfx` (default `sinc64`)
  - Applies to non-music channels (typically many short SFX voices).
- `SoundResamplerMusic` (default `sinc128`)
  - Applies to music/ambience channels (higher quality, fewer voices).

Accepted values:

| Value     | Kernel | Phases | Notes |
|-----------|--------|--------|-------|
| `linear`  | 2 taps (lerp) | 1 | Legacy bilinear interpolation. Lowest CPU, weakest filtering. Useful only for debugging.
| `sinc64`  | 64 taps | 256 | Blackman–Harris window, ≈90 dB stop-band. Balanced for dense SFX mixes.
| `sinc96`  | 96 taps | 256 | Better transition band and stop-band. Good compromise if SFX aliasing is still audible.
| `sinc128` | 128 taps | 512 | Highest quality / lowest aliasing. Intended for music, ambience or other exposed content.

Implementation notes:

- Polyphase LUT with 256 or 512 fractional phases, symmetric kernel halves to minimise multiplies.
- Three cutoff banks (0.90 / 0.70 / 0.50 Nyquist) cover down-sampling cases; the runtime picks the closest cutoff based on the current pitch ratio.
- Preferences can be edited in `prefs_default.txt`, `prefs_testbed.txt`, or via the in-game config UI once exposed.
- The Sound Debug Overlay (F3) lists the active SFX/Music resampler tier, taps/phases, current bank usage, and any voices that fell back to linear interpolation.

## SDL Push/Queue Mode Highlights

Push mode renders small slices in software and feeds them to SDL via `SDL_QueueAudio`, decoupling change granularity from the total safety horizon. Key preferences (see `docs/sdl_push_mode_guide.md` for deep dives):

- `SoundUsePushMode` (default `1`): 1 = push/queue, 0 = SDL callback.
- `SoundPeriodFrames` (default `128`): render quantum per slice. Smaller = crisper response, higher CPU overhead.
- `SoundTargetLatencyMs`, `SoundDeviceQueueLowMs`, `SoundDeviceQueueHighMs`: tune audible latency vs. underrun safety.
- `SoundRingMs`: software ring horizon premixed ahead of the device queue.
- `SoundTimedScheduling` / `SoundAudioClockedADSR`: enable precise onset placement and audio-clocked envelopes.

Keep the period small (64–128) for tight ADSR and adjust the device queue/ring horizons upward when underruns occur on slower CPUs.

## Per-Voice Minimum Fade-In (SDL Software Mixer)

Applies a short fade-in to newly started non-music voices to reduce correlated start transients when many sounds begin in the same frame (e.g., zooming into multiple nukes). Disabled by default.

- `SoundMinFadeIn` (int; 0=off, 1=on; default `0`)
  - Master switch. When enabled, each new channel gets a minimum fade envelope applied across successive mix buffers.
- `SoundMinFadeInMs` (float, ms; default `3.0`)
  - Base fade-in length. 2.0–5.0 ms preserves punch while smoothing clicks.
- `SoundFadeInJitterMs` (float, ms; default `0.75`)
  - Adds a deterministic per-channel jitter (± value) to decorrelate simultaneous starts.

Notes:

- The envelope multiplies the normal per-channel gain (ADSR, distance, pan); it doesn’t replace content Attack.
- Deterministic jitter is derived from the channel index, so behaviour is stable run-to-run while still spreading onsets.

## SFX De-Correlation (Optional)

- `SoundRandomStartMs` (float, ms; default `0.0`)
  - Applies only to non-music sounds on the software mixer path. When > 0, each new SFX instance starts at a random offset up to this many milliseconds inside the sample. This de-correlates large bursts of identical waveforms (for example, many rumble loops that start together), reducing phase-coherent summing and peak stacking. Set to a small value like 2–5 ms if dense stacks still feel “tearing” under heavy load. Leave at 0.0 to preserve exact onsets.

## Debug Overlay (F3)

- **Sound Debug Overlay**
  - Displays runtime stats (callbacks, buffering, queue fill, active resampler tiers, etc.). Useful for timing/throughput verification and spotting underruns.
