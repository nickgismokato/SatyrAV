# Video pipeline

How SatyrAV plays a video file: the three ways a frame can reach the screen, the preload window, how sound and picture are kept together, and what runs on which thread. Current as of **2.0.0**. Everything here lives in [`MediaPlayer.cpp`](../src/render/MediaPlayer.cpp) and [`D3D11Interop.cpp`](../src/render/D3D11Interop.cpp).

## Three decode paths

Each clip picks one path when it is set up, and keeps it for its lifetime.

| Path | Used when | Where the frame travels |
|---|---|---|
| **Zero-copy** *(2.0.0)* | Windows, D3D11 renderer, hardware decode available, 8-bit source, `zero_copy = true` | Decoder → GPU copy → screen. Never leaves VRAM. |
| **Hardware read-back** | Hardware decode available but zero-copy is not (10-bit source, `zero_copy = false`, or zero-copy setup failed) | Decoder (VRAM) → system RAM → `sws_scale` → VRAM |
| **Software** | Linux, or a codec the GPU cannot decode | CPU decoder → `sws_scale` → VRAM |

The debug overlay (`D`) names the path the playing clip is on: `Hardware (zero-copy)` in green, `Hardware (read-back)` in white, `Software` in orange. The log records the choice for every clip, including clips that are only preloaded.

### Zero-copy

The decoder runs on the **slave renderer's own D3D11 device** rather than a private one. A decoded frame is a surface in VRAM; the frame ring stores a *reference* to it. When the frame is due, the main thread copies that surface into an NV12 texture with a single GPU-to-GPU `CopySubresourceRegion` and drops the reference, which hands the surface back to the decoder.

It is a copy rather than a hand-over because decoder surfaces are decoder-only slices of one texture array, and SDL cannot sample from those. The copy is cheap (it is submitted in microseconds and done by the GPU), and it has a useful side effect: the picture lives on in SatyrAV's own texture, which is what keeps the last frame on screen during a pause.

Three details follow from this design:

- **The texture is the size of the decoder surface, not the picture.** A 1080p H.264 clip decodes into 1920×1088 surfaces; HEVC pads further. The video is therefore drawn with a source rectangle that crops to the picture (`MediaPlayer::GetVideoSrcRect`), stopping half a texel short of the padding so that scaling up cannot blend padding into the edge.
- **No CPU scaling.** The frame stays at source resolution and the renderer scales it when drawing.
- **The decoder must be given enough surfaces.** Its default pool is about 20. The ring holds 32 references, so the decoder is opened with `extra_hw_frames` to cover the ring plus a few spare.

### Hardware read-back and software

These are the pre-2.0.0 paths and are unchanged. Both end with YUV420P frames in system RAM, scaled down to fit the slave if the source is larger, uploaded with `SDL_UpdateYUVTexture` to an IYUV texture.

- **Read-back:** the decode thread calls `av_hwframe_transfer_data` to pull the frame out of VRAM, then `sws_scale` converts it. The transfer format is whatever the driver produces (usually NV12), so the conversion context is created on the first frame once the real format is known.
- **Software:** FFmpeg's multi-threaded decoders produce the frame and `sws_scale` converts it straight into the ring.

The ring is YUV420P rather than NV12 because libswscale's NV12-to-NV12 path was found to leave the UV plane empty, giving a green picture.

### Falling back

Every step down is automatic and logged:

- No D3D11 renderer, or the device refuses multithread protection → the decoder creates a device of its own and uses read-back.
- The decoder will not open on the shared device → it retries on a device of its own.
- A 10-bit source → read-back, where it is converted down to 8-bit for display.
- No hardware decode for the codec → software.
- A clip's **first** zero-copy frame cannot be presented → zero-copy is switched off for the rest of the session and that clip restarts on the read-back path.

`[video] zero_copy = false` in `config.toml` forces read-back on a device of the decoder's own, which is exactly the 1.x route.

## Why the shared device has to be thread-safe

