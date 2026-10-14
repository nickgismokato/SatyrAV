#include "render/MediaPlayer.hpp"
#include "core/Logger.hpp"
#include "render/D3D11Interop.hpp"
#include <SDL3_image/SDL_image.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <deque>
#include "core/Config.hpp"

namespace SatyrAV{

// ── StandaloneAudio ──────────────────────────────────────────
//
// (2.0.0) Destroying the stream also unbinds it from the logical device, so
// the device itself is untouched and any other sound carries on playing.

StandaloneAudio::~StandaloneAudio(){ Close(); }

void StandaloneAudio::Close(){
	if(stream)   { SDL_DestroyAudioStream(stream); stream = nullptr; }
	if(frame)    { av_frame_free(&frame); }
	if(packet)   { av_packet_free(&packet); }
	if(swrCtx)   { swr_free(&swrCtx); }
	if(codecCtx) { avcodec_free_context(&codecCtx); }
	if(formatCtx){ avformat_close_input(&formatCtx); }
	streamIndex = -1;
	sampleRate = channels = 0;
	eof = false;
}

// ── OpenCodec helper ────────────────────────────────────────

bool OpenCodec(AVFormatContext* fmtCtx, AVCodecContext*& codecCtx, int streamIdx){
	auto* params = fmtCtx->streams[streamIdx]->codecpar;
	const AVCodec* codec = avcodec_find_decoder(params->codec_id);
	if(!codec) return false;
	codecCtx = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(codecCtx, params);
	codecCtx->thread_count = SDL_GetNumLogicalCPUCores();
	codecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
	if(avcodec_open2(codecCtx, codec, nullptr) < 0){
		avcodec_free_context(&codecCtx);
		return false;
	}
	return true;
}

// ── VideoPlayback ────────────────────────────────────────────

static int VPDecodeThreadFunc(void* data){
	static_cast<VideoPlayback*>(data)->DecodeLoop();
	return 0;
}

VideoPlayback::~VideoPlayback(){
	StopAndClose();
}

enum AVPixelFormat VideoPlayback::GetHwFormat(
	AVCodecContext* ctx, const enum AVPixelFormat* fmts){
	(void)ctx;
	for(const enum AVPixelFormat* p = fmts; *p != AV_PIX_FMT_NONE; p++){
		if(*p == AV_PIX_FMT_D3D11) return *p;
	}
	return fmts[0];
}

// Compute downscaled output size that fits inside slaveW×slaveH,
// preserving aspect ratio, with even dimensions.
static void ComputeOutputSize(int srcW, int srcH, int slaveW, int slaveH,
	int& outW, int& outH){
	if(slaveW <= 0 || slaveH <= 0 || (srcW <= slaveW && srcH <= slaveH)){
		outW = srcW;
		outH = srcH;
		return;
	}
	double srcAR = (double)srcW / (double)srcH;
	double dstAR = (double)slaveW / (double)slaveH;
	if(srcAR > dstAR){
		outW = slaveW;
		outH = (int)((double)slaveW / srcAR);
	} else{
		outH = slaveH;
		outW = (int)((double)slaveH * srcAR);
	}
	outW &= ~1;
	outH &= ~1;
	if(outW < 2 || outH < 2){ outW = srcW; outH = srcH; }
}

// (2.0.0) Zero-copy ring depth. Decoder surfaces are a finite pool in VRAM,
// so this cannot be the seconds-deep buffer the read-back paths keep in RAM.
// 32 frames is about half a second at 60 fps: enough that the first frames
// are ready when the cue fires, at ~100 MB of VRAM per 1080p clip. The
// hardware decoder refills it many times faster than playback drains it.
static const int kZeroCopyRingFrames  = 32;
// Surfaces beyond the ring: the frame the decode thread is holding while it
// waits for a free slot, plus slack.
static const int kZeroCopySpareFrames = 4;

// Zero-copy handles 8-bit 4:2:0 only, which the decoder emits as NV12.
// 10-bit sources decode to P010 and stay on the read-back path.
[[maybe_unused]] // only the Windows hardware path calls it
static bool IsEightBit420(int pixFmt){
	return pixFmt == AV_PIX_FMT_YUV420P || pixFmt == AV_PIX_FMT_YUVJ420P;
}

bool VideoPlayback::Setup(const std::string& p, SDL_Renderer* renderer,
	int slaveW, int slaveH, int capFrames, AVBufferRef* sharedHw,
	bool allowZeroCopy, bool allowAudio){
	path = p;

	if(avformat_open_input(&formatCtx, path.c_str(), nullptr, nullptr) < 0) return false;
	if(avformat_find_stream_info(formatCtx, nullptr) < 0){
		avformat_close_input(&formatCtx); return false;
	}

	videoStreamIdx = -1;
	audioStreamIdx = -1;
	for(unsigned i = 0; i < formatCtx->nb_streams; i++){
		auto t = formatCtx->streams[i]->codecpar->codec_type;
		if(t == AVMEDIA_TYPE_VIDEO && videoStreamIdx < 0) videoStreamIdx = (int)i;
		else if(t == AVMEDIA_TYPE_AUDIO && audioStreamIdx < 0) audioStreamIdx = (int)i;
	}
	if(videoStreamIdx < 0){
		avformat_close_input(&formatCtx); return false;
	}

	srcWidth  = formatCtx->streams[videoStreamIdx]->codecpar->width;
	srcHeight = formatCtx->streams[videoStreamIdx]->codecpar->height;

	// (1.6.6) Cache the source duration for fade-out timing. AV_NOPTS or a
	// non-positive duration leaves the field at 0 so the fade-out logic
	// silently no-ops on un-seekable streams.
	durationSec = (formatCtx->duration > 0)
		? (double)formatCtx->duration / (double)AV_TIME_BASE : 0.0;

	// ── Try D3D11VA hardware decode ──────────────────────────
	// (2.0.0) Preferred: decode on the slave renderer's own D3D11 device
	// (`sharedHw`), which is what makes handing frames to SDL without a trip
	// through system RAM possible. D3D11Interop multithread-protects that
	// device before it gets here.
	// Fallback: let FFmpeg create a device of its own, as before 2.0.0 —
	// used when sharing is unavailable, switched off in config, or the
	// decoder refuses to open on the shared device.
	useHwDecode = false;
	sharedDevice = false;
	zeroCopy = false;
	presentedAny = false;
	zeroCopyErrorLogged = false;
#ifdef _WIN32
	{
		auto* params = formatCtx->streams[videoStreamIdx]->codecpar;
		const AVCodec* codec = avcodec_find_decoder(params->codec_id);
		if(codec){
			bool codecSupportsD3D11 = false;
			for(int i = 0; ; i++){
				const AVCodecHWConfig* cfg = avcodec_get_hw_config(codec, i);
				if(!cfg) break;
				if(cfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
					cfg->device_type == AV_HWDEVICE_TYPE_D3D11VA){
					codecSupportsD3D11 = true;
					break;
				}
			}
			// Two attempts at most: the shared device first (when offered),
			// then a device of FFmpeg's own.
			for(int attempt = 0; codecSupportsD3D11 && !useHwDecode && attempt < 2; attempt++){
				const bool shared = (attempt == 0);
				if(shared && !sharedHw) continue;

				int ret = 0;
				if(shared){
					hwDeviceCtx = av_buffer_ref(sharedHw);
					if(!hwDeviceCtx) ret = -1;
				} else{
					ret = av_hwdevice_ctx_create(&hwDeviceCtx,
						AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0);
				}
				if(ret < 0) continue;

				videoCodecCtx = avcodec_alloc_context3(codec);
				avcodec_parameters_to_context(videoCodecCtx, params);
				videoCodecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
				videoCodecCtx->get_format = GetHwFormat;
				// (2.0.0) Zero-copy needs frames to stay on the GPU, which is only
				// possible on the shared device, and only for 8-bit sources. The
				// decoder must be told *before it opens* to allocate enough
				// surfaces to cover the ring; its default pool is about 20, and
				// holding more references than it has surfaces stalls decoding.
				const bool wantZeroCopy = shared && allowZeroCopy &&
					IsEightBit420(params->format);
				if(wantZeroCopy){
					videoCodecCtx->extra_hw_frames =
						kZeroCopyRingFrames + kZeroCopySpareFrames;
				}
				if(avcodec_open2(videoCodecCtx, codec, nullptr) >= 0){
					useHwDecode = true;
					sharedDevice = shared;
					zeroCopy = wantZeroCopy;
					Logger::Instance().Infof(
						"MediaPlayer: D3D11VA hardware decode active (%s) for %s",
						zeroCopy ? "shared device, zero-copy"
						         : (shared ? "shared device, read-back" : "own device, read-back"),
						path.c_str());
				} else{
					if(shared){
						Logger::Instance().Warnf(
							"MediaPlayer: decoder would not open on the shared device for %s — "
							"retrying on a device of its own", path.c_str());
					}
					avcodec_free_context(&videoCodecCtx);
					av_buffer_unref(&hwDeviceCtx);
					hwDeviceCtx = nullptr;
				}
			}
		}
	}
#endif

	// ── SW decode fallback ───────────────────────────────────
	if(!useHwDecode){
		if(!OpenCodec(formatCtx, videoCodecCtx, videoStreamIdx)){
			avformat_close_input(&formatCtx); return false;
		}
		// (2.0.0) This case used to be silent — only the *success* of hardware
		// decode was ever reported, so a fallback looked identical to a normal
		// start. On Windows it is a genuine fallback worth flagging: software
		// decode costs far more CPU and is the usual reason a 4K clip stutters.
		// On Linux it is simply the only available path, so it is not a warning.
#ifdef _WIN32
		Logger::Instance().Warnf(
			"MediaPlayer: D3D11VA unavailable — falling back to SOFTWARE decode for %s",
			path.c_str());
#else
		Logger::Instance().Infof("MediaPlayer: software decode for %s", path.c_str());
#endif
	}

	srcWidth  = videoCodecCtx->width;
	srcHeight = videoCodecCtx->height;
	if((srcWidth & 1) || (srcHeight & 1)){
		Logger::Instance().Errorf("MediaPlayer: odd video dimensions not supported");
		StopAndClose(); return false;
	}

	// Output size: downscale to fit slave (same for both paths)
	ComputeOutputSize(srcWidth, srcHeight, slaveW, slaveH, outWidth, outHeight);
	setupSlaveW = slaveW;
	setupSlaveH = slaveH;
	// (2.0.0) Zero-copy never rescales on the CPU: the frame stays at source
	// size on the GPU and the renderer scales it when it is drawn.
	if(zeroCopy){ outWidth = srcWidth; outHeight = srcHeight; }
	videoTimeBase = av_q2d(formatCtx->streams[videoStreamIdx]->time_base);
	decodeFrame = av_frame_alloc();

	if(zeroCopy){
		// (2.0.0) Nothing to prepare: no transfer frame and no sws context,
		// because the frame is never brought into system RAM.
	} else if(useHwDecode){
		// HW path: GPU decodes → D3D11 texture, decode thread transfers to
		// system RAM via av_hwframe_transfer_data, then sws_scale downscales
		// to NV12@output into the ring. All on the decode thread — no D3D11
		// calls from the main thread.
		//
		// We do NOT pre-allocate the transferFrame buffer here. Let
		// av_hwframe_transfer_data pick the actual pixel format the driver
		// produces (it may be NV12, YUV420P, P010, etc. — driver-dependent).
		// The sws context is created lazily in the decode loop once we know
		// the real format. Pre-allocating with AV_PIX_FMT_NV12 caused a green
		// tint when the driver actually output a different format.
		transferFrame = av_frame_alloc();
		if(!transferFrame){ StopAndClose(); return false; }
		// swsCtx is nullptr here for HW path; created lazily in DecodeLoop.
	} else{
		// SW path: sws from codec pix_fmt → YUV420P@output
		swsCtx = sws_getContext(
			srcWidth, srcHeight, videoCodecCtx->pix_fmt,
			outWidth, outHeight, AV_PIX_FMT_YUV420P,
			(srcWidth == outWidth && srcHeight == outHeight)
				? SWS_POINT : SWS_FAST_BILINEAR,
			nullptr, nullptr, nullptr);
		if(!swsCtx){ StopAndClose(); return false; }
	}

	// ── Ring: YUV420P@output, pre-allocated, identical for both paths ──
	// YUV420P (planar) is used instead of NV12 (semi-planar) because
	// libswscale's NV12→NV12 path has an edge case where it silently
	// skips writing the UV plane, resulting in U=V=0 (green on screen).
	// YUV420P→IYUV is fully planar, sws handles it correctly every time.
	//
	// (2.0.0) In zero-copy mode the ring has the same shape but its slots are
	// empty AVFrames that the decode thread fills with *references* to
	// decoder surfaces. Its depth is fixed and shallow — see
	// kZeroCopyRingFrames — whatever depth the caller asked for.
	if(capFrames < 2) capFrames = 2;
	ringCapacity = zeroCopy ? kZeroCopyRingFrames : capFrames;
	ring.assign(ringCapacity, nullptr);
	ringPts.assign(ringCapacity, 0.0);
	ringReady.assign(ringCapacity, 0);
	for(int i = 0; i < ringCapacity; i++){
		AVFrame* f = av_frame_alloc();
		if(!f){ StopAndClose(); return false; }
		if(!zeroCopy){
			f->format = AV_PIX_FMT_YUV420P;
			f->width  = outWidth;
			f->height = outHeight;
			if(av_frame_get_buffer(f, 32) < 0){
				av_frame_free(&f);
				StopAndClose(); return false;
			}
		}
		ring[i] = f;
	}

	if(zeroCopy){
		// Counted against the same preload budget as the RAM rings, so the
		// number of clips preloaded at once stays bounded. This is VRAM, and
		// an estimate: the decoder allocates its whole surface pool up front
		// (its own ~20 plus the extra we asked for), each surface padded to
		// the codec's alignment.
		const int align = (videoCodecCtx->codec_id == AV_CODEC_ID_HEVC ||
		                   videoCodecCtx->codec_id == AV_CODEC_ID_AV1) ? 128 : 16;
		const size_t surfW = (size_t)((srcWidth  + align - 1) / align * align);
		const size_t surfH = (size_t)((srcHeight + align - 1) / align * align);
		const size_t pool  = (size_t)(20 + kZeroCopyRingFrames + kZeroCopySpareFrames);
		memoryBytes = pool * surfW * surfH * 3 / 2;
		// `texture` stays null here. The decoder surface size is only known
		// once a frame exists, so D3D11Interop::CopyFrameToTexture creates
		// the NV12 texture when the first frame is presented.
	} else{
		memoryBytes = (size_t)ringCapacity * (size_t)outWidth * (size_t)outHeight * 3 / 2;

		texture = SDL_CreateTexture(renderer,
			SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
			outWidth, outHeight);
		if(!texture){
			Logger::Instance().Errorf("MediaPlayer: IYUV texture creation failed: %s", SDL_GetError());
			StopAndClose(); return false;
		}
		SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
	}

	// ── Audio ────────────────────────────────────────────────
	hasAudio = false;
	// (2.0.1) With no audio device to play on, the clip is set up silent:
	// its sound is never decoded and its clock runs on wall time.
	if(audioStreamIdx >= 0 && allowAudio){
		if(OpenCodec(formatCtx, audioCodecCtx, audioStreamIdx)){
			int sr = audioCodecCtx->sample_rate;
			int ch = audioCodecCtx->ch_layout.nb_channels;
			if(ch < 1) ch = 2;
			AVChannelLayout outLayout;
			av_channel_layout_default(&outLayout, ch);
			int ret = swr_alloc_set_opts2(&swrCtx,
				&outLayout, AV_SAMPLE_FMT_S16, sr,
				&audioCodecCtx->ch_layout, audioCodecCtx->sample_fmt,
				audioCodecCtx->sample_rate, 0, nullptr);
			if(ret >= 0 && swrCtx && swr_init(swrCtx) >= 0){
				hasAudio = true;
				audioSampleRate = sr;
				audioChannels = ch;
				audioTimeBase = av_q2d(formatCtx->streams[audioStreamIdx]->time_base);
				audioStartPTS = 0.0;
				audioStartPTSSet = false;
				audioBytesPushed.store(0);
				// (2.0.0) Created here but left UNBOUND: a preloaded video
				// must be able to decode its audio ahead of time, and an
				// unbound stream still queues. MediaPlayer binds it to the
				// logical device when this video becomes active. Output
				// format is set properly at bind time.
				SDL_AudioSpec src{};
				src.format   = SDL_AUDIO_S16;
				src.channels = ch;
				src.freq     = sr;
				audioStream = SDL_CreateAudioStream(&src, &src);
				if(!audioStream){
					Logger::Instance().Errorf(
						"MediaPlayer: audio stream creation failed for %s: %s",
						path.c_str(), SDL_GetError());
					hasAudio = false;
					swr_free(&swrCtx);
					avcodec_free_context(&audioCodecCtx);
				}
			} else{
				if(swrCtx) swr_free(&swrCtx);
				avcodec_free_context(&audioCodecCtx);
			}
		}
	}

	mtx            = SDL_CreateMutex();
	frameAvailable = SDL_CreateCondition();
	frameConsumed  = SDL_CreateCondition();
	stopThread = false;
	threadRunning = true;
	decodeThread = SDL_CreateThread(VPDecodeThreadFunc, "SatyrAV_Decode", this);
	return true;
}

void VideoPlayback::DecodeLoop(){
	AVPacket* packet = av_packet_alloc();
	AVFrame* audioFrame = hasAudio ? av_frame_alloc() : nullptr;

	// (2.0.0) After a seek the demuxer lands on the keyframe *before* the
	// target. Everything earlier than this is decoded (the decoder needs it)
	// but thrown away, so playback resumes at the requested time rather than
	// up to a keyframe interval early.
	double dropBeforeSec = -1.0e18;

	// Decodes one audio packet and pushes its PCM into the audio stream.
	auto decodeAudioPacket = [&](AVPacket* pkt){
		if(!hasAudio || !audioFrame || audioSilenced.load()) return;
		if(avcodec_send_packet(audioCodecCtx, pkt) < 0) return;
		while(avcodec_receive_frame(audioCodecCtx, audioFrame) == 0){
			if(audioFrame->pts != AV_NOPTS_VALUE && audioCodecCtx->sample_rate > 0){
				const double start = audioFrame->pts * audioTimeBase;
				const double len = (double)audioFrame->nb_samples /
					(double)audioCodecCtx->sample_rate;
				if(start + len <= dropBeforeSec) continue; // pre-seek audio
			}
			if(!audioStartPTSSet && audioFrame->pts != AV_NOPTS_VALUE){
				SDL_LockMutex(mtx);
				audioStartPTS = audioFrame->pts * audioTimeBase;
				audioStartPTSSet = true;
				SDL_UnlockMutex(mtx);
			}
			int outSamples = swr_get_out_samples(swrCtx, audioFrame->nb_samples);
			int bufSize = outSamples * audioChannels * 2;
			std::vector<uint8_t> tmp(bufSize);
			uint8_t* outPtr = tmp.data();
			int conv = swr_convert(swrCtx, &outPtr, outSamples,
				(const uint8_t**)audioFrame->data, audioFrame->nb_samples);
			if(conv > 0){
				const int total = conv * audioChannels * 2;
				// (2.0.0) Back-pressure. An SDL_AudioStream queues without
				// limit, so a preloading video would otherwise buffer its
				// entire soundtrack in RAM. Cap the queue at ~8 s, the same
				// depth the old ring was sized to, and park until the device
				// drains it, exactly as the ring used to make us wait.
				const int capBytes = audioSampleRate * audioChannels * 2 * 8;
				while(!stopThread && !seekPending.load() && !audioSilenced.load() &&
				      SDL_GetAudioStreamQueued(audioStream) > capBytes){
					SDL_Delay(5);
				}
				if(!stopThread && !seekPending.load() && !audioSilenced.load()){
					if(SDL_PutAudioStreamData(audioStream, outPtr, total)){
						audioBytesPushed.fetch_add((uint64_t)total);
					} else{
						Logger::Instance().Errorf(
							"MediaPlayer: audio push failed: %s", SDL_GetError());
					}
				}
			}
		}
	};

	// (2.0.0) Read-ahead, zero-copy path only.
	//
	// The playback clock runs on audio. With the deep RAM ring of the
	// read-back paths the decode thread is always seconds ahead, so audio is
	// always queued. The zero-copy ring holds only 32 frames, and a thread
	// that simply blocks when the ring is full also stops reading the file —
	// and with it the audio. If the file interleaves its audio further apart
	// than those 32 frames cover (routine at 60 fps), audio runs dry, the
	// clock stops, no frame is ever consumed, and the ring never drains: a
	// deadlock that shows as a frozen first frame.
	//
	// So while waiting for a free video slot, keep reading the file whenever
	// queued audio drops below ~2 s: audio packets are decoded straight away,
	// and video packets are parked — still compressed, so they are small —
	// to be decoded in order once slots free up.
	std::deque<AVPacket*> pendingVideo;
	size_t pendingBytes = 0;
	bool fileEof = false;
	const size_t kPendingCapBytes = 256u * 1024u * 1024u;
	auto audioRunningLow = [&]() -> bool{
		// A silenced clip has no audio to keep up, so no reason to read ahead.
		if(!hasAudio || !audioStream || audioSilenced.load()) return false;
		return SDL_GetAudioStreamQueued(audioStream) <
			audioSampleRate * audioChannels * 2 * 2;
	};

	while(!stopThread){
		// ── (2.0.0) Seek ─────────────────────────────────────
		// Handled here and only here: at the top of the loop this thread is
		// not holding a decoded frame or a claimed ring slot, so the ring can
		// be reset without a half-published frame turning up afterwards.
		if(seekPending.load()){
			SDL_LockMutex(mtx);
			const double target = seekTargetSec;
			for(int i = 0; i < ringCapacity; i++){
				// Zero-copy slots pin decoder surfaces; hand them back.
				if(zeroCopy && ring[i]) av_frame_unref(ring[i]);
				ringReady[i] = 0;
			}
			writeIdx = readIdx = 0;
			bufferedFrames = 0;
			// The audio clock restarts from the first audio frame kept after
			// the seek; until that arrives the main thread uses the wall
			// clock, which SeekVideo has already re-based to the target.
			audioStartPTS = 0.0;
			audioStartPTSSet = false;
			audioClockAnchored = false;
			SDL_UnlockMutex(mtx);

			for(AVPacket* parked : pendingVideo) av_packet_free(&parked);
			pendingVideo.clear();
			pendingBytes = 0;
			fileEof = false;

			av_seek_frame(formatCtx, -1, (int64_t)(target * (double)AV_TIME_BASE),
				AVSEEK_FLAG_BACKWARD);
			avcodec_flush_buffers(videoCodecCtx);
			if(hasAudio){
				avcodec_flush_buffers(audioCodecCtx);
				// Drop everything queued for the old position. Pushed-minus-
				// queued is the clock, so both sides go back to zero together.
				SDL_ClearAudioStream(audioStream);
				audioBytesPushed.store(0);
			}
			dropBeforeSec = target;

			SDL_LockMutex(mtx);
			threadRunning = true; // it may have been parked at end of file
			seekPending.store(false);
			SDL_UnlockMutex(mtx);
			continue;
		}

		int ret = 0;
		bool atEof = false;
		if(!pendingVideo.empty()){
			// Parked video packets go first, so decode order is file order.
			AVPacket* parked = pendingVideo.front();
			pendingVideo.pop_front();
			pendingBytes -= (size_t)parked->size;
			av_packet_move_ref(packet, parked);
			av_packet_free(&parked);
		} else if(fileEof){
			atEof = true;
		} else{
			ret = av_read_frame(formatCtx, packet);
			if(ret < 0) atEof = true;
		}

		// (2.0.0) At end of file the thread used to exit. It now parks
		// instead, so a seek can still rewind a clip whose decoding has
		// finished but whose last frames are still playing out of the ring.
		// `threadRunning = false` keeps its old meaning for everyone else:
		// "no more frames are coming", which is what end-of-clip detection
		// and the start-up wait rely on. A parked thread costs nothing; it
		// leaves when the video is destroyed.
		if(atEof){
			// (2.0.1) The resampler keeps the last few milliseconds of sound
			// until it is told no more is coming. Flushing lets them play.
			if(hasAudio && audioStream && !audioSilenced.load()){
				SDL_FlushAudioStream(audioStream);
			}
			SDL_LockMutex(mtx);
			threadRunning = false;
			SDL_SignalCondition(frameAvailable);
			while(!stopThread && !seekPending.load()){
				SDL_WaitCondition(frameConsumed, mtx);
			}
			SDL_UnlockMutex(mtx);
			continue;
		}

		if(packet->stream_index == audioStreamIdx && hasAudio && audioFrame){
			decodeAudioPacket(packet);
		} else if(packet->stream_index == videoStreamIdx){
			if(avcodec_send_packet(videoCodecCtx, packet) >= 0){
				while(avcodec_receive_frame(videoCodecCtx, decodeFrame) == 0){
					double pts = 0.0;
					if(decodeFrame->best_effort_timestamp != AV_NOPTS_VALUE){
						pts = decodeFrame->best_effort_timestamp * videoTimeBase;
					} else if(decodeFrame->pts != AV_NOPTS_VALUE){
						pts = decodeFrame->pts * videoTimeBase;
					}
					// Frames between the keyframe a seek landed on and the
					// requested time: needed by the decoder, not by the screen.
					if(pts < dropBeforeSec - 0.0005){
						av_frame_unref(decodeFrame);
						continue;
					}

					// Wait for a free ring slot
					SDL_LockMutex(mtx);
					while(!stopThread && !seekPending.load() &&
					      bufferedFrames >= ringCapacity){
						if(!zeroCopy){
							// Read-back paths: unchanged, block until consumed.
							SDL_WaitCondition(frameConsumed, mtx);
						} else if(!fileEof && audioRunningLow() &&
						          pendingBytes < kPendingCapBytes){
							SDL_UnlockMutex(mtx);
							AVPacket* ahead = av_packet_alloc();
							if(av_read_frame(formatCtx, ahead) < 0){
								fileEof = true;
								av_packet_free(&ahead);
							} else if(ahead->stream_index == audioStreamIdx){
								decodeAudioPacket(ahead);
								av_packet_free(&ahead);
							} else if(ahead->stream_index == videoStreamIdx){
								pendingBytes += (size_t)ahead->size;
								pendingVideo.push_back(ahead);
							} else{
								av_packet_free(&ahead);
							}
							SDL_LockMutex(mtx);
						} else{
							// Timed, so the audio level is re-checked even if
							// no frame is consumed in the meantime.
							SDL_WaitConditionTimeout(frameConsumed, mtx, 10);
						}
					}
					// A stop or a seek both abandon this frame; the seek is then
					// picked up at the top of the loop.
					if(stopThread || seekPending.load()){
						SDL_UnlockMutex(mtx); av_frame_unref(decodeFrame); break;
					}
					SDL_UnlockMutex(mtx);

					// ── Produce NV12@output in ring[writeIdx] ────────
					AVFrame* dst = ring[writeIdx];

					if(zeroCopy){
						// (2.0.0) Zero-copy: the slot takes over the reference to
						// the decoder's surface. Nothing is read back and nothing
						// is converted; the surface stays in VRAM until the main
						// thread has copied it to the display texture (or skipped
						// it) and drops the reference, returning it to the pool.
						av_frame_unref(dst);
						av_frame_move_ref(dst, decodeFrame);
					} else if(useHwDecode){
						// HW: transfer D3D11 texture → CPU RAM via av_hwframe_transfer_data.
						// Release previous buffer first so FFmpeg can (re-)allocate it in
						// whatever format the driver actually produces.
						av_frame_unref(transferFrame);
						ret = av_hwframe_transfer_data(transferFrame, decodeFrame, 0);
						if(ret < 0){
							char errbuf[128];
							av_strerror(ret, errbuf, sizeof(errbuf));
							Logger::Instance().Errorf("MediaPlayer: hwframe transfer failed: %s", errbuf);
							av_frame_unref(decodeFrame);
							continue;
						}

						// Use the ACTUAL coded dimensions from the transferred frame,
						// not the display dimensions (srcWidth/srcHeight).
						// H.264 1080p has coded_height=1088 (pad to 16-px macroblock),
						// so av_hwframe_transfer_data gives us a 1920x1088 frame.
						// If we configure sws for 1920x1080 but the UV plane in the
						// transferred frame sits after 1088 Y rows, sws reads chroma
						// from the wrong offset → green tint. Using the actual coded
						// dimensions (1088) for both sws and sws_scale is correct;
						// the slight scale 1088→1080 is imperceptible.
						AVPixelFormat actualFmt = (AVPixelFormat)transferFrame->format;
						int actualW = transferFrame->width;
						int actualH = transferFrame->height;

						if(!swsCtx || hwTransferFmt != actualFmt ||
							hwTransferW != actualW || hwTransferH != actualH){
							if(swsCtx){ sws_freeContext(swsCtx); swsCtx = nullptr; }
							hwTransferFmt = actualFmt;
							hwTransferW   = actualW;
							hwTransferH   = actualH;
							swsCtx = sws_getContext(
								actualW, actualH, actualFmt,
								outWidth, outHeight, AV_PIX_FMT_YUV420P,
								(actualW == outWidth && actualH == outHeight)
									? SWS_POINT : SWS_FAST_BILINEAR,
								nullptr, nullptr, nullptr);
							if(!swsCtx){
								Logger::Instance().Errorf(
									"MediaPlayer: sws_getContext failed for HW "
									"fmt=%s %dx%d",
									av_get_pix_fmt_name(actualFmt), actualW, actualH);
								av_frame_unref(decodeFrame);
								break; // fatal — exit decode loop
							}
							Logger::Instance().Infof(
								"MediaPlayer: HW transfer fmt=%s coded=%dx%d "
								"display=%dx%d out=%dx%d",
								av_get_pix_fmt_name(actualFmt),
								actualW, actualH, srcWidth, srcHeight,
								outWidth, outHeight);
						}

						// Process all coded rows — sws scales coded→output dimensions.
						sws_scale(swsCtx,
							(const uint8_t* const*)transferFrame->data,
							transferFrame->linesize,
							0, actualH,
							dst->data, dst->linesize);
					} else{
						// SW: sws from codec pixfmt → NV12@output directly
						sws_scale(swsCtx,
							decodeFrame->data, decodeFrame->linesize,
							0, srcHeight,
							dst->data, dst->linesize);
					}

					SDL_LockMutex(mtx);
					ringPts[writeIdx] = pts;
					ringReady[writeIdx] = 1;
					writeIdx = (writeIdx + 1) % ringCapacity;
					bufferedFrames++;
					SDL_SignalCondition(frameAvailable);
					SDL_UnlockMutex(mtx);

					av_frame_unref(decodeFrame);
				}
			}
		}

		av_packet_unref(packet);
	}

	for(AVPacket* parked : pendingVideo) av_packet_free(&parked);
	pendingVideo.clear();
	if(audioFrame) av_frame_free(&audioFrame);
	av_packet_free(&packet);
	SDL_LockMutex(mtx);
	threadRunning = false;
	SDL_SignalCondition(frameAvailable);
	SDL_UnlockMutex(mtx);
}

void VideoPlayback::StopAndClose(){
	if(decodeThread){
		SDL_LockMutex(mtx);
		stopThread = true;
		SDL_BroadcastCondition(frameConsumed);
		SDL_BroadcastCondition(frameAvailable);
		SDL_UnlockMutex(mtx);
		SDL_WaitThread(decodeThread, nullptr);
		decodeThread = nullptr;
	}
	if(mtx)            { SDL_DestroyMutex(mtx); mtx = nullptr; }
	if(frameAvailable) { SDL_DestroyCondition(frameAvailable); frameAvailable = nullptr; }
	if(frameConsumed)  { SDL_DestroyCondition(frameConsumed); frameConsumed = nullptr; }

	if(texture)       { SDL_DestroyTexture(texture); texture = nullptr; }
	for(auto*& f : ring) if(f){ av_frame_free(&f); f = nullptr; }
	ring.clear(); ringPts.clear(); ringReady.clear();
	ringCapacity = 0; writeIdx = readIdx = 0; bufferedFrames = 0;
	memoryBytes = 0;

	if(transferFrame) { av_frame_free(&transferFrame); }
	if(decodeFrame)   { av_frame_free(&decodeFrame); }
	if(swsCtx)        { sws_freeContext(swsCtx); swsCtx = nullptr; }
	if(videoCodecCtx) { avcodec_free_context(&videoCodecCtx); }
	if(swrCtx)        { swr_free(&swrCtx); }
	if(audioCodecCtx) { avcodec_free_context(&audioCodecCtx); }
	if(hwDeviceCtx)   { av_buffer_unref(&hwDeviceCtx); hwDeviceCtx = nullptr; }
	if(formatCtx)     { avformat_close_input(&formatCtx); }

	// (2.0.0) Destroying the stream unbinds it from the logical device too,
	// so tearing down a video never disturbs the device or any other sound
	// playing on it. Must happen after the decode thread has joined above —
	// it is the producer.
	if(audioStream){ SDL_DestroyAudioStream(audioStream); audioStream = nullptr; }
	audioBytesPushed.store(0);
	hasAudio = false;
	useHwDecode = false;
	videoStreamIdx = audioStreamIdx = -1;
	started = false;
	threadRunning = false;
	audioStartPTSSet = false;
}

// ── MediaPlayer ──────────────────────────────────────────────

bool MediaPlayer::Init(){ return true; }
void MediaPlayer::Shutdown(){ StopAll(); }

// (2.0.0) SDL3 identifies devices by SDL_AudioDeviceID, not by name string.
// Config still stores a human-readable name, so resolve it here.
// Returns 0 when a non-empty configured name matches nothing.
static SDL_AudioDeviceID FindPlaybackDeviceByName(const std::string& name){
	if(name.empty()) return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;

	int count = 0;
	SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&count);
	if(!ids) return 0;

