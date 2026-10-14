// (1.8.4) The `[Cue]` section and the `\Name` command reference that binds
// a note to a navigator row.
//
// Deliberately not covered: the Cue Notes column itself — layout, lockstep
// scrolling and note wrapping live in MasterWindow and are SDL-bound.
// Render-layer tests are deferred to 2.1.0; the wiring below is what makes
// that column's data correct.

#include <doctest/doctest.h>
#include "helpers.hpp"

using namespace SatyrAV;
using SatyrTest::ParseScene;

static const CueNote* Find(const Scene& s, const std::string& name){
	return FindCueNote(s, name);
}

static bool HasWarningContaining(const Scene& s, const std::string& needle){
	for(const auto& w : s.warnings){
		if(w.message.find(needle) != std::string::npos) return true;
	}
	return false;
}

// ── The [Cue] section ────────────────────────────────────────

TEST_CASE("[Cue] parses name and note text"){
	auto s = ParseScene(
		"[Cue]\n"
		"Cue1 Play audio when Actor leaves the scene\n"
		"Cue2 Stop audio when Actor begin screaming from outside the scene\n"
		"\n"
		"[Commands]\n"
		"clear\n");
	REQUIRE(s.cueNotes.size() == 2);
	CHECK(s.cueNotes[0].name == "Cue1");
	CHECK(s.cueNotes[0].text == "Play audio when Actor leaves the scene");
	CHECK(s.cueNotes[1].name == "Cue2");
	CHECK(s.cueNotes[1].text
		== "Stop audio when Actor begin screaming from outside the scene");
	CHECK(s.cueNotes[0].lineNumber == 2);
	CHECK(s.cueNotes[1].lineNumber == 3);
}

TEST_CASE("[Cue] note text keeps its internal spacing and punctuation"){
	auto s = ParseScene(
		"[Cue]\n"
		"A   lots   of   spaces, commas & \"quotes\" (and parens)\n"
		"[Commands]\n"
		"clear \\A\n");
	REQUIRE(s.cueNotes.size() == 1);
	CHECK(s.cueNotes[0].text
		== "lots   of   spaces, commas & \"quotes\" (and parens)");
}

TEST_CASE("[Cue] names may use letters, digits and underscores"){
	auto s = ParseScene(
		"[Cue]\n"
		"Cue1 first\n"
		"stage_left_2 second\n"
		"[Commands]\n"
		"clear \\Cue1\n"
		"clear \\stage_left_2\n");
	CHECK(s.cueNotes.size() == 2);
	REQUIRE(s.commands.size() == 2);
	CHECK(s.commands[0].cueName == "Cue1");
	CHECK(s.commands[1].cueName == "stage_left_2");
	CHECK(s.warnings.empty());
}

TEST_CASE("A trailing comment does not become part of the note"){
	auto s = ParseScene(
		"[Cue]\n"
		"Cue1 The real note   #authoring reminder\n"
		"[Commands]\n"
		"clear \\Cue1\n");
	REQUIRE(s.cueNotes.size() == 1);
	CHECK(s.cueNotes[0].text == "The real note");
}

TEST_CASE("A scene with no [Cue] section has no notes and no warnings"){
	auto s = ParseScene("[Commands]\nclear\ntext \"a\"\n");
	CHECK(s.cueNotes.empty());
	CHECK(s.warnings.empty());
	for(const auto& c : s.commands) CHECK(c.cueName.empty());
}

// ── Referencing a note from a command ────────────────────────

TEST_CASE("A \\Name reference binds a note and is stripped from the command"){
	auto s = ParseScene(
		"[Cue]\n"
		"Cue1 Play audio when Actor leaves\n"
		"[Commands]\n"
		"play Music.mp3 \\Cue1\n");
	REQUIRE(s.commands.size() == 1);
	CHECK(s.commands[0].type == CommandType::Play);
	CHECK(s.commands[0].cueName == "Cue1");
	// The reference must not leak into the filename.
	CHECK(s.commands[0].argument == "Music.mp3");
	CHECK(s.warnings.empty());
}

