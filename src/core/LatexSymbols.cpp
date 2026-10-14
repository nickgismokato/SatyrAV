#include "core/LatexSymbols.hpp"
#include <cstdint>
#include <unordered_map>

namespace SatyrAV{

// Append `cp` to `out` as UTF-8. Handles the 4-byte range because the
// double-struck capitals outside the Letterlike block (e.g. 𝔽 U+1D53D)
// live above the BMP.
static void AppendUtf8(std::string& out, uint32_t cp){
	if(cp < 0x80){
		out += (char)cp;
	} else if(cp < 0x800){
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
}

static bool IsAsciiAlpha(char c){
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// `\mathbb{X}` for any A–Z. Seven double-struck capitals were encoded
// early, in the Letterlike Symbols block; the rest sit contiguously in
// Mathematical Alphanumeric Symbols. All 26 are present in every bundled
// DejaVu face (regular / bold / oblique / bold-oblique), so the general
// rule is safe rather than only covering the common ones.
static uint32_t DoubleStruckCapital(char letter){
	switch(letter){
		case 'C': return 0x2102; // ℂ
		case 'H': return 0x210D; // ℍ
		case 'N': return 0x2115; // ℕ
		case 'P': return 0x2119; // ℙ
		case 'Q': return 0x211A; // ℚ
		case 'R': return 0x211D; // ℝ
		case 'Z': return 0x2124; // ℤ
		default:  return 0x1D538 + (uint32_t)(letter - 'A'); // 𝔸 … 𝕐
	}
}

static const std::unordered_map<std::string, uint32_t>& SymbolTable(){
	static const std::unordered_map<std::string, uint32_t> table = {
		// ── Greek, lower case ────────────────────────────────
		// LaTeX's \epsilon and \phi are the lunate/closed forms; the
		// familiar ε and φ are \varepsilon and \varphi. That trips people
		// up, so both spellings are listed in docs/commands.md.
		{"alpha",   0x3B1}, {"beta",    0x3B2}, {"gamma",   0x3B3},
		{"delta",   0x3B4}, {"epsilon", 0x3F5}, {"zeta",    0x3B6},
		{"eta",     0x3B7}, {"theta",   0x3B8}, {"iota",    0x3B9},
		{"kappa",   0x3BA}, {"lambda",  0x3BB}, {"mu",      0x3BC},
		{"nu",      0x3BD}, {"xi",      0x3BE}, {"omicron", 0x3BF},
		{"pi",      0x3C0}, {"rho",     0x3C1}, {"sigma",   0x3C3},
		{"tau",     0x3C4}, {"upsilon", 0x3C5}, {"phi",     0x3D5},
		{"chi",     0x3C7}, {"psi",     0x3C8}, {"omega",   0x3C9},

		// Variant forms.
		{"varepsilon", 0x3B5}, {"vartheta", 0x3D1}, {"varpi",   0x3D6},
		{"varrho",     0x3F1}, {"varsigma", 0x3C2}, {"varphi",  0x3C6},

		// ── Greek, upper case ────────────────────────────────
		// Only the eleven that LaTeX itself defines — the others are
		// visually identical to Latin capitals, so LaTeX omits them and
		// so do we. Type `A` for Alpha.
		{"Gamma",  0x393}, {"Delta",   0x394}, {"Theta", 0x398},
		{"Lambda", 0x39B}, {"Xi",      0x39E}, {"Pi",    0x3A0},
		{"Sigma",  0x3A3}, {"Upsilon", 0x3A5}, {"Phi",   0x3A6},
		{"Psi",    0x3A8}, {"Omega",   0x3A9},

		// ── Arithmetic and relations ─────────────────────────
		{"times",  0x00D7}, {"cdot",   0x22C5}, {"pm",     0x00B1},
		{"mp",     0x2213}, {"approx", 0x2248}, {"sim",    0x223C},
		{"simeq",  0x2243}, {"neq",    0x2260}, {"leq",    0x2264},
		{"geq",    0x2265}, {"ll",     0x226A}, {"gg",     0x226B},
		{"infty",  0x221E}, {"ldots",  0x2026},
		// \deg is a LaTeX *operator* (it typesets "deg"); mapping it to
		// the degree sign is a deliberate convenience for scene authors.
		// \degree is the gensymb spelling and is accepted as an alias.
		{"deg",    0x00B0}, {"degree", 0x00B0},

		// ── Big operators ────────────────────────────────────
		{"sum", 0x2211}, {"prod", 0x220F}, {"int", 0x222B},

		// ── Set theory ───────────────────────────────────────
		{"in",       0x2208}, {"notin",    0x2209}, {"subset",   0x2282},
		{"supset",   0x2283}, {"subseteq", 0x2286}, {"supseteq", 0x2287},
		{"cap",      0x2229}, {"cup",      0x222A}, {"emptyset", 0x2205},

		// ── Logic ────────────────────────────────────────────
		// \land / \lor are LaTeX's own aliases for \wedge / \vee.
		{"forall", 0x2200}, {"exists", 0x2203}, {"nexists", 0x2204},
		{"wedge",  0x2227}, {"vee",    0x2228}, {"land",    0x2227},
		{"lor",    0x2228}, {"neg",    0x00AC},

		// ── Arrows ───────────────────────────────────────────
		{"rightarrow",     0x2192}, {"leftarrow",     0x2190},
		{"leftrightarrow", 0x2194}, {"Rightarrow",    0x21D2},
		{"Leftarrow",      0x21D0}, {"Leftrightarrow", 0x21D4},
	};
	return table;
}

std::string ExpandLatexSymbols(const std::string& text){
	// Overwhelmingly the common case — skip the walk entirely.
	if(text.find('\\') == std::string::npos) return text;

	std::string out;
	out.reserve(text.size());

	size_t i = 0;
	while(i < text.size()){
		if(text[i] != '\\'){
			out += text[i++];
			continue;
		}

		// A macro name is the run of ASCII letters after the backslash.
		// Reading greedily is what makes prefixes unambiguous: `\in` and
		// `\infty` can coexist without a longest-match rule.
		size_t nameEnd = i + 1;
		while(nameEnd < text.size() && IsAsciiAlpha(text[nameEnd])) nameEnd++;

		if(nameEnd == i + 1){
			// A lone backslash, or one followed by a non-letter. Copy it
			// and move on.
			out += text[i++];
			continue;
		}

		const std::string name = text.substr(i + 1, nameEnd - i - 1);

		if(name == "mathbb"){
			// `\mathbb{X}` — exactly one A–Z letter in braces.
			if(nameEnd + 2 < text.size()
				&& text[nameEnd] == '{'
				&& text[nameEnd + 2] == '}'
				&& text[nameEnd + 1] >= 'A' && text[nameEnd + 1] <= 'Z'){
				AppendUtf8(out, DoubleStruckCapital(text[nameEnd + 1]));
				i = nameEnd + 3;
				continue;
			}
			// Malformed (no braces, lower case, more than one letter) —
			// leave it alone rather than guessing.
			out.append(text, i, nameEnd - i);
			i = nameEnd;
			continue;
		}

		auto it = SymbolTable().find(name);
		if(it != SymbolTable().end()){
			AppendUtf8(out, it->second);
		} else{
			// Unknown macro: copy verbatim. This is the branch that keeps
			// the renderer's `\n` line-break marker intact.
			out.append(text, i, nameEnd - i);
		}
		i = nameEnd;
	}

	return out;
}

} // namespace SatyrAV
