// (1.8.1) Types.hpp / Types.cpp — the pure-data layer.
//
// Deliberately not covered: the plain-aggregate structs (Command, Scene,
// RevySchema, RenderModifiers, …) have no logic of their own, only member
// defaults. Those defaults are asserted where they actually matter — in
// test_schema_parser.cpp (a missing key must leave the default in place)
// and test_cue_engine.cpp (the cascade falls through to them) — rather
// than restated here, where a test would just mirror the header.

#include <doctest/doctest.h>
#include "core/Types.hpp"

using namespace SatyrAV;

TEST_CASE("RevyTypeName covers every enumerator"){
	CHECK(std::string(RevyTypeName(RevyType::MatRevy))   == "MatRevy");
	CHECK(std::string(RevyTypeName(RevyType::BioRevy))   == "BioRevy");
	CHECK(std::string(RevyTypeName(RevyType::KemiRevy))  == "KemiRevy");
	CHECK(std::string(RevyTypeName(RevyType::MBKRevy))   == "MBKRevy");
	CHECK(std::string(RevyTypeName(RevyType::PsykoRevy)) == "PsykoRevy");
	CHECK(std::string(RevyTypeName(RevyType::FysikRevy)) == "FysikRevy");
	CHECK(std::string(RevyTypeName(RevyType::GeoRevy))   == "GeoRevy");
	CHECK(std::string(RevyTypeName(RevyType::DIKURevy))  == "DIKURevy");
	CHECK(std::string(RevyTypeName(RevyType::SaTyR))     == "SaTyR");
	CHECK(std::string(RevyTypeName(RevyType::Other))     == "Other");
}

TEST_CASE("RevyTypeName never returns null or empty"){
	// Guards the switch against a future enumerator being added without a
	// matching case — the trailing `return "Unknown"` must stay reachable
	// and non-empty so the Load Project screen never renders a blank group.
	for(int i = 0; i <= (int)RevyType::Other; i++){
		const char* name = RevyTypeName((RevyType)i);
		REQUIRE(name != nullptr);
		CHECK(name[0] != '\0');
	}
}

TEST_CASE("Colour::FromHex unpacks RRGGBB and forces opaque alpha"){
	auto c = Colour::FromHex(0xFF5500);
	CHECK((int)c.r == 0xFF);
	CHECK((int)c.g == 0x55);
	CHECK((int)c.b == 0x00);
	CHECK((int)c.a == 255);

	SUBCASE("black and white round-trip"){
		auto black = Colour::FromHex(0x000000);
		CHECK((int)black.r == 0);
		CHECK((int)black.g == 0);
		CHECK((int)black.b == 0);
		CHECK((int)black.a == 255);

		auto white = Colour::FromHex(0xFFFFFF);
		CHECK((int)white.r == 255);
		CHECK((int)white.g == 255);
		CHECK((int)white.b == 255);
	}

	SUBCASE("bits above 24 are ignored, not folded into the channels"){
		// A caller passing a 0xAARRGGBB value by mistake must not have the
		// alpha byte leak into red.
		auto c2 = Colour::FromHex(0xAB123456);
		CHECK((int)c2.r == 0x12);
		CHECK((int)c2.g == 0x34);
		CHECK((int)c2.b == 0x56);
		CHECK((int)c2.a == 255);
	}
}

TEST_CASE("RenderModifiers defaults describe 'nothing applied'"){
	// The dispatcher relies on these being neutral: any command that sets
	// no modifiers must render opaque, unrotated, unscaled and centred.
	RenderModifiers m;
	CHECK(m.transparency == doctest::Approx(1.0f));
	CHECK(m.rotation     == doctest::Approx(0.0f));
	CHECK(m.width  == 0);
	CHECK(m.height == 0);
	CHECK(m.particleType == ParticleType::None);
	CHECK(m.trailLength == 0);
	CHECK(m.bounceEdges == false);
	CHECK(m.hasMove == false);
	CHECK(m.spinDegPerSec == doctest::Approx(0.0f));
	CHECK(m.bold == false);
	CHECK(m.italic == false);
	// Alpha 0 on the colour override is the "not set" sentinel.
	CHECK((int)m.textColour.a == 0);
}

TEST_CASE("RenderModifiers::HasPosition distinguishes set from centred"){
	RenderModifiers m;
	CHECK_FALSE(m.HasPosition()); // -1 / -1 == centred

	m.posX = 50.0f;
	CHECK_FALSE(m.HasPosition()); // only one axis set is still not a position

	m.posY = 25.0f;
	CHECK(m.HasPosition());

	SUBCASE("zero is a legal position, not 'unset'"){
		RenderModifiers origin;
		origin.posX = 0.0f;
		origin.posY = 0.0f;
		CHECK(origin.HasPosition());
	}
}

TEST_CASE("DistSpec defaults to Uniform"){
	// Uniform is the identity case — 1.4 behaviour is reproduced exactly
	// when a scene sets no distribution at all.
	DistSpec d;
	CHECK(d.type == Distribution::Uniform);
}

TEST_CASE("TargetedDisplay zero-initialises to the 'fill display' sentinel"){
	// width == 0 is what tells the slave to fill the whole physical
	// display on first load; a non-zero default would silently letterbox
	// every new project.
	TargetedDisplay td;
	CHECK(td.width  == 0);
	CHECK(td.height == 0);
	CHECK(td.centerX == 0);
	CHECK(td.centerY == 0);
	CHECK(td.rotation == doctest::Approx(0.0f));
}
