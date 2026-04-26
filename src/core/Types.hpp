#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace SatyrAV{

// ── Colours ──────────────────────────────────────────────────
struct Colour{
	uint8_t r, g, b, a;

	static Colour FromHex(uint32_t hex){
		return {
			(uint8_t)((hex >> 16) & 0xFF),
			(uint8_t)((hex >> 8) & 0xFF),
			(uint8_t)(hex & 0xFF),
			255
		};
	}
};

namespace Colours{
	constexpr Colour BACKGROUND   = {0x23, 0x33, 0x48, 0xFF};
	constexpr Colour BLACK        = {0x00, 0x00, 0x00, 0xFF};
	constexpr Colour WHITE        = {0xFF, 0xFF, 0xFF, 0xFF};
	constexpr Colour ORANGE       = {0xFF, 0xA5, 0x00, 0xFF};
	constexpr Colour LIME_GREEN   = {0x32, 0xCD, 0x32, 0xFF};
	constexpr Colour HIGHLIGHT    = {0x2E, 0x42, 0x5C, 0xFF};
	constexpr Colour HEADER_BG    = {0x19, 0x27, 0x38, 0xFF};
}

// ── Revy identifiers ────────────────────────────────────────
enum class RevyType{
	MatRevy,
	BioRevy,
	KemiRevy,
	MBKRevy,
	PsykoRevy,
	FysikRevy,
	GeoRevy,
	DIKURevy,
	SaTyR,
	Other
};

const char* RevyTypeName(RevyType type);

// ── Command types parsed from .ngk ──────────────────────────
enum class CommandType{
	Text,
	TextCont,       // (1.4) — append to the most recently pushed text entry
	// (1.6.1) Style-default variants of Text / TextCont. Parser sets
	// `bold` / `italic` on every produced run; dispatch is identical to
	// the unstyled forms. Inline `bold(...)` / `it(...)` wrappers stack on
	// top, so a run can be bold-italic via either route.
	TextBf,
	TextIt,
	TextBfCont,
	TextItCont,
	// (1.6.1) `textD "primary", "secondary"` — primary renders as a normal
	// centred text entry; secondary renders as a separate bottom-anchored
	// entry with italic-grey defaults from project options. If only one
	// argument is given the parser collapses to plain `Text`. Both pieces
	// support the full `+` concat / `bold(...)` / `it(...)` / `color(...)`
	// machinery; the secondary text lives in `Command::subtitleRuns`.
	TextD,
	Clear,
	ClearText,
	ClearImages,
	ClearAll,
	Play,
	Stop,
	StopParticleCont, // (1.4) — stop a particle system but let live particles finish
	Show,
	Loop,
	Compound,
	Comment
};

// (1.4) Distributions for sampling spawn positions in particle systems.
// p1/p2 are interpreted per-distribution:
//   Uniform   — unused.
//   Normal    — p1 = mean (0-1), p2 = stddev (0-1). Clamped to [0, 1].
//   LogNormal — p1 = mu,         p2 = sigma. Output clamped to [0, 1].
//   Binomial  — p1 = n (int ≥1), p2 = p (0-1). Output = successes / n.
//   Bernoulli — p1 = p (0-1).    Output = 0 or 1.
//   Gamma     — p1 = shape k,    p2 = scale θ. Output clamped to [0, 1].
enum class Distribution{
	Uniform,
	Normal,
	LogNormal,
	Binomial,
	Bernoulli,
	Gamma
};

struct DistSpec{
	Distribution type = Distribution::Uniform;
	float p1 = 0.5f;
	float p2 = 0.1f;
};

// Particle emulation types
enum class ParticleType{
	None,
	Rain,
	Snow,
	Confetti,
	Rise,
	Scatter,
	// (1.4) New motion models
	Brownian,     // random-walk velocity drift, no directional gravity
	Oscillation,  // sinusoidal x-motion, linear y drift
	Orbit,        // circular motion around the screen centre
	Noise,        // velocity driven by a 2D value-noise flow field
	LifeCurve     // linear motion with scale & alpha driven by lifetime curve
};

