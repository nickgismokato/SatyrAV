#include "core/Project.hpp"
#include "core/SchemaParser.hpp"
#include "core/SceneParser.hpp"
#include "core/AssetPath.hpp"
#include <filesystem>
#include <fstream>
#include <toml++/toml.hpp>

namespace fs = std::filesystem;

namespace SatyrAV{

ProjectData Project::Create(
	RevyType type, int year,
	const std::string& creatorName,
	const std::string& projectPath,
	const std::string& revyName){
	ProjectData data;
	data.revyType = type;
	// If no explicit display name was supplied (e.g. older callers),
	// fall back to the enum's built-in name so the schema is still valid.
	data.revyName = revyName.empty() ? RevyTypeName(type) : revyName;
	data.year = year;
	data.creatorName = creatorName;
	data.projectPath = projectPath;

	// Create directory structure
	fs::create_directories(projectPath);
	fs::create_directories(projectPath + "/scenes");
	fs::create_directories(projectPath + "/movies");
	fs::create_directories(projectPath + "/sound");
	fs::create_directories(projectPath + "/pictures");
	// (2.0.1) For the `O` mark-line log; opening a project also makes it.
	fs::create_directories(projectPath + "/fix-it");

	// Copy the logo as an example image
	std::string logoSrc = FindAsset("satyr-logo-body.png");
	std::string logoDst = projectPath + "/pictures/satyr-logo.png";
	if(fs::exists(logoSrc)){
		fs::copy_file(logoSrc, logoDst, fs::copy_options::skip_existing);
	}

	// Generate default schema and the four example scenes it references
	GenerateDefaultSchema(projectPath + "/schema.toml", data);
	GenerateExampleScenes(projectPath + "/scenes");

	// Reload the project so it's fully populated
	return Load(projectPath);
}

ProjectData Project::Load(const std::string& projectPath){
	ProjectData data;
	data.projectPath = projectPath;

	std::string schemaPath = projectPath + "/schema.toml";
	if(!fs::exists(schemaPath)) return data;

	data.schema = SchemaParser::ParseFile(schemaPath);
	// (1.6.2) Mirror creator up to ProjectData so callers reading the
	// top-level field see the persisted value. revyName/year stay split
	// across `data` (bare name + int year, set at create-time) and
	// `schema` (combined "Name YYYY" string + parsed int) intentionally
	// — the LoadProject screen reads the schema fields directly.
	data.creatorName = data.schema.creatorName;
	if(data.schema.year > 0) data.year = data.schema.year;

	// Load all scene files
	for(auto& akt : data.schema.akts){
		for(auto& sceneName : akt.sceneNames){
			std::string scenePath = projectPath + "/scenes/" + sceneName + ".ngk";
			Scene scene = SceneParser::ParseFile(scenePath);
			if(scene.name.empty()){
				scene.name = sceneName;
			}
			akt.scenes.push_back(std::move(scene));
		}
	}

	return data;
}

void Project::Save(const ProjectData& data){
	// Round-trip the schema TOML: load → mutate → write. Only the fields
	// we actively manage from inside the app are updated; everything else
	// (Options, RevyData, Structure.*) is preserved byte-for-byte by toml++
	// on emit. Note: toml++ does not preserve stand-alone comments — users
	// who hand-edit schema.toml with # notes will lose them here.
	std::string schemaPath = data.projectPath + "/schema.toml";
	if(!fs::exists(schemaPath)) return;

	toml::table tbl;
	try{
		tbl = toml::parse_file(schemaPath);
	} catch(const toml::parse_error&){
		return;
	}

	// Ensure [Display] exists, then update its keys from the in-memory schema.
	if(!tbl.contains("Display") || !tbl["Display"].is_table()){
		tbl.insert_or_assign("Display", toml::table{});
	}
	auto& disp = *tbl["Display"].as_table();
	const auto& td = data.schema.targetedDisplay;
	disp.insert_or_assign("Width",    (int64_t)td.width);
	disp.insert_or_assign("Height",   (int64_t)td.height);
	disp.insert_or_assign("CenterX",  (int64_t)td.centerX);
	disp.insert_or_assign("CenterY",  (int64_t)td.centerY);
	disp.insert_or_assign("Rotation", (double)td.rotation);

	std::ofstream out(schemaPath);
	if(!out.is_open()) return;
	out << tbl;
}

// (1.8.0) Every option the schema parser reads is emitted at its default
// value, uncommented, so an operator can edit in place instead of looking
// the key up in the docs. The one exception is the ten `[Particle.<TYPE>]`
// tables: their default is "no override at all", and writing 10 × 11 no-op
// keys into every new project is noise — so one fully-documented commented
// template stands in for all ten.
//
// (1.8.4) `FontSize` is the one value that deliberately differs from the
// parser's built-in fallback. The fallback is 25, which suits a desktop
// window but is far too small on a projected slave display — new projects
// therefore start at 60, a usable size for a hall. The fallback itself is
// left alone: changing it would resize every existing project that omits
// the key, and the whole point of the 1.8.0 generator is that it never
// alters how an existing scene renders.
//
// Note: `Project::Save` round-trips this file through toml++, which does
// not preserve stand-alone comments. The first Debug → S rect save strips
// the commentary below; the keys and values survive.
void Project::GenerateDefaultSchema(const std::string& path, const ProjectData& data){
	std::ofstream out(path);
	if(!out.is_open()) return;

	out << R"NGK(# ============================================================
# schema.toml - project definition
# ============================================================
# Every option is listed at its default value. Scene [Options] in a
# .ngk file override these for that scene only; see the cascade table
# in docs/project-structure.md.

)NGK";