	SDL_AudioDeviceID found = 0;
	for(int i = 0; i < count; i++){
		const char* dn = SDL_GetAudioDeviceName(ids[i]);
		if(dn && name == dn){ found = ids[i]; break; }
	}
	SDL_free(ids);
	return found;
}

// (2.0.0) Opens the single logical device the whole app shares, or reopens
// it if the operator changed the device in Options since. Idempotent.
bool MediaPlayer::EnsureAudioDevice(){
	const auto& cfg = Config::Instance();
	if(audioDevice != 0 && cfg.audioDeviceName == openedDeviceName) return true;

	if(audioDevice != 0) CloseAudioDevice();
	// (2.0.1) Retried for every clip; only the first failure in a row is
	// logged, so a machine without a device does not fill the log.
	const bool quiet = audioOpenFailureLogged;

	// (1.6.3) Honour `Config::audioDeviceName`. Empty = system default. If
	// the named device is gone (unplugged USB interface, switched HDMI
	// output) fall back to the default so audio still plays rather than
	// silently failing.
	SDL_AudioDeviceID devId = FindPlaybackDeviceByName(cfg.audioDeviceName);
	if(devId == 0){
		if(!quiet) Logger::Instance().Warnf(
			"MediaPlayer: configured device '%s' not found; falling back to default",
			cfg.audioDeviceName.c_str());
		devId = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
	}

	// nullptr spec = let SDL pick the device's preferred format. Every
	// stream converts into it independently, so there is no longer a reason
	// to impose one clip's sample rate on the device.
	audioDevice = SDL_OpenAudioDevice(devId, nullptr);
	if(audioDevice == 0 && devId != SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK){
		if(!quiet) Logger::Instance().Warnf(
			"MediaPlayer: device '%s' failed to open (%s); falling back to default",
			cfg.audioDeviceName.c_str(), SDL_GetError());
		audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
	}
	if(audioDevice == 0){
		if(!quiet){
			Logger::Instance().Errorf(
				"MediaPlayer: no audio output device could be opened (%s) — "
				"videos will play without sound, standalone sounds will not play",
				SDL_GetError());
		}
		audioOpenFailureLogged = true;
		return false;
	}
	audioOpenFailureLogged = false;

	SDL_zero(deviceSpec);
	SDL_GetAudioDeviceFormat(audioDevice, &deviceSpec, nullptr);
	openedDeviceName = cfg.audioDeviceName;
	Logger::Instance().Infof(
		"MediaPlayer: audio device open (%s) — %d Hz, %d ch",
		cfg.audioDeviceName.empty() ? "system default" : cfg.audioDeviceName.c_str(),
		deviceSpec.freq, deviceSpec.channels);
	SDL_ResumeAudioDevice(audioDevice);
	return true;
}