TEST_CASE("The TODO's worked example parses as specified"){
	auto s = ParseScene(
		"[Cue]\n"
		"Cue1 Play audio when Actor leaves the scene\n"
		"Cue2 Stop audio when Actor begin screaming from outside the scene\n"
		"\n"
		"[Commands]\n"
		"show backgroundImage.png\n"
		"play Music.mp3 \\Cue1\n"
		"stop \\Cue2\n");
	REQUIRE(s.commands.size() == 3);
	CHECK(s.commands[0].argument == "backgroundImage.png");
	CHECK(s.commands[0].cueName.empty());
	CHECK(s.commands[1].argument == "Music.mp3");
	CHECK(s.commands[1].cueName == "Cue1");
	CHECK(s.commands[2].type == CommandType::Stop);
	CHECK(s.commands[2].cueName == "Cue2");
	CHECK(s.warnings.empty());
}

TEST_CASE("[Cue] may appear after [Commands]"){
	// The pre-pass exists so section order does not matter.
	auto s = ParseScene(
		"[Commands]\n"
		"play Music.mp3 \\Cue1\n"
		"\n"
		"[Cue]\n"
		"Cue1 Play audio when Actor leaves\n");
	REQUIRE(s.commands.size() == 1);
	CHECK(s.commands[0].cueName == "Cue1");
	CHECK(s.commands[0].argument == "Music.mp3");
	CHECK(s.warnings.empty());
}

TEST_CASE("A reference works on every command shape"){
	auto s = ParseScene(
		"[Cue]\n"
		"A one\n"
		"B two\n"
		"C three\n"
		"D four\n"
		"[Commands]\n"
		"text \"hello\" \\A\n"
		"show pos([50, 50], logo.png) \\B\n"
		"countdown(30, \"en\") \\C\n"
		"typewrite(40, \"typed\") \\D\n");
	REQUIRE(s.commands.size() == 4);
	CHECK(s.commands[0].cueName == "A");
	CHECK(s.commands[0].runs[0].text == "hello");
	CHECK(s.commands[1].cueName == "B");
	CHECK(s.commands[1].argument == "logo.png");
	CHECK(s.commands[2].cueName == "C");
	CHECK(s.commands[2].countdownSeconds == 30);
	CHECK(s.commands[3].cueName == "D");
	CHECK(s.commands[3].typeMsPerChar == 40);
	CHECK(s.warnings.empty());
}

TEST_CASE("A reference on a compound attaches to the row, not a sub-command"){
	// The notes column draws beside navigator rows, and `&` produces one
	// row — so the note belongs to the Compound.
	auto s = ParseScene(
		"[Cue]\n"
		"Cue1 both at once\n"
		"[Commands]\n"
		"clear & play Music.mp3 \\Cue1\n");
	REQUIRE(s.commands.size() == 1);
	REQUIRE(s.commands[0].type == CommandType::Compound);
	CHECK(s.commands[0].cueName == "Cue1");
	REQUIRE(s.commands[0].subCommands.size() == 2);
	CHECK(s.commands[0].subCommands[0].cueName.empty());
	CHECK(s.commands[0].subCommands[1].cueName.empty());
	CHECK(s.commands[0].subCommands[1].argument == "Music.mp3");
}

