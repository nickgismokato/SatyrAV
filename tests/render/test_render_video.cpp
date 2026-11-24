// The video pipeline with a real clip: play, seek, seek while paused, the
// end of a clip. The clip is encoded here with FFmpeg (MPEG-4, no sound),
// each frame a flat grey whose brightness gives its time, so a pixel read
// back from the screen tells which frame is up.

#include "render_helpers.hpp"
#include "helpers.hpp"
#include "render/MediaPlayer.hpp"

extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

using namespace SatyrRender;
using SatyrTest::TempDir;

namespace{

constexpr int kFps = 25;
constexpr int kSeconds = 4;

// Luma of frame i: 20 at the start, rising 2 per frame (216 at 3.92 s).
int LumaFor(int frame){ return 20 + 2 * frame; }

bool MakeTestClip(const std::string& path){
	AVFormatContext* oc = nullptr;
	if(avformat_alloc_output_context2(&oc, nullptr, nullptr, path.c_str()) < 0 || !oc) return false;
	const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
	AVStream* st = codec ? avformat_new_stream(oc, nullptr) : nullptr;
	AVCodecContext* c = codec ? avcodec_alloc_context3(codec) : nullptr;
	bool ok = st && c;
	if(ok){
		c->width = 320; c->height = 240;
		c->time_base = AVRational{1, kFps};
		c->framerate = AVRational{kFps, 1};
		c->pix_fmt = AV_PIX_FMT_YUV420P;
		c->gop_size = kFps;      // a keyframe every second, to seek to
		c->max_b_frames = 0;
		c->bit_rate = 400000;
		if(oc->oformat->flags & AVFMT_GLOBALHEADER) c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		ok = avcodec_open2(c, codec, nullptr) >= 0 &&
			avcodec_parameters_from_context(st->codecpar, c) >= 0;
	}
	if(ok){
		st->time_base = c->time_base;
		if(!(oc->oformat->flags & AVFMT_NOFILE))
			ok = avio_open(&oc->pb, path.c_str(), AVIO_FLAG_WRITE) >= 0;
	}
	if(ok) ok = avformat_write_header(oc, nullptr) >= 0;

	AVFrame* f = av_frame_alloc();
	AVPacket* pkt = av_packet_alloc();
	if(ok){
		f->format = c->pix_fmt; f->width = c->width; f->height = c->height;
		ok = av_frame_get_buffer(f, 0) >= 0;
	}
	auto send = [&](AVFrame* in){
		avcodec_send_frame(c, in);
		while(avcodec_receive_packet(c, pkt) == 0){
			av_packet_rescale_ts(pkt, c->time_base, st->time_base);
			pkt->stream_index = st->index;
			av_interleaved_write_frame(oc, pkt);
		}
	};
	for(int i = 0; ok && i < kSeconds * kFps; i++){
		av_frame_make_writable(f);
		for(int y = 0; y < c->height; y++)
			memset(f->data[0] + y * f->linesize[0], LumaFor(i), c->width);
		for(int y = 0; y < c->height / 2; y++){
			memset(f->data[1] + y * f->linesize[1], 128, c->width / 2);
			memset(f->data[2] + y * f->linesize[2], 128, c->width / 2);
		}
		f->pts = i;
		send(f);
	}
	if(ok){
		send(nullptr);
		av_write_trailer(oc);
	}
	av_packet_free(&pkt);
	av_frame_free(&f);
	if(c) avcodec_free_context(&c);
	if(oc && !(oc->oformat->flags & AVFMT_NOFILE)) avio_closep(&oc->pb);
	avformat_free_context(oc);
	return ok;
}

// The luma of the video picture as drawn on the slave canvas.
int ScreenLuma(OffscreenRig& rig, MediaPlayer& media){
	SDL_Texture* tex = media.GetVideoTexture();
	if(!tex) return -1;
	SDL_FRect src;
	const bool hasSrc = media.GetVideoSrcRect(&src);
	SDL_Surface* s = rig.RenderCanvas([&]{
		SDL_SetRenderDrawColor(rig.r.GetSlave(), 0, 0, 0, 255);
		SDL_RenderClear(rig.r.GetSlave());
		SDL_RenderTexture(rig.r.GetSlave(), tex, hasSrc ? &src : nullptr, nullptr);
	});
	const int l = At(s, rig.W() / 2, rig.H() / 2).Luma();
	SDL_DestroySurface(s);
	return l;
}

// Expected on-screen luma at `sec` (video range 16-235 stretched to
// 0-255), less a small allowance for MPEG-4 and the colour conversion: the
// frames are 2 luma steps apart, a seek's error is tens of steps.
int ExpectedLuma(double sec){
	const int y = LumaFor((int)(sec * kFps));
	return (int)((y - 16) * 255.0 / 219.0) - 6;
}

} // namespace