// (2.0.1) Takes a video's sound out of play and moves its clock to wall
// time, so the picture keeps going. Safe to call more than once.
void MediaPlayer::SilenceVideoAudio(VideoPlayback* vp, const char* reason){
	if(!vp || vp->audioSilenced.exchange(true)) return;
	if(vp->audioStream){
		SDL_UnbindAudioStream(vp->audioStream);
		SDL_ClearAudioStream(vp->audioStream);
	}
	vp->wallClockFallback = true;
	Logger::Instance().Warnf("MediaPlayer: %s — playing %s without sound",
		reason, vp->path.c_str());
}

void MediaPlayer::CloseAudioDevice(){
	// Streams bound to this device are owned by their sources; closing the
	// device unbinds them without destroying them.
	if(audioDevice != 0){
		SDL_CloseAudioDevice(audioDevice);
		audioDevice = 0;
	}
	SDL_zero(deviceSpec);
	openedDeviceName.clear();
}

// Binds a source's stream to the shared device, converting to the device
// format. Used for standalone SFX; video streams are created in Setup and
// bound at activation.
SDL_AudioStream* MediaPlayer::CreateBoundStream(int sampleRate, int channels){
	if(!EnsureAudioDevice()) return nullptr;

	SDL_AudioSpec src{};
	src.format   = SDL_AUDIO_S16;
	src.channels = channels;
	src.freq     = sampleRate;

	SDL_AudioStream* s = SDL_CreateAudioStream(&src, &deviceSpec);
	if(!s){
		Logger::Instance().Errorf("MediaPlayer: audio stream creation failed: %s",
			SDL_GetError());
		return nullptr;
	}
	if(!SDL_BindAudioStream(audioDevice, s)){
		Logger::Instance().Errorf("MediaPlayer: audio stream bind failed: %s",
			SDL_GetError());
		SDL_DestroyAudioStream(s);
		return nullptr;
	}
	return s;
}

