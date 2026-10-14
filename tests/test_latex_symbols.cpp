// (1.8.2) LatexSymbols — `\alpha` → α expansion in displayed text.
//
// Split in two halves: the pure string function on its own, then a few
// integration cases proving it is actually wired into every text-bearing
// command path rather than only working in isolation.

#include <doctest/doctest.h>
#include "helpers.hpp"
#include "core/LatexSymbols.hpp"

using namespace SatyrAV;
using SatyrTest::ParseOneCommand;
using SatyrTest::ParseScene;

// The expected values are written as explicit UTF-8 byte sequences rather
// than as source-file literals, so the assertions cannot silently depend
// on the encoding this test file happens to be saved in.
static std::string U(unsigned long cp){
	std::string out;
	if(cp < 0x80){ out += (char)cp; }
	else if(cp < 0x800){
		out += (char)(0xC0 | (cp >> 6));
		out += (char)(0x80 | (cp & 0x3F));
	} else if(cp < 0x10000){
		out += (char)(0xE0 | (cp >> 12));
		out += (char)(0x80 | ((cp >> 6) & 0x3F));
		out += (char)(0x80 | (cp & 0x3F));
	} else{
		out += (char)(0xF0 | (cp >> 18));
		out += (char)(0x80 | ((cp >> 12) & 0x3F));
		out += (char)(0x80 | ((cp >> 6) & 0x3F));
		out += (char)(0x80 | (cp & 0x3F));
	}
	return out;
}

// ── The expansion function ───────────────────────────────────

TEST_CASE("Text with no backslash is returned unchanged"){
	CHECK(ExpandLatexSymbols("") == "");
	CHECK(ExpandLatexSymbols("Hello, world!") == "Hello, world!");
	CHECK(ExpandLatexSymbols("alpha beta gamma") == "alpha beta gamma");
}

TEST_CASE("Greek lower case"){
	CHECK(ExpandLatexSymbols("\\alpha")  == U(0x3B1));
	CHECK(ExpandLatexSymbols("\\beta")   == U(0x3B2));
	CHECK(ExpandLatexSymbols("\\gamma")  == U(0x3B3));
	CHECK(ExpandLatexSymbols("\\delta")  == U(0x3B4));
	CHECK(ExpandLatexSymbols("\\lambda") == U(0x3BB));
	CHECK(ExpandLatexSymbols("\\mu")     == U(0x3BC));
	CHECK(ExpandLatexSymbols("\\pi")     == U(0x3C0));
	CHECK(ExpandLatexSymbols("\\sigma")  == U(0x3C3));
	CHECK(ExpandLatexSymbols("\\omega")  == U(0x3C9));
	CHECK(ExpandLatexSymbols("\\omicron")== U(0x3BF));
}

TEST_CASE("LaTeX's epsilon/phi split is honoured"){
	// The single most common surprise: LaTeX's \epsilon and \phi are the
	// lunate/closed glyphs; the familiar ε and φ are the \var forms.
	CHECK(ExpandLatexSymbols("\\epsilon")    == U(0x3F5));
	CHECK(ExpandLatexSymbols("\\varepsilon") == U(0x3B5));
	CHECK(ExpandLatexSymbols("\\phi")        == U(0x3D5));
	CHECK(ExpandLatexSymbols("\\varphi")     == U(0x3C6));
}

TEST_CASE("Variant Greek forms"){
	CHECK(ExpandLatexSymbols("\\vartheta") == U(0x3D1));
	CHECK(ExpandLatexSymbols("\\varpi")    == U(0x3D6));
	CHECK(ExpandLatexSymbols("\\varrho")   == U(0x3F1));
	CHECK(ExpandLatexSymbols("\\varsigma") == U(0x3C2));
}

TEST_CASE("Greek upper case — only the eleven LaTeX defines"){
	CHECK(ExpandLatexSymbols("\\Gamma")   == U(0x393));
	CHECK(ExpandLatexSymbols("\\Delta")   == U(0x394));
	CHECK(ExpandLatexSymbols("\\Theta")   == U(0x398));
	CHECK(ExpandLatexSymbols("\\Lambda")  == U(0x39B));
	CHECK(ExpandLatexSymbols("\\Xi")      == U(0x39E));
	CHECK(ExpandLatexSymbols("\\Pi")      == U(0x3A0));
	CHECK(ExpandLatexSymbols("\\Sigma")   == U(0x3A3));
	CHECK(ExpandLatexSymbols("\\Upsilon") == U(0x3A5));
	CHECK(ExpandLatexSymbols("\\Phi")     == U(0x3A6));
	CHECK(ExpandLatexSymbols("\\Psi")     == U(0x3A8));
	CHECK(ExpandLatexSymbols("\\Omega")   == U(0x3A9));

	SUBCASE("the visually-Latin ones are not defined, matching LaTeX"){
		// LaTeX has no \Alpha / \Beta / \Epsilon — you type A, B, E.
		// Leaving them unexpanded keeps a scene that uses them portable
		// to real LaTeX rather than silently diverging.
		CHECK(ExpandLatexSymbols("\\Alpha")   == "\\Alpha");
		CHECK(ExpandLatexSymbols("\\Beta")    == "\\Beta");
		CHECK(ExpandLatexSymbols("\\Epsilon") == "\\Epsilon");
	}
}

