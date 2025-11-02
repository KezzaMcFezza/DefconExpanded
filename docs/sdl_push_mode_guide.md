# SDL Push/Queue Audio Mode – Guide and Tuning

This document explains the SDL push/queue audio path, its parameters, defaults, and recommended settings for two common goals:

- Low latency, crisp sound (fast reaction, tight envelopes)
- Higher safety (more headroom against underruns) while still preserving crisp sound

It also clarifies how timing, scheduling, and ADSR respond under push mode vs. the legacy callback mode.

## Overview

There are two SDL output modes:

- Callback mode (pull): SDL calls us with a device callback requesting a big block (`SoundBufferSize` samples). Larger blocks smear fast changes and ADSR because changes are applied once per callback.
- Push/queue mode: We render small “slices” (period) in software and push them to SDL with `SDL_QueueAudio`. This gives fine-grained change granularity (small period) independent from the total safety horizon. The device queue is kept short for fast reaction while a software ring buffer is mixed further ahead for stability.

Key ideas in push mode:

- Period granularity is set by `SoundPeriodFrames` (e.g., 64–128). Small periods mean crisp ADSR and fast parameter response.
- Audible reaction time is dominated by the device queue length (low/high water marks). The ring buffer provides the extra safety horizon without locking in audible latency.
- Timestamped scheduling places new sound starts at exact future sample indices on the “audio timeline” (playback cursor), avoiding missed short bleeps even with a deep safety horizon.
- Optional audio‑clocked ADSR drives the envelope from the audio playhead (not wall clock), so envelopes are aligned to what you actually hear.

## Audio Quality vs Latency (Ring Optional)

- Audio quality (crisp ADSR and precise onsets) depends on a small render period and timeline alignment, not on having a deep software ring.
- With no ring or a very shallow one, quality is identical across CPUs. The only difference is latency, which is set by the device queue depth (low/high water marks).
- Keep `SoundPeriodFrames` small (64–128) and enable `SoundTimedScheduling = 1` and `SoundAudioClockedADSR = 1`. This preserves envelope crispness and onset precision regardless of ring depth.
- A deeper ring provides extra safety headroom without increasing the device queue, but any audio mixed far ahead cannot be altered later. If you want late ADSR/starts to affect pending audio, keep the ring shallow (or effectively off) and use the device queue for headroom.

Why quality stays the same without a deep ring:
- Same mixing/resampling pipeline and bit depth; fidelity doesn’t change.
- Small `SoundPeriodFrames` gives fine change granularity.
- Timed scheduling and audio‑clocked ADSR align starts and envelopes to the playback clock.

## Parameters and Defaults

All preferences live in `localisation/data/prefs_default.txt` and can be overridden in user prefs.

- `SoundUsePushMode` (default: `1`)
  - 1 = push/queue mode, 0 = SDL callback mode.

- `SoundPeriodFrames` (default: `128`)
  - Render quantum (frames) per slice. Smaller = crisper response and envelope shape; higher CPU and queue ops.

- `SoundUpdatePeriod` (default: `0.02`)
  - Interval between `SoundSystem::Advance` passes. Keep it longer than a single slice (e.g. ≥ 4–8 × `SoundPeriodFrames / sampleRate`) so the main thread can reshuffle channels before the queue dries up, while still staying comfortably below the overall latency target. Shorter periods reduce control latency but add main-thread work.

- `SoundTargetLatencyMs` (default: `95`)
  - Target audible latency for scheduling new onsets. Used to compute the earliest start time relative to the current playback cursor. This does not force the device queue size; see device low/high below. The shipping default pairs with the balanced queue profile to cushion moderate CPU spikes without heavy latency.


- `SoundDeviceQueueLowMs` / `SoundDeviceQueueHighMs` (defaults: `45` / `70`)
  - Device queue refill bounds used in push mode. The defaults favour a balanced trade-off between underrun headroom and latency on modest CPUs; raise them if you encounter underruns or lower them to chase faster reaction. The feeder tops up to High when the queue drops below Low.

- `SoundRingMs` (default: `80`)
  - Software ring buffer horizon to keep pre‑mixed ahead of `copyIndex`. This is your underrun safety margin that can still be rewritten until copied to the device. Shipping defaults keep the ring slightly above the device queue high watermark to retain a rewrite window without letting large stale regions build.

- `SoundTimedScheduling` (default: `1`)
  - Enables timestamped scheduling of new sound starts on the audio timeline (prevents missed onsets at high safety).

- `SoundAudioClockedADSR` (default: `1`)
  - Drives ADSR against the audio playback clock instead of wall clock when push mode is active. Ensures the envelope you hear is aligned to playback, not to when the event was created.

- `SoundResamplerSfx` (default: `linear`)
  - `linear` is the default interpolation that replaced nearest neighbor, a huge improvement but not perfect
  - Resampler kernel for sound effects; `sinc64` gives sharper reconstruction than linear without a large CPU hit.

- `SoundResamplerMusic` (default: `linear`)
  - The same applies above for `linear`
  - Resampler kernel for music streams; `sinc128` favours higher fidelity for long-running content.
  - In the Sound Options menu, `Sound FX Quality` and `Music Quality` expose these settings with `Normal` → `linear`, `High` → `sinc64`, and `Very High` → `sinc128`.

