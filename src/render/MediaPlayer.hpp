#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

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

// ── Audio ring buffer ───────────────────────────────────────
struct AudioBuffer{
	std::vector<uint8_t> data;
	size_t readPos  = 0;
	size_t writePos = 0;
	size_t size     = 0;
	size_t capacity = 0;
	size_t totalConsumed = 0;
	SDL_mutex* mtx  = nullptr;

	void Init(size_t cap);
	size_t Write(const uint8_t* src, size_t len);
	size_t Read(uint8_t* dst, size_t len);
	void Clear();
	void Destroy();
};

// ── Per-video decode context ────────────────────────────────
//
// Two decode paths, transparent to the main thread:
//
//   HW (D3D11VA, Windows):
//     GPU decodes H.264/HEVC → D3D11 texture (VRAM).
//     Decode thread calls av_hwframe_transfer_data → NV12 in system RAM,
//     then sws_scale downscales NV12@source → NV12@slave into the ring.
//     FFmpeg owns its own D3D11 device (separate from SDL's) so there
//     are zero cross-thread D3D11 calls.
//
//   SW (Linux / fallback):
//     CPU decodes → YUV420P, sws_scale converts+downscales to NV12@slave
//     directly into the ring.
//
// In both cases the ring holds NV12 CPU frames at output resolution.
// The main thread just calls SDL_UpdateNVTexture — same code path either way.
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
	AudioBuffer audioBuffer;

	// NV12 ring — pre-allocated AVFrames at outWidth×outHeight.
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
	SDL_mutex*  mtx          = nullptr;
	SDL_cond*   frameAvailable = nullptr;
	SDL_cond*   frameConsumed  = nullptr;
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

	~VideoPlayback();
	bool Setup(const std::string& path, SDL_Renderer* renderer,
		int slaveW, int slaveH, int capFrames);
	void StopAndClose();
	void DecodeLoop();

	static enum AVPixelFormat GetHwFormat(
		AVCodecContext* ctx, const enum AVPixelFormat* fmts);
};

// ── Standalone audio state ──
struct StandaloneAudio{
	AVFormatContext* formatCtx = nullptr;
	AVCodecContext* codecCtx   = nullptr;
	SwrContext* swrCtx         = nullptr;
	int streamIndex            = -1;
	int sampleRate             = 0;
	int channels               = 0;
	AVFrame* frame             = nullptr;
	AVPacket* packet           = nullptr;
};

class MediaPlayer{
public:
	bool Init();
	void Shutdown();

	bool PlayVideo(const std::string& path, SDL_Renderer* renderer,
		int slaveW = 0, int slaveH = 0);
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

	void PreloadVideosForScenes(const std::vector<std::string>& priorityPaths,
		SDL_Renderer* renderer, int slaveW, int slaveH);
	bool IsVideoPreloaded(const std::string& path) const;

	bool PlayAudio(const std::string& path);
	void StopAudio(const std::string& path = "");
	void ToggleAudioPause();
	bool IsAudioPlaying() const;
	void Update();

	bool ShowImage(const std::string& path, SDL_Renderer* renderer);
	SDL_Texture* GetImageTexture() const;
	int GetImageWidth() const;
	int GetImageHeight() const;
	void ClearImage();

	void StopAll();

private:
	enum class AudioMode{ None, Video, Standalone };

	std::unordered_map<std::string, std::unique_ptr<VideoPlayback>> videos;
	VideoPlayback* active = nullptr;

	StandaloneAudio sa;
	AudioBuffer standaloneBuf;

	SDL_AudioDeviceID audioDeviceID = 0;
	AudioMode audioMode = AudioMode::None;
	int openedSampleRate = 0;
	int openedChannels   = 0;
	bool audioPaused     = false;
	bool videoPaused     = false;

	SDL_Texture* imageTexture = nullptr;
	int imageW = 0, imageH = 0;

	bool OpenAudioDeviceFor(int sampleRate, int channels, AudioMode mode);
	void CloseAudioDevice();
	void CloseStandaloneAudio();
	void EvictVideosNotIn(const std::vector<std::string>& keep);
	void ActivateVideo(VideoPlayback* vp);
	bool StartPreloadedVideo(const std::string& path);
	void RefillStandaloneAudio();
	double GetAudioClock() const;
	size_t UsedPreloadBytes() const;

	static void AudioCallback(void* userdata, uint8_t* stream, int len);
};

bool OpenCodec(AVFormatContext* fmtCtx, AVCodecContext*& codecCtx, int streamIdx);

} // namespace SatyrAV