TEST_CASE("References work inside every block form"){
	auto s = ParseScene(
		"[Cue]\n"
		"M macro note\n"
		"R run note\n"
		"L loop note\n"
		"G group note\n"
		"[Macro]\n"
		"Mac{\n"
		"\tclear \\M\n"
		"}\n"
		"[Commands]\n"
		"run{\n"
		"\tclear \\R\n"
		"\tshow a.png\n"
		"}\n"
		"loop(2){\n"
		"\ttext \"x\" \\L\n"
		"}\n"
		"group G1{\n"
		"\tshow a.png \\G\n"
		"}\n"
		"macro Mac\n");

	// run{} -> 1 row, loop(2) -> 2 rows, group -> 1 row, macro -> 1 row.
	REQUIRE(s.commands.size() == 5);
	CHECK(s.commands[0].type == CommandType::Compound);
	CHECK(s.commands[1].cueName == "L");
	CHECK(s.commands[2].cueName == "L"); // the loop body is duplicated per iteration
	CHECK(s.commands[3].cueName == "G");
	CHECK(s.commands[3].groupName == "G1");
	CHECK(s.commands[4].cueName == "M");
	CHECK(s.warnings.empty());
}

TEST_CASE("A run{} row surfaces a reference from inside its body"){
	// run{} collapses its body into one Compound row, so a note written on
	// a command inside it has to surface on the row or it would never be
	// displayed at all.
	auto s = ParseScene(
		"[Cue]\n"
		"R run note\n"
		"[Commands]\n"
		"run{\n"
		"\tclear \\R\n"
		"\tshow a.png\n"
		"}\n");
	REQUIRE(s.commands.size() == 1);
	REQUIRE(s.commands[0].type == CommandType::Compound);
	CHECK(s.commands[0].cueName.empty());          // not on the row itself…
	CHECK(RowCueName(s.commands[0]) == "R");       // …but the row shows it
	CHECK(s.warnings.empty());
}

TEST_CASE("RowCueName prefers the row's own reference"){
	auto s = ParseScene(
		"[Cue]\n"
		"Outer outer note\n"
		"Inner inner note\n"
		"[Commands]\n"
		"clear \\Inner & show a.png \\Outer\n");
	REQUIRE(s.commands.size() == 1);
	// The trailing reference binds to the row; the earlier one sits on a
	// sub-command and only acts as a fallback.
	CHECK(s.commands[0].cueName == "Outer");
	CHECK(RowCueName(s.commands[0]) == "Outer");
}

TEST_CASE("RowCueName is empty when nothing references a note"){
	auto s = ParseScene("[Commands]\nclear\nrun{\n\tclear\n}\n");
	REQUIRE(s.commands.size() == 2);
	CHECK(RowCueName(s.commands[0]).empty());
	CHECK(RowCueName(s.commands[1]).empty());
}

TEST_CASE("A stray reference on a closing brace still closes the block"){
	// An author reaching for "a note on the whole run{}" naturally writes
	// `} \R`. Before 1.8.4 that line was not the terminator, so the block
	// ran on and swallowed the rest of the scene silently. It now closes
	// the block; the reference itself is dropped, because notes bind to
	// command rows.
	auto s = ParseScene(
		"[Cue]\n"
		"R run note\n"
		"[Commands]\n"
		"run{\n"
		"\tclear\n"
		"} \\R\n"
		"text \"after the block\"\n"
		"stop\n");
	REQUIRE(s.commands.size() == 3);
	CHECK(s.commands[0].type == CommandType::Compound);
	CHECK(s.commands[1].runs[0].text == "after the block");
	CHECK(s.commands[2].type == CommandType::Stop);
}

// ── Interaction with the 1.8.2 LaTeX macros ──────────────────

TEST_CASE("An undefined \\Name is left alone for the symbol expander"){
	// This is the whole reason a reference must name a *defined* cue:
	// `\alpha` and `\Cue1` are the same shape, and text wins ties.
	auto s = ParseScene("[Commands]\ntext \"pick \\delta\"\n");
	REQUIRE(s.commands.size() == 1);
	CHECK(s.commands[0].cueName.empty());
	CHECK(s.commands[0].runs[0].text == "pick \xCE\xB4"); // U+03B4
}