// ── Targeted display rectangle ───────────────────────────────
// The slave always paints black at the physical display's full
// resolution, then composites slave content into this sub-rect.
// Stored per-project in schema.toml under [Display].
struct TargetedDisplay{
	int   width    = 0;    // pixels; 0 = uninitialised → fill physical display on first load
	int   height   = 0;
	int   centerX  = 0;    // pixels relative to physical display top-left
	int   centerY  = 0;
	float rotation = 0.0f; // degrees, applied around (centerX, centerY)
};

// Render modifiers applied to text/show/play commands
struct RenderModifiers{
	float transparency = 1.0f; // 1.0 = fully opaque, 0.0 = invisible
	float rotation     = 0.0f; // degrees
	float posX         = -1.0f; // grid 0-100, -1 = centered (default)
	float posY         = -1.0f;

	// Colour override for text (alpha 0 = not set, use default)
	Colour textColour  = {0, 0, 0, 0};

	// Size override
	int width  = 0; // 0 = original size
	int height = 0;

	// Random size range
	bool useRandomSize     = false;
	int minWidth  = 0, minHeight = 0;
	int maxWidth  = 0, maxHeight = 0;
	bool keepAspectRatio   = true;

	// Particle
	ParticleType particleType = ParticleType::None;
	float particleIntensity   = 1.0f;

	// (1.4) Trail length — number of past frames kept per particle and
	// rendered behind it with fading alpha. 0 disables the trail.
	int   trailLength         = 0;
	// (1.4.1) Trail style:
	//   Ghost   — distance-gated ring of past frames, exponential alpha decay
	//             and age-shrink scale. Smoother and less smeary than 1.4.0.
	//   Stretch — single elongated quad per particle, rotated along the
	//             motion direction. Looks like real motion blur.
	//   Glow    — Ghost with additive blending. Best for bright sprites.
	enum class TrailMode{ Ghost, Stretch, Glow };
	TrailMode trailMode       = TrailMode::Ghost;

	// (1.4) Boundary collision — when true, particles reflect off the
	// four screen edges with an 0.8 energy-loss coefficient on the
	// reflected velocity component.
	bool  bounceEdges         = false;

	// (1.6.1) Per-segment text style flags set by inline `bold(...)` /
	// `it(...)` wrappers in a `text` argument. ParseTextRuns copies these
	// onto each produced TextRun; the slave renderer picks the matching
	// font face. Ignored by every non-text command type.
	bool bold   = false;
	bool italic = false;

	// (1.4) Per-system tunables resolved at dispatch time from the
	// scene → project → default cascade. Only consulted when
	// `particleType != None`.
	float speedMul            = 1.0f;
	float densityMul          = 1.0f;
	DistSpec xDist;
	// (1.5) Velocity distributions. Shape sampling of vx/vy within each
	// motion model's built-in range — Uniform reproduces the 1.4 behaviour
	// exactly. Formula-driven types (Orbit, Oscillation) ignore these.
	DistSpec vxDist;
	DistSpec vyDist;

	// Random images for particles (multiple textures, one picked per spawn)
	std::vector<std::string> randomImagePaths; // 0-1, maps to spawn rate

	// ── Animation: MOVE ──────────────────────────────────────
	// Content slides linearly from (moveFromX, moveFromY) to (moveToX,
	// moveToY) over moveDurationSec and then holds at the destination.
	// Coordinates are 0–100 percent of the targeted display. If a side
	// sentinel is set (moveFromSide / moveToSide != 0, values 1=N, 2=S,
	// 3=E, 4=W), the X/Y fields for that endpoint are ignored and the
	// endpoint is resolved to an off-screen position relative to the
	// content's own size at draw time.
	bool  hasMove         = false;
	float moveFromX       = 0.0f;
	float moveFromY       = 0.0f;
	float moveToX         = 0.0f;
	float moveToY         = 0.0f;
	float moveDurationSec = 0.0f;
	int   moveFromSide    = 0; // 0 = use X/Y; 1=N, 2=S, 3=E, 4=W
	int   moveToSide      = 0;

