#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>

extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace SatyrAV{

// (2.0.0) The hand-rolled `AudioBuffer` ring that used to live here is gone.
// An SDL_AudioStream *is* a thread-safe buffer: the producer calls
// SDL_PutAudioStreamData and the device pulls from the other end, so the
// ring, its mutex, and the pull callback that fed it were all redundant.
// SDL_GetAudioStreamQueued replaces `totalConsumed` as the clock source —
// it reports bytes in the stream's INPUT format, so the clock arithmetic is
// unchanged.

// ── Per-video decode context ────────────────────────────────
//
// (2.0.0) Three ways a frame reaches the screen. The first is new and is
// used whenever it can be; the other two are unchanged and remain as the
// automatic fall-backs.
//
//   Zero-copy (D3D11VA on the slave renderer's own device, 8-bit sources):
//     GPU decodes → the ring holds *references* to the decoder's surfaces,
//     which stay in VRAM. The main thread copies the chosen surface into an
//     NV12 SDL texture on the GPU (D3D11Interop::CopyFrameToTexture). No
//     system-RAM round trip and no sws_scale. The ring is shallow (32
//     frames) because decoder surfaces are a finite pool.
//
// The two read-back paths, transparent to the main thread:
//
//   HW (D3D11VA, Windows):
//     GPU decodes H.264/HEVC → D3D11 texture (VRAM).
//     Decode thread calls av_hwframe_transfer_data → system RAM in whatever
//     format the driver produces, then sws_scale converts+downscales to
//     YUV420P@slave into the ring. FFmpeg owns its own D3D11 device
//     (separate from SDL's) so there are zero cross-thread D3D11 calls.
//
//   SW (Linux / fallback):
//     CPU decodes → YUV420P, sws_scale converts+downscales to YUV420P@slave
//     directly into the ring.
//
// In both cases the ring holds YUV420P CPU frames at output resolution and
// the main thread just calls SDL_UpdateYUVTexture on an IYUV texture — same
// code path either way. NV12 was tried first and abandoned: libswscale's
// NV12→NV12 path silently zeroed the UV plane, producing green output.
//
struct VideoPlayback{
	std::string path;

	// FFmpeg
	AVFormatContext* formatCtx     = nullptr;
	AVCodecContext* videoCodecCtx  = nullptr;
	AVCodecContext* audioCodecCtx  = nullptr;
	SwsContext* swsCtx             = nullptr;
	SwrContext* swrCtx             = nullptr;
	AVFrame* decodeFrame           = nullptr;
	int videoStreamIdx             = -1;
	int audioStreamIdx             = -1;
	int srcWidth = 0, srcHeight = 0;
	int outWidth = 0, outHeight = 0;
	double videoTimeBase           = 0.0;

	// HW decode
	bool useHwDecode               = false;
	AVBufferRef* hwDeviceCtx       = nullptr;
	// (2.0.0) True when hwDeviceCtx is the slave renderer's own D3D11 device
	// rather than one FFmpeg created for itself.
	bool sharedDevice              = false;
	// (2.0.0) True when decoded frames stay on the GPU: ring slots then hold
	// references to decoder surfaces instead of YUV420P pixel buffers, and
	// `texture` is an NV12 texture created on the first presented frame.
	bool zeroCopy                  = false;
	bool presentedAny              = false; // a frame has reached `texture`
	bool zeroCopyErrorLogged       = false; // mid-clip copy failure reported once
	int  setupSlaveW = 0, setupSlaveH = 0;  // kept so the clip can be re-set-up

	// (2.0.0) Seek request, main thread → decode thread. The main thread
	// writes `seekTargetSec` and raises `seekPending` under `mtx`; the decode
	// thread does the actual work at the top of its loop — the one point where
	// it holds no half-produced frame — and lowers the flag when the ring,
	// decoders and audio queue have all been reset. While the flag is up the
	// main thread presents nothing from the ring.
	std::atomic<bool> seekPending{false};
	double seekTargetSec = 0.0;

	// (2.0.1) Playing without sound. Set when there is no audio device to
	// play the clip's sound on, or when the device stops taking sound part
	// way through. The decode thread then discards audio, and the clock
	// runs on wall time. Before 2.0.1 a clip in this state froze on its
	// first frame, because its clock is "how much sound has been played".
	std::atomic<bool> audioSilenced{false};
	// (2.0.1) True once the clip's clock has moved from the sound card to
	// wall time — after audioSilenced, or when the sound track simply ended
	// before the picture did.
	bool     wallClockFallback = false;
	// (2.0.1) Clock watchdog: the consumed-byte count last seen, and when it
	// last changed. Zero tick means "start watching afresh".
	size_t   wdLastConsumed   = 0;
	uint64_t wdLastChangeTick = 0;
	AVFrame* transferFrame         = nullptr; // CPU frame from av_hwframe_transfer_data
	AVPixelFormat hwTransferFmt    = AV_PIX_FMT_NONE; // actual fmt detected after first transfer
	int hwTransferW                = 0;       // actual coded width after transfer (may != srcWidth)
	int hwTransferH                = 0;       // actual coded height after transfer (may != srcHeight)

	// Audio
	bool hasAudio                  = false;
	int audioSampleRate            = 0;
	int audioChannels              = 0;
	double audioTimeBase           = 0.0;
	double audioStartPTS           = 0.0;
	bool audioStartPTSSet          = false;
	// (2.0.0) The decode thread pushes converted S16 PCM straight into this
	// stream. It is created at Setup and left *unbound*, which is what lets a
	// preloaded video accumulate its audio ahead of time — an unbound stream
	// still queues. Activation binds it to the shared logical device.
	SDL_AudioStream* audioStream   = nullptr;
	// Total bytes handed to `audioStream`, counted in its INPUT format.
	// Minus SDL_GetAudioStreamQueued this gives the bytes the device has
	// actually consumed, which is the audio-master clock. Written by the
	// decode thread, read by the main thread.
	std::atomic<uint64_t> audioBytesPushed{0};

	// YUV420P ring — pre-allocated AVFrames at outWidth×outHeight.
	// Identical layout for both HW and SW paths.
	std::vector<AVFrame*> ring;
	std::vector<double>   ringPts;
	std::vector<uint8_t>  ringReady;
	int ringCapacity   = 0;
	int writeIdx       = 0;
	int readIdx        = 0;
	int bufferedFrames = 0;
	size_t memoryBytes = 0;

	// Decode thread + sync
	SDL_Thread* decodeThread = nullptr;
	SDL_Mutex*  mtx          = nullptr;
	SDL_Condition*   frameAvailable = nullptr;
	SDL_Condition*   frameConsumed  = nullptr;
	bool stopThread   = false;
	bool threadRunning = false;

	// Smooth audio clock — interpolated between SDL audio callbacks.
	// SDL fires the callback every ~93ms (4096 samples / 44100Hz), so
	// totalConsumed advances in discrete jumps. We anchor the clock
	// each time consumed changes, then add wall-time elapsed since the
	// anchor to get a smooth, per-frame clock value.
	uint64_t audioClockAnchorTick  = 0;
	size_t   audioClockAnchorBytes = 0;
	double   audioClockAnchorPTS   = 0.0;
	bool     audioClockAnchored    = false;

	// Playback state
	SDL_Texture* texture = nullptr;
	bool started = false;
	bool paused  = false;
	double nextPTS = 0.0;
	uint64_t startTick = 0;

	// (1.6.6) Total source duration in seconds, captured from
	// formatCtx->duration during Setup. Used by the fade-out logic to
	// know when to start ramping brightness down; 0 disables fade-out
	// for streams without a known duration.
	double durationSec = 0.0;
	// (1.6.6) Per-playback fade times in seconds (0 = disabled). Set by
	// MediaPlayer::PlayVideo from the parsed `play FILE FADE_IN, FADE_OUT`
	// spec on the cue. Applied as a brightness multiplier on the video
	// texture every frame in UpdateVideoFrame.
	float fadeInSec  = 0.0f;
	float fadeOutSec = 0.0f;

	~VideoPlayback();
	// `sharedHw` (may be null) is a D3D11VA device context built on the
	// renderer's own device — see D3D11Interop. Null means "create your own".
	bool Setup(const std::string& path, SDL_Renderer* renderer,
		int slaveW, int slaveH, int capFrames, AVBufferRef* sharedHw = nullptr,
		bool allowZeroCopy = true, bool allowAudio = true);
	void StopAndClose();
	void DecodeLoop();

	static enum AVPixelFormat GetHwFormat(
		AVCodecContext* ctx, const enum AVPixelFormat* fmts);
};

// ── Standalone audio (F-key SFX and `play x.mp3`) ────────────
//
// (2.0.0) Several of these can now play at once — over each other *and*
// over a video's own audio. Each owns an SDL_AudioStream bound to the
// shared logical device and SDL mixes them.
//
// Before 2.0.0 there was exactly one, and starting it closed the audio
// device and tore down any playing video, because a single device could
// only be pointed at one buffer at a time.
struct StandaloneAudio{
	std::string path;                     // identity, for toggle-off by name
	AVFormatContext* formatCtx = nullptr;
	AVCodecContext* codecCtx   = nullptr;
	SwrContext* swrCtx         = nullptr;
	int streamIndex            = -1;
	int sampleRate             = 0;
	int channels               = 0;
	AVFrame* frame             = nullptr;
	AVPacket* packet           = nullptr;
	SDL_AudioStream* stream    = nullptr;
	bool eof                   = false; // container drained; drain the stream

	~StandaloneAudio();
	void Close();
};

class MediaPlayer{
public:
	bool Init();
	void Shutdown();

	bool PlayVideo(const std::string& path, SDL_Renderer* renderer,
		int slaveW = 0, int slaveH = 0,
		int fadeInMs = 0, int fadeOutMs = 0);
	void StopVideo();
	void ToggleVideoPause();
	bool IsVideoPlaying() const;
	bool HasVideo() const;
	void UpdateVideoFrame(SDL_Renderer* renderer);
	SDL_Texture* GetVideoTexture() const;
	int GetVideoWidth() const;
	int GetVideoHeight() const;
	int GetBufferedFrames() const;
	int GetVideoRingCapacity() const;
	size_t GetPreloadedBytes() const;
	bool IsUsingHwDecode() const;
	// (2.0.0) True when the active video's frames never leave the GPU.
	bool IsVideoZeroCopy() const;
	// (2.0.0) The zero-copy texture is the size of the decoder SURFACE, which
	// is larger than the picture (1920x1088 for a 1080p clip). Returns true
	// and fills `out` with the part that is picture; false means draw the
	// whole texture, as the read-back paths always have.
	bool GetVideoSrcRect(SDL_FRect* out) const;

	// (2.0.0) Seek support. This is the plumbing only: nothing in the app
	// calls SeekVideo yet. The fast-forward / rewind / restart commands that
	// will use it are 2.1.0 work.
	//
	// Jumps the active video to `seconds` (clamped to the clip). Picture and
	// sound both restart from there, in sync. Works on all three decode
	// paths, and after the decoder has already reached the end of the file.
	// While paused, the new picture appears when playback resumes.
	bool   SeekVideo(double seconds);
	// Timestamp, in seconds, of the frame currently on screen; -1 if none.
	double GetVideoPosition() const;
	// Length of the active clip in seconds; 0 if unknown or none.
	double GetVideoDuration() const;

	void PreloadVideosForScenes(const std::vector<std::string>& priorityPaths,
		SDL_Renderer* renderer, int slaveW, int slaveH);
	bool IsVideoPreloaded(const std::string& path) const;

	bool PlayAudio(const std::string& path);
	void StopAudio(const std::string& path = "");
	void ToggleAudioPause();
	bool IsAudioPlaying() const;
	// (2.0.0) Number of standalone sounds currently playing. More than one is
	// now possible — see StandaloneAudio. Surfaced in the debug overlay so an
	// operator can see how many effects are layered.
	int  GetActiveSoundCount() const;
	// (2.0.0) Opt-in: when true, a `play` command's fade times ramp the
	// clip's audio gain alongside its brightness. Driven from the project /
	// scene `audioFade` option via CueEngine::GetEffectiveAudioFade().
	void SetAudioFade(bool enabled);
	void Update();

	bool ShowImage(const std::string& path, SDL_Renderer* renderer);
	SDL_Texture* GetImageTexture() const;
	int GetImageWidth() const;
	int GetImageHeight() const;
	void ClearImage();

	void StopAll();

private:
	std::unordered_map<std::string, std::unique_ptr<VideoPlayback>> videos;
	VideoPlayback* active = nullptr;

	// (2.0.0) One *logical* audio device for the whole application, opened
	// on first use and held open. Every source binds its own stream to it,
	// so nothing closes or reopens the device mid-show and simultaneous
	// sources mix instead of evicting one another.
	//
	// A second benefit falls out of per-stream conversion: clips at
	// different sample rates now coexist. Previously the device was closed
	// and reopened to match each video's rate.
	SDL_AudioDeviceID audioDevice = 0;
	SDL_AudioSpec     deviceSpec{};   // resolved device format, for stream outputs
	std::string       openedDeviceName; // config value the device was opened for

	// (2.0.0) D3D11VA device context layered on the slave renderer's device,
	// shared by every VideoPlayback. Built lazily for the renderer it is first
	// asked about and dropped in StopAll, which always runs before the slave
	// renderer is destroyed. Null when sharing is unavailable or switched off.
	AVBufferRef*  sharedHwDevice   = nullptr;
	SDL_Renderer* sharedHwRenderer = nullptr; // renderer the attempt was made for
	AVBufferRef* SharedHwDeviceFor(SDL_Renderer* renderer);
	void ReleaseSharedHwDevice();
	// (2.0.0) Set if a clip's very first zero-copy frame could not be put on
	// screen. Zero-copy is then off for the rest of the session and clips use
	// the read-back path — a driver that fails once will fail every time.
	bool zeroCopyDisabled = false;

	// (2.0.1) Opening the audio device is retried every time a clip is set
	// up; this keeps a machine with no device from logging the failure for
	// every clip. Cleared when a device opens.
	bool audioOpenFailureLogged = false;
	// Detaches a video's sound and moves its clock to wall time.
	void SilenceVideoAudio(VideoPlayback* vp, const char* reason);

	// Concurrently-playing standalone sounds.
	std::vector<std::unique_ptr<StandaloneAudio>> sfx;

	bool audioPaused     = false;
	bool videoPaused     = false;
	bool audioFadeEnabled = false;   // see SetAudioFade

	SDL_Texture* imageTexture = nullptr;
	int imageW = 0, imageH = 0;

	// Opens the logical device if it isn't open, or reopens it if the
	// configured device name has changed since. Returns false only if no
	// device could be opened at all.
	bool EnsureAudioDevice();
	void CloseAudioDevice();
	// Creates a stream converting `sampleRate`/`channels` S16 to the device
	// format and binds it. Returns nullptr if the device is unavailable.
	SDL_AudioStream* CreateBoundStream(int sampleRate, int channels);

	void EvictVideosNotIn(const std::vector<std::string>& keep);
	void ActivateVideo(VideoPlayback* vp);
	bool StartPreloadedVideo(const std::string& path);
	// Decodes more of each live SFX into its stream, and reaps finished ones.
	void PumpSfx();
	double GetAudioClock() const;
	size_t UsedPreloadBytes() const;
};

bool OpenCodec(AVFormatContext* fmtCtx, AVCodecContext*& codecCtx, int streamIdx);

} // namespace SatyrAV