The decoder works on the decode thread while SDL draws on the main thread, both through one D3D11 device. SDL creates its device **single-threaded** unless `SDL_HINT_RENDER_DIRECT3D_THREADSAFE` is set before the renderer is created, and a single-threaded device used from two threads is undefined behaviour that does not announce itself. `Renderer.cpp` sets the hint for every renderer, and `D3D11Interop::CreateSharedDeviceCtx` switches the device into multithread-protected mode and refuses to share it if that fails. The GPU copy itself is made under the same lock FFmpeg's decoder uses.

## Preload

Video cues are scripted, so the next few are known in advance. When the operator enters a scene, `PerformanceScreen::PreloadForCurrentScene` lists the videos of the current scene, then the next, then the previous, and `MediaPlayer::PreloadVideosForScenes` sets each one up in that order. Every `VideoPlayback` is self-contained: its own demuxer, decoders, ring, audio stream and decode thread. Clips that fall out of the three-scene window are evicted at once.

A preloaded clip decodes ahead and then waits, so when `play` fires the first frames are already there.

### How far ahead

| Path | Ring depth | Held in |
|---|---|---|
| Zero-copy | 32 frames, fixed | VRAM |
| Read-back, software | up to `preload_max_seconds` (default 60 s), limited by the budget | System RAM |

The zero-copy ring is shallow because decoder surfaces are a finite pool. Thirty-two frames is about half a second at 60 fps: enough for an instant start, with the hardware decoder refilling it many times faster than playback drains it. What is given up, compared with the RAM ring, is cushion against a slow disk.

### Budget

`[video] preload_budget_mb` (default 4096) caps the total across all preloaded clips, filled in priority order. A RAM ring costs `width × height × 1.5` bytes per frame at output size. A zero-copy clip is charged an estimate of its VRAM: the decoder's whole surface pool (about 56 surfaces) at padded source size, roughly 175 MB for a 1080p clip and 700 MB for 4K.

| Output size | Per frame | 60 s at 60 fps (RAM ring) |
|---|---|---|
| 1080p | 3.1 MB | ~11 GB, so the budget caps it at ~22 s |
| 1440p | 5.5 MB | capped at ~13 s |
| 4K | 12.4 MB | capped at ~6 s |

### Back-pressure

The decode thread fills the ring and then waits for the main thread to consume a frame. Sound is throttled separately: at most about 8 seconds of audio are queued per clip.

On the zero-copy path the wait is not idle. The playback clock runs on audio, and a thread that stops reading the file when 32 frames are buffered also stops reading audio. If a file interleaves its audio further apart than those 32 frames cover (routine at 60 fps), the audio runs dry, the clock stops, no frame is consumed, and the ring never drains. So while it waits for a free slot, the decode thread keeps reading the file whenever less than about 2 seconds of audio is queued: audio packets are decoded at once, and video packets are parked, still compressed, until slots free up.

## Sound

*(Rewritten in 2.0.0.)* One logical audio device is opened on first use and held open for the life of the application. Every source of sound owns an `SDL_AudioStream` bound to it, and SDL mixes them:

- Each video creates its stream at setup and leaves it **unbound**, so a preloaded clip can queue its sound ahead of time. Starting the clip binds the stream.
- Each standalone sound (`play x.mp3`, F-key hotkeys) has its own stream. Several can play at once, over each other and over a video's sound.

Streams convert independently into the device's format, so clips with different sample rates coexist without the device being reopened. The device follows `[audio] device` in `config.toml` and falls back to the system default if the named device is missing.

The optional `AudioFade` project/scene option ramps a video stream's gain together with the picture fade. SDL applies stream gain when the device pulls audio, not when it is queued, so a fade is heard immediately even though seconds of audio are already queued.

*(2.0.1)* **Pausing one source.** `K` pauses the video and its sound; `M` pauses the standalone sounds. Neither pauses the device, because pausing the device would silence every source on it. A paused source's stream is unbound instead (its queued audio is kept) and bound again on resume, so the other sources play on.