double MediaPlayer::GetAudioClock() const{
	if(!active || !active->hasAudio || !active->audioStartPTSSet) return -1.0;
	int bps = active->audioChannels * 2; // bytes per sample frame (S16 stereo = 4)
	if(bps <= 0 || active->audioSampleRate <= 0) return -1.0;

	// (2.0.0) The clock is still driven by the sound card, so it still
	// cannot drift — only its source changed. `audioBytesPushed` is what the
	// decode thread has handed to SDL; SDL_GetAudioStreamQueued is what has
	// not been consumed yet. The difference is what the device has actually
	// taken, which is what `AudioBuffer::totalConsumed` used to report.
	//
	// Both figures are in the stream's INPUT format (verified against SDL
	// 3.4.14 — the queued count is deliberately *not* in output units), so
	// the bytes→seconds arithmetic below is unchanged.
	size_t consumed = 0;
	if(active->audioStream){
		uint64_t pushed = active->audioBytesPushed.load();
		int queued = SDL_GetAudioStreamQueued(active->audioStream);
		if(queued < 0) queued = 0;
		consumed = (pushed > (uint64_t)queued) ? (size_t)(pushed - (uint64_t)queued) : 0;
	}
	uint64_t now    = SDL_GetPerformanceCounter();
	double   freq   = (double)SDL_GetPerformanceFrequency();

	// Re-anchor whenever the device has consumed more bytes. Between the
	// device's pulls the anchor is fixed and we interpolate with wall time,
	// so the clock advances smoothly every frame rather than in steps.
	if(!active->audioClockAnchored || consumed != active->audioClockAnchorBytes){
		active->audioClockAnchorPTS   = active->audioStartPTS +
			(double)consumed / (double)(active->audioSampleRate * bps);
		active->audioClockAnchorTick  = now;
		active->audioClockAnchorBytes = consumed;
		active->audioClockAnchored    = true;
	}

	// Wall-time elapsed since last anchor, capped at 200ms to guard against
	// stalls (e.g. audio buffer empty, device paused, etc.)
	double wallElapsed = (double)(now - active->audioClockAnchorTick) / freq;
	if(wallElapsed > 0.2) wallElapsed = 0.2;

	return active->audioClockAnchorPTS + wallElapsed;
}

