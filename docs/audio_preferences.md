# Audio Headroom, Limiter, and Dynamic Attenuation Preferences

This project adds robust, consistent protection against clipping across SDL and DirectSound backends by combining a fixed headroom margin with backend‑specific peak protection.

Use the settings below to tune behavior. The shared headroom stage is now optional: enable it with `SoundHeadroomEnabled = 1` when you want a fixed safety margin applied everywhere. Once enabled, `SoundHeadroomDb` remains the primary “don’t clip” knob; the limiter (SDL) and dynamic attenuation (DirectSound) provide extra safety only when needed.

Quick toggles (default off)

- SoundHeadroomEnabled = 0
  - Shared fixed headroom stage. Set to 1 when you want a constant pre-mix margin applied on both backends.
- SoundLimiter = 0
  - SDL mix‑bus limiter. Set to 1 if you want automatic peak catching on the software mixer.
- SoundSDLDynEnabled = 0
  - SDL crowd attenuation heuristic. Set to 1 if you want the mixer to pre‑duck whenever many channels run loud.
- SoundDSDynEnabled = 0
  - DirectSound dynamic attenuation. Set to 1 to keep the DS backend’s classic crowd attenuation behavior.

## Software Resampler Modes (SDL software mixer)

Per-voice resampling now uses a windowed-sinc FIR by default. Two preferences control the quality tier:

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

Additional implementation details:

- Polyphase LUT with 256 or 512 fractional phases, symmetric kernel halves to minimise multiplies.
- Three cutoff banks (0.90 / 0.70 / 0.50 Nyquist) cover down-sampling cases; the runtime picks the closest cutoff based on the current pitch ratio.
- Preferences can be edited in `prefs_default.txt`, `prefs_testbed.txt`, or via the in-game config UI once exposed.
- The Sound Debug Overlay (F3) now lists the active SFX/Music resampler tier, taps/phases, current bank usage, and any voices that fell back to linear interpolation.

## Overview

- Fixed headroom (shared, optional): When `SoundHeadroomEnabled = 1`, applies a small attenuation to keep day‑to‑day mixes below full‑scale.
- SDL mix‑bus limiter: Catches rare peaks that still exceed headroom when many loud sounds overlap.
- SDL crowd attenuation: Pre‑emptively lowers the SDL software mix when many non‑music channels are running hot, mirroring the DirectSound heuristic.
- DirectSound dynamic anti‑clip: Pre‑attenuates the whole mix when many channels are simultaneously loud (since the OS/driver performs the final mix).

## Shared Headroom Controls

- SoundHeadroomEnabled (int, 0/1; default 0)
  - Master toggle for the shared fixed headroom stage. When 0, channel levels are untouched and only the limiter/dynamic stages remain.
- SoundHeadroomDb (float, dB; default 12)
  - Fixed safety margin applied before mixing (SDL) or before setting per‑channel volume (DirectSound) whenever the toggle above is enabled.
  - Higher values = more headroom (safer, slightly quieter overall). Lower values = louder, less margin.
  - Recommended: 12 to match empirical safe point on DS; 6–12 is a reasonable range.
  - SoundHeadroomDb just offsets every channel downward before mixing, so raising it buys you more margin and keeps the limiter idle (as long as the toggle is enabled). Dropping headroom pushes the mix hotter; the limiter will engage more often and the soft‑clip curve will imprint itself on transients. That’s a subjective choice: if you like the punch the limiter adds, run with less headroom; if you want cleaner dynamics, keep the headroom high enough that Limiter bus gain stays near 1.0 except on true outliers.
  - Hard clipping cannot occur on the SDL path because the limiter always pulls the bus gain down before samples exceed the threshold and the output stage applies a tanh soft clip before writing PCM (regardless of the headroom toggle).

## SDL Mix‑Bus Limiter (Extra Safety)

Disabled by default. Enable by setting `SoundLimiter = 1`.

Applies after mixing to floats, only when needed. It measures the peak within each audio block and reduces gain enough to keep the block under the threshold, with smoothing. A soft‑clip (tanh) avoids harsh edges for any residual overs.

- SoundLimiterThreshold (float, PCM; default 28000)

  - Peak level where limiting starts (full‑scale ≈ 32767). Lower = earlier limiting (safer), higher = later limiting (louder, riskier).

- SoundLimiterAttack (float, 0..1; default 1.0)

  - Smoothing when gain needs to decrease (limiter engages). 1.0 = instant, guaranteeing the current block is pulled under threshold.