	out << "[RevyData]\n";
	// data.revyName already includes the Jubi suffix and/or custom name
	// chosen by the user in the New Project screen.
	out << "Revy    = \"" << data.revyName << " " << data.year << "\"\n";
	// (1.6.2) Persist the creator so the Load Project screen can show
	// authorship in its second column. Empty string is acceptable for
	// users who skip the field; older projects can backfill by hand.
	out << "Creator = \"" << data.creatorName << "\"\n\n";

	out << R"NGK([Options]
FontSize             = 60        # Slave text size, in points
TextOutline          = 4         # Black outline thickness in points, 0 = off
Capitalize           = false     # Uppercase every line of slave text
AudioFade            = false     # Let `play` fade times fade the sound too, not just the picture
FixedTextCentre      = true      # Centre text on all its lines up to the next clear, so lines don't jump
BackgroundColour     = "black"   # A colour name, or a "#RRGGBB" string
TextColour           = "white"   # Default slave text colour: a colour name, or "#RRGGBB"
Font                 = ""        # Regular face: full path, or a bare FONTS/ filename
FontItalic           = ""        # Italic face, used by textit and it(...)
FontBold             = ""        # Bold face, used by textbf and bold(...)
FontBoldItalic       = ""        # Combined face, used when a run is bold AND italic
SubtitleItalic       = true      # Render the textD translation line italic
SubtitleColour       = "grey"    # grey | white | black, or a "#RRGGBB" string
SubtitleTransparency = 0.0       # Percent: 0 = opaque, 100 = invisible
SubtitlePosY         = 90.0      # Percent down from the top of the canvas

[Structure]
akter = 2

[Structure.Akt1]
schema = [
	"TextShowcase",
	"PictureShowcase"
]

[Structure.Akt2]
schema = [
	"ParticleShowcase",
	"MacroShowcase"
]

# Targeted display rect inside the physical secondary screen. All zero
# means "fill the whole display". Adjust it live from the Debug popup
# (D) -> S; ESC writes the result back here.
[Display]
Width    = 0
Height   = 0
CenterX  = 0
CenterY  = 0
Rotation = 0.0

# F1-F12 audio hotkeys for one-tap playback during a performance.
# Bare filenames resolve against this project's sound/ folder. An empty
# value leaves the key unbound. Pressing the same key while its sound is
# still playing stops it.
[Hotkeys]
F1  = ""
F2  = ""
F3  = ""
F4  = ""
F5  = ""
F6  = ""
F7  = ""
F8  = ""
F9  = ""
F10 = ""
F11 = ""
F12 = ""

# Backstage screen. Every scene change POSTs the current scene and the
# next two to the backstage ticker, taken from each scene's [Backstage]
# block; scenes without one are left out, so nothing is sent until a scene
# has a block. Enabled = false switches it off and keeps the Url.
[Backstage]
Enabled = true
Url     = "https://backstage.texnik.dk/set-bck"

# Project-level particle tuning, one table per type. The block below
# lists every key with its default value - uncomment the table and the
# keys you want to change. The same keys exist for SNOW, CONFETTI, RISE,
# SCATTER, BROWNIAN, OSCILLATION, ORBIT, NOISE and LIFECURVE.
# Distributions: UNIFORM, NORMAL, LOGNORMAL, BINOMIAL, BERNOULLI, GAMMA.
# [Particle.RAIN]
# Speed    = 1.0        # Velocity multiplier
# Density  = 1.0        # Spawn-rate multiplier
# XDist    = "UNIFORM"  # Spawn-x distribution
# XDistP1  = 0.5
# XDistP2  = 0.1
# VXDist   = "UNIFORM"  # Horizontal velocity distribution
# VXDistP1 = 0.5
# VXDistP2 = 0.1
# VYDist   = "UNIFORM"  # Vertical velocity distribution
# VYDistP1 = 0.5
# VYDistP2 = 0.1
)NGK";
}

