# Audio Headroom Preferences

This project keeps mixes safe by relying on a fixed, shared headroom margin that behaves consistently across SDL and DirectSound backends.

Use the settings below to tune behavior. The shared headroom stage is optional: enable it with `SoundHeadroomEnabled = 1` when you want a fixed safety margin applied everywhere. Once enabled, `SoundHeadroomDb` remains the primary “don’t clip” knob for both renderers.

Quick toggles (default off)

- SoundHeadroomEnabled = 0
  - Shared fixed headroom stage. Set to 1 when you want a constant pre-mix margin applied on both backends.

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

## Shared Headroom Controls

- SoundHeadroomEnabled (int, 0/1; default 0)
  - Master toggle for the shared fixed headroom stage. When 0, channel levels are untouched and no extra safety margin is applied.
- SoundHeadroomDb (float, dB; default 12)
  - Fixed safety margin applied before mixing (SDL) or before setting per‑channel volume (DirectSound) whenever the toggle above is enabled.
  - Higher values = more headroom (safer, slightly quieter overall). Lower values = louder, less margin.
  - Recommended: 12 to match empirical safe point on DS; 6–12 is a reasonable range.
  - SoundHeadroomDb offsets every channel downward before mixing, so raising it buys you more margin and reduces the chance of clipping. Dropping headroom pushes the mix hotter, which increases the risk of saturation if many loud sounds stack at once.

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

## Defaults

These defaults aim for robust protection with minimal audible side‑effects:

- SoundHeadroomEnabled = 0
- SoundHeadroomDb = 12 (applies when the toggle above is enabled)

## Tuning Guide

Common goals and the most relevant knobs:

- “Just make clipping impossible”

  - Set SoundHeadroomEnabled = 1, then increase SoundHeadroomDb (e.g., 14). This remains the primary safety lever once the stage is active.

## SFX De‑Correlation (Optional)

- SoundRandomStartMs (float, ms; default 0.0)
  - Applies only to non‑music sounds on the software mixer path. When > 0, each new SFX instance starts at a random offset up to this many milliseconds inside the sample. This de‑correlates large bursts of identical waveforms (for example, many rumble loops that start together), reducing phase‑coherent summing and peak stacking. Set to a small value like 2–5 ms if dense stacks still feel “tearing” under heavy load. Leave at 0.0 to preserve exact onsets.

## FAQ

- Why not just turn down the master volume?

  - Lowering master works but penalizes loudness all the time. Headroom (when enabled) preserves full loudness for normal play and only reduces level when many hot sounds stack together.

- Why did SDL and DS have different “safe” master values before?

  - Their original mappings and mixing stages differed. With a unified headroom strategy and protection, one set of defaults works consistently across both.

## Debug Overlays (F3, F4)

- F3: Sound Debug Overlay

  - Existing runtime stats (callbacks, buffering, etc.). Useful for timing/throughput checks.

- F4: Sound Safety Overlay
  - Shows the safety knobs and whether protection is currently engaged. Only displays the values relevant to the active backend.
  - Shared
    - SoundHeadroomDb (dB): main safety margin (overlay appends “(disabled)” when `SoundHeadroomEnabled = 0`).
  - SDL (software mixer active)
    - Effective headroom reported by the active mixer and a snapshot of the SDL device format/frequency.
  - DirectSound (DS backend active)
    - Fixed headroom (dB).