size_t MediaPlayer::UsedPreloadBytes() const{
	size_t total = 0;
	for(auto& kv : videos) total += kv.second->memoryBytes;
	return total;
}

size_t MediaPlayer::GetPreloadedBytes() const{ return UsedPreloadBytes(); }

bool MediaPlayer::IsUsingHwDecode() const{
	return active && active->useHwDecode;
}

bool MediaPlayer::IsVideoZeroCopy() const{
	return active && active->zeroCopy;
}

// (2.0.0) Plumbing for the 2.1.0 fast-forward / rewind / restart commands.
// Nothing calls this yet. The work is done by the decode thread; this only
// files the request and re-bases the clocks.
bool MediaPlayer::SeekVideo(double seconds){
	if(!active || !active->started) return false;
	VideoPlayback* vp = active;
	if(seconds < 0.0) seconds = 0.0;
	if(vp->durationSec > 0.0 && seconds > vp->durationSec) seconds = vp->durationSec;

	SDL_LockMutex(vp->mtx);
	vp->seekTargetSec = seconds;
	vp->seekPending.store(true);
	// Wakes the decode thread wherever it is waiting: for a free ring slot,
	// or parked at end of file.
	SDL_BroadcastCondition(vp->frameConsumed);
	SDL_UnlockMutex(vp->mtx);

	// The wall clock (used for silent clips, and for the moment before the
	// first post-seek audio arrives) and the pause/resume bookmark both move
	// to the target now. The audio clock re-anchors itself from the first
	// audio frame the decode thread keeps.
	vp->startTick = SDL_GetPerformanceCounter() -
		(uint64_t)(seconds * (double)SDL_GetPerformanceFrequency());
	vp->nextPTS = seconds;
	// (2.0.1) A clip that fell back to wall time because its sound track had
	// ended can return to the sound clock after a seek. A silenced clip
	// cannot: it has no sound to follow.
	if(!vp->audioSilenced.load()) vp->wallClockFallback = false;
	vp->wdLastChangeTick = 0;
	return true;
}

double MediaPlayer::GetVideoPosition() const{
	return (active && active->started) ? active->nextPTS : -1.0;
}

double MediaPlayer::GetVideoDuration() const{
	return active ? active->durationSec : 0.0;
}

bool MediaPlayer::GetVideoSrcRect(SDL_FRect* out) const{
	if(!out || !active || !active->zeroCopy || !active->texture) return false;
	float texW = 0.0f, texH = 0.0f;
	SDL_GetTextureSize(active->texture, &texW, &texH);
	float w = (float)active->srcWidth;
	float h = (float)active->srcHeight;
	// Where the surface is padded beyond the picture, stop half a texel
	// short. When the picture is scaled *up*, linear filtering at the very
	// edge would otherwise blend in the first padding row or column, which
	// can show as a thin discoloured line along the bottom or right.
	if(texW > w) w -= 0.5f;
	if(texH > h) h -= 0.5f;
	*out = SDL_FRect{0.0f, 0.0f, w, h};
	return true;
}

// ── Preload ─────────────────────────────────────────────────