*(2.0.1)* **No sound device.** If no audio output can be opened — no device at start-up, or a device that stops taking audio mid-clip — videos play without sound on the wall clock instead of waiting for audio that never drains. This is logged once. Standalone sounds cannot play in that state.

## Clock

Sound is the master. For a clip with audio:

```
position = audioStartPTS + (bytesPushed − bytesStillQueued) / bytesPerSecond
```

`bytesPushed` is what the decode thread has handed to the stream; `SDL_GetAudioStreamQueued` reports what the device has not yet taken. The difference is what the sound card has actually consumed, so the clock is driven by the hardware and does not drift. Between the device's pulls the value is interpolated with wall-clock time so it advances smoothly every frame.

A clip without audio uses the wall clock. *(2.0.1)* So does a clip whose sound stops reaching the device: a watchdog notices when the consumed count has not moved for 0.75 s, re-anchors the wall clock at the current position and carries on, so the picture never freezes waiting for sound. A sound track that is simply shorter than the picture takes the same route. Either way, each frame the main thread takes the newest ring frame whose timestamp is not later than the clock, skips any older ones, and presents it.

## Seeking

*(2.0.0, plumbing only.)* `MediaPlayer::SeekVideo(seconds)` jumps the active clip to a time. Nothing in the application calls it yet; the fast-forward, rewind and restart commands are planned for 2.1.0.

The main thread files the request; the decode thread carries it out at the top of its loop, the one point where it holds no half-produced frame. It empties the ring, seeks the file to the keyframe before the target, flushes both decoders, clears the queued audio, and then discards the frames and audio between that keyframe and the requested time, so playback resumes where asked. While the request is pending the main thread presents nothing from the ring.

At end of file the decode thread parks instead of exiting, which is what allows a clip to be rewound after its decoding has finished.

## Threads

```
Main thread                 Decode thread (one per clip)        SDL audio thread
  UpdateVideoFrame            read packet                          pull from every
    read the clock            audio → decode → audio stream          bound stream,
    pick the due frame        video → decode → ring slot             mix, play
    zero-copy: GPU copy         (zero-copy: a surface reference;
    otherwise: texture upload    otherwise: pixels in RAM)
  draw, present               wait when the ring is full
                              park at end of file
```

Preloaded clips that have filled their ring use no CPU. Only the playing clip's decode thread does ongoing work.

## Renderer

`Renderer` asks SDL for the `direct3d11` driver by name and falls back to SDL's own choice if that fails (on Linux this gives OpenGL). SDL converts YUV to RGB in a shader on every path. Both the NV12 and IYUV textures use SDL's default colour conversion (BT.601, limited range), so a clip looks the same whichever path it takes.

## Settings

In `config.toml`:

| Key | Default | Meaning |
|---|---|---|
| `[video] zero_copy` | `true` | Allow the zero-copy path. `false` restores the 1.x read-back route. |
| `[video] preload_budget_mb` | `4096` | Total budget across preloaded clips. |
| `[video] preload_max_seconds` | `60` | Longest RAM ring per clip. |
| `[audio] device` | empty | Output device by name; empty means the system default. |

## Known limitations

- **One video plays at a time.** Several can be preloaded; one is active.
- **10-bit sources lose their extra depth.** They are kept off the zero-copy path and converted to 8-bit. This has not been tested with a real 10-bit file.
- **The last frames of a clip are dropped,** because the decoder is not flushed at end of file. A few frames with hardware decode; up to one per decoder thread with software decode.
- **On the read-back and software paths the video is decoded at the size of the targeted-display rect,** so a shrunk rect lowers the resolution the capture mirror receives. Zero-copy is not affected.
- **The capture mirror still reads the composed frame back through system RAM** every frame. That is separate from video decoding and is the leading suspect for the capture mirror's stutter.
- **Seeking has no commands yet,** and a seek made while paused shows its picture only on resume.