TEST_CASE("Relations, arithmetic and big operators"){
	CHECK(ExpandLatexSymbols("\\times")  == U(0x00D7));
	CHECK(ExpandLatexSymbols("\\cdot")   == U(0x22C5));
	CHECK(ExpandLatexSymbols("\\pm")     == U(0x00B1));
	CHECK(ExpandLatexSymbols("\\mp")     == U(0x2213));
	CHECK(ExpandLatexSymbols("\\approx") == U(0x2248));
	CHECK(ExpandLatexSymbols("\\sim")    == U(0x223C));
	CHECK(ExpandLatexSymbols("\\simeq")  == U(0x2243));
	CHECK(ExpandLatexSymbols("\\neq")    == U(0x2260));
	CHECK(ExpandLatexSymbols("\\leq")    == U(0x2264));
	CHECK(ExpandLatexSymbols("\\geq")    == U(0x2265));
	CHECK(ExpandLatexSymbols("\\ll")     == U(0x226A));
	CHECK(ExpandLatexSymbols("\\gg")     == U(0x226B));
	CHECK(ExpandLatexSymbols("\\infty")  == U(0x221E));
	CHECK(ExpandLatexSymbols("\\ldots")  == U(0x2026));
	CHECK(ExpandLatexSymbols("\\deg")    == U(0x00B0));
	CHECK(ExpandLatexSymbols("\\degree") == U(0x00B0));
	CHECK(ExpandLatexSymbols("\\sum")    == U(0x2211));
	CHECK(ExpandLatexSymbols("\\prod")   == U(0x220F));
	CHECK(ExpandLatexSymbols("\\int")    == U(0x222B));
}

TEST_CASE("Set theory and logic"){
	CHECK(ExpandLatexSymbols("\\in")       == U(0x2208));
	CHECK(ExpandLatexSymbols("\\notin")    == U(0x2209));
	CHECK(ExpandLatexSymbols("\\subset")   == U(0x2282));
	CHECK(ExpandLatexSymbols("\\supset")   == U(0x2283));
	CHECK(ExpandLatexSymbols("\\subseteq") == U(0x2286));
	CHECK(ExpandLatexSymbols("\\supseteq") == U(0x2287));
	CHECK(ExpandLatexSymbols("\\cap")      == U(0x2229));
	CHECK(ExpandLatexSymbols("\\cup")      == U(0x222A));
	CHECK(ExpandLatexSymbols("\\emptyset") == U(0x2205));
	CHECK(ExpandLatexSymbols("\\forall")   == U(0x2200));
	CHECK(ExpandLatexSymbols("\\exists")   == U(0x2203));
	CHECK(ExpandLatexSymbols("\\nexists")  == U(0x2204));
	CHECK(ExpandLatexSymbols("\\neg")      == U(0x00AC));

	SUBCASE("wedge/vee and their land/lor aliases agree"){
		CHECK(ExpandLatexSymbols("\\wedge") == U(0x2227));
		CHECK(ExpandLatexSymbols("\\land")  == U(0x2227));
		CHECK(ExpandLatexSymbols("\\vee")   == U(0x2228));
		CHECK(ExpandLatexSymbols("\\lor")   == U(0x2228));
	}
}

TEST_CASE("Arrows"){
	CHECK(ExpandLatexSymbols("\\rightarrow")     == U(0x2192));
	CHECK(ExpandLatexSymbols("\\leftarrow")      == U(0x2190));
	CHECK(ExpandLatexSymbols("\\leftrightarrow") == U(0x2194));
	CHECK(ExpandLatexSymbols("\\Rightarrow")     == U(0x21D2));
	CHECK(ExpandLatexSymbols("\\Leftarrow")      == U(0x21D0));
	CHECK(ExpandLatexSymbols("\\Leftrightarrow") == U(0x21D4));
}

