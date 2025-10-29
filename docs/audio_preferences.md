# Audio Headroom, Limiter, and Dynamic Attenuation Preferences

This project adds robust, consistent protection against clipping across SDL and DirectSound backends by combining a fixed headroom margin with backend‑specific peak protection.

Use the settings below to tune behavior. The one knob that matters most for “don’t clip” is SoundHeadroomDb; the limiter (SDL) and dynamic attenuation (DirectSound) provide extra safety only when needed.

## Overview

- Fixed headroom (shared): Always applies a small attenuation to keep day‑to‑day mixes below full‑scale.
- SDL mix‑bus limiter: Catches rare peaks that still exceed headroom when many loud sounds overlap.
- DirectSound dynamic anti‑clip: Pre‑attenuates the whole mix when many channels are simultaneously loud (since the OS/driver performs the final mix).

## Main Knob (Shared)

- SoundHeadroomDb (float, dB; default 12)
  - Fixed safety margin applied before mixing (SDL) or before setting per‑channel volume (DirectSound).
  - Higher values = more headroom (safer, slightly quieter overall). Lower values = louder, less margin.
  - Recommended: 12 to match empirical safe point on DS; 6–12 is a reasonable range.
  - SoundHeadroomDb just offsets every channel downward before mixing, so raising it buys you more margin and keeps the limiter idle. Dropping headroom pushes the mix hotter; the limiter will engage more often and the soft‑clip curve will imprint itself on transients. That’s a subjective choice: if you like the punch the limiter adds, run with less headroom; if you want cleaner dynamics, keep the headroom high enough that Limiter bus gain stays near 1.0 except on true outliers.
  - Hard clipping cannot occur on the SDL path because the limiter always pulls the bus gain down before samples exceed the threshold and the output stage applies a tanh soft clip before writing PCM.

## SDL Mix‑Bus Limiter (Extra Safety)

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

DirectSound mixes in the OS/driver, where we cannot insert a true mix‑bus limiter. Instead, a global pre‑attenuation kicks in when many channels are “loud,” with attack/release smoothing.

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

- SoundHeadroomDb = 12
- SDL: SoundLimiterThreshold = 28000, SoundLimiterAttack = 1.0, SoundLimiterRelease = 0.02
- DS: SoundDSDynLoudVolume = 7.0, SoundDSDynStartCount = 2, SoundDSDynDbPerExtra = 2.0, SoundDSDynMaxDb = 12.0, SoundDSDynAttack = 0.5, SoundDSDynRelease = 0.1

## Tuning Guide

Common goals and the most relevant knobs:

- “Just make clipping impossible”

  - Increase SoundHeadroomDb (e.g., 14). This is the primary safety lever.

- “Catch more peaks on SDL without killing dynamics”

  - Lower SoundLimiterThreshold a little (e.g., 27000); keep Attack at 1.0; adjust Release between 0.01–0.05.

- “Be more proactive on DirectSound when many sounds stack”
  - Lower SoundDSDynStartCount, or increase SoundDSDynDbPerExtra and/or SoundDSDynMaxDb.

## FAQ

- Why not just turn down the master volume?

  - Lowering master works but penalizes loudness all the time. Headroom + limiter/attenuation preserves full loudness for normal play and intervenes only when necessary.

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
    - SoundHeadroomDb (dB): main safety margin.
  - SDL (software mixer active)
    - Limiter threshold (PCM), pre‑headroom equivalent threshold, and margin to full‑scale.
    - Limiter attack/release (smoothing).
    - Peak (pre‑limit) vs threshold.
    - Limiter bus gain (runtime):
      - Red when limiting is active (bus gain < 1.0), accompanied by “LIMITER ACTIVE”.
      - Peak line turns red when the measured pre‑limit peak exceeds threshold (headroom alone was insufficient).
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

- SDL: Red peak or bus‑gain lines mean the limiter had to engage — consider increasing SoundHeadroomDb or lowering SoundLimiterThreshold if this happens frequently.
- DS: Red dynamic‑attenuation or loud‑channel lines mean global ducking is in effect or imminent — consider increasing SoundHeadroomDb, reducing SoundDSDynStartCount, or adjusting SoundDSDynDbPerExtra/MaxDb.
