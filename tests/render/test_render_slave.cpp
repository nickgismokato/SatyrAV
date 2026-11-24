// The slave as the audience sees it: text and images drawn by the real
// SlaveWindow / TextRenderer into the slave canvas and read back.

#include "render_helpers.hpp"
#include "render/SlaveWindow.hpp"
#include "render/MediaPlayer.hpp"

using namespace SatyrRender;

namespace{

bool IsRed(const Pixel& p){ return p.r > 180 && p.g < 80 && p.b < 80; }
bool IsBlue(const Pixel& p){ return p.b > 180 && p.r < 80 && p.g < 80; }
bool NotBlack(const Pixel& p){ return p.r > 40 || p.g > 40 || p.b > 40; }

} // namespace

TEST_CASE("Render: text is drawn in its colour, and clear removes it"){
	OffscreenRig rig;
	REQUIRE(rig.ok);
	SlaveWindow slave;
	MediaPlayer media;

	RenderModifiers mods;
	mods.textColour = {0xFF, 0x00, 0x00, 0xFF};
	slave.PushText("HELLO WORLD", mods);

	SDL_Surface* s = rig.RenderCanvas([&]{
		slave.Update(rig.r, media, 0.016f);
		slave.DrawText(rig.r, rig.text);
	});
	REQUIRE(s);
	const int W = rig.W(), H = rig.H();
	// Centred: red glyph pixels in the middle band, none in the corners.
	CHECK(Count(s, W / 4, H / 3, 3 * W / 4, 2 * H / 3, IsRed) > 20);
	CHECK(Count(s, 0, 0, W / 8, H / 8, NotBlack) == 0);
	SDL_DestroySurface(s);

	slave.ClearText();
	s = rig.RenderCanvas([&]{
		slave.Update(rig.r, media, 0.016f);
		slave.DrawText(rig.r, rig.text);
	});
	REQUIRE(s);
	CHECK(Count(s, 0, 0, W, H, NotBlack) == 0);
	SDL_DestroySurface(s);
}

TEST_CASE("Render: the background colour fills the canvas"){
	OffscreenRig rig;
	REQUIRE(rig.ok);
	SlaveWindow slave;
	MediaPlayer media;
	slave.SetBackgroundColour({0x00, 0x00, 0xFF, 0xFF});
	SDL_Surface* s = rig.RenderCanvas([&]{ slave.Update(rig.r, media, 0.016f); });
	REQUIRE(s);
	CHECK(IsBlue(At(s, 5, 5)));
	CHECK(IsBlue(At(s, rig.W() - 5, rig.H() - 5)));
	SDL_DestroySurface(s);
}

TEST_CASE("Render: a higher layer() is drawn on top, whatever the show order"){
	OffscreenRig rig;
	REQUIRE(rig.ok);
	MediaPlayer media;
	SDL_Texture* red  = SolidTexture(rig.r.GetSlave(), 64, 64, 255, 0, 0);
	SDL_Texture* blue = SolidTexture(rig.r.GetSlave(), 64, 64, 0, 0, 255);
	REQUIRE(red);
	REQUIRE(blue);

	auto centre = [&](int redLayer, int blueLayer){
		SlaveWindow slave;
		RenderModifiers rm, bm;
		rm.layer = redLayer;  rm.width = 400; rm.height = 400;
		bm.layer = blueLayer; bm.width = 400; bm.height = 400;
		slave.ShowImage(red, 64, 64, rm, "", "red.png");
		slave.ShowImage(blue, 64, 64, bm, "", "blue.png");
		SDL_Surface* s = rig.RenderCanvas([&]{ slave.Update(rig.r, media, 0.016f); });
		const Pixel p = At(s, rig.W() / 2, rig.H() / 2);
		SDL_DestroySurface(s);
		return p;
	};
	CHECK(IsBlue(centre(0, 0)));   // same layer: the later show on top
	CHECK(IsRed(centre(1, 0)));    // red's layer is higher
	CHECK(IsBlue(centre(0, 1)));

	SDL_DestroyTexture(red);
	SDL_DestroyTexture(blue);
}

TEST_CASE("Render: shown images keep their file name and a unique id"){
	OffscreenRig rig;
	REQUIRE(rig.ok);
	SDL_Texture* t = SolidTexture(rig.r.GetSlave(), 8, 8, 255, 255, 255);
	SlaveWindow slave;
	slave.ShowImage(t, 8, 8, {}, "", "/p/pictures/a.png");
	slave.ShowImage(t, 8, 8, {}, "", "/p/pictures/a.png");
	const auto& im = slave.GetImageEntries();
	REQUIRE(im.size() == 2);
	CHECK(im[0].source == "/p/pictures/a.png");
	CHECK(im[0].id != im[1].id);
	SDL_DestroyTexture(t);
}

TEST_CASE("Render: the font family name, for the .ass subtitles"){
	OffscreenRig rig;
	REQUIRE(rig.ok);
	CHECK(rig.text.FontFamilyName(FindAsset("fonts/DejaVuSans-Bold.ttf")) == "DejaVu Sans");
	CHECK(rig.text.FontFamilyName("") == "DejaVu Sans");
	// Not a font: the file name stands in.
	CHECK(rig.text.FontFamilyName("/nowhere/My Font.ttf") == "My Font");
}