TEST_CASE("mathbb — the Letterlike seven and the rest"){
	// Seven double-struck capitals were encoded early in the Letterlike
	// block; the other nineteen sit above the BMP.
	CHECK(ExpandLatexSymbols("\\mathbb{N}") == U(0x2115));
	CHECK(ExpandLatexSymbols("\\mathbb{Z}") == U(0x2124));
	CHECK(ExpandLatexSymbols("\\mathbb{Q}") == U(0x211A));
	CHECK(ExpandLatexSymbols("\\mathbb{R}") == U(0x211D));
	CHECK(ExpandLatexSymbols("\\mathbb{C}") == U(0x2102));
	CHECK(ExpandLatexSymbols("\\mathbb{H}") == U(0x210D));
	CHECK(ExpandLatexSymbols("\\mathbb{P}") == U(0x2119));

	SUBCASE("F needs a 4-byte UTF-8 sequence"){
		auto f = ExpandLatexSymbols("\\mathbb{F}");
		CHECK(f == U(0x1D53D));
		CHECK(f.size() == 4);
	}
	SUBCASE("every A-Z letter resolves to something"){
		for(char L = 'A'; L <= 'Z'; L++){
			CAPTURE(L);
			std::string in = std::string("\\mathbb{") + L + "}";
			auto out = ExpandLatexSymbols(in);
			CHECK(out != in);        // it expanded
			CHECK(out.size() <= 4);  // one character, not a passthrough
		}
	}
	SUBCASE("malformed forms are left alone rather than guessed at"){
		CHECK(ExpandLatexSymbols("\\mathbb")     == "\\mathbb");
		CHECK(ExpandLatexSymbols("\\mathbb{}")   == "\\mathbb{}");
		CHECK(ExpandLatexSymbols("\\mathbb{r}")  == "\\mathbb{r}"); // lower case
		CHECK(ExpandLatexSymbols("\\mathbb{RS}") == "\\mathbb{RS}");
		CHECK(ExpandLatexSymbols("\\mathbb{R")   == "\\mathbb{R");
		CHECK(ExpandLatexSymbols("\\mathbbR")    == "\\mathbbR");
	}
}

TEST_CASE("Unknown macros pass through byte-for-byte"){
	CHECK(ExpandLatexSymbols("\\foo")       == "\\foo");
	CHECK(ExpandLatexSymbols("\\alpah")     == "\\alpah"); // typo, not eaten
	CHECK(ExpandLatexSymbols("\\frac{1}{2}")== "\\frac{1}{2}");
}

TEST_CASE("The renderer's \\n line-break marker is preserved"){
	// This is the reason unknown macros pass through untouched: the slave
	// splits run text on a literal backslash-n, and that has to survive.
	CHECK(ExpandLatexSymbols("line 1\\nline 2") == "line 1\\nline 2");
	CHECK(ExpandLatexSymbols("\\n") == "\\n");

	SUBCASE("a symbol and a line break coexist"){
		CHECK(ExpandLatexSymbols("\\alpha\\nbeta")
			== U(0x3B1) + "\\nbeta");
	}
}

TEST_CASE("Greedy name matching makes prefixes unambiguous"){
	// \in is a prefix of \infty and \int; reading the whole letter run
	// means there is no longest-match rule to get wrong.
	CHECK(ExpandLatexSymbols("\\in")    == U(0x2208));
	CHECK(ExpandLatexSymbols("\\infty") == U(0x221E));
	CHECK(ExpandLatexSymbols("\\int")   == U(0x222B));
	CHECK(ExpandLatexSymbols("\\pi")    == U(0x3C0));
	CHECK(ExpandLatexSymbols("\\pm")    == U(0x00B1));
	// \subset is a prefix of \subseteq.
	CHECK(ExpandLatexSymbols("\\subset")   == U(0x2282));
	CHECK(ExpandLatexSymbols("\\subseteq") == U(0x2286));
}

TEST_CASE("Macros mix freely with surrounding text"){
	CHECK(ExpandLatexSymbols("Let \\alpha be small")
		== "Let " + U(0x3B1) + " be small");
	CHECK(ExpandLatexSymbols("\\alpha\\beta\\gamma")
		== U(0x3B1) + U(0x3B2) + U(0x3B3));
	CHECK(ExpandLatexSymbols("(\\alpha)") == "(" + U(0x3B1) + ")");
	CHECK(ExpandLatexSymbols("\\alpha, \\beta")
		== U(0x3B1) + ", " + U(0x3B2));

	SUBCASE("no space is swallowed after a macro"){
		// Real LaTeX eats it; we deliberately do not.
		CHECK(ExpandLatexSymbols("\\alpha x") == U(0x3B1) + " x");
	}
	SUBCASE("a digit ends the name"){
		CHECK(ExpandLatexSymbols("\\alpha2") == U(0x3B1) + "2");
	}
	SUBCASE("a trailing or lone backslash is harmless"){
		CHECK(ExpandLatexSymbols("end\\")  == "end\\");
		CHECK(ExpandLatexSymbols("\\")     == "\\");
		CHECK(ExpandLatexSymbols("a\\\\b") == "a\\\\b");
		CHECK(ExpandLatexSymbols("\\ x")   == "\\ x");
	}
}

