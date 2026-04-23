# Video pipeline

This document describes how SatyrAV plays video files: the D3D11VA hardware decode path, the software decode fallback, the preload window, and the audio/video synchronisation strategy.

## Decode strategy

SatyrAV v1.1.0 uses a **two-tier decode strategy**:

1. **D3D11VA hardware decode (Windows, primary).** The GPU's video decode engine (DXVA2/D3D11VA) decodes H.264/HEVC frames directly into VRAM textures. The decode thread holds `AVFrame` references to these GPU textures in a small ring (4–16 slots). On the main thread, `av_hwframe_transfer_data` reads the NV12 data from the idle GPU texture into a pre-allocated system-memory frame, then `SDL_UpdateNVTexture` uploads it to SDL's streaming texture for display. Because the source texture has been idle for several ring slots by the time it's displayed, the transfer returns quickly with no GPU pipeline stall.

2. **Software decode fallback (Linux, or when D3D11VA is unavailable).** FFmpeg's multi-threaded software decoders (H.264/HEVC, `FF_THREAD_FRAME | FF_THREAD_SLICE`) produce YUV420P frames; `sws_scale` converts to NV12 (and optionally downscales to slave resolution) directly into pre-allocated ring frames. `SDL_UpdateNVTexture` uploads to the GPU, and SDL's D3D11/OpenGL renderer does the YUV→RGB conversion in-shader.

Both paths are combined with the **preload architecture** described below — videos are decoded ahead of time during scene preload, so the decode thread is already well ahead of the playback clock when the operator fires the cue.

## Why preload

Video cues in theatre shows are scripted — the operator knows the next few scenes in advance. SatyrAV exploits that: instead of putting decoding on the critical path at play-time, videos referenced by the current, next, and previous scene are **decoded into the ring in the background** while the operator is still driving earlier cues. When the operator hits ENTER on a `play movie.mkv` command the decode thread is already ahead of the playback clock, so the first frames go out instantly.

v1.0 opened, decoded, and played all at once when the cue fired. At 4K60 that path could not sustain real-time software decode + YUV→RGBA conversion + upload, so the ring drained and playback stuttered or froze. v1.1.0 adds hardware decode as the primary path and moves all remaining work off the critical path via preloading.

## Architecture

### Per-video decode contexts

`MediaPlayer` owns a map of `VideoPlayback` instances keyed by resolved file path:

```cpp
std::unordered_map<std::string, std::unique_ptr<VideoPlayback>> videos;
VideoPlayback* active = nullptr;
```

Each `VideoPlayback` is a fully self-contained decode context: it owns its `AVFormatContext`, video + optional audio `AVCodecContext`, `SwsContext`, `SwrContext`, a pre-allocated NV12 ring, a per-video audio buffer, and its own decode thread. Multiple instances can exist concurrently — one is `active` (currently playing to the slave window and driving the audio device); the others are preloaded and idle.

### NV12 ring

The ring serves two modes:

- **HW mode (D3D11VA):** Ring slots hold `AVFrame` clones containing references to D3D11 textures in VRAM. The ring is small (4–16 slots) because GPU decoder surfaces are a finite hardware resource. When a frame is consumed, the main thread calls `av_hwframe_transfer_data` to read the NV12 planes into a pre-allocated `transferFrame`, then uploads via `SDL_UpdateNVTexture`. The clone is freed, returning the decoder surface to the pool.

- **SW mode:** Ring slots are `AVFrame`s allocated once via `av_frame_get_buffer(f, 32)`, each with its own NV12 Y+UV planes. `sws_scale` writes directly into `dst->data`/`dst->linesize` — zero intermediate copies. The main thread reads `f->data[0]`/`f->data[1]` and calls `SDL_UpdateNVTexture`. Ring frames are recycled for the life of the `VideoPlayback`; no allocation churn.

### Resolution handling

- **HW mode:** The texture is created at source resolution. The GPU decoder produces frames at the native size; scaling to the slave display happens for free during `SDL_RenderCopy` at present-time.
- **SW mode:** The ring texture is created at **slave resolution**. For a 4K source on a 1080p slave, `sws_scale` downscales as part of its NV12 conversion, capping main-thread upload cost at 1080p regardless of source resolution. Smaller-than-slave sources keep their native resolution; the GPU upscales for free at present-time.

### 4 GB budget, priority fill

When the operator enters a scene (or the scene window slides via navigation), `PerformanceScreen::PreloadForCurrentScene` builds a priority-ordered list of video paths — current scene in command order, then next scene, then previous scene — and calls `MediaPlayer::PreloadVideosForScenes`. For each path in order:

1. Probe the container for source resolution, fps, duration.
2. Compute `frameBytes = outWidth * outHeight * 3/2`.
3. Compute `ringFrames = min(clipFrames, maxSecondsCap * fps, remainingBudget / frameBytes)`.
4. If `ringFrames` is big enough to be useful (≥60 frames ≈ 1 s at 60fps), create the `VideoPlayback` and spawn its decode thread. Otherwise stop — remaining videos fall through to the on-demand path.

Videos that have dropped out of the prev/curr/next window are evicted immediately (their decode threads are stopped and their RAM is returned to the budget). Videos still in the window are held across scene transitions.

Both the budget (`videoPreloadBudgetMB`, default 4096) and the per-video cap (`videoPreloadMaxSeconds`, default 60) live in the global config under `[video]` and can be overridden by the user.

### Back-pressure

The decode thread produces video frames until the ring is full, then `SDL_CondWait`s on `frameConsumed`. This is exactly the right behaviour for both modes:

