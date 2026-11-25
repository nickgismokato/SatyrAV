// Where a name in a scene points on disk (core/ProjectPaths): folders,
// spaces, `\`, case, absolute paths, and [Preload] wildcards. The show,
// the preload cache and the project scanner all use this lookup.

#include <doctest/doctest.h>
#include "helpers.hpp"
#include "core/ProjectPaths.hpp"

using namespace SatyrAV;
using SatyrTest::TempDir;

namespace{

// A small project: pictures in nested folders, with spaces.
struct Proj{
	TempDir dir{"paths"};
	std::string p;
	Proj(){
		p = dir.Str();
		for(const char* f : {
			"pictures/logo.png",
			"pictures/MEGA/5.png", "pictures/MEGA/6.png", "pictures/MEGA/notes.txt",
			"pictures/MEGA/Lyric/Sequence 1/1A.png",
			"pictures/MEGA/Lyric/Sequence 1/2A.png",
			"pictures/MEGA/Lyric/Sequence 2/1B.PNG",
			"pictures/my picture.png",
			"movies/intro.mp4",
			"sound/ding.mp3",
			"stage/plan.png"}){
			dir.Write(f, "x");
		}
	}
	std::string F(const std::string& rel) const{ return p + "/" + rel; }
};

} // namespace

TEST_CASE("ResolveMedia: plain names, folders and spaces"){
	Proj pr;
	CHECK(ProjectPaths::ResolveMedia(pr.p, "logo.png") == pr.F("pictures/logo.png"));
	CHECK(ProjectPaths::ResolveMedia(pr.p, "intro.mp4") == pr.F("movies/intro.mp4"));
	CHECK(ProjectPaths::ResolveMedia(pr.p, "ding.mp3") == pr.F("sound/ding.mp3"));
	// Folders under pictures/, with spaces.
	CHECK(ProjectPaths::ResolveMedia(pr.p, "MEGA/Lyric/Sequence 1/1A.png")
		== pr.F("pictures/MEGA/Lyric/Sequence 1/1A.png"));
	CHECK(ProjectPaths::ResolveMedia(pr.p, "my picture.png") == pr.F("pictures/my picture.png"));
	// A folder in the project itself.
	CHECK(ProjectPaths::ResolveMedia(pr.p, "stage/plan.png") == pr.F("stage/plan.png"));
	// And pictures/ written out.
	CHECK(ProjectPaths::ResolveMedia(pr.p, "pictures/MEGA/5.png") == pr.F("pictures/MEGA/5.png"));
}

TEST_CASE("ResolveMedia: backslashes, ./, case, absolute, missing"){
	Proj pr;
	CHECK(ProjectPaths::ResolveMedia(pr.p, "MEGA\\Lyric\\Sequence 1\\2A.png")
		== pr.F("pictures/MEGA/Lyric/Sequence 1/2A.png"));
	CHECK(ProjectPaths::ResolveMedia(pr.p, "./MEGA/5.png") == pr.F("pictures/MEGA/5.png"));
	// Case as Windows sees it: the names on disk come back.
	CHECK(ProjectPaths::ResolveMedia(pr.p, "mega/lyric/SEQUENCE 2/1b.png")
		== pr.F("pictures/MEGA/Lyric/Sequence 2/1B.PNG"));
	// Absolute: as it is.
	const std::string abs = pr.F("pictures/logo.png");
	CHECK(ProjectPaths::ResolveMedia("/elsewhere", abs) == abs);
	// Missing: the project path, so it can still be named.
	CHECK(ProjectPaths::ResolveMedia(pr.p, "MEGA/1.png") == pr.F("MEGA/1.png"));
	CHECK_FALSE(ProjectPaths::IsFile(ProjectPaths::ResolveMedia(pr.p, "MEGA/1.png")));
}

TEST_CASE("Wildcards: *, ?, ** and case"){
	Proj pr;
	using V = std::vector<std::string>;
	CHECK(ProjectPaths::HasWildcard("MEGA/*.png"));
	CHECK_FALSE(ProjectPaths::HasWildcard("MEGA/5.png"));

	// One folder, not below it; files only.
	CHECK(ProjectPaths::ExpandWildcard(pr.p, "MEGA/*.png")
		== V{pr.F("pictures/MEGA/5.png"), pr.F("pictures/MEGA/6.png")});
	CHECK(ProjectPaths::ExpandWildcard(pr.p, "Mega/*.PNG").size() == 2);   // case ignored
	CHECK(ProjectPaths::ExpandWildcard(pr.p, "MEGA/?.png").size() == 2);
	CHECK(ProjectPaths::ExpandWildcard(pr.p, "MEGA/*").size() == 3);       // notes.txt too
	// A folder with a space, and a wildcard folder.
	CHECK(ProjectPaths::ExpandWildcard(pr.p, "MEGA/Lyric/Sequence 1/*.png").size() == 2);
	CHECK(ProjectPaths::ExpandWildcard(pr.p, "MEGA/Lyric/Sequence */*.png").size() == 3);
	// ** : any depth, including none.
	CHECK(ProjectPaths::ExpandWildcard(pr.p, "MEGA/**/*.png").size() == 5);
	CHECK(ProjectPaths::ExpandWildcard(pr.p, "**/1*.png").size() == 2);
	// Nothing matches.
	CHECK(ProjectPaths::ExpandWildcard(pr.p, "Nope/*.png").empty());
	// Results are joined with / exactly as ResolveMedia joins them — the
	// media cache is keyed on the string.
	CHECK(ProjectPaths::ExpandWildcard(pr.p, "MEGA/Lyric/Sequence 1/1*.png")[0]
		== ProjectPaths::ResolveMedia(pr.p, "MEGA/Lyric/Sequence 1/1A.png"));
}

TEST_CASE("ExpandPreload: patterns expanded, names resolved"){
	Proj pr;
	const auto out = ProjectPaths::ExpandPreload(pr.p, {"logo.png", "MEGA/*.png", "gone.png"});
	REQUIRE(out.size() == 4);
	CHECK(out[0] == pr.F("pictures/logo.png"));
	CHECK(out[1] == pr.F("pictures/MEGA/5.png"));
	CHECK(out[2] == pr.F("pictures/MEGA/6.png"));
	CHECK(out[3] == pr.F("gone.png"));
}