	// ── Animation: SPIN ──────────────────────────────────────
	// Continuous rotation at spinDegPerSec degrees per second (negative
	// = counter-clockwise). Added on top of the static `rotate(...)` so
	// they compose (static = initial offset, spin = dt-driven delta).
	float spinDegPerSec   = 0.0f;

	bool HasPosition() const{ return posX >= 0.0f && posY >= 0.0f; }
};

// A styled segment on a text line. Multiple runs come from the `+`
// concatenation operator (e.g. `text color(RED,"A ") + "B"`). Per-run
// fields override entry-level defaults; the rest of `RenderModifiers`
// stays at the line level because position/move/spin/rotation must act
// on the whole line, not each segment. Alpha 0 on `colour` = "not set".
struct TextRun{
	std::string text;
	Colour colour = {0, 0, 0, 0};
	float transparency = 1.0f;
	// (1.6.1) Per-segment style. Set by line-level keywords (`textbf` /
	// `textit` mark every produced run) or by inline wrappers (`bold(...)`
	// / `it(...)`) inside a `text "..." + ... ` concatenation. The slave
	// renderer picks the matching font face — regular / italic / bold /
	// bold-italic — falling back to regular if the project hasn't
	// configured the corresponding font path.
	bool bold = false;
	bool italic = false;
};

struct Command{
	CommandType type;
	std::string argument;
	std::string comment;
	int lineNumber = 0;

	// Render modifiers
	RenderModifiers mods;

	// (1.4) Multi-run text for the `+` concatenation operator and for
	// `textCont`. When non-empty, overrides `argument` as the source of
	// text. Always populated for Text and TextCont commands (single run
	// when no `+` was used). Empty for every other command type.
	std::vector<TextRun> runs;

	// (1.6.1) Subtitle/translation runs for `textD`. Empty for every
	// other command type. Same per-run modifier surface as `runs` — the
	// dispatcher pushes them as a separate bottom-anchored entry with
	// project-configured italic/colour/transparency defaults applied
	// before any per-run override.
	std::vector<TextRun> subtitleRuns;

	// For loop commands
	int loopCount     = 0;
	int clearEveryN   = 0;
	std::vector<Command> loopBody;

	// For compound commands (joined with &)
	std::vector<Command> subCommands;

	// Group tag: when a command is authored inside `group NAME{ … }`, every
	// resulting command (and every sub-command of a Compound) is tagged with
	// the group name so the slave can later clear that group selectively.
	std::string groupName;

	// (1.6.3) Set to the offending keyword when the parser fell through to
	// `Text` because it didn't recognise the leading bareword. Empty when
	// the command was either real text (started with a quote / `+`) or a
	// recognised keyword. The Overview Debug popup walks scenes and lifts
	// this into the warnings list so authors can spot typos.
	std::string unknownKeyword;
};

// (1.4) Per-particle-type tunables. Storage lives on Scene and RevySchema;
// dispatch resolves the effective tuning via scene → project → defaults.
struct ParticleTuning{
	float speed   = 1.0f;
	float density = 1.0f;
	DistSpec xDist;
	// (1.5) Velocity distributions — see RenderModifiers::vxDist/vyDist.
	DistSpec vxDist;
	DistSpec vyDist;
	bool haveSpeed   = false;
	bool haveDensity = false;
	bool haveXDist   = false;
	bool haveVxDist  = false;
	bool haveVyDist  = false;
};

struct Scene{
	std::string name;
	std::string fileName;
	Colour backgroundColor = Colours::BLACK;
	std::vector<std::string> preloadPaths;
	std::vector<Command> commands;

