#pragma once

struct SDL_Renderer;
struct SDL_Texture;
struct AVBufferRef;
struct AVFrame;

namespace SatyrAV{

// (2.0.0) Everything that needs <d3d11.h> lives behind this header, so the
// Windows headers (and their macros) stay out of MediaPlayer. On platforms
// other than Windows every function is a stub that reports "unavailable",
// which sends callers down the existing software path.
namespace D3D11Interop{

// Builds an FFmpeg D3D11VA device context on top of the D3D11 device that
// `renderer` already owns, so the hardware decoder and SDL share one device.
// The device is put into multithread-protected mode first, because the
// decoder runs on the decode thread while SDL draws on the main thread.
//
// Returns a new reference the caller must av_buffer_unref, or nullptr when
// sharing is not possible (not Windows, renderer is not D3D11, or the device
// refuses multithread protection). nullptr is not an error: the caller falls
// back to letting FFmpeg create a device of its own, as it did before 2.0.0.
AVBufferRef* CreateSharedDeviceCtx(SDL_Renderer* renderer);

// The zero-copy step. Copies a hardware-decoded frame (AV_PIX_FMT_D3D11)
// into `*texture` entirely on the GPU — the frame never visits system RAM
// and no pixel-format conversion runs on the CPU.
//
// It is a GPU-to-GPU copy rather than a hand-over because the decoder's
// surfaces are decoder-only slices of one texture array, which SDL cannot
// sample from. The copy also means the decoder surface can be released the
// moment this returns, and the picture survives in `*texture` (which is what
// keeps the last frame on screen during a pause).
//
// `*texture` is created on first use, and recreated if the decoder surface
// size changes. It is sized to the decoder SURFACE, which is larger than the
// picture (1920x1088 for a 1080p H.264 clip, more for HEVC), so the caller
// must draw it with a source rectangle that crops to the picture.
//
// Main thread only. `deviceCtx` must be the context CreateSharedDeviceCtx
// returned for this same renderer. Returns false — touching nothing — if the
// frame is not an 8-bit NV12 surface or any step fails.
bool CopyFrameToTexture(SDL_Renderer* renderer, AVBufferRef* deviceCtx,
	const AVFrame* frame, SDL_Texture** texture);

} // namespace D3D11Interop
} // namespace SatyrAV