// ── Wiring into the parser ───────────────────────────────────

TEST_CASE("Symbols expand in every text-bearing command"){
	SUBCASE("text"){
		CHECK(ParseOneCommand("text \"\\alpha decay\"").runs[0].text
			== U(0x3B1) + " decay");
	}
	SUBCASE("the styled and Cont variants"){
		CHECK(ParseOneCommand("textbf \"\\Omega\"").runs[0].text == U(0x3A9));
		CHECK(ParseOneCommand("textit \"\\pi\"").runs[0].text    == U(0x3C0));
		CHECK(ParseOneCommand("textCont \"\\mu\"").runs[0].text  == U(0x3BC));
	}
	SUBCASE("both sides of textD"){
		auto c = ParseOneCommand("textD \"\\Sigma sum\", \"\\Sigma sum (da)\"");
		CHECK(c.runs[0].text         == U(0x3A3) + " sum");
		CHECK(c.subtitleRuns[0].text == U(0x3A3) + " sum (da)");
	}
	SUBCASE("each segment of a + concatenation"){
		auto c = ParseOneCommand(
			"text color(RED, \"\\alpha\") + \" and \" + \"\\beta\"");
		REQUIRE(c.runs.size() == 3);
		CHECK(c.runs[0].text == U(0x3B1));
		CHECK(c.runs[2].text == U(0x3B2));
	}
	SUBCASE("inside nested modifiers"){
		auto c = ParseOneCommand("text pos([50, 50], bold(\"\\lambda\"))");
		REQUIRE(c.runs.size() == 1);
		CHECK(c.runs[0].text == U(0x3BB));
		CHECK(c.runs[0].bold);
	}
	SUBCASE("comment rows, for the operator's benefit"){
		CHECK(ParseOneCommand("comment \"after the \\Omega speech\"").argument
			== "after the " + U(0x3A9) + " speech");
	}
	SUBCASE("the concatenated argument mirrors the expanded runs"){
		CHECK(ParseOneCommand("text \"\\alpha\" + \"\\beta\"").argument
			== U(0x3B1) + U(0x3B2));
	}
}

TEST_CASE("Symbols do not leak into non-text arguments"){
	// Filenames and group names must stay byte-for-byte — a file really
	// could be called `\alpha.png` on disk.
	CHECK(ParseOneCommand("show \\alpha.png").argument == "\\alpha.png");
	CHECK(ParseOneCommand("play \\beta.mp3").argument  == "\\beta.mp3");
	CHECK(ParseOneCommand("clear \\Gamma").argument     == "\\Gamma");
}

TEST_CASE("A full scene expands symbols in every block form"){
	auto scene = ParseScene(
		"[Macro]\n"
		"M{\n"
		"\ttext \"\\theta from a macro\"\n"
		"}\n"
		"[Commands]\n"
		"text \"\\alpha\"\n"
		"run{\n"
		"\ttext \"\\beta\"\n"
		"}\n"
		"loop(2){\n"
		"\ttext \"\\gamma\"\n"
		"}\n"
		"group G{\n"
		"\ttext \"\\delta\"\n"
		"}\n"
		"macro M\n");
	REQUIRE(scene.commands.size() == 6);
	CHECK(scene.commands[0].runs[0].text == U(0x3B1));            // text
	CHECK(scene.commands[1].subCommands[0].runs[0].text == U(0x3B2)); // run{}
	CHECK(scene.commands[2].runs[0].text == U(0x3B3));            // loop 1
	CHECK(scene.commands[3].runs[0].text == U(0x3B3));            // loop 2
	CHECK(scene.commands[4].runs[0].text == U(0x3B4));            // group
	CHECK(scene.commands[5].runs[0].text == U(0x3B8) + " from a macro");
	CHECK(scene.warnings.empty());
}

TEST_CASE("A realistic maths line survives end to end"){
	auto c = ParseOneCommand(
		"text \"\\forall \\varepsilon > 0 \\exists \\delta "
		"\\in \\mathbb{R} : |x| \\leq \\delta\"");
	REQUIRE(c.runs.size() == 1);
	const std::string expected =
		U(0x2200) + " " + U(0x3B5) + " > 0 " + U(0x2203) + " " + U(0x3B4)
		+ " " + U(0x2208) + " " + U(0x211D) + " : |x| " + U(0x2264)
		+ " " + U(0x3B4);
	CHECK(c.runs[0].text == expected);
}
