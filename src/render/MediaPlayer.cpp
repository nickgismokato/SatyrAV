#include "render/MediaPlayer.hpp"
#include <SDL2/SDL_image.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include "core/Config.hpp"

namespace SatyrAV{

// ── AudioBuffer ──────────────────────────────────────────────

void AudioBuffer::Init(size_t cap){
	if(!mtx) mtx = SDL_CreateMutex();
	capacity = cap;
	data.assign(cap, 0);
	readPos = writePos = size = totalConsumed = 0;
}

size_t AudioBuffer::Write(const uint8_t* src, size_t len){
	SDL_LockMutex(mtx);
	size_t toWrite = std::min(len, capacity - size);
	for(size_t i = 0; i < toWrite; i++){
		data[writePos] = src[i];
		writePos = (writePos + 1) % capacity;
	}
	size += toWrite;
	SDL_UnlockMutex(mtx);
	return toWrite;
}

size_t AudioBuffer::Read(uint8_t* dst, size_t len){
	SDL_LockMutex(mtx);
	size_t toRead = std::min(len, size);
	for(size_t i = 0; i < toRead; i++){
		dst[i] = data[readPos];
		readPos = (readPos + 1) % capacity;
	}
	size -= toRead;
	totalConsumed += toRead;
	if(toRead < len) memset(dst + toRead, 0, len - toRead);
	SDL_UnlockMutex(mtx);
	return toRead;
}

void AudioBuffer::Clear(){
	if(mtx) SDL_LockMutex(mtx);
	readPos = writePos = size = totalConsumed = 0;
	if(mtx) SDL_UnlockMutex(mtx);
}

void AudioBuffer::Destroy(){
	Clear();
	if(mtx){ SDL_DestroyMutex(mtx); mtx = nullptr; }
	data.clear();
	capacity = 0;
}

// ── OpenCodec helper ────────────────────────────────────────

bool OpenCodec(AVFormatContext* fmtCtx, AVCodecContext*& codecCtx, int streamIdx){
	auto* params = fmtCtx->streams[streamIdx]->codecpar;
	const AVCodec* codec = avcodec_find_decoder(params->codec_id);
	if(!codec) return false;
	codecCtx = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(codecCtx, params);
	codecCtx->thread_count = SDL_GetCPUCount();
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

bool VideoPlayback::Setup(const std::string& p, SDL_Renderer* renderer,
	int slaveW, int slaveH, int capFrames){
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

	// ── Try D3D11VA hardware decode ──────────────────────────
	// Creates its own D3D11 device — completely separate from SDL's renderer
	// device. No cross-thread D3D11 calls, no device-context races.
	useHwDecode = false;
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
			if(codecSupportsD3D11){
				// Let FFmpeg create its own D3D11 device (no sharing with SDL)
				int ret = av_hwdevice_ctx_create(&hwDeviceCtx,
					AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0);
				if(ret >= 0){
					videoCodecCtx = avcodec_alloc_context3(codec);
					avcodec_parameters_to_context(videoCodecCtx, params);
					videoCodecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
					videoCodecCtx->get_format = GetHwFormat;
					if(avcodec_open2(videoCodecCtx, codec, nullptr) >= 0){
						useHwDecode = true;
						fprintf(stderr, "MediaPlayer: D3D11VA hardware decode active for %s\n",
							path.c_str());
					} else{
						avcodec_free_context(&videoCodecCtx);
						av_buffer_unref(&hwDeviceCtx);
						hwDeviceCtx = nullptr;
					}
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
	}

	srcWidth  = videoCodecCtx->width;
	srcHeight = videoCodecCtx->height;
	if((srcWidth & 1) || (srcHeight & 1)){
		fprintf(stderr, "MediaPlayer: odd video dimensions not supported\n");
		StopAndClose(); return false;
	}

	// Output size: downscale to fit slave (same for both paths)
	ComputeOutputSize(srcWidth, srcHeight, slaveW, slaveH, outWidth, outHeight);
	videoTimeBase = av_q2d(formatCtx->streams[videoStreamIdx]->time_base);
	decodeFrame = av_frame_alloc();

	if(useHwDecode){
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
	if(capFrames < 2) capFrames = 2;
	ringCapacity = capFrames;
	ring.assign(ringCapacity, nullptr);
	ringPts.assign(ringCapacity, 0.0);
	ringReady.assign(ringCapacity, 0);
	for(int i = 0; i < ringCapacity; i++){
		AVFrame* f = av_frame_alloc();
		f->format = AV_PIX_FMT_YUV420P;
		f->width  = outWidth;
		f->height = outHeight;
		if(av_frame_get_buffer(f, 32) < 0){
			av_frame_free(&f);
			StopAndClose(); return false;
		}
		ring[i] = f;
	}
	memoryBytes = (size_t)ringCapacity * (size_t)outWidth * (size_t)outHeight * 3 / 2;

	texture = SDL_CreateTexture(renderer,
		SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
		outWidth, outHeight);
	if(!texture){
		fprintf(stderr, "MediaPlayer: IYUV texture creation failed: %s\n", SDL_GetError());
		StopAndClose(); return false;
	}
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

	// ── Audio ────────────────────────────────────────────────
	hasAudio = false;
	if(audioStreamIdx >= 0){
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
				audioBuffer.Init((size_t)(sr * ch * 2 * 8));
			} else{
				if(swrCtx) swr_free(&swrCtx);
				avcodec_free_context(&audioCodecCtx);
			}
		}
	}

	mtx            = SDL_CreateMutex();
	frameAvailable = SDL_CreateCond();
	frameConsumed  = SDL_CreateCond();
	stopThread = false;
	threadRunning = true;
	decodeThread = SDL_CreateThread(VPDecodeThreadFunc, "SatyrAV_Decode", this);
	return true;
}

void VideoPlayback::DecodeLoop(){
	AVPacket* packet = av_packet_alloc();
	AVFrame* audioFrame = hasAudio ? av_frame_alloc() : nullptr;

	while(!stopThread){
		int ret = av_read_frame(formatCtx, packet);
		if(ret < 0) break; // EOF

		if(packet->stream_index == audioStreamIdx && hasAudio && audioFrame){
			if(avcodec_send_packet(audioCodecCtx, packet) >= 0){
				while(avcodec_receive_frame(audioCodecCtx, audioFrame) == 0){
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
						size_t wrote = 0;
						size_t total = (size_t)(conv * audioChannels * 2);
						while(wrote < total && !stopThread){
							wrote += audioBuffer.Write(outPtr + wrote, total - wrote);
							if(wrote < total) SDL_Delay(5);
						}
					}
				}
			}
		} else if(packet->stream_index == videoStreamIdx){
			if(avcodec_send_packet(videoCodecCtx, packet) >= 0){
				while(avcodec_receive_frame(videoCodecCtx, decodeFrame) == 0){
					double pts = 0.0;
					if(decodeFrame->best_effort_timestamp != AV_NOPTS_VALUE){
						pts = decodeFrame->best_effort_timestamp * videoTimeBase;
					} else if(decodeFrame->pts != AV_NOPTS_VALUE){
						pts = decodeFrame->pts * videoTimeBase;
					}

					// Wait for a free ring slot
					SDL_LockMutex(mtx);
					while(!stopThread && bufferedFrames >= ringCapacity){
						SDL_CondWait(frameConsumed, mtx);
					}
					if(stopThread){ SDL_UnlockMutex(mtx); av_frame_unref(decodeFrame); break; }
					SDL_UnlockMutex(mtx);

					// ── Produce NV12@output in ring[writeIdx] ────────
					AVFrame* dst = ring[writeIdx];

					if(useHwDecode){
						// HW: transfer D3D11 texture → CPU RAM via av_hwframe_transfer_data.
						// Release previous buffer first so FFmpeg can (re-)allocate it in
						// whatever format the driver actually produces.
						av_frame_unref(transferFrame);
						ret = av_hwframe_transfer_data(transferFrame, decodeFrame, 0);
						if(ret < 0){
							char errbuf[128];
							av_strerror(ret, errbuf, sizeof(errbuf));
							fprintf(stderr, "MediaPlayer: hwframe transfer failed: %s\n", errbuf);
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
								fprintf(stderr,
									"MediaPlayer: sws_getContext failed for HW "
									"fmt=%s %dx%d\n",
									av_get_pix_fmt_name(actualFmt), actualW, actualH);
								av_frame_unref(decodeFrame);
								break; // fatal — exit decode loop
							}
							fprintf(stderr,
								"MediaPlayer: HW transfer fmt=%s coded=%dx%d "
								"display=%dx%d out=%dx%d\n",
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
					SDL_CondSignal(frameAvailable);
					SDL_UnlockMutex(mtx);

					av_frame_unref(decodeFrame);
				}
			}
		}

		av_packet_unref(packet);
	}

	if(audioFrame) av_frame_free(&audioFrame);
	av_packet_free(&packet);
	SDL_LockMutex(mtx);
	threadRunning = false;
	SDL_CondSignal(frameAvailable);
	SDL_UnlockMutex(mtx);
}

void VideoPlayback::StopAndClose(){
	if(decodeThread){
		SDL_LockMutex(mtx);
		stopThread = true;
		SDL_CondBroadcast(frameConsumed);
		SDL_CondBroadcast(frameAvailable);
		SDL_UnlockMutex(mtx);
		SDL_WaitThread(decodeThread, nullptr);
		decodeThread = nullptr;
	}
	if(mtx)            { SDL_DestroyMutex(mtx); mtx = nullptr; }
	if(frameAvailable) { SDL_DestroyCond(frameAvailable); frameAvailable = nullptr; }
	if(frameConsumed)  { SDL_DestroyCond(frameConsumed); frameConsumed = nullptr; }

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

	audioBuffer.Destroy();
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

void MediaPlayer::AudioCallback(void* userdata, uint8_t* stream, int len){
	auto* buf = static_cast<AudioBuffer*>(userdata);
	buf->Read(stream, (size_t)len);
}

bool MediaPlayer::OpenAudioDeviceFor(int sampleRate, int channels, AudioMode mode){
	CloseAudioDevice();

	AudioBuffer* buf = nullptr;
	if(mode == AudioMode::Video){
		if(!active) return false;
		buf = &active->audioBuffer;
	} else if(mode == AudioMode::Standalone){
		standaloneBuf.Init((size_t)(sampleRate * channels * 2 * 4));
		buf = &standaloneBuf;
	} else{
		return false;
	}

	SDL_AudioSpec want, have;
	SDL_zero(want);
	want.freq     = sampleRate;
	want.format   = AUDIO_S16SYS;
	want.channels = (uint8_t)channels;
	want.samples  = 4096;
	want.callback = AudioCallback;
	want.userdata = buf;

	audioDeviceID = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
	if(audioDeviceID == 0){
		fprintf(stderr, "MediaPlayer: SDL audio open failed: %s\n", SDL_GetError());
		return false;
	}
	audioMode = mode;
	openedSampleRate = sampleRate;
	openedChannels   = channels;
	SDL_PauseAudioDevice(audioDeviceID, 0);
	return true;
}

void MediaPlayer::CloseAudioDevice(){
	if(audioDeviceID != 0){
		SDL_PauseAudioDevice(audioDeviceID, 1);
		SDL_CloseAudioDevice(audioDeviceID);
		audioDeviceID = 0;
	}
	if(audioMode == AudioMode::Standalone){
		standaloneBuf.Destroy();
	}
	audioMode = AudioMode::None;
}

double MediaPlayer::GetAudioClock() const{
	if(!active || !active->hasAudio || !active->audioStartPTSSet) return -1.0;
	int bps = active->audioChannels * 2; // bytes per sample frame (S16 stereo = 4)
	if(bps <= 0 || active->audioSampleRate <= 0) return -1.0;

	size_t consumed = active->audioBuffer.totalConsumed;
	uint64_t now    = SDL_GetPerformanceCounter();
	double   freq   = (double)SDL_GetPerformanceFrequency();

	// Re-anchor whenever the SDL audio callback has consumed more bytes.
	// Between callbacks (~93ms apart at 44100Hz/4096-sample buffer) the
	// anchor is fixed and we interpolate with wall time so the displayed
	// clock advances smoothly every frame instead of jumping every 93ms.
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
		if(!vp->Setup(path, renderer, slaveW, slaveH, ringFrames)){
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

	if(active && active != vp){
		std::string prevPath = active->path;
		active = nullptr;
		CloseAudioDevice();
		videos.erase(prevPath);
	} else{
		CloseAudioDevice();
	}

	active = vp;
	videoPaused = false;

	if(vp->hasAudio){
		OpenAudioDeviceFor(vp->audioSampleRate, vp->audioChannels, AudioMode::Video);
	}

	// Wait up to 1 s for frames
	int waitTarget = 60;
	SDL_LockMutex(vp->mtx);
	uint32_t start = SDL_GetTicks();
	while(vp->bufferedFrames < waitTarget && vp->threadRunning &&
		(SDL_GetTicks() - start) < 1000){
		SDL_CondWaitTimeout(vp->frameAvailable, vp->mtx, 50);
	}
	SDL_UnlockMutex(vp->mtx);

	vp->started = true;
	vp->startTick = SDL_GetPerformanceCounter();
	vp->nextPTS = 0.0;
}

// ── PlayVideo ───────────────────────────────────────────────

bool MediaPlayer::PlayVideo(const std::string& path, SDL_Renderer* renderer,
	int slaveW, int slaveH){
	if(IsVideoPreloaded(path)){
		return StartPreloadedVideo(path);
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
	if(!vp->Setup(path, renderer, slaveW, slaveH, ringFrames)) return false;
	VideoPlayback* raw = vp.get();
	videos.emplace(path, std::move(vp));
	ActivateVideo(raw);
	return true;
}

// ── UpdateVideoFrame ────────────────────────────────────────
// Main thread only. Ring holds YUV420P CPU frames for both HW and SW paths.

void MediaPlayer::UpdateVideoFrame(SDL_Renderer* renderer){
	(void)renderer;
	if(!active || !active->started || videoPaused) return;
	VideoPlayback* vp = active;

	double clock;
	if(vp->hasAudio && vp->audioStartPTSSet){
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
			vp->ringReady[vp->readIdx] = 0;
			vp->readIdx = (vp->readIdx + 1) % vp->ringCapacity;
			vp->bufferedFrames--;
		}
		AVFrame* show = vp->ring[vp->readIdx];
		// Ring frames are YUV420P (planar Y, U, V). Upload via SDL_UpdateYUVTexture
		// to the IYUV texture — avoids the libswscale NV12→NV12 semi-planar bug
		// that silently zeroed the UV plane (producing green output).
		SDL_UpdateYUVTexture(vp->texture, nullptr,
			show->data[0], show->linesize[0],
			show->data[1], show->linesize[1],
			show->data[2], show->linesize[2]);
		vp->nextPTS = vp->ringPts[vp->readIdx];
		vp->ringReady[vp->readIdx] = 0;
		vp->readIdx = (vp->readIdx + 1) % vp->ringCapacity;
		vp->bufferedFrames--;
		SDL_CondSignal(vp->frameConsumed);
	}

	bool done = (vp->bufferedFrames <= 0 && !vp->threadRunning);
	SDL_UnlockMutex(vp->mtx);

	if(done){
		std::string p = vp->path;
		active = nullptr;
		CloseAudioDevice();
		videos.erase(p);
	}
}

void MediaPlayer::StopVideo(){
	if(!active) return;
	std::string p = active->path;
	active = nullptr;
	CloseAudioDevice();
	videos.erase(p);
}

void MediaPlayer::ToggleVideoPause(){
	if(!active) return;
	videoPaused = !videoPaused;
	if(!videoPaused){
		active->startTick = SDL_GetPerformanceCounter() -
			(uint64_t)(active->nextPTS * SDL_GetPerformanceFrequency());
	}
	if(audioDeviceID != 0 && active->hasAudio){
		SDL_PauseAudioDevice(audioDeviceID, videoPaused ? 1 : 0);
	}
}

bool MediaPlayer::IsVideoPlaying() const{
	return active != nullptr && active->started && !videoPaused;
}

SDL_Texture* MediaPlayer::GetVideoTexture() const{ return active ? active->texture : nullptr; }
int MediaPlayer::GetVideoWidth() const{ return active ? active->outWidth : 0; }
int MediaPlayer::GetVideoHeight() const{ return active ? active->outHeight : 0; }
int MediaPlayer::GetBufferedFrames() const{ return active ? active->bufferedFrames : 0; }
int MediaPlayer::GetVideoRingCapacity() const{ return active ? active->ringCapacity : 0; }

// ── Standalone audio ────────────────────────────────────────

bool MediaPlayer::PlayAudio(const std::string& path){
	CloseStandaloneAudio();
	if(active){
		std::string p = active->path;
		active = nullptr;
		CloseAudioDevice();
		videos.erase(p);
	} else{
		CloseAudioDevice();
	}

	sa.formatCtx = nullptr;
	if(avformat_open_input(&sa.formatCtx, path.c_str(), nullptr, nullptr) < 0) return false;
	if(avformat_find_stream_info(sa.formatCtx, nullptr) < 0){
		avformat_close_input(&sa.formatCtx); return false;
	}
	sa.streamIndex = -1;
	for(unsigned i = 0; i < sa.formatCtx->nb_streams; i++){
		if(sa.formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
			sa.streamIndex = (int)i; break;
		}
	}
	if(sa.streamIndex < 0){ avformat_close_input(&sa.formatCtx); return false; }
	if(!OpenCodec(sa.formatCtx, sa.codecCtx, sa.streamIndex)){
		avformat_close_input(&sa.formatCtx); return false;
	}
	int sr = sa.codecCtx->sample_rate;
	int ch = sa.codecCtx->ch_layout.nb_channels;
	if(ch < 1) ch = 2;
	AVChannelLayout outLayout;
	av_channel_layout_default(&outLayout, ch);
	int ret = swr_alloc_set_opts2(&sa.swrCtx,
		&outLayout, AV_SAMPLE_FMT_S16, sr,
		&sa.codecCtx->ch_layout, sa.codecCtx->sample_fmt,
		sa.codecCtx->sample_rate, 0, nullptr);
	if(ret < 0 || !sa.swrCtx || swr_init(sa.swrCtx) < 0){
		CloseStandaloneAudio(); return false;
	}
	sa.frame = av_frame_alloc();
	sa.packet = av_packet_alloc();
	sa.sampleRate = sr;
	sa.channels   = ch;
	OpenAudioDeviceFor(sr, ch, AudioMode::Standalone);
	RefillStandaloneAudio();
	audioPaused = false;
	return true;
}

void MediaPlayer::RefillStandaloneAudio(){
	if(!sa.formatCtx || !sa.packet || !sa.frame || audioPaused) return;
	if(audioMode != AudioMode::Standalone) return;
	while(standaloneBuf.size < standaloneBuf.capacity / 2){
		int ret = av_read_frame(sa.formatCtx, sa.packet);
		if(ret < 0) break;
		if(sa.packet->stream_index != sa.streamIndex){
			av_packet_unref(sa.packet); continue;
		}
		ret = avcodec_send_packet(sa.codecCtx, sa.packet);
		av_packet_unref(sa.packet);
		if(ret < 0) continue;
		while(avcodec_receive_frame(sa.codecCtx, sa.frame) == 0){
			int outSamples = swr_get_out_samples(sa.swrCtx, sa.frame->nb_samples);
			int bufSize = outSamples * sa.channels * 2;
			std::vector<uint8_t> tmp(bufSize);
			uint8_t* outPtr = tmp.data();
			int conv = swr_convert(sa.swrCtx, &outPtr, outSamples,
				(const uint8_t**)sa.frame->data, sa.frame->nb_samples);
			if(conv > 0){
				standaloneBuf.Write(outPtr, (size_t)(conv * sa.channels * 2));
			}
		}
	}
}

void MediaPlayer::Update(){ RefillStandaloneAudio(); }

void MediaPlayer::StopAudio(const std::string& path){
	(void)path;
	CloseStandaloneAudio();
}

void MediaPlayer::ToggleAudioPause(){
	audioPaused = !audioPaused;
	if(audioDeviceID != 0) SDL_PauseAudioDevice(audioDeviceID, audioPaused ? 1 : 0);
}

bool MediaPlayer::IsAudioPlaying() const{
	return (sa.formatCtx != nullptr || (active && active->hasAudio)) && !audioPaused;
}

void MediaPlayer::CloseStandaloneAudio(){
	if(sa.frame)     { av_frame_free(&sa.frame); }
	if(sa.packet)    { av_packet_free(&sa.packet); }
	if(sa.swrCtx)    { swr_free(&sa.swrCtx); }
	if(sa.codecCtx)  { avcodec_free_context(&sa.codecCtx); }
	if(sa.formatCtx) { avformat_close_input(&sa.formatCtx); }
	sa.streamIndex = -1;
	sa.sampleRate = sa.channels = 0;
	if(audioMode == AudioMode::Standalone){
		CloseAudioDevice();
	}
}

// ── Image ────────────────────────────────────────────────────

bool MediaPlayer::ShowImage(const std::string& path, SDL_Renderer* renderer){
	ClearImage();
	SDL_Surface* surface = IMG_Load(path.c_str());
	if(!surface) return false;
	imageW = surface->w; imageH = surface->h;
	imageTexture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);
	return imageTexture != nullptr;
}

SDL_Texture* MediaPlayer::GetImageTexture() const{ return imageTexture; }
int MediaPlayer::GetImageWidth() const{ return imageW; }
int MediaPlayer::GetImageHeight() const{ return imageH; }
void MediaPlayer::ClearImage(){
	if(imageTexture){ SDL_DestroyTexture(imageTexture); imageTexture = nullptr; }
	imageW = imageH = 0;
}

void MediaPlayer::StopAll(){
	active = nullptr;
	CloseAudioDevice();
	videos.clear();
	CloseStandaloneAudio();
	ClearImage();
}

} // namespace SatyrAV