void MediaPlayer::EvictVideosNotIn(const std::vector<std::string>& keep){
	for(auto it = videos.begin(); it != videos.end(); ){
		bool keepThis = false;
		for(auto& k : keep) if(k == it->first){ keepThis = true; break; }
		if(!keepThis && it->second.get() != active){
			it = videos.erase(it);
		} else{
			++it;
		}
	}
}

void MediaPlayer::PreloadVideosForScenes(const std::vector<std::string>& priorityPaths,
	SDL_Renderer* renderer, int slaveW, int slaveH){
	if(!renderer) return;

	EvictVideosNotIn(priorityPaths);

	auto& cfg = Config::Instance();
	size_t budgetBytes = (size_t)cfg.videoPreloadBudgetMB * 1024ull * 1024ull;
	int maxSeconds = cfg.videoPreloadMaxSeconds > 0 ? cfg.videoPreloadMaxSeconds : 60;

	for(const auto& path : priorityPaths){
		if(videos.count(path)) continue;

		size_t used = UsedPreloadBytes();
		if(used >= budgetBytes) break;
		size_t remaining = budgetBytes - used;

		AVFormatContext* probe = nullptr;
		if(avformat_open_input(&probe, path.c_str(), nullptr, nullptr) < 0) continue;
		if(avformat_find_stream_info(probe, nullptr) < 0){
			avformat_close_input(&probe); continue;
		}
		int vStream = -1;
		for(unsigned i = 0; i < probe->nb_streams; i++){
			if(probe->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){ vStream = (int)i; break; }
		}
		if(vStream < 0){ avformat_close_input(&probe); continue; }

		int sw = probe->streams[vStream]->codecpar->width;
		int sh = probe->streams[vStream]->codecpar->height;
		double fps = av_q2d(probe->streams[vStream]->avg_frame_rate);
		if(fps < 1.0 || fps > 240.0) fps = 60.0;
		double durationSec = probe->duration > 0
			? (double)probe->duration / (double)AV_TIME_BASE : (double)maxSeconds;
		avformat_close_input(&probe);

		int outW, outH;
		ComputeOutputSize(sw, sh, slaveW, slaveH, outW, outH);
		size_t frameBytes = (size_t)outW * (size_t)outH * 3 / 2;
		if(frameBytes == 0) continue;

		double capSec = std::min(durationSec, (double)maxSeconds);
		int desiredFrames = (int)(capSec * fps);
		if(desiredFrames < 60) desiredFrames = 60;
		int budgetFrames = (int)(remaining / frameBytes);
		int ringFrames = std::min(desiredFrames, budgetFrames);
		if(ringFrames < 60) break;

		auto vp = std::make_unique<VideoPlayback>();
		if(!vp->Setup(path, renderer, slaveW, slaveH, ringFrames,
				SharedHwDeviceFor(renderer), !zeroCopyDisabled,
				EnsureAudioDevice())){
			continue;
		}
		videos.emplace(path, std::move(vp));
	}
}

bool MediaPlayer::IsVideoPreloaded(const std::string& path) const{
	auto it = videos.find(path);
	return it != videos.end() && it->second.get() != active;
}

bool MediaPlayer::StartPreloadedVideo(const std::string& path){
	auto it = videos.find(path);
	if(it == videos.end()) return false;
	ActivateVideo(it->second.get());
	return true;
}

void MediaPlayer::ActivateVideo(VideoPlayback* vp){
	if(!vp) return;

	// Retiring the previous video destroys its stream, which unbinds it.
	// The device stays open throughout, so any standalone SFX playing over
	// the top is undisturbed.
	if(active && active != vp){
		std::string prevPath = active->path;
		active = nullptr;
		videos.erase(prevPath);
	}

	active = vp;
	videoPaused = false;

	// (2.0.0) Bind the already-populated stream rather than opening a device.
	// A preloaded clip has been queueing audio into this stream while
	// unbound, so playback starts from a full buffer.
	//
	// (2.0.1) If the sound cannot be attached, the clip plays silent on the
	// wall clock rather than freezing on its first frame.
	if(vp->hasAudio && vp->audioStream && !vp->audioSilenced.load()){
		if(!EnsureAudioDevice()){
			SilenceVideoAudio(vp, "no audio output device");
		} else if(!SDL_BindAudioStream(audioDevice, vp->audioStream)){
			Logger::Instance().Errorf("MediaPlayer: failed to bind video audio: %s",
				SDL_GetError());
			SilenceVideoAudio(vp, "the video's sound could not be attached to the audio device");
		}
	}
	vp->wdLastChangeTick = 0;

	// Wait up to 1 s for frames
	// (2.0.0) Capped at the ring's depth. A zero-copy ring holds 32 frames, so
	// waiting for 60 could never succeed and every cue would start a full
	// second late.
	int waitTarget = std::min(60, vp->ringCapacity);
	SDL_LockMutex(vp->mtx);
	// (2.0.0) SDL_GetTicks widened from Uint32 to Uint64. Storing it in a
	// uint32_t would truncate the anchor while the subtrahend stayed 64-bit,
	// corrupting this timeout after ~49 days of uptime.
	uint64_t start = SDL_GetTicks();
	while(vp->bufferedFrames < waitTarget && vp->threadRunning &&
		(SDL_GetTicks() - start) < 1000){
		SDL_WaitConditionTimeout(vp->frameAvailable, vp->mtx, 50);
	}
	SDL_UnlockMutex(vp->mtx);

	vp->started = true;
	vp->startTick = SDL_GetPerformanceCounter();
	vp->nextPTS = 0.0;
}

// ── PlayVideo ───────────────────────────────────────────────

bool MediaPlayer::PlayVideo(const std::string& path, SDL_Renderer* renderer,
	int slaveW, int slaveH, int fadeInMs, int fadeOutMs){
	if(IsVideoPreloaded(path)){
		bool ok = StartPreloadedVideo(path);
		// (1.6.6) Stamp fades onto the now-active VideoPlayback. The
		// fresh-decode path below repeats the stamp after its ActivateVideo.
		if(ok && active){
			active->fadeInSec  = (float)fadeInMs  / 1000.0f;
			active->fadeOutSec = (float)fadeOutMs / 1000.0f;
		}
		return ok;
	}
	videos.erase(path);

	auto& cfg = Config::Instance();
	size_t budgetBytes = (size_t)cfg.videoPreloadBudgetMB * 1024ull * 1024ull;
	int maxSeconds = cfg.videoPreloadMaxSeconds > 0 ? cfg.videoPreloadMaxSeconds : 60;
	size_t used = UsedPreloadBytes();
	size_t remaining = budgetBytes > used ? budgetBytes - used : 0;

	int ringFrames = (int)(maxSeconds * 60);
	if(ringFrames < 60) ringFrames = 60;
	if(remaining > 0){
		int ow = slaveW > 0 ? slaveW : 1920;
		int oh = slaveH > 0 ? slaveH : 1080;
		size_t fb = (size_t)ow * (size_t)oh * 3 / 2;
		if(fb > 0){
			int budgetFrames = (int)(remaining / fb);
			if(budgetFrames < ringFrames) ringFrames = budgetFrames;
		}
	}
	if(ringFrames < 60) ringFrames = 60;

	auto vp = std::make_unique<VideoPlayback>();
	if(!vp->Setup(path, renderer, slaveW, slaveH, ringFrames,
			SharedHwDeviceFor(renderer), !zeroCopyDisabled,
				EnsureAudioDevice())) return false;
	VideoPlayback* raw = vp.get();
	videos.emplace(path, std::move(vp));
	ActivateVideo(raw);
	// (1.6.6) Stamp fade times after activation — same path as the
	// preloaded branch above, just with a freshly-set-up VideoPlayback.
	if(active){
		active->fadeInSec  = (float)fadeInMs  / 1000.0f;
		active->fadeOutSec = (float)fadeOutMs / 1000.0f;
	}
	return true;
}

// ── UpdateVideoFrame ────────────────────────────────────────
// Main thread only. The ring holds YUV420P CPU frames on the read-back
// paths, and references to decoder surfaces in VRAM on the zero-copy path.