The defaults above mirror the **Small CPU, Balanced** preset described later. Use it as the new baseline and adjust in either direction once you have validated underrun headroom on your target machines.

Related (for callback mode):

- `SoundBufferSize` (default: `512`)
  - SDL callback block size. Larger values smear fast changes and ADSR in callback mode. Not used to size the period in push mode.

## How It Works (Push Mode)

- The feeder thread mixes small slices of size `SoundPeriodFrames` into a power‑of‑two ring buffer.
- The device queue is kept short using `SoundDeviceQueueLowMs/HighMs`. When the queue dips below Low, the feeder copies slices from the ring into SDL until it reaches High.
- In the background, the ring is topped up to `SoundRingMs` ahead of the device copy index.
- New sounds are scheduled to start at the earliest future sample time that isn’t already in the device queue (timeline scheduling), optionally anchoring ADSR to that start.

Notes on the ring vs device queue:
- The device queue determines audible latency. Larger Low/High values increase latency but improve underrun resilience.
- The ring horizon (`SoundRingMs`) determines how far ahead audio is pre‑mixed. Larger horizons increase safety but create a region of future audio that won’t reflect late changes.

## What “Crisp” Means

- Fast ADSR: Envelopes change shape within small slices; with audio‑clocked ADSR on, the envelope aligns to playback time.
- Fast scheduling: New onsets are placed at the next audible frames not yet in the device, avoiding missed short bleeps. Audible reaction is limited by the current device queue length by design.

## Recommended Settings

### CPU‑Class Presets (current implementation)

All presets keep quality features on: `SoundUsePushMode = 1`, `SoundTimedScheduling = 1`, `SoundAudioClockedADSR = 1`. They differ only in latency (device queue) and safety horizon (ring). Each profile lists a single ready‑to‑use value per knob so you can copy it straight into prefs.

Preset A — Fast CPU, Low Latency
- `SoundPeriodFrames = 64`
- `SoundDeviceQueueLowMs = 12`
- `SoundDeviceQueueHighMs = 24`
- `SoundRingMs = 28` (kept a touch higher than High for a small rewrite window)
- `SoundTargetLatencyMs = 40`

Preset B — Small CPU, Balanced (default shipping profile)
- `SoundPeriodFrames = 128`
- `SoundDeviceQueueLowMs = 45`
- `SoundDeviceQueueHighMs = 70`
- `SoundRingMs = 80` (High + ~10 ms keeps a safety cushion without stale audio)
- `SoundTargetLatencyMs = 95`

Preset C — Ultra‑Safe (fallback when spikes are severe)
- `SoundPeriodFrames = 128`
- `SoundDeviceQueueLowMs = 90`
- `SoundDeviceQueueHighMs = 140`
- `SoundRingMs = 160` (roughly High + 20 ms for maximum rewrite room)
- `SoundTargetLatencyMs = 180`

Tuning guidance:
- Start from the closest preset and adjust Low/High upward if underruns occur. Keep `SoundPeriodFrames` small to preserve crispness.
- Prefer increasing device Low/High (latency) over increasing period size on small CPUs. Only raise period to 256 if unavoidable.
- Keep `SoundRingMs` slightly above High so you retain a rewrite window without letting a stale "future mix" build up.

## Overlay Diagnostics (F3)

In the SDL 2D section:

- Push mode: yes/no
- Device period: frames (ms)
- Queued latency: bytes (ms) – runtime reported by SDL
- Device queue: frames (ms) – current copy‑ahead window
- Ring fill/horizon: frames (ms) / configured ms
- Device low/high: ms
- Target latency: ms (for onset scheduling)
- Timed scheduling: yes/no
- Audio‑clocked ADSR: on/off
- Playback cursor: frames (ms)
- Total queued audio: frames (s)

In the Preferences section:

- Use push mode, Period frames, Target latency
- Device low/high, Ring horizon, Timed scheduling, Audio‑clocked ADSR

## Callback Mode Notes

- In callback mode, `SoundBufferSize` directly controls granularity: bigger buffers smear changes and ADSR more.
- If you must use callback mode, keep `SoundBufferSize` small (e.g., 256) to reduce smear, or sub‑slice internally (not enabled by default).

## Trade‑offs and Tips

- CPU: Smaller `SoundPeriodFrames` increases mixing calls. Start at 128 and move to 64 if needed and CPU allows.
- Stability: If underruns occur, first increase `SoundRingMs`, then, if absolutely necessary, slightly raise device low/high.
- Physics: You cannot make audible changes earlier than the device queue length. The ring gives safety without increasing audible delay.

## Why Small CPUs Don’t Need Larger Periods

- Period controls change granularity, not total mix work per second. Mixing 44100 frames/sec costs roughly the same whether done in 64‑frame or 256‑frame chunks; the main difference is small per‑slice overhead.
- Buy headroom with latency instead of coarsening granularity: keep `SoundPeriodFrames` small (64–128) for envelope sharpness and precise onsets, and increase device Low/High so small CPUs have time to mix without underruns.

Net effect: same audio quality and “speedy” envelopes/onsets across machines; only the audible delay differs with CPU headroom.

## Summary

Use push mode with small periods for crisp ADSR and parameter changes. Keep the device queue short for fast audible reaction and push extra safety into the software ring horizon. Enable timed scheduling and audio‑clocked ADSR so onsets and envelopes align with the audio timeline.