TEST_CASE("A \\Name inside quotes is text, never a reference"){
	auto s = ParseScene(
		"[Cue]\n"
		"Cue1 a note\n"
		"[Commands]\n"
		"text \"see \\Cue1 in the script\" \\Cue1\n");
	REQUIRE(s.commands.size() == 1);
	// Bound once, by the trailing reference outside the quotes...
	CHECK(s.commands[0].cueName == "Cue1");
	// ...while the quoted one stays literal text (it is not a known symbol).
	CHECK(s.commands[0].runs[0].text == "see \\Cue1 in the script");
}

TEST_CASE("A reference must be preceded by whitespace"){
	auto s = ParseScene(
		"[Cue]\n"
		"Cue1 a note\n"
		"[Commands]\n"
		"show file\\Cue1.png\n");
	REQUIRE(s.commands.size() == 1);
	CHECK(s.commands[0].cueName.empty());
	CHECK(s.commands[0].argument == "file\\Cue1.png");
}

TEST_CASE("Symbols still expand on a command that also carries a reference"){
	auto s = ParseScene(
		"[Cue]\n"
		"Cue1 a note\n"
		"[Commands]\n"
		"text \"\\alpha decay\" \\Cue1\n");
	REQUIRE(s.commands.size() == 1);
	CHECK(s.commands[0].cueName == "Cue1");
	CHECK(s.commands[0].runs[0].text == "\xCE\xB1 decay"); // U+03B1
}

// ── Warnings ─────────────────────────────────────────────────

TEST_CASE("An unreferenced note warns — this is how a typo surfaces"){
	auto s = ParseScene(
		"[Cue]\n"
		"Cue1 Play audio when Actor leaves\n"
		"[Commands]\n"
		"play Music.mp3 \\Cue11\n"); // typo: Cue11, not Cue1
	REQUIRE(s.commands.size() == 1);
	CHECK(s.commands[0].cueName.empty());
	CHECK(HasWarningContaining(s, "Cue1"));
	CHECK(HasWarningContaining(s, "never referenced"));
}

TEST_CASE("A duplicate cue name warns and the first definition wins"){
	auto s = ParseScene(
		"[Cue]\n"
		"Cue1 the first note\n"
		"Cue1 the second note\n"
		"[Commands]\n"
		"clear \\Cue1\n");
	REQUIRE(s.cueNotes.size() == 1);
	CHECK(s.cueNotes[0].text == "the first note");
	CHECK(HasWarningContaining(s, "Duplicate cue note"));
}

TEST_CASE("A name with no note text warns"){
	auto s = ParseScene("[Cue]\nCue1\n[Commands]\nclear\n");
	CHECK(s.cueNotes.empty());
	CHECK(HasWarningContaining(s, "no text"));
}

TEST_CASE("A referenced note raises no warning"){
	auto s = ParseScene(
		"[Cue]\n"
		"Cue1 note one\n"
		"Cue2 note two\n"
		"[Commands]\n"
		"clear \\Cue1\n"
		"stop \\Cue2\n");
	CHECK(s.warnings.empty());
}

TEST_CASE("An unknown section is not swallowed as cue notes"){
	auto s = ParseScene(
		"[Cue]\n"
		"Cue1 a note\n"
		"[Rehearsal]\n"
		"Note this is not a cue\n"
		"[Commands]\n"
		"clear \\Cue1\n");
	REQUIRE(s.cueNotes.size() == 1);
	CHECK(s.cueNotes[0].name == "Cue1");
	CHECK(Find(s, "Note") == nullptr);
}

TEST_CASE("Lookup helper finds notes by name"){
	auto s = ParseScene(
		"[Cue]\n"
		"Alpha first\n"
		"Beta second\n"
		"[Commands]\n"
		"clear \\Alpha\n"
		"stop \\Beta\n");
	REQUIRE(Find(s, "Alpha") != nullptr);
	CHECK(Find(s, "Alpha")->text == "first");
	REQUIRE(Find(s, "Beta") != nullptr);
	CHECK(Find(s, "Beta")->text == "second");
	CHECK(Find(s, "Gamma") == nullptr);
}