- **Preload**: the ring fills completely in the background, the thread parks, and the `VideoPlayback` sits Ready. No CPU spent until playback starts or the slot is freed.
- **Playback**: the main thread consumes a frame per tick, signalling `frameConsumed` — the decode thread wakes, decodes the next frame, parks again. Steady-state, the ring stays near full.

Audio packets from the same container are never blocked by the video ring: the decode thread decodes and resamples them immediately into the per-video audio buffer. If the audio buffer fills (during preload of a long clip) the thread briefly `SDL_Delay`s and retries — it does not drop audio, and it does not block video decode (the video path only blocks on the video ring).

### Play-time dispatch

`PerformanceScreen::OnSlaveCommand` still calls `media.PlayVideo(path, …)`. `PlayVideo` now checks whether the path is already preloaded:

- **Preloaded** → `StartPreloadedVideo` transitions it to the `active` slot, opens the SDL audio device with its sample rate and channel count, waits up to 1 second for 60 frames of headroom (usually returns immediately), and starts playback.
- **Not preloaded** (budget exhausted, unexpected path, or standalone test) → falls through to an on-demand synchronous preload: creates a new `VideoPlayback`, spawns its decode thread, then activates it the same way. The 1 s / 60-frame wait is the fallback's initial buffering.

### Activation and audio routing

There is one `SDL_AudioDeviceID` at a time. When a video becomes active, the device is closed and reopened with `userdata = &active->audioBuffer` and a callback that reads directly from that buffer. When a video finishes or `StopAll` is called, the device is closed and the VideoPlayback is erased from the map.

Standalone audio (`.wav`/`.mp3` cues) uses its own `StandaloneAudio` state and a shared `standaloneBuf`; starting standalone audio closes any active video and reopens the device pointed at `standaloneBuf`.

## Clock

The audio-master clock is unchanged from v1.0 in principle: when `active->hasAudio`, the clock is derived from `active->audioBuffer.totalConsumed` — the exact byte count the SDL callback has pulled out of the buffer, which is driven by the sound card's DAC and therefore cannot drift.

```
audioPosition = audioStartPTS + (totalConsumed / bytesPerSecond)
```

For video-only files, `SDL_GetPerformanceCounter` drives the clock. Either way, the main thread scans the ring for the latest frame whose PTS is ≤ the clock, skips any earlier frames, uploads that one via `SDL_UpdateNVTexture`, and signals `frameConsumed`.

## Lifecycle summary

```
Scene preload (background):
    probe → compute ringFrames from budget → Setup → spawn decode thread
    decode thread: read packets → decode → sws → ring slot → block when full

Play command:
    if preloaded → activate (close/reopen audio device, wait ≤1s for 60 frames)
    else         → synchronous preload + activate

Playback:
    main thread UpdateVideoFrame (per tick):
        compute clock from audio buffer consumed bytes
        scan ring → pick latest frame with pts ≤ clock
        SDL_UpdateNVTexture(ring frame's Y + UV planes)
        signal frameConsumed
    SDL audio callback: read from active->audioBuffer → DAC

Scene navigation:
    rebuild priority list → evict out-of-window videos → preload new ones

End of clip:
    bufferedFrames hits 0 with threadRunning == false
    → erase from videos map, audio device closes
```

## Memory usage

| Slave output | Per slot (NV12) | Per 60 s at 60 fps |
|---|---|---|
| 1080p | 3.1 MB  | ~186 MB |
| 1440p | 5.5 MB  | ~330 MB |
| 4K    | 12.4 MB | ~744 MB |

At the 4 GB default budget you can preload ~22 minutes of 1080p video, or ~4 minutes at native 4K. Since the scene window only holds prev/curr/next, the budget in practice is dedicated to whichever 1–4 clips are currently in-window.

## Threading model

```
Main thread               Decode thread(s)          SDL audio thread
   |                          | (one per                |
   |                          |  VideoPlayback)         |
   | UpdateVideoFrame         | av_read_frame           | AudioCallback
   |   find best frame        |   audio → audioBuffer   |   read from
   |   SDL_UpdateNVTexture    |   video → sws → ring    |     active->audioBuffer
   |   signal frameConsumed   |   block when ring full  |   increment totalConsumed
   | Present                  |                         |
```

Idle preloaded videos consume zero CPU — their decode threads are parked on `frameConsumed`. Only the active video's decode thread is doing ongoing work.

## Renderer backend

`Renderer::InitMaster` sets `SDL_HINT_RENDER_DRIVER=direct3d11` on Windows. This is critical for two reasons:

1. SDL's D3D11 renderer has a built-in NV12→RGB shader, so `SDL_UpdateNVTexture` + `SDL_RenderCopy` does YUV→RGB conversion entirely on the GPU.
2. `SDL_RenderGetD3D11Device` returns the `ID3D11Device*` that we pass to FFmpeg's `AVD3D11VADeviceContext`, so the hardware decoder and SDL share the same D3D11 device. This avoids cross-device copies.

On Linux, SDL falls through to its OpenGL renderer, which also supports NV12. D3D11VA is not available; the software decode path is used.

## Known limitations

- **YUV420P / NV12 assumed.** 10-bit (P010) sources are not supported unless the decoder silently downcasts.
- **No seeking.** Playback is strictly linear from start to EOF.
- **One active video at a time.** Multiple preloaded instances can coexist; only one plays.
- **HW readback still involves a copy.** `av_hwframe_transfer_data` copies NV12 from VRAM to system RAM, then `SDL_UpdateNVTexture` copies it back. True zero-copy (wrapping the D3D11VA output texture directly as an SDL texture) would eliminate both copies but requires SDL3 or direct D3D11 interop. The current path is still dramatically faster than software decode because the GPU's video decode engine runs in parallel with the CPU and the readback latency is hidden by the ring buffer.