static void WriteSceneFile(const std::string& path, const char* content){
	std::ofstream out(path);
	if(!out.is_open()) return;
	out << content;
}

// (1.8.0) Four tutorial scenes across the two generated akter, one per
// layer of the engine. Every scene only references `satyr-logo.png` —
// the single asset `Create` copies into `pictures/` — so a brand-new
// project runs end to end without the operator adding any media first.
void Project::GenerateExampleScenes(const std::string& scenesDir){
	// ── Akt 1, scene 1 — the text layer ──────────────────────
	WriteSceneFile(scenesDir + "/TextShowcase.ngk", R"NGK(# =============================================
# TextShowcase.ngk - the text layer
# =============================================
# Every non-comment line under [Commands] is one cue. Press ENTER to
# fire the next one. Anything after a # is a comment and never runs.
# Full command reference: docs/commands.md

[Options]
# BackgroundColour = blue  # Overrides the project BackgroundColour
# TextColour = yellow      # Default text colour for this scene
# FontSize = 30            # Overrides the project FontSize for this scene
# Capitalize = true        # Uppercase every line of text

# What the backstage screen shows for this scene, when the project's
# schema.toml has a [Backstage] Url. Scenes without this block are left
# off the backstage screen.
# [Backstage]
# Name         = Text showcase
# ExpectedTime = 2m40s
# Actors       = ["Actor 1", "Actor 2"]
# Ninjas       = ["Ninja 1"]

[Commands]
comment "--- Plain text ---"
text "Welcome to SatyrAV!"
text "Each line here is one cue - press ENTER for the next."
clear

comment "--- Styling ---"
textbf "This line is bold"
textit "and this one is italic"
clear
text colour(RED, "Red") + " sitting next to " + colour(LIME, "green")
text "Plain " + bold("BOLD") + " plus " + it("italic") + " on one line"
clear

comment "--- Revealing a line piece by piece ---"
text "Loading"
textCont " ."
textCont " ."
textCont " . done!"
clear

comment "--- Placement and translation ---"
text pos([50, 25], "Positioned 50% across, 25% down")
clear
textD "Good evening!", "Godaften!"
clear

comment "--- Live countdown ---"
text "The show starts in:"
countdown(10, "en")
clearCountdown
clear

text "That is the text layer. Next scene: pictures."
clear
)NGK");

	// ── Akt 1, scene 2 — the image layer ─────────────────────
	WriteSceneFile(scenesDir + "/PictureShowcase.ngk", R"NGK(# =============================================
# PictureShowcase.ngk - the image layer
# =============================================
# Image paths resolve against the project root and then pictures/,
# so `show satyr-logo.png` finds pictures/satyr-logo.png.
# Supported: .png .jpg .bmp .webp .gif (animated GIFs cycle on their own).

[Options]
# BackgroundColour = black

[Preload]
satyr-logo.png

[Commands]
comment "--- A plain image ---"
text "show puts a picture on the slave:"
show satyr-logo.png
clear

comment "--- Size and position ---"
show size([200, 200], satyr-logo.png)
clear
show pos([25, 50], size([200, 200], satyr-logo.png))
show pos([75, 50], size([200, 200], satyr-logo.png))
clear

comment "--- Images stack until you clear them ---"
show pos([30, 30], size([150, 150], satyr-logo.png))
show pos([50, 50], size([150, 150], satyr-logo.png))
show pos([70, 70], size([150, 150], satyr-logo.png))
clearImages

comment "--- Transparency and rotation ---"
show trans(60, size([300, 300], satyr-logo.png))
clear
show rotate(30, size([250, 250], satyr-logo.png))
clear
show spin(90, size([250, 250], satyr-logo.png))
clear

comment "--- Movement ---"
show move(W, [50, 50], 1200, size([200, 200], satyr-logo.png))
clear
show move([0, 100], [100, 0], 2000, size([150, 150], satyr-logo.png))
clear

comment "--- Random size, and one cue doing two things ---"
show random(satyr-logo.png, [80, 80], [320, 320], true)
clear & text "clear and text in a single cue, joined with &"
clear
)NGK");

	// ── Akt 2, scene 1 — the particle layer ──────────────────
	WriteSceneFile(scenesDir + "/ParticleShowcase.ngk", R"NGK(# =============================================
# ParticleShowcase.ngk - the particle layer
# =============================================
# particle(TYPE, [intensity,] image) turns an image into an emitter.
# Intensity is 1-100 and defaults to 100.
# Types: RAIN SNOW CONFETTI RISE SCATTER BROWNIAN OSCILLATION
#        ORBIT NOISE LIFECURVE

[Options]
# BackgroundColour = black
# Per-type tuning for this scene only. Project-wide defaults live in
# schema.toml under [Particle.<TYPE>]; each field cascades on its own.
SNOW.SPEED  = 0.8                  # Velocity multiplier
SNOW.DENS   = 1.5                  # Spawn-rate multiplier
SNOW.X.DIST = NORMAL(0.5, 0.15)    # Cluster spawns near the middle

[Preload]
satyr-logo.png

[Commands]
comment "--- Falling particles ---"
text "SNOW, using this scene's tuning:"
show particle(SNOW, 40, size([48, 48], satyr-logo.png))
stopParticleCont                   #Stop spawning, let the live ones land
clear
show particle(RAIN, 60, size([32, 32], satyr-logo.png))
clear

comment "--- Bursts and rising ---"
show particle(CONFETTI, 50, size([32, 32], satyr-logo.png))
clear
show particle(RISE, 40, size([40, 40], satyr-logo.png))
clear

comment "--- Motion models ---"
show particle(ORBIT, 30, size([40, 40], satyr-logo.png))
clear
show particle(NOISE, 30, size([32, 32], satyr-logo.png))
clear
show particle(BROWNIAN, 30, bounce(true, size([40, 40], satyr-logo.png)))
clear

comment "--- Trails ---"
show particle(ORBIT, 20, trail(16, size([40, 40], satyr-logo.png)))
clear
show particle(NOISE, 20, trailGlow(16, size([32, 32], satyr-logo.png)))
clear

text "Particles are dropped by clear, clearImages and clearAll."
clear
)NGK");

	// ── Akt 2, scene 2 — grouping, macros and blocks ─────────
	WriteSceneFile(scenesDir + "/MacroShowcase.ngk", R"NGK(# =============================================
# MacroShowcase.ngk - groups, macros and blocks
# =============================================
# [Macro] defines reusable blocks; `macro NAME` inlines one at parse
# time. Macro bodies are simple - no loops, groups or nested macros.
#
# run{ }    fires a whole block on one key press.
# loop(n,c){ } repeats a block, clearing after every c iterations.
# group N{ }   tags what the body produces so `clear N` drops just it.

[Options]
# BackgroundColour = black

[Preload]
satyr-logo.png

[Macro]
Title{
	text colour(ORANGE, "SatyrAV")
	textCont " - macro and group showcase"
}

LogoPair{
	show pos([30, 50], size([180, 180], satyr-logo.png))
	show pos([70, 50], size([180, 180], satyr-logo.png))
}

[Commands]
comment "--- macro: one reference, two cues ---"
macro Title
clear

comment "--- run{}: one key press, everything at once ---"
run{
	text "Both of these appeared together"
	show pos([50, 70], size([150, 150], satyr-logo.png))
}
clear

comment "--- the same macro, reused ---"
macro LogoPair
clearImages

comment "--- group: clear part of the screen by name ---"
group Backdrop{
	run{
		show pos([50, 50], trans(70, size([400, 400], satyr-logo.png)))
	}
}
text "This text is ungrouped."
text "clearText drops the text but leaves the backdrop alone."
clearText
clear Backdrop

comment "--- loop: repeat a block ---"
loop(3, 1){
	text "Loop iteration"
	text "Cleared after every iteration"
}
text "Loop finished!"
clearAll

text "Edit these .ngk files, or add your own under scenes/."
text "Remember to list any new scene in schema.toml."
clear
)NGK");
}

} // namespace SatyrAV