void MediaPlayer::UpdateVideoFrame(SDL_Renderer* renderer){
	if(!active || !active->started || videoPaused) return;
	VideoPlayback* vp = active;
	bool zeroCopyFirstFrameFailed = false;
	// (2.0.0) A seek is in flight: the ring is about to be thrown away by the
	// decode thread, so nothing in it should reach the screen. The last
	// presented frame simply stays up for the few milliseconds this takes.
	if(vp->seekPending.load()) return;

	// (2.0.1) Clock watchdog. The clock is driven by how much sound the device
	// has taken. If that stops moving while the clip plays, the clip would
	// freeze — so after a short grace period the clock moves to wall time at
	// the current position. If sound is still queued, the device has stopped
	// taking it (unplugged, failed), and the clip is silenced too. If the
	// queue is empty, the sound track has simply ended before the picture.
	const double kAudioStallSec = 0.75;
	if(vp->hasAudio && vp->audioStartPTSSet && !vp->wallClockFallback &&
	   vp->audioStream){
		const uint64_t now = SDL_GetPerformanceCounter();
		const double freq = (double)SDL_GetPerformanceFrequency();
		const uint64_t pushed = vp->audioBytesPushed.load();
		int queued = SDL_GetAudioStreamQueued(vp->audioStream);
		if(queued < 0) queued = 0;
		const size_t consumed = pushed > (uint64_t)queued
			? (size_t)(pushed - (uint64_t)queued) : 0;
		if(vp->wdLastChangeTick == 0 || consumed != vp->wdLastConsumed){
			vp->wdLastConsumed = consumed;
			vp->wdLastChangeTick = now;
		} else if((double)(now - vp->wdLastChangeTick) / freq > kAudioStallSec){
			double pos = GetAudioClock();
			if(pos < 0.0) pos = vp->nextPTS;
			vp->startTick = now - (uint64_t)(pos * freq);
			// More than a quarter of a second still queued means the device
			// stopped taking it. Less is the few milliseconds SDL's resampler
			// holds back at the end of a sound track — not a fault.
			const int deadDeviceBytes =
				vp->audioSampleRate * vp->audioChannels * 2 / 4;
			if(queued > deadDeviceBytes){
				SilenceVideoAudio(vp, "the audio device stopped playing sound");
			} else{
				Logger::Instance().Infof(
					"MediaPlayer: sound track of %s ended before the picture — "
					"continuing on the wall clock", vp->path.c_str());
			}
			vp->wallClockFallback = true;
		}
	}

	double clock;
	if(vp->hasAudio && vp->audioStartPTSSet && !vp->wallClockFallback){
		clock = GetAudioClock();
		if(clock < 0.0){
			clock = (double)(SDL_GetPerformanceCounter() - vp->startTick) /
				(double)SDL_GetPerformanceFrequency();
		}
	} else{
		clock = (double)(SDL_GetPerformanceCounter() - vp->startTick) /
			(double)SDL_GetPerformanceFrequency();
	}

	SDL_LockMutex(vp->mtx);

	int bestIdx = -1;
	int skipCount = 0;
	int tmpIdx = vp->readIdx;
	for(int i = 0; i < vp->bufferedFrames; i++){
		if(!vp->ringReady[tmpIdx]) break;
		if(vp->ringPts[tmpIdx] <= clock){
			bestIdx = tmpIdx;
			skipCount = i;
		} else{
			break;
		}
		tmpIdx = (tmpIdx + 1) % vp->ringCapacity;
	}

	if(bestIdx >= 0){
		for(int i = 0; i < skipCount; i++){
			// A skipped zero-copy frame still pins a decoder surface until its
			// reference is dropped.
			if(vp->zeroCopy) av_frame_unref(vp->ring[vp->readIdx]);
			vp->ringReady[vp->readIdx] = 0;
			vp->readIdx = (vp->readIdx + 1) % vp->ringCapacity;
			vp->bufferedFrames--;
		}
		AVFrame* show = vp->ring[vp->readIdx];
		if(vp->zeroCopy){
			// (2.0.0) GPU-to-GPU: decoder surface → NV12 display texture.
			// The reference is dropped straight afterwards, which hands the
			// surface back to the decoder; the picture lives on in the texture.
			const bool ok = D3D11Interop::CopyFrameToTexture(
				renderer, vp->hwDeviceCtx, show, &vp->texture);
			av_frame_unref(show);
			if(ok){
				vp->presentedAny = true;
			} else if(!vp->presentedAny){
				zeroCopyFirstFrameFailed = true;
			} else if(!vp->zeroCopyErrorLogged){
				vp->zeroCopyErrorLogged = true;
				Logger::Instance().Errorf(
					"MediaPlayer: zero-copy frame copy failed mid-clip for %s",
					vp->path.c_str());
			}
		} else{
			// Ring frames are YUV420P (planar Y, U, V). Upload via SDL_UpdateYUVTexture
			// to the IYUV texture — avoids the libswscale NV12→NV12 semi-planar bug
			// that silently zeroed the UV plane (producing green output).
			SDL_UpdateYUVTexture(vp->texture, nullptr,
				show->data[0], show->linesize[0],
				show->data[1], show->linesize[1],
				show->data[2], show->linesize[2]);
		}
		vp->nextPTS = vp->ringPts[vp->readIdx];
		vp->ringReady[vp->readIdx] = 0;
		vp->readIdx = (vp->readIdx + 1) % vp->ringCapacity;
		vp->bufferedFrames--;
		SDL_SignalCondition(vp->frameConsumed);
	}

	bool done = (vp->bufferedFrames <= 0 && !vp->threadRunning);
	SDL_UnlockMutex(vp->mtx);

	// (2.0.0) Automatic fall-back. If the very first zero-copy frame cannot be
	// put on screen, the driver is doing something this path does not handle,
	// and it will keep doing it. Switch zero-copy off for the session and
	// start this clip again on the read-back path. Nothing has been shown
	// yet, so the restart costs the audience nothing but a moment.
	if(zeroCopyFirstFrameFailed){
		Logger::Instance().Errorf(
			"MediaPlayer: zero-copy could not present the first frame of %s — "
			"disabling zero-copy for this session and restarting the clip on the read-back path",
			vp->path.c_str());
		zeroCopyDisabled = true;
		const std::string p = vp->path;
		const int w = vp->setupSlaveW, h = vp->setupSlaveH;
		const int fadeInMs  = (int)(vp->fadeInSec  * 1000.0f);
		const int fadeOutMs = (int)(vp->fadeOutSec * 1000.0f);
		active = nullptr;
		videos.clear(); // preloaded clips were set up for zero-copy too
		PlayVideo(p, renderer, w, h, fadeInMs, fadeOutMs);
		return;
	}

	// (1.6.6) Fade-in / fade-out derived from elapsed playback time.
	// (2.0.0) The factor is now computed once and drives both the picture
	// and — when the project or scene opts in — the soundtrack.
	double fade = 1.0;
	if(vp->fadeInSec > 0.0f && clock < (double)vp->fadeInSec){
		fade = clock / (double)vp->fadeInSec;
	}
	if(vp->fadeOutSec > 0.0f && vp->durationSec > 0.0){
		double remaining = vp->durationSec - clock;
		if(remaining < (double)vp->fadeOutSec){
			double b = remaining / (double)vp->fadeOutSec;
			if(b < fade) fade = b;
		}
	}
	if(fade < 0.0) fade = 0.0;
	if(fade > 1.0) fade = 1.0;

	// brightness=0 renders fully black (colour-mod multiplies the source),
	// brightness=1 leaves the frame untouched. Cheap enough per frame, and a
	// no-op (mod = 255) when no fade is set.
	if(vp->texture){
		uint8_t bm = (uint8_t)(fade * 255.0 + 0.5);
		SDL_SetTextureColorMod(vp->texture, bm, bm, bm);
	}

	// (2.0.0) Opt-in audio fade. Off by default, so scenes written before
	// this behave exactly as they did — the picture fades, the sound cuts.
	//
	// Safe despite the audio queue running several seconds ahead of the
	// speaker: SDL applies stream gain when the device *pulls*, not when we
	// push, so changing it here affects the audio being heard right now
	// rather than audio queued seconds ago.
	if(audioFadeEnabled && vp->hasAudio && vp->audioStream){
		SDL_SetAudioStreamGain(vp->audioStream, (float)fade);
	}

	if(done){
		std::string p = vp->path;
		active = nullptr;
		// (2.0.0) Deliberately does NOT close the device. Erasing the
		// VideoPlayback destroys its stream, which unbinds it; closing the
		// shared device here would cut off any SFX layered over the video.
		videos.erase(p);
	}
}

void MediaPlayer::StopVideo(){
	if(!active) return;
	std::string p = active->path;
	active = nullptr;
	// Erasing destroys the VideoPlayback, whose stream unbinds itself. The
	// device and any SFX playing on it are untouched.
	videos.erase(p);
}

void MediaPlayer::ToggleVideoPause(){
	if(!active) return;
	videoPaused = !videoPaused;
	if(!videoPaused){
		active->startTick = SDL_GetPerformanceCounter() -
			(uint64_t)(active->nextPTS * SDL_GetPerformanceFrequency());
	}
	// (2.0.1) Pause only this video's sound. SDL_PauseAudioStreamDevice, used
	// in 2.0.0, pauses the whole device the stream is bound to — and every
	// sound shares that device, so K silenced F-key sounds too. Detaching the
	// stream stops it being played but keeps its queued sound, and the clock
	// (pushed minus queued) simply stops while it is detached.
	if(active->hasAudio && active->audioStream && !active->audioSilenced.load()){
		if(videoPaused) SDL_UnbindAudioStream(active->audioStream);
		else if(audioDevice) SDL_BindAudioStream(audioDevice, active->audioStream);
	}
	if(!videoPaused){
		// Re-anchor the smoothed clock now, rather than interpolating across
		// the pause, and give the watchdog a fresh start.
		active->audioClockAnchored = false;
		active->wdLastChangeTick = 0;
	}
}

