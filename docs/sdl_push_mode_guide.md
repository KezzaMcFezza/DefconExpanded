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

## Parameters and Defaults

All preferences live in `localisation/data/prefs_default.txt` and can be overridden in user prefs.

- `SoundUsePushMode` (default: `1`)
  - 1 = push/queue mode, 0 = SDL callback mode.

- `SoundPeriodFrames` (default: `128`)
  - Render quantum (frames) per slice. Smaller = crisper response and envelope shape; higher CPU and queue ops.

- `SoundTargetLatencyMs` (default: `80`)
  - Target audible latency for scheduling new onsets. Used to compute the earliest start time relative to the current playback cursor. This does not force the device queue size; see device low/high below.


- `SoundDeviceQueueLowMs` / `SoundDeviceQueueHighMs` (defaults: `20` / `35`)
  - Device queue refill bounds used in push mode. Keep these small to get fast audible reaction. The feeder tops up to High when the queue drops below Low.

- `SoundRingMs` (default: `160`)
  - Software ring buffer horizon to keep pre‑mixed ahead of `copyIndex`. This is your underrun safety margin that can still be rewritten until copied to the device.

- `SoundTimedScheduling` (default: `1`)
  - Enables timestamped scheduling of new sound starts on the audio timeline (prevents missed onsets at high safety).

- `SoundAudioClockedADSR` (default: `1`)
  - Drives ADSR against the audio playback clock instead of wall clock when push mode is active. Ensures the envelope you hear is aligned to playback, not to when the event was created.

Related (for callback mode):

- `SoundBufferSize` (default: `512`)
  - SDL callback block size. Larger values smear fast changes and ADSR in callback mode. Not used to size the period in push mode.

## How It Works (Push Mode)

- The feeder thread mixes small slices of size `SoundPeriodFrames` into a power‑of‑two ring buffer.
- The device queue is kept short using `SoundDeviceQueueLowMs/HighMs`. When the queue dips below Low, the feeder copies slices from the ring into SDL until it reaches High.
- In the background, the ring is topped up to `SoundRingMs` ahead of the device copy index.
- New sounds are scheduled to start at the earliest future sample time that isn’t already in the device queue (timeline scheduling), optionally anchoring ADSR to that start.

## What “Crisp” Means

- Fast ADSR: Envelopes change shape within small slices; with audio‑clocked ADSR on, the envelope aligns to playback time.
- Fast scheduling: New onsets are placed at the next audible frames not yet in the device, avoiding missed short bleeps. Audible reaction is limited by the current device queue length by design.

## Recommended Settings

### A) Low Latency, Crisp Sound (fast reaction)

- `SoundUsePushMode = 1`
- `SoundPeriodFrames = 64` or `128`
- `SoundDeviceQueueLowMs = 10–20`
- `SoundDeviceQueueHighMs = 20–35`
- `SoundRingMs = 60–120`
- `SoundTimedScheduling = 1`
- `SoundAudioClockedADSR = 1`

Notes:
- Keep the device queue small; you’ll hear changes roughly within this window.
- Keep the ring horizon modest but sufficient for your workload; increase if you see underruns.

### B) Higher Safety, Still Crisp Sound (deeper headroom)

- `SoundUsePushMode = 1`
- `SoundPeriodFrames = 64` or `128`
- `SoundDeviceQueueLowMs = 15–25`
- `SoundDeviceQueueHighMs = 30–45`
- `SoundRingMs = 120–200` (or higher if needed)
- `SoundTimedScheduling = 1`
- `SoundAudioClockedADSR = 1`

Notes:
- Device queue remains short for fast audible reaction; the ring carries the extra safety.
- Timeline scheduling guarantees short onsets aren’t missed inside the ring horizon.

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

## Summary

Use push mode with small periods for crisp ADSR and parameter changes. Keep the device queue short for fast audible reaction and push extra safety into the software ring horizon. Enable timed scheduling and audio‑clocked ADSR so onsets and envelopes align with the audio timeline.