- SoundLimiterRelease (float, 0..1; default 0.02)
  - Smoothing when gain recovers after a peak. Smaller values = quicker recovery but more audible gain riding; larger = smoother but slower.

Notes:

- The limiter acts only when a block’s peak exceeds SoundLimiterThreshold. Under normal play it remains transparent.
- If you prefer fewer gain changes during extreme stacks, raise the threshold slightly or increase release.

## SDL Crowd Attenuation (Dynamic Pre-Duck)

Disabled by default. Enable by setting `SoundSDLDynEnabled = 1`.

Mirrors the DirectSound anti-clip heuristic, but runs entirely within the SDL software mixer. When many non-music channels sit at high volume simultaneously, the system gently lowers the bus before the limiter needs to engage, then releases once things calm down.

- SoundSDLDynLoudVolume (float, 0..10; default 7.0)

  - Per-channel volume considered “loud.” Channels at or above this level contribute to the loud-channel count.

- SoundSDLDynStartCount (int; default 2)

  - Loud channels allowed before attenuation begins. Choose a value based on how dense you expect gameplay stacks to get.

- SoundSDLDynDbPerExtra (float, dB; default 2.0)

  - Attenuation amount added for each loud channel beyond SoundSDLDynStartCount.

- SoundSDLDynMaxDb (float, dB; default 12.0)

  - Hard cap on the attenuation amount. Prevents the mix from getting too quiet even if the loud-channel count spikes.

- SoundSDLDynAttack (float, 0..1; default 0.5)

  - Smoothing factor when attenuation increases. Higher = reacts faster to sudden dense stacks.

- SoundSDLDynRelease (float, 0..1; default 0.1)

  - Smoothing when attenuation relaxes. Lower = quicker bounce-back; higher = smoother decay.

Notes:

- Only non-music channels are counted; music stays steady so score cues do not pump.
- Works in tandem with the limiter: the dynamic ducking handles sustained density, while the limiter protects single rogue peaks.

## Per‑Voice Minimum Fade‑In (SDL Software Mixer)

Applies a very short fade‑in to newly started non‑music voices to reduce correlated start transients when many sounds begin in the same frame (e.g., zooming into multiple nukes). Disabled by default.

- SoundMinFadeIn (int; 0=off, 1=on; default 0)

  - Master switch. When enabled, each new channel gets a minimum fade envelope applied across successive mix buffers.

- SoundMinFadeInMs (float, ms; default 3.0)

  - Base fade‑in length. 2.0–5.0 ms is a good range: short enough to preserve punch, long enough to smooth clicks.

- SoundFadeInJitterMs (float, ms; default 0.75)

  - Adds a small deterministic per‑channel jitter (± value) to the fade length to decorrelate simultaneous starts, reducing the chance that many transients align.

Notes:

- This envelope multiplies the normal per‑channel volume (ADSR, distance, pan); it doesn’t replace your content’s Attack parameter.
- Deterministic jitter is derived from the channel index, so behavior is stable run‑to‑run while still spreading onsets.

## DirectSound Dynamic Anti‑Clip (Extra Safety)

Disabled by default. Enable by setting `SoundDSDynEnabled = 1`.

DirectSound mixes in the OS/driver, where we cannot insert a true mix‑bus limiter. Instead, a global pre‑attenuation kicks in when many channels are “loud,” with attack/release smoothing.
The SDL software mixer exposes the same heuristic via the SoundSDLDyn* settings described above, so you can keep both backends aligned.

- SoundDSDynLoudVolume (float, 0..10; default 7.0)

  - Per‑channel volume considered “loud.” Channels at or above this level contribute to the loud channel count.

- SoundDSDynStartCount (int; default 2)

  - Number of loud channels before attenuation begins.

- SoundDSDynDbPerExtra (float, dB; default 2.0)

  - Attenuation per loud channel beyond SoundDSDynStartCount.

- SoundDSDynMaxDb (float, dB; default 12.0)

  - Maximum dynamic attenuation cap.

- SoundDSDynAttack (float, 0..1; default 0.5)

  - Smoothing when increasing attenuation (faster means more responsive).

- SoundDSDynRelease (float, 0..1; default 0.1)
  - Smoothing when reducing attenuation (slower prevents pumping).

Notes:

- This heuristic anticipates clipping risk when dense stacks of loud channels occur. It preserves loudness under normal conditions and ducks the whole mix only under heavy load.

## Defaults

These defaults aim for robust protection with minimal audible side‑effects:

- Quick toggles: SoundLimiter = 0, SoundSDLDynEnabled = 0, SoundDSDynEnabled = 0
- SoundHeadroomEnabled = 0
- SoundHeadroomDb = 12 (applies when the toggle above is enabled)
- SDL: SoundLimiterThreshold = 28000, SoundLimiterAttack = 1.0, SoundLimiterRelease = 0.02
- SDL crowd attenuation: SoundSDLDynLoudVolume = 7.0, SoundSDLDynStartCount = 2, SoundSDLDynDbPerExtra = 2.0, SoundSDLDynMaxDb = 12.0, SoundSDLDynAttack = 0.5, SoundSDLDynRelease = 0.1
- DS: SoundDSDynLoudVolume = 7.0, SoundDSDynStartCount = 2, SoundDSDynDbPerExtra = 2.0, SoundDSDynMaxDb = 12.0, SoundDSDynAttack = 0.5, SoundDSDynRelease = 0.1

## Tuning Guide

Common goals and the most relevant knobs:

- “Just make clipping impossible”

  - Set SoundHeadroomEnabled = 1, then increase SoundHeadroomDb (e.g., 14). This remains the primary safety lever once the stage is active.

- “Catch more peaks on SDL without killing dynamics”

  - Lower SoundLimiterThreshold a little (e.g., 27000); keep Attack at 1.0; adjust Release between 0.01–0.05.

- “Be more proactive when many sounds stack (crowd attenuation)”
  - Lower SoundDSDynStartCount (and/or SoundSDLDynStartCount), or increase the per-extra and max dB values on the relevant backend to duck earlier and harder.

## FAQ

- Why not just turn down the master volume?

  - Lowering master works but penalizes loudness all the time. Headroom (when enabled) plus limiter/attenuation preserves full loudness for normal play and intervenes only when necessary.

- Why did SDL and DS have different “safe” master values before?

  - Their original mappings and mixing stages differed. With a unified headroom strategy and protection, one set of defaults works consistently across both.

- Does changing SoundLimiterThreshold change overall loudness?
  - No under normal conditions. It only affects behavior when peaks cross the threshold. For consistent everyday level, use SoundHeadroomDb.

## Debug Overlays (F3, F4)

- F3: Sound Debug Overlay

  - Existing runtime stats (callbacks, buffering, etc.). Useful for timing/throughput checks.

- F4: Sound Safety Overlay
  - Shows the safety knobs and whether protection is currently engaged. Only displays the values relevant to the active backend.
  - Shared
    - SoundHeadroomDb (dB): main safety margin (overlay appends “(disabled)” when `SoundHeadroomEnabled = 0`).
  - SDL (software mixer active)
    - Limiter threshold (PCM), pre‑headroom equivalent threshold, and margin to full‑scale.
    - Limiter attack/release (smoothing).
    - Peak (pre‑limit) vs threshold.
    - Limiter bus gain (runtime):
      - Red when limiting is active (bus gain < 1.0), accompanied by “LIMITER ACTIVE”.
      - Peak line turns red when the measured pre‑limit peak exceeds threshold (headroom alone was insufficient).
    - Crowd attenuation snapshot:
      - Shows current vs target attenuation, start count, loud threshold, per-extra/max dB, attack/release, and loud-channel count.
      - When attenuation is active, the line turns red and “CROWD ATTENUATION ACTIVE” appears.
  - DirectSound (DS backend active)
    - Fixed headroom (dB).
    - Dynamic attenuation (current dB):
      - Red when any attenuation is applied (> 0 dB), accompanied by “ATTENUATION ACTIVE”.
    - Start@loud count and loud volume threshold (what counts as “loud”).
    - Per‑extra/max dB (strength/cap).
    - Attack/release (smoothing).
    - Loud channels vs non‑music channels:
      - Red when the number of loud channels exceeds the start count (dynamic attenuation likely to engage).

Interpreting red indicators

- SDL: Red peak/bus‑gain lines or the crowd‑attenuation line (“CROWD ATTENUATION ACTIVE”) mean protection is ducking the mix — consider increasing SoundHeadroomDb, lowering SoundLimiterThreshold, or adjusting SoundSDLDyn* if this happens frequently.
- DS: Red dynamic‑attenuation or loud‑channel lines mean global ducking is in effect or imminent — consider increasing SoundHeadroomDb, reducing SoundDSDynStartCount, or adjusting SoundDSDynDbPerExtra/MaxDb.