bool MediaPlayer::IsVideoPlaying() const{
	return active != nullptr && active->started && !videoPaused;
}

// (1.6) True while a video is loaded — including when paused. Lets the
// slave keep drawing the last decoded frame on `K` instead of falling
// back to black. The texture retains its last YUV upload because
// `Update()` skips `SDL_UpdateYUVTexture` while paused.
bool MediaPlayer::HasVideo() const{
	return active != nullptr && active->started && active->texture != nullptr;
}

SDL_Texture* MediaPlayer::GetVideoTexture() const{ return active ? active->texture : nullptr; }
int MediaPlayer::GetVideoWidth() const{ return active ? active->outWidth : 0; }
int MediaPlayer::GetVideoHeight() const{ return active ? active->outHeight : 0; }
int MediaPlayer::GetBufferedFrames() const{ return active ? active->bufferedFrames : 0; }
int MediaPlayer::GetVideoRingCapacity() const{ return active ? active->ringCapacity : 0; }

// ── Standalone audio ────────────────────────────────────────

// (2.0.0) Starting a sound no longer touches anything else. Previously this
// closed the audio device and *erased the playing video* — the only way to
// hand a single device to a different buffer. Now each sound gets its own
// stream on the shared device and SDL mixes them, so effects layer over one
// another and over video audio.
bool MediaPlayer::PlayAudio(const std::string& path){
	// Re-triggering a sound that is already playing restarts it rather than
	// stacking a second copy on top of itself.
	StopAudio(path);

	auto s = std::make_unique<StandaloneAudio>();
	s->path = path;

	if(avformat_open_input(&s->formatCtx, path.c_str(), nullptr, nullptr) < 0){
		Logger::Instance().Errorf("MediaPlayer: cannot open audio '%s'", path.c_str());
		return false;
	}
	if(avformat_find_stream_info(s->formatCtx, nullptr) < 0) return false;

	for(unsigned i = 0; i < s->formatCtx->nb_streams; i++){
		if(s->formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
			s->streamIndex = (int)i; break;
		}
	}
	if(s->streamIndex < 0) return false;
	if(!OpenCodec(s->formatCtx, s->codecCtx, s->streamIndex)) return false;

	int sr = s->codecCtx->sample_rate;
	int ch = s->codecCtx->ch_layout.nb_channels;
	if(ch < 1) ch = 2;
	AVChannelLayout outLayout;
	av_channel_layout_default(&outLayout, ch);
	int ret = swr_alloc_set_opts2(&s->swrCtx,
		&outLayout, AV_SAMPLE_FMT_S16, sr,
		&s->codecCtx->ch_layout, s->codecCtx->sample_fmt,
		s->codecCtx->sample_rate, 0, nullptr);
	if(ret < 0 || !s->swrCtx || swr_init(s->swrCtx) < 0) return false;

	s->frame      = av_frame_alloc();
	s->packet     = av_packet_alloc();
	s->sampleRate = sr;
	s->channels   = ch;
	s->stream     = CreateBoundStream(sr, ch);
	if(!s->stream) return false;

	sfx.push_back(std::move(s));
	audioPaused = false;
	PumpSfx();   // prime it so playback starts immediately
	return true;
}

// Decodes more of every live sound into its stream and reaps the finished.
// Called once per frame from Update().
void MediaPlayer::PumpSfx(){
	for(auto& s : sfx){
		if(!s->stream || audioPaused) continue;

		// Keep roughly a second queued. Small enough that a stopped sound
		// goes quiet promptly, large enough to survive a slow frame.
		const int capBytes = s->sampleRate * s->channels * 2;
		while(!s->eof && SDL_GetAudioStreamQueued(s->stream) < capBytes){
			if(av_read_frame(s->formatCtx, s->packet) < 0){ s->eof = true; break; }
			if(s->packet->stream_index != s->streamIndex){
				av_packet_unref(s->packet); continue;
			}
			int r = avcodec_send_packet(s->codecCtx, s->packet);
			av_packet_unref(s->packet);
			if(r < 0) continue;

			while(avcodec_receive_frame(s->codecCtx, s->frame) == 0){
				int outSamples = swr_get_out_samples(s->swrCtx, s->frame->nb_samples);
				std::vector<uint8_t> tmp((size_t)outSamples * s->channels * 2);
				uint8_t* outPtr = tmp.data();
				int conv = swr_convert(s->swrCtx, &outPtr, outSamples,
					(const uint8_t**)s->frame->data, s->frame->nb_samples);
				if(conv > 0){
					SDL_PutAudioStreamData(s->stream, outPtr, conv * s->channels * 2);
				}
			}
		}
	}

	// A sound is finished once the container is drained *and* the device has
	// played out everything queued — dropping it at EOF would cut the tail.
	for(auto it = sfx.begin(); it != sfx.end(); ){
		bool drained = (*it)->eof && (*it)->stream &&
			SDL_GetAudioStreamQueued((*it)->stream) <= 0 &&
			SDL_GetAudioStreamAvailable((*it)->stream) <= 0;
		if(drained) it = sfx.erase(it);
		else        ++it;
	}
}

void MediaPlayer::Update(){ PumpSfx(); }

// Empty path stops every sound; a path stops just that one.
void MediaPlayer::StopAudio(const std::string& path){
	if(path.empty()){ sfx.clear(); return; }
	for(auto it = sfx.begin(); it != sfx.end(); ){
		if((*it)->path == path) it = sfx.erase(it);
		else                    ++it;
	}
}

void MediaPlayer::ToggleAudioPause(){
	audioPaused = !audioPaused;
	// (2.0.1) Detach / re-attach each standalone sound's own stream, so a
	// video's sound keeps playing (see ToggleVideoPause for why).
	for(auto& s : sfx){
		if(!s->stream) continue;
		if(audioPaused) SDL_UnbindAudioStream(s->stream);
		else if(audioDevice) SDL_BindAudioStream(audioDevice, s->stream);
	}
}

bool MediaPlayer::IsAudioPlaying() const{
	return (!sfx.empty() || (active && active->hasAudio)) && !audioPaused;
}

int MediaPlayer::GetActiveSoundCount() const{ return (int)sfx.size(); }

void MediaPlayer::SetAudioFade(bool enabled){
	// Turning it off mid-project must not leave a clip stuck quiet at
	// whatever gain the last fade had reached.
	if(audioFadeEnabled && !enabled && active && active->audioStream){
		SDL_SetAudioStreamGain(active->audioStream, 1.0f);
	}
	audioFadeEnabled = enabled;
}

// ── Image ────────────────────────────────────────────────────

bool MediaPlayer::ShowImage(const std::string& path, SDL_Renderer* renderer){
	ClearImage();
	SDL_Surface* surface = IMG_Load(path.c_str());
	if(!surface) return false;
	imageW = surface->w; imageH = surface->h;
	imageTexture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_DestroySurface(surface);
	return imageTexture != nullptr;
}

SDL_Texture* MediaPlayer::GetImageTexture() const{ return imageTexture; }
int MediaPlayer::GetImageWidth() const{ return imageW; }
int MediaPlayer::GetImageHeight() const{ return imageH; }
void MediaPlayer::ClearImage(){
	if(imageTexture){ SDL_DestroyTexture(imageTexture); imageTexture = nullptr; }
	imageW = imageH = 0;
}

// (2.0.0) One D3D11VA device context on the slave renderer's device, shared
// by every video. Attempted once per renderer; a failed attempt is remembered
// so each clip does not retry (and re-log) it.
AVBufferRef* MediaPlayer::SharedHwDeviceFor(SDL_Renderer* renderer){
	if(!renderer || !Config::Instance().videoZeroCopy) return nullptr;
	if(sharedHwRenderer == renderer) return sharedHwDevice;

	ReleaseSharedHwDevice();
	sharedHwDevice   = D3D11Interop::CreateSharedDeviceCtx(renderer);
	sharedHwRenderer = renderer;
	return sharedHwDevice;
}

void MediaPlayer::ReleaseSharedHwDevice(){
	if(sharedHwDevice) av_buffer_unref(&sharedHwDevice);
	sharedHwDevice   = nullptr;
	sharedHwRenderer = nullptr;
}

void MediaPlayer::StopAll(){
	active = nullptr;
	videos.clear();   // each VideoPlayback destroys (and unbinds) its stream
	// After the videos, which each hold a reference to it. StopAll always
	// runs before the slave renderer is destroyed, so the cached context can
	// never outlive its renderer or be matched to a new renderer that
	// happens to reuse the old pointer value.
	ReleaseSharedHwDevice();
	sfx.clear();      // ditto for every standalone sound
	CloseAudioDevice();
	ClearImage();
}

} // namespace SatyrAV