TEST_CASE("Video: play, seek, seek while paused, end of clip"){
	TempDir tmp("render-video");
	const std::string clip = tmp.File("clip.mp4");
	REQUIRE(MakeTestClip(clip));

	OffscreenRig rig;
	REQUIRE(rig.ok);
	MediaPlayer media;
	REQUIRE(media.Init());
	SDL_Renderer* sr = rig.r.GetSlave();
	auto pump = [&]{ media.Update(); media.UpdateVideoFrame(sr); };

	REQUIRE(media.PlayVideo(clip, sr, rig.W(), rig.H()));
	CHECK(media.HasVideo());
	const uint64_t serial = media.GetVideoSerial();
	CHECK(serial != 0);
	CHECK(media.GetVideoPath() == clip);
	CHECK(media.GetVideoDuration() == doctest::Approx(4.0).epsilon(0.05));

	// It plays: the position moves on from the start.
	CHECK(WaitFor(2000, pump, [&]{ return media.GetVideoPosition() > 0.3; }));
	CHECK(ScreenLuma(rig, media) < ExpectedLuma(1.5));

	SUBCASE("seek forward while playing"){
		REQUIRE(media.SeekVideo(2.5));
		CHECK(WaitFor(2000, pump, [&]{ return media.GetVideoPosition() > 2.55; }));
		CHECK(ScreenLuma(rig, media) >= ExpectedLuma(2.4));
		// Back by 2 s from there.
		REQUIRE(media.SeekVideoBy(-2.0));
		CHECK(media.GetVideoPosition() < 1.5);
	}

	SUBCASE("seek while paused shows the new frame at once and stays paused"){
		media.ToggleVideoPause();
		REQUIRE(media.IsVideoPaused());
		const int before = ScreenLuma(rig, media);
		REQUIRE(media.SeekVideo(3.0));
		// Paused: the frame at 3 s must arrive without resuming.
		CHECK(WaitFor(2000, pump, [&]{ return ScreenLuma(rig, media) >= ExpectedLuma(2.9); }));
		CHECK(ScreenLuma(rig, media) > before + 50);
		const double held = media.GetVideoPosition();
		CHECK(held == doctest::Approx(3.0).epsilon(0.03));
		// ...and it holds there.
		for(int i = 0; i < 20; i++){ pump(); SleepMs(10); }
		CHECK(media.GetVideoPosition() == doctest::Approx(held));
		CHECK(media.IsVideoPaused());
	}

	SUBCASE("restart goes back to the first frame"){
		REQUIRE(media.SeekVideo(2.0));
		WaitFor(1000, pump, [&]{ return media.GetVideoPosition() > 2.05; });
		REQUIRE(media.SeekVideo(0.0));
		CHECK(WaitFor(1000, pump, [&]{
			const double p = media.GetVideoPosition();
			return p >= 0.0 && p < 0.5 && ScreenLuma(rig, media) < ExpectedLuma(0.6);
		}));
	}

	SUBCASE("seeking past the end ends the clip"){
		REQUIRE(media.SeekVideoBy(100.0));
		CHECK(WaitFor(2000, pump, [&]{ return !media.HasVideo(); }));
		CHECK(media.GetVideoSerial() == 0);
	}

	media.StopAll();
	media.Shutdown();
}