	// Per-scene options (0 / empty = use project/global fallback)
	int fontSize = 0;
	std::string fontPath;
	// (1.6.1) Per-style font faces. Empty = inherit from project/global, or
	// fall back to regular at draw time if no variant is configured.
	std::string fontPathItalic;
	std::string fontPathBold;
	std::string fontPathBoldItalic;
	Colour fontColour = {0, 0, 0, 0}; // alpha 0 = not set
	bool capitalize = false;

	// (1.4) Per-type particle tunables. Missing entries fall back to
	// project-level tunings, then to built-in defaults.
	std::unordered_map<int, ParticleTuning> particleTunings;

	// (1.6.3) Parser warnings raised while reading this scene's `.ngk`.
	// Populated for unknown keywords and other recoverable issues so the
	// Overview Debug popup can surface them. Empty on a clean parse.
	struct Warning{
		int lineNumber = 0;
		std::string message;
	};
	std::vector<Warning> warnings;
};

struct Akt{
	int number;
	std::vector<std::string> sceneNames;
	std::vector<Scene> scenes;
};

struct RevySchema{
	std::string revyName;
	// (1.6.2) Creator name from `[RevyData].Creator`. Empty for projects
	// authored before 1.6.2 — the LoadProject screen renders blank in
	// that case rather than fabricating a value.
	std::string creatorName;
	int year = 0;
	int fontSize = 25;
	int aktCount = 0;
	std::vector<Akt> akts;

	// Project-level options
	std::string fontPath;
	// (1.6.1) Project-level italic/bold font faces. Empty = use the
	// global Config-level path, falling back to regular at draw time.
	std::string fontPathItalic;
	std::string fontPathBold;
	std::string fontPathBoldItalic;
	Colour fontColour = {0, 0, 0, 0};
	Colour backgroundColor = Colours::BLACK;
	int textOutline = 4;
	bool capitalize = false; // outline thickness in points, 0 = disabled

	// (1.6.1) Project-level defaults for `textD` subtitle rendering.
	// These can be tuned live via the project's `schema.toml` `[Options]`
	// table while we figure out the right look. The colour defaults to
	// a light grey; the position is a percent from the top of the canvas.
	bool   subtitleItalic       = true;
	Colour subtitleColour       = {0xAA, 0xAA, 0xAA, 0xFF};
	float  subtitleTransparency = 1.0f;     // 1.0 = fully opaque
	float  subtitlePosY         = 90.0f;    // percent of canvas height

	// Per-project targeted display rect (width==0 means "not yet set —
	// initialise to fill the physical display on first load").
	TargetedDisplay targetedDisplay;

	// (1.4) Project-level particle tunables, keyed by ParticleType cast
	// to int. Per-scene `[Options]` can override any of the fields.
	std::unordered_map<int, ParticleTuning> particleTunings;

	// (1.6.3) Function-key audio hotkeys. Key = 1..12 (the F-number),
	// value = audio file path (resolved against the project's `sound/`
	// folder at dispatch time). Empty / missing entry = the F-key does
	// nothing in this project. Read from the schema's `[Hotkeys]` table.
	std::unordered_map<int, std::string> functionKeyAudio;
};

struct ProjectData{
	RevyType revyType     = RevyType::Other;
	// Display name used for the folder suffix and the schema's Revy field.
	// For standard revy types this is usually "<RevyTypeName>" or
	// "<RevyTypeName>Jubi" (e.g. "MatRevy", "MatRevyJubi"). For Other it
	// is the user-supplied custom name, optionally with "Jubi" appended.
	std::string revyName;
	int year              = 2026;
	std::string creatorName;
	std::string projectPath;
	RevySchema schema;
};

// ── Constants ────────────────────────────────────────────────
constexpr int MAX_CREATOR_NAME_LENGTH = 48;
constexpr int MIN_PROJECT_YEAR        = 1936;
constexpr int SCENE_NAME_DISPLAY_MAX  = 40;
constexpr int SCENE_NAME_TRUNCATE_AT  = 37;
constexpr int SPLASH_DURATION_MS      = 3000;
constexpr int JUMP_STEP               = 5;

} // namespace SatyrAV
