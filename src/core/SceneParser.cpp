#include "core/SceneParser.hpp"
#include "core/OptionKeys.hpp"
#include "core/Backstage.hpp"
#include "core/LatexSymbols.hpp"
#include "core/Platform.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <functional>

namespace SatyrAV{

static std::string Trim(const std::string& s){
	auto lt = s.find_first_not_of(" \t");
	if(lt == std::string::npos) return "";
	auto rt = s.find_last_not_of(" \t");
	return s.substr(lt, rt - lt + 1);
}

// (1.7) String delimiters supported in scene files:
//   "..."   primary; `'` and `´` inside are literal
//   '...'   single quote; `"` and `´` inside are literal
//   ´...´   acute accent (UTF-8 0xC2 0xB4); `"` and `'` inside are literal
//
// All three forms are interchangeable — pick whichever doesn't clash with
// the literal characters you want to display. Open and close must match;
// mismatched delimiters inside a string are treated as literal content.
//
// Returns the byte length of the delimiter starting at `s[i]` (0 if no
// delimiter is there), and writes a distinguishing kind code (1/2/3) into
// `*kind` so quote-aware loops can require the closer to match the opener.
static size_t DelimAt(const std::string& s, size_t i, int* kind = nullptr){
	if(i >= s.size()) return 0;
	unsigned char c = (unsigned char)s[i];
	if(c == '"'){  if(kind) *kind = 1; return 1; }
	if(c == '\''){ if(kind) *kind = 2; return 1; }
	// ´ (U+00B4) → 0xC2 0xB4 in UTF-8
	if(c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xB4){
		if(kind) *kind = 3;
		return 2;
	}
	return 0;
}

// (1.8.2) Find the next *structural* occurrence of `target` — one that is
// outside every quoted string and outside every modifier argument list.
//
// Two separators need exactly this scan: the `#` comment marker and the
// `&` compound separator. They were each written with their own ad-hoc
// splitting and each grew the same class of bug, fixed years apart:
//   - `#` used a plain `line.find('#')`, so `text "Look at #1"` was
//     truncated to `text "Look at ` (fixed 1.6.6 by tracking quotes), and
//     `text color(#FF5500, "hi")` was truncated to `text color(` — the
//     documented hex colour never reached ParseNamedColour (fixed 1.8.2
//     by tracking parens).
//   - `&` used a getline split, so `text "Tom & Jerry"` became two bogus
//     commands (fixed 1.8.2 by routing it through here).
// Sharing one scanner is the point: a third separator, or a fourth quote
// style, is now a single edit rather than N places to remember.
//
// Rationale for paren depth over the alternatives considered for `#`:
//   - "no comment when `#` is followed by 6 hex digits" would silently
//     eat a legitimate comment line like `#FFFFFF looks nice here`.
//   - "require whitespace before `#`" breaks on `color( #FF5500, "x")`,
//     where the author simply spaced out the argument list.
//
// Parens are counted only outside strings, so `text "smile :)"` cannot
// unbalance the depth, and a stray closer clamps at zero rather than
// going negative and disabling the scan for the rest of the line. The
// walk always starts at index 0 so quote/paren state is correct; `from`
// only gates which matches are returned.
//
// Returns std::string::npos when there is no structural occurrence.
static size_t FindStructural(const std::string& line, char target, size_t from = 0){
	int quoteKind = 0;
	int parenDepth = 0;
	for(size_t i = 0; i < line.size(); ){
		int k = 0;
		size_t dl = DelimAt(line, i, &k);
		if(dl > 0){
			if(quoteKind == 0)       quoteKind = k;
			else if(quoteKind == k)  quoteKind = 0;
			// else: different delimiter inside an open quote → literal.
			i += dl;
			continue;
		}
		if(quoteKind == 0){
			if(line[i] == '(')      parenDepth++;
			else if(line[i] == ')') { if(parenDepth > 0) parenDepth--; }
			else if(line[i] == target && parenDepth == 0 && i >= from) return i;
		}
		i++;
	}
	return std::string::npos;
}

// Start of a `#` line comment, or npos. See FindStructural.
static size_t FindCommentStart(const std::string& line){
	return FindStructural(line, '#');
}

// (2.0.1) Comment start for a line in a scene's `[Options]` section, where a
// value may itself begin with `#`: a hex colour such as
// `TextColour = #FF8800`. Before 2.0.1 that `#` was read as a comment, so
// the option silently received an empty value.
//
// Only a `#` that starts the value and is followed by exactly 3 or 6 hex
// digits is protected. A later `#` still starts a comment
// (`bc = #102030   # warm blue`), and so does any `#` before the `=`.
static size_t FindOptionCommentStart(const std::string& line){
	const size_t eq = line.find('=');
	if(eq != std::string::npos &&
	   FindStructural(line.substr(0, eq), '#') == std::string::npos){
		size_t v = eq + 1;
		while(v < line.size() && std::isspace((unsigned char)line[v])) v++;
		if(v < line.size() && line[v] == '#'){
			size_t h = v + 1;
			while(h < line.size() && std::isxdigit((unsigned char)line[h])) h++;
			const size_t digits = h - v - 1;
			const bool ends = (h == line.size() ||
				std::isspace((unsigned char)line[h]) || line[h] == '#');
			if((digits == 3 || digits == 6) && ends){
				const size_t rest = FindStructural(line.substr(h), '#');
				return rest == std::string::npos ? std::string::npos : h + rest;
			}
		}
	}
	return FindStructural(line, '#');
}

// ── Modifier parser ─────────────────────────────────────────
// Parses nested modifier functions like:
//   trans(50, picture.jpg)
//   rotate(180, "Hello")
//   pos([50,50], trans(80, image.png))
//   random(heart.png, [50,50], [100,100], true)
//   particle(RAIN, random(heart.png, [50,50], [100,100], true))

// Find matching closing paren, respecting nesting
static size_t FindMatchingParen(const std::string& s, size_t openPos){
	int depth = 1;
	for(size_t i = openPos + 1; i < s.size(); i++){
		if(s[i] == '(') depth++;
		else if(s[i] == ')'){
			depth--;
			if(depth == 0) return i;
		}
	}
	return std::string::npos;
}

// Find matching bracket
static size_t FindMatchingBracket(const std::string& s, size_t openPos){
	int depth = 1;
	for(size_t i = openPos + 1; i < s.size(); i++){
		if(s[i] == '[') depth++;
		else if(s[i] == ']'){
			depth--;
			if(depth == 0) return i;
		}
	}
	return std::string::npos;
}

// Split arguments at top-level commas (not inside parens, brackets, or
// quoted strings). *(1.7)* Honours `'` and `´` as alternate quote pairs.
static std::vector<std::string> SplitArgs(const std::string& s){
	std::vector<std::string> args;
	int parenDepth = 0, bracketDepth = 0;
	int quoteKind = 0;
	size_t start = 0;

	for(size_t i = 0; i < s.size(); ){
		int k = 0;
		size_t dl = DelimAt(s, i, &k);
		if(dl > 0){
			if(quoteKind == 0)      quoteKind = k;
			else if(quoteKind == k) quoteKind = 0;
			i += dl;
			continue;
		}
		if(quoteKind != 0){ i++; continue; }

		char c = s[i];
		if(c == '(') parenDepth++;
		else if(c == ')') parenDepth--;
		else if(c == '[') bracketDepth++;
		else if(c == ']') bracketDepth--;
		else if(c == ',' && parenDepth == 0 && bracketDepth == 0){
			args.push_back(Trim(s.substr(start, i - start)));
			start = i + 1;
		}
		i++;
	}
	args.push_back(Trim(s.substr(start)));
	return args;
}

// Parse [x, y] bracket notation
static bool ParseBracketPair(const std::string& s, int& a, int& b){
	if(s.size() < 3 || s.front() != '[' || s.back() != ']') return false;
	std::string inner = s.substr(1, s.size() - 2);
	auto comma = inner.find(',');
	if(comma == std::string::npos) return false;
	try{
		a = std::stoi(Trim(inner.substr(0, comma)));
		b = std::stoi(Trim(inner.substr(comma + 1)));
		return true;
	} catch(...){ return false; }
}

static bool ParseBracketPairF(const std::string& s, float& a, float& b){
	if(s.size() < 3 || s.front() != '[' || s.back() != ']') return false;
	std::string inner = s.substr(1, s.size() - 2);
	auto comma = inner.find(',');
	if(comma == std::string::npos) return false;
	try{
		a = std::stof(Trim(inner.substr(0, comma)));
		b = std::stof(Trim(inner.substr(comma + 1)));
		return true;
	} catch(...){ return false; }
}

// Strip quotes from a string argument. *(1.7)* Accepts `"..."`, `'...'`,
// or `´...´` — the closer must match the opener. Returns the input
// unchanged if it isn't a recognised quoted string.
static std::string StripQuotes(const std::string& s){
	if(s.size() >= 2){
		if(s.front() == '"'  && s.back() == '"')  return s.substr(1, s.size() - 2);
		if(s.front() == '\'' && s.back() == '\'') return s.substr(1, s.size() - 2);
	}
	// ´...´ uses two bytes at each end (UTF-8 0xC2 0xB4).
	if(s.size() >= 4
		&& (unsigned char)s[0] == 0xC2 && (unsigned char)s[1] == 0xB4
		&& (unsigned char)s[s.size()-2] == 0xC2 && (unsigned char)s[s.size()-1] == 0xB4){
		return s.substr(2, s.size() - 4);
	}
	return s;
}

// (1.4) Parse a distribution spec of the form `NAME(p1, p2)` or `NAME`.
// Returns true on recognised distribution name; out-param holds parsed values.
static bool ParseDistSpec(const std::string& raw, DistSpec& out){
	std::string s = Trim(raw);
	if(s.empty()) return false;

	std::string name = s;
	std::string args;
	auto paren = s.find('(');
	if(paren != std::string::npos){
		name = Trim(s.substr(0, paren));
		auto end = s.rfind(')');
		if(end != std::string::npos && end > paren){
			args = s.substr(paren + 1, end - paren - 1);
		}
	}

	std::string lower = name;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
	if(lower == "uniform")        out.type = Distribution::Uniform;
	else if(lower == "normal")    out.type = Distribution::Normal;
	else if(lower == "lognormal") out.type = Distribution::LogNormal;
	else if(lower == "binomial")  out.type = Distribution::Binomial;
	else if(lower == "bernoulli") out.type = Distribution::Bernoulli;
	else if(lower == "gamma")     out.type = Distribution::Gamma;
	else return false;

	if(!args.empty()){
		auto comma = args.find(',');
		std::string a = (comma == std::string::npos) ? args : args.substr(0, comma);
		std::string b = (comma == std::string::npos) ? "" : args.substr(comma + 1);
		try{ out.p1 = std::stof(Trim(a)); } catch(...){}
		if(!b.empty()){ try{ out.p2 = std::stof(Trim(b)); } catch(...){} }
	}
	return true;
}

// (1.4) Strict lookup — returns false on unknown name instead of defaulting
// to Rain. Used by the [Options] parser to reject typos.
static bool ParseParticleTypeStrict(const std::string& s, ParticleType& out){
	std::string lower = s;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
	if(lower == "rain")             { out = ParticleType::Rain;        return true; }
	if(lower == "snow")             { out = ParticleType::Snow;        return true; }
	if(lower == "confetti")         { out = ParticleType::Confetti;    return true; }
	if(lower == "rise")             { out = ParticleType::Rise;        return true; }
	if(lower == "scatter")          { out = ParticleType::Scatter;     return true; }
	if(lower == "brownian")         { out = ParticleType::Brownian;    return true; }
	if(lower == "oscillation")      { out = ParticleType::Oscillation; return true; }
	if(lower == "orbit")            { out = ParticleType::Orbit;       return true; }
	if(lower == "noise")            { out = ParticleType::Noise;       return true; }
	if(lower == "lifecurve")        { out = ParticleType::LifeCurve;   return true; }
	return false;
}

static ParticleType ParseParticleType(const std::string& s){
	std::string lower = s;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
	if(lower == "rain")        return ParticleType::Rain;
	if(lower == "snow")        return ParticleType::Snow;
	if(lower == "confetti")    return ParticleType::Confetti;
	if(lower == "rise")        return ParticleType::Rise;
	if(lower == "scatter")     return ParticleType::Scatter;
	// (1.4) New motion models
	if(lower == "brownian")    return ParticleType::Brownian;
	if(lower == "oscillation") return ParticleType::Oscillation;
	if(lower == "orbit")       return ParticleType::Orbit;
	if(lower == "noise")       return ParticleType::Noise;
	if(lower == "lifecurve")   return ParticleType::LifeCurve;
	return ParticleType::Rain; // default
}

// Parse one endpoint for move(): either `N|S|E|W` (case-insensitive single
// letter) → side sentinel, or `[x,y]` percent coords. Returns true on success.
static bool ParseMovePoint(const std::string& raw, float& x, float& y, int& side){
	std::string s = Trim(raw);
	if(s.empty()) return false;

	// Side keyword — exactly one letter.
	if(s.size() == 1){
		char c = (char)std::toupper((unsigned char)s[0]);
		if(c == 'N'){ side = 1; x = 0.0f; y = 0.0f; return true; }
		if(c == 'S'){ side = 2; x = 0.0f; y = 0.0f; return true; }
		if(c == 'E'){ side = 3; x = 0.0f; y = 0.0f; return true; }
		if(c == 'W'){ side = 4; x = 0.0f; y = 0.0f; return true; }
	}
	// Otherwise expect [x, y].
	side = 0;
	return ParseBracketPairF(s, x, y);
}

// Recursively parse modifiers from an argument string.
// Returns the final "content" (filename or text) and fills mods.
static std::string ParseModifiers(const std::string& input, RenderModifiers& mods){
	std::string s = Trim(input);

	// colour(COLOUR, content). (2.0.1) `colour` is the documented spelling;
	// `color` is still accepted so every existing scene reads the same.
	{
		size_t open = std::string::npos;
		if(s.rfind("colour(", 0) == 0)     open = 6;
		else if(s.rfind("color(", 0) == 0) open = 5;
		if(open != std::string::npos){
			auto close = FindMatchingParen(s, open);
			if(close != std::string::npos){
				std::string inner = s.substr(open + 1, close - open - 1);
				auto args = SplitArgs(inner);
				if(args.size() >= 2){
					// An unrecognised colour has always meant white here.
					Colour c = Colours::WHITE;
					ParseColour(args[0], c);
					mods.textColour = c;
					return ParseModifiers(args[1], mods);
				}
			}
		}
	}

	// trans(percentage, content)
	if(s.rfind("trans(", 0) == 0){
		auto close = FindMatchingParen(s, 5);
		if(close != std::string::npos){
			std::string inner = s.substr(6, close - 6);
			auto args = SplitArgs(inner);
			if(args.size() >= 2){
				try{ mods.transparency = 1.0f - (std::stof(args[0]) / 100.0f); } catch(...){}
				return ParseModifiers(args[1], mods);
			}
		}
	}

	// (1.6.1) bold(content) — wrap a segment to render in the bold face.
	// Composes with `it(...)` and `color(...)` like any other modifier.
	if(s.rfind("bold(", 0) == 0){
		auto close = FindMatchingParen(s, 4);
		if(close != std::string::npos){
			std::string inner = s.substr(5, close - 5);
			auto args = SplitArgs(inner);
			if(args.size() >= 1){
				mods.bold = true;
				return ParseModifiers(args[0], mods);
			}
		}
	}

	// (1.6.1) it(content) — italic counterpart of bold(). Short name
	// chosen to match the user's preferred LaTeX-flavoured spelling
	// (`\it`) and to keep concat lines compact: `text "A " + it("B")`.
	if(s.rfind("it(", 0) == 0){
		auto close = FindMatchingParen(s, 2);
		if(close != std::string::npos){
			std::string inner = s.substr(3, close - 3);
			auto args = SplitArgs(inner);
			if(args.size() >= 1){
				mods.italic = true;
				return ParseModifiers(args[0], mods);
			}
		}
	}

	// rotate(degrees, content)
	if(s.rfind("rotate(", 0) == 0){
		auto close = FindMatchingParen(s, 6);
		if(close != std::string::npos){
			std::string inner = s.substr(7, close - 7);
			auto args = SplitArgs(inner);
			if(args.size() >= 2){
				try{ mods.rotation = std::stof(args[0]); } catch(...){}
				return ParseModifiers(args[1], mods);
			}
		}
	}

	// move(FROM, TO, DURATION_MS, content)
	// FROM / TO are either [x,y] percent coords or a side keyword (N/S/E/W).
	if(s.rfind("move(", 0) == 0){
		auto close = FindMatchingParen(s, 4);
		if(close != std::string::npos){
			std::string inner = s.substr(5, close - 5);
			auto args = SplitArgs(inner);
			if(args.size() >= 4){
				bool okFrom = ParseMovePoint(args[0], mods.moveFromX, mods.moveFromY, mods.moveFromSide);
				bool okTo   = ParseMovePoint(args[1], mods.moveToX,   mods.moveToY,   mods.moveToSide);
				float ms = 0.0f;
				try{ ms = std::stof(args[2]); } catch(...){}
				if(okFrom && okTo && ms > 0.0f){
					mods.hasMove = true;
					mods.moveDurationSec = ms / 1000.0f;
				}
				return ParseModifiers(args[3], mods);
			}
		}
	}

	// (1.8.3) pathMove(P1, P2, …, PN, DURATION_MS [, MODE], content)
	// Two or more waypoints in move()'s endpoint vocabulary, then the total
	// time for the whole path, then an optional repeat mode, then content.
	// Waypoints are collected greedily from the front: the scan stops at
	// the first argument that is neither `[x,y]` nor a side letter, which
	// is necessarily the duration. Like move(), a malformed spec still
	// recurses into the content so the image/text is shown unanimated
	// rather than the whole cue disappearing.
	if(s.rfind("pathMove(", 0) == 0){
		auto close = FindMatchingParen(s, 8);
		if(close != std::string::npos){
			std::string inner = s.substr(9, close - 9);
			auto args = SplitArgs(inner);
			// A valid spec needs at least 4 args (2 waypoints + duration +
			// content), but anything with a plausible content slot is worth
			// unwrapping: a malformed path then still shows its image or
			// text, unanimated, instead of printing the raw modifier string
			// on the slave.
			if(args.size() >= 2){
				std::vector<RenderModifiers::PathPoint> pts;
				size_t i = 0;
				for(; i < args.size(); i++){
					RenderModifiers::PathPoint p;
					if(!ParseMovePoint(args[i], p.x, p.y, p.side)) break;
					pts.push_back(p);
				}

				bool ok = (pts.size() >= 2) && (i < args.size());
				float ms = 0.0f;
				if(ok){
					try{ ms = std::stof(Trim(args[i])); } catch(...){ ok = false; }
					if(ms <= 0.0f) ok = false;
				}
				if(ok) i++;

				// An optional repeat mode sits between the duration and the
				// content, so exactly one spare argument means a mode is
				// present. An unrecognised word there is a typo, not a
				// silent Once — reject the spec so the author sees it.
				RenderModifiers::PathRepeat rep = RenderModifiers::PathRepeat::Once;
				if(ok && args.size() - i == 2){
					std::string mode = Trim(args[i]);
					std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
					if(mode == "loop")          rep = RenderModifiers::PathRepeat::Loop;
					else if(mode == "pingpong") rep = RenderModifiers::PathRepeat::PingPong;
					else if(mode == "once")     rep = RenderModifiers::PathRepeat::Once;
					else ok = false;
					if(ok) i++;
				}

				if(ok && args.size() - i == 1){
					mods.pathPoints      = std::move(pts);
					mods.pathDurationSec = ms / 1000.0f;
					mods.pathRepeat      = rep;
					// Also flag the two-point field so every existing
					// "animated?" check sees this entry as animated.
					mods.hasMove         = true;
				}
				return ParseModifiers(args.back(), mods);
			}
		}
	}

	// (1.8.3) layer(N, content) — draw-order for images. Higher is nearer
	// the front; 0 is the default and reproduces push-order behaviour.
	if(s.rfind("layer(", 0) == 0){
		auto close = FindMatchingParen(s, 5);
		if(close != std::string::npos){
			std::string inner = s.substr(6, close - 6);
			auto args = SplitArgs(inner);
			if(args.size() >= 2){
				try{ mods.layer = std::stoi(Trim(args[0])); } catch(...){}
				return ParseModifiers(args[1], mods);
			}
		}
	}

	// spin(degreesPerSecond, content)
	if(s.rfind("spin(", 0) == 0){
		auto close = FindMatchingParen(s, 4);
		if(close != std::string::npos){
			std::string inner = s.substr(5, close - 5);
			auto args = SplitArgs(inner);
			if(args.size() >= 2){
				try{ mods.spinDegPerSec = std::stof(args[0]); } catch(...){}
				return ParseModifiers(args[1], mods);
			}
		}
	}

	// (1.4) bounce(true|false, content) — reflect particles off screen edges.
	if(s.rfind("bounce(", 0) == 0){
		auto close = FindMatchingParen(s, 6);
		if(close != std::string::npos){
			std::string inner = s.substr(7, close - 7);
			auto args = SplitArgs(inner);
			if(args.size() >= 2){
				std::string v = Trim(args[0]);
				mods.bounceEdges = (v == "true" || v == "1");
				return ParseModifiers(args[1], mods);
			}
		}
	}

	// (1.4) trail(N, content) — keep N past frames behind each particle.
	// (1.4.1) Optional middle MODE arg: trail(N, GHOST|STRETCH, content).
	if(s.rfind("trail(", 0) == 0){
		auto close = FindMatchingParen(s, 5);
		if(close != std::string::npos){
			std::string inner = s.substr(6, close - 6);
			auto args = SplitArgs(inner);
			if(args.size() >= 3){
				try{ mods.trailLength = std::stoi(args[0]); } catch(...){}
				mods.trailLength = std::clamp(mods.trailLength, 0, 64);
				std::string mode = Trim(args[1]);
				std::transform(mode.begin(), mode.end(), mode.begin(), ::toupper);
				if(mode == "STRETCH")    mods.trailMode = RenderModifiers::TrailMode::Stretch;
				else if(mode == "GLOW")  mods.trailMode = RenderModifiers::TrailMode::Glow;
				else                     mods.trailMode = RenderModifiers::TrailMode::Ghost;
				return ParseModifiers(args[2], mods);
			} else if(args.size() >= 2){
				try{ mods.trailLength = std::stoi(args[0]); } catch(...){}
				mods.trailLength = std::clamp(mods.trailLength, 0, 64);
				mods.trailMode = RenderModifiers::TrailMode::Ghost;
				return ParseModifiers(args[1], mods);
			}
		}
	}

	// (1.4.1) trailGlow(N, content) — additive-blend variant of GHOST.
	if(s.rfind("trailGlow(", 0) == 0){
		auto close = FindMatchingParen(s, 9);
		if(close != std::string::npos){
			std::string inner = s.substr(10, close - 10);
			auto args = SplitArgs(inner);
			if(args.size() >= 2){
				try{ mods.trailLength = std::stoi(args[0]); } catch(...){}
				mods.trailLength = std::clamp(mods.trailLength, 0, 64);
				mods.trailMode = RenderModifiers::TrailMode::Glow;
				return ParseModifiers(args[1], mods);
			}
		}
	}

	// pos([x, y], content)
	if(s.rfind("pos(", 0) == 0){
		auto close = FindMatchingParen(s, 3);
		if(close != std::string::npos){
			std::string inner = s.substr(4, close - 4);
			auto args = SplitArgs(inner);
			if(args.size() >= 2){
				ParseBracketPairF(args[0], mods.posX, mods.posY);
				return ParseModifiers(args[1], mods);
			}
		}
	}

	// random(file, [minW,minH], [maxW,maxH], keepAspect)
	if(s.rfind("random(", 0) == 0){
		auto close = FindMatchingParen(s, 6);
		if(close != std::string::npos){
			std::string inner = s.substr(7, close - 7);
			auto args = SplitArgs(inner);
			if(args.size() >= 4){
				mods.useRandomSize = true;
				ParseBracketPair(args[1], mods.minWidth, mods.minHeight);
				ParseBracketPair(args[2], mods.maxWidth, mods.maxHeight);
				std::string ka = Trim(args[3]);
				mods.keepAspectRatio = (ka == "true" || ka == "1");
				return ParseModifiers(args[0], mods);
			}
		}
	}

	// particle(TYPE, content) or particle(TYPE, intensity, content)
	if(s.rfind("particle(", 0) == 0){
		auto close = FindMatchingParen(s, 8);
		if(close != std::string::npos){
			std::string inner = s.substr(9, close - 9);
			auto args = SplitArgs(inner);
			if(args.size() >= 3){
				// particle(TYPE, intensity, content)
				mods.particleType = ParseParticleType(args[0]);
				try{ mods.particleIntensity = std::stof(args[1]) / 100.0f; } catch(...){}
				mods.particleIntensity = std::clamp(mods.particleIntensity, 0.01f, 1.0f);
				return ParseModifiers(args[2], mods);
			} else if(args.size() >= 2){
				// particle(TYPE, content) — 100% intensity
				mods.particleType = ParseParticleType(args[0]);
				mods.particleIntensity = 1.0f;
				return ParseModifiers(args[1], mods);
			}
		}
	}

	// size([w, h], content) — and its alias scale(content, [w, h])
	// which matches random()'s argument order for consistency.
	if(s.rfind("size(", 0) == 0){
		auto close = FindMatchingParen(s, 4);
		if(close != std::string::npos){
			std::string inner = s.substr(5, close - 5);
			auto args = SplitArgs(inner);
			if(args.size() >= 2){
				ParseBracketPair(args[0], mods.width, mods.height);
				return ParseModifiers(args[1], mods);
			}
		}
	}
	if(s.rfind("scale(", 0) == 0){
		auto close = FindMatchingParen(s, 5);
		if(close != std::string::npos){
			std::string inner = s.substr(6, close - 6);
			auto args = SplitArgs(inner);
			if(args.size() >= 2){
				// scale(content, [w,h]) — content first, dims second.
				ParseBracketPair(args[1], mods.width, mods.height);
				return ParseModifiers(args[0], mods);
			}
		}
	}

	// randomImages([pic1.png, pic2.png, ...]) or randomImages(folderName)
	if(s.rfind("randomImages(", 0) == 0){
		auto close = FindMatchingParen(s, 12);
		if(close != std::string::npos){
			std::string inner = Trim(s.substr(13, close - 13));
			if(!inner.empty() && inner.front() == '[' && inner.back() == ']'){
				// Explicit list: [pic1.png, pic2.png, ...]
				std::string listInner = inner.substr(1, inner.size() - 2);
				auto items = SplitArgs(listInner);
				for(auto& item : items){
					item = Trim(item);
					item = StripQuotes(item);
					if(!item.empty()) mods.randomImagePaths.push_back(item);
				}
			} else{
				// Folder name — stored as single entry, resolved at runtime
				mods.randomImagePaths.push_back("DIR:" + StripQuotes(inner));
			}
			// Return empty — the image will be chosen at runtime
			return "";
		}
	}

	// No modifier — this is the final content
	return StripQuotes(s);
}

// Split a text argument at top-level `+` tokens, respecting quotes, parens,
// and brackets. Used by `text` / `textCont` to support the concatenation
// operator — each piece is parsed independently via ParseModifiers so every
// segment can carry its own color/trans wrapper on one line.
// *(1.7)* Quote tracking honours `'` and `´` as alternates to `"`.
static std::vector<std::string> SplitPlus(const std::string& s){
	std::vector<std::string> out;
	int parenDepth = 0, bracketDepth = 0;
	int quoteKind = 0;
	size_t start = 0;
	for(size_t i = 0; i < s.size(); ){
		int k = 0;
		size_t dl = DelimAt(s, i, &k);
		if(dl > 0){
			if(quoteKind == 0)      quoteKind = k;
			else if(quoteKind == k) quoteKind = 0;
			i += dl;
			continue;
		}
		if(quoteKind != 0){ i++; continue; }
		char c = s[i];
		if(c == '(') parenDepth++;
		else if(c == ')') parenDepth--;
		else if(c == '[') bracketDepth++;
		else if(c == ']') bracketDepth--;
		else if(c == '+' && parenDepth == 0 && bracketDepth == 0){
			out.push_back(Trim(s.substr(start, i - start)));
			start = i + 1;
		}
		i++;
	}
	out.push_back(Trim(s.substr(start)));
	// Drop empties (e.g. trailing `+`).
	out.erase(std::remove_if(out.begin(), out.end(),
		[](const std::string& x){ return x.empty(); }), out.end());
	return out;
}

// Parse the argument of `text` / `textCont` into a list of TextRuns plus a
// single line-level RenderModifiers. Splits on top-level `+`, then walks each
// segment through ParseModifiers. Per-run fields (textColour, transparency)
// land on the run; line-level fields (pos, move, spin, rotate, size, random,
// particle) fold into the shared lineMods — **leftmost wins** when two
// segments both try to set the same line-level field. The rule is documented
// in docs/commands.md under the `+` operator.
static std::vector<TextRun> ParseTextRuns(const std::string& argRaw,
	RenderModifiers& lineMods){
	std::vector<TextRun> runs;
	auto pieces = SplitPlus(argRaw);
	if(pieces.empty()) return runs;

	bool lineSetPos = false, lineSetMove = false, lineSetSpin = false;
	bool lineSetRotate = false, lineSetSize = false, lineSetRandom = false;
	bool lineSetParticle = false, lineSetRandomImages = false;

	for(const auto& piece : pieces){
		RenderModifiers segMods;
		std::string content = ParseModifiers(piece, segMods);

		TextRun run;
		// (1.8.2) LaTeX symbol macros are expanded here, at the very end
		// of the modifier walk, so every text-bearing command inherits it
		// through one code path — `text`, the styled and `Cont` variants,
		// both sides of `textD`, and each segment of a `+` concatenation.
		// Doing it at parse time means the navigator, the wrap measuring
		// and the glyph rendering all see real characters.
		run.text = ExpandLatexSymbols(content);
		if(segMods.textColour.a > 0) run.colour = segMods.textColour;
		run.transparency = segMods.transparency;
		// (1.6.1) Inline `bold(...)` / `it(...)` set the per-run style
		// flags via segMods.bold/italic. Line-level keywords (textbf etc.)
		// OR additional flags onto every run after this loop returns.
		run.bold   = segMods.bold;
		run.italic = segMods.italic;
		runs.push_back(std::move(run));

		// Fold line-level fields into lineMods with leftmost-wins.
		if(!lineSetPos && segMods.HasPosition()){
			lineMods.posX = segMods.posX;
			lineMods.posY = segMods.posY;
			lineSetPos = true;
		}
		if(!lineSetMove && segMods.hasMove){
			lineMods.hasMove         = true;
			lineMods.moveFromX       = segMods.moveFromX;
			lineMods.moveFromY       = segMods.moveFromY;
			lineMods.moveToX         = segMods.moveToX;
			lineMods.moveToY         = segMods.moveToY;
			lineMods.moveDurationSec = segMods.moveDurationSec;
			lineMods.moveFromSide    = segMods.moveFromSide;
			lineMods.moveToSide      = segMods.moveToSide;
			// (1.8.3) pathMove() rides along on the same leftmost-wins slot;
			// a path sets hasMove, so it can never be folded independently
			// of the two-point fields and get half-applied.
			lineMods.pathPoints      = segMods.pathPoints;
			lineMods.pathDurationSec = segMods.pathDurationSec;
			lineMods.pathRepeat      = segMods.pathRepeat;
			lineSetMove = true;
		}
		if(!lineSetSpin && segMods.spinDegPerSec != 0.0f){
			lineMods.spinDegPerSec = segMods.spinDegPerSec;
			lineSetSpin = true;
		}
		if(!lineSetRotate && segMods.rotation != 0.0f){
			lineMods.rotation = segMods.rotation;
			lineSetRotate = true;
		}
		if(!lineSetSize && segMods.width > 0 && segMods.height > 0){
			lineMods.width  = segMods.width;
			lineMods.height = segMods.height;
			lineSetSize = true;
		}
		if(!lineSetRandom && segMods.useRandomSize){
			lineMods.useRandomSize    = true;
			lineMods.minWidth         = segMods.minWidth;
			lineMods.minHeight        = segMods.minHeight;
			lineMods.maxWidth         = segMods.maxWidth;
			lineMods.maxHeight        = segMods.maxHeight;
			lineMods.keepAspectRatio  = segMods.keepAspectRatio;
			lineSetRandom = true;
		}
		if(!lineSetParticle && segMods.particleType != ParticleType::None){
			lineMods.particleType      = segMods.particleType;
			lineMods.particleIntensity = segMods.particleIntensity;
			lineSetParticle = true;
		}
		if(!lineSetRandomImages && !segMods.randomImagePaths.empty()){
			lineMods.randomImagePaths = segMods.randomImagePaths;
			lineSetRandomImages = true;
		}
	}

	return runs;
}

// ── Command parsing ─────────────────────────────────────────

static Command ParseSingleCommand(const std::string& line, int lineNumber){
	Command cmd;
	cmd.lineNumber = lineNumber;

	// (1.7) `countdown(SECONDS)` and `countdown(SECONDS, "LANG")` are the
	// only paren-style commands in the grammar; the rest still split on the
	// first space. Handle countdown up front so it gets paren-aware arg
	// extraction without disturbing the existing keyword dispatch.
	if(line.size() > 10 && line.compare(0, 10, "countdown(") == 0){
		cmd.type = CommandType::Countdown;
		size_t close = line.rfind(')');
		std::string inner = (close != std::string::npos && close > 9)
			? line.substr(10, close - 10) : "";
		auto args = SplitArgs(inner);
		if(!args.empty()){
			try{ cmd.countdownSeconds = std::stoi(Trim(args[0])); }
			catch(...){ cmd.countdownSeconds = 0; }
		}
		if(args.size() >= 2){
			std::string lang = StripQuotes(Trim(args[1]));
			std::transform(lang.begin(), lang.end(), lang.begin(), ::tolower);
			// Recognised tags get a label; unknown tags fall back to "no
			// label" (just the digits). Empty second arg = explicit no-label.
			if(lang == "en" || lang == "da") cmd.countdownLabel = lang;
		}
		// Clamp to MM:SS range (max 59:59 = 3599s).
		if(cmd.countdownSeconds < 0)    cmd.countdownSeconds = 0;
		if(cmd.countdownSeconds > 3599) cmd.countdownSeconds = 3599;
		cmd.argument = inner; // for the master command list / debug
		return cmd;
	}

	// (1.8.2) `typewrite(MS_PER_CHAR, TEXT)` — the second paren-style
	// command. TEXT is the full text expression, so `+` concatenation and
	// every text modifier work exactly as they do in `text`; the split is
	// on the first *structural* comma, which leaves commas inside
	// `color(RED, "a")` alone.
	if(line.size() > 10 && line.compare(0, 10, "typewrite(") == 0){
		cmd.type = CommandType::Typewrite;
		size_t close = line.rfind(')');
		std::string inner = (close != std::string::npos && close > 9)
			? line.substr(10, close - 10) : "";

		size_t comma = FindStructural(inner, ',');
		std::string msRaw   = (comma == std::string::npos)
			? inner : inner.substr(0, comma);
		std::string textRaw = (comma == std::string::npos)
			? std::string() : inner.substr(comma + 1);

		try{ cmd.typeMsPerChar = std::stoi(Trim(msRaw)); }
		catch(...){ cmd.typeMsPerChar = 0; }
		// 0 = instant (degrades to plain `text`); the upper bound keeps a
		// fat-fingered value from pinning a line on screen for an hour.
		if(cmd.typeMsPerChar < 0)    cmd.typeMsPerChar = 0;
		if(cmd.typeMsPerChar > 5000) cmd.typeMsPerChar = 5000;

		cmd.runs = ParseTextRuns(Trim(textRaw), cmd.mods);
		std::string concat;
		for(auto& r : cmd.runs) concat += r.text;
		cmd.argument = concat;
		return cmd;
	}

	auto spacePos = line.find(' ');
	std::string keyword = (spacePos != std::string::npos)
		? line.substr(0, spacePos) : line;
	std::string argRaw = (spacePos != std::string::npos)
		? Trim(line.substr(spacePos + 1)) : "";

	if(keyword == "text")              cmd.type = CommandType::Text;
	else if(keyword == "textCont")     cmd.type = CommandType::TextCont;
	else if(keyword == "textbf")       cmd.type = CommandType::TextBf;
	else if(keyword == "textit")       cmd.type = CommandType::TextIt;
	else if(keyword == "textbfCont")   cmd.type = CommandType::TextBfCont;
	else if(keyword == "textitCont")   cmd.type = CommandType::TextItCont;
	else if(keyword == "textD")        cmd.type = CommandType::TextD;
	else if(keyword == "clear")        cmd.type = CommandType::Clear;
	else if(keyword == "clearText")    cmd.type = CommandType::ClearText;
	else if(keyword == "clearImages")  cmd.type = CommandType::ClearImages;
	else if(keyword == "clearAll")     cmd.type = CommandType::ClearAll;
	else if(keyword == "play")         cmd.type = CommandType::Play;
	else if(keyword == "stop")         cmd.type = CommandType::Stop;
	else if(keyword == "stopParticleCont") cmd.type = CommandType::StopParticleCont;
	else if(keyword == "show")            cmd.type = CommandType::Show;
	else if(keyword == "comment")         cmd.type = CommandType::Comment;
	else if(keyword == "clearCountdown")  cmd.type = CommandType::ClearCountdown;
	else{
		cmd.type = CommandType::Text;
		// (1.6.3) Heuristic: a leading bareword (no quote, no `+`, no
		// `#`) that didn't match any keyword is almost certainly a typo
		// — flag it so the Overview Debug popup can surface it. Strings
		// that genuinely start with text (quoted or with leading non-
		// alpha) skip this path and behave as before.
		// *(1.7)* Also let `´…´` through (UTF-8 0xC2 0xB4) so the new
		// acute-accent quote pair doesn't get warned on.
		if(!keyword.empty()){
			char c0 = keyword.front();
			bool startsWithAcute = (keyword.size() >= 2
				&& (unsigned char)c0 == 0xC2
				&& (unsigned char)keyword[1] == 0xB4);
			bool isBareWord = (c0 != '"' && c0 != '\'' && c0 != '+' && c0 != '#'
				&& !startsWithAcute);
			if(isBareWord) cmd.unknownKeyword = keyword;
		}
	}

	// Parse modifiers from the argument
	if(cmd.type == CommandType::Comment){
		// Comments: take the quoted text verbatim; no modifier parsing.
		// (1.8.2) Symbol macros still expand — a navigator row reading
		// "Cue after the Ω speech" is more use to the operator than one
		// reading "Cue after the \Omega speech".
		cmd.argument = ExpandLatexSymbols(StripQuotes(argRaw));
	} else if(cmd.type == CommandType::Clear){
		// clear may take an optional group name (bareword, no quotes).
		cmd.argument = Trim(argRaw);
	} else if(cmd.type == CommandType::ClearText
		|| cmd.type == CommandType::ClearImages
		|| cmd.type == CommandType::ClearAll
		|| cmd.type == CommandType::ClearCountdown){
		// These take no arguments — any trailing text is ignored.
		cmd.argument = "";
	} else if(cmd.type == CommandType::TextD){
		// (1.6.1) Split argRaw on top-level commas — SplitArgs already
		// respects quoted strings and parens, so nested `color(RED, "x,y")`
		// inside one of the pieces stays intact. Whitespace around the
		// comma is irrelevant.
		auto pieces = SplitArgs(argRaw);
		std::string primaryRaw = pieces.empty() ? std::string() : pieces[0];
		std::string secondaryRaw = pieces.size() >= 2 ? pieces[1] : std::string();

		// Always parse the primary side via the run-producer so `+` and
		// inline modifiers work like in `text`.
		cmd.runs = ParseTextRuns(primaryRaw, cmd.mods);

		if(!secondaryRaw.empty()){
			RenderModifiers subtitleMods; // standalone — not folded into line mods
			cmd.subtitleRuns = ParseTextRuns(secondaryRaw, subtitleMods);
		} else{
			// No second arg given — collapse to a plain Text command. The
			// dispatcher will then take the standard text path. We keep the
			// CommandType::TextD identity so the master command list can
			// still display the original `textD` keyword if desired, but
			// dispatch checks `subtitleRuns.empty()` and falls through.
		}

		std::string concat;
		for(auto& r : cmd.runs) concat += r.text;
		cmd.argument = concat;
	} else if(cmd.type == CommandType::Text || cmd.type == CommandType::TextCont
		|| cmd.type == CommandType::TextBf || cmd.type == CommandType::TextIt
		|| cmd.type == CommandType::TextBfCont || cmd.type == CommandType::TextItCont){
		// Text and its style/cont variants always go through the run-producing
		// path so the `+` concatenation operator is supported uniformly.
		// Single-segment lines produce a single run (identical behaviour to
		// pre-1.4 for `text`).
		cmd.runs = ParseTextRuns(argRaw, cmd.mods);

		// (1.6.1) Style-keyword variants set a line-level default that ORs
		// onto every produced run. Inline `bold(...)` / `it(...)` already
		// set per-run flags during ParseTextRuns; this just adds the line
		// default so every run inherits the keyword's style. So
		//   `textbf "A " + it("B")` → both bold; B is bold-italic.
		bool lineBold = (cmd.type == CommandType::TextBf
			|| cmd.type == CommandType::TextBfCont);
		bool lineItalic = (cmd.type == CommandType::TextIt
			|| cmd.type == CommandType::TextItCont);
		if(lineBold || lineItalic){
			for(auto& r : cmd.runs){
				r.bold   = r.bold   || lineBold;
				r.italic = r.italic || lineItalic;
			}
		}

		// Keep `argument` filled with the concatenated plain text so debug
		// output, logs, and any legacy consumer that reads `argument` still
		// see something sensible.
		std::string concat;
		for(auto& r : cmd.runs) concat += r.text;
		cmd.argument = concat;
	} else if(cmd.type == CommandType::StopParticleCont){
		// Optional group name as a bareword; no modifier parsing.
		cmd.argument = Trim(argRaw);
	} else if(cmd.type == CommandType::Play){
		// (1.6.6) `play FILE [FADE_IN_MS, FADE_OUT_MS]`. The fade spec is a
		// trailing `<int>, <int>` pair separated from the filename by
		// whitespace. Bare `play FILE` keeps the pre-1.6.6 behaviour
		// (0/0 fades). Only video dispatch reads the fade fields; audio
		// ignores them.
		std::string rest = Trim(argRaw);
		auto comma = rest.find(',');
		if(comma != std::string::npos){
			// Anchor on the comma, trim its trailing whitespace, walk
			// backward through the fade-in digits to find their start,
			// then require at least one whitespace between the digits and
			// the filename so a filename ending in digits is not eaten.
			size_t end = comma;
			while(end > 0 && (rest[end - 1] == ' ' || rest[end - 1] == '\t')) end--;
			size_t fadeStart = end;
			while(fadeStart > 0 && rest[fadeStart - 1] >= '0'
				&& rest[fadeStart - 1] <= '9'){
				fadeStart--;
			}
			bool hasGap = fadeStart > 0 && fadeStart < end
				&& (rest[fadeStart - 1] == ' ' || rest[fadeStart - 1] == '\t');
			std::string fadeOutStr = Trim(rest.substr(comma + 1));
			bool fadeOutDigits = !fadeOutStr.empty();
			for(char c : fadeOutStr){
				if(c < '0' || c > '9'){ fadeOutDigits = false; break; }
			}
			if(hasGap && fadeOutDigits){
				std::string fadeInStr = rest.substr(fadeStart, end - fadeStart);
				try{ cmd.fadeInMs  = std::stoi(fadeInStr);  } catch(...){ cmd.fadeInMs = 0; }
				try{ cmd.fadeOutMs = std::stoi(fadeOutStr); } catch(...){ cmd.fadeOutMs = 0; }
				if(cmd.fadeInMs  < 0) cmd.fadeInMs  = 0;
				if(cmd.fadeOutMs < 0) cmd.fadeOutMs = 0;
				size_t fileEnd = fadeStart - 1;
				while(fileEnd > 0 && (rest[fileEnd - 1] == ' '
					|| rest[fileEnd - 1] == '\t')){
					fileEnd--;
				}
				rest = rest.substr(0, fileEnd);
			}
		}
		cmd.argument = StripQuotes(Trim(rest));
	} else if(!argRaw.empty() && cmd.type != CommandType::Stop){
		cmd.argument = ParseModifiers(argRaw, cmd.mods);
	} else{
		cmd.argument = StripQuotes(argRaw);
	}

	return cmd;
}

// (1.8.4) Strip a trailing `\Name` cue reference from a command line and
// return the name ("" if there is none). The reference must:
//   - sit at the very end of the line,
//   - be preceded by whitespace,
//   - be structural (outside quotes and modifier parens), and
//   - name a cue that the scene's [Cue] section actually defines.
//
// That last condition is what keeps this from colliding with the 1.8.2
// LaTeX macros. `text \alpha` written without quotes has the same shape as
// a cue reference; requiring the name to be defined means an undefined
// `\alpha` is left alone for the symbol expander. The cost is that a
// mistyped reference is silently not a reference — which is why an
// unreferenced note raises a warning instead (see ParseFile).
static std::string ExtractCueRef(std::string& line,
	const std::vector<CueNote>& cues){
	if(cues.empty()) return "";

	// Walk back over the trailing identifier.
	size_t end = line.size();
	while(end > 0 && (line[end - 1] == ' ' || line[end - 1] == '\t')) end--;
	size_t start = end;
	while(start > 0){
		char c = line[start - 1];
		bool ident = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9') || c == '_';
		if(!ident) break;
		start--;
	}
	if(start == end || start == 0) return "";          // no identifier
	if(line[start - 1] != '\\') return "";             // not a `\Name`
	size_t slash = start - 1;
	if(slash == 0) return "";                          // needs leading space
	char before = line[slash - 1];
	if(before != ' ' && before != '\t') return "";

	// The backslash must be structural — not inside a string or a modifier
	// argument list, where a backslash is content.
	if(FindStructural(line, '\\', slash) != slash) return "";

	std::string name = line.substr(start, end - start);
	bool defined = false;
	for(const auto& c : cues){
		if(c.name == name){ defined = true; break; }
	}
	if(!defined) return "";

	line = line.substr(0, slash);
	// Trim the whitespace the reference was hanging off.
	while(!line.empty() && (line.back() == ' ' || line.back() == '\t')){
		line.pop_back();
	}
	return name;
}

// (1.8.4) True when a block-body line closes the block. A bare `}` is the
// normal case; a closing brace that picked up a stray `\Name` also counts.
//
// An author who wants a note on a whole `run{ … }` naturally reaches for
// `} \Cue1`, and before this check that line simply wasn't the terminator
// — so the block ran on and swallowed the rest of the scene with no error.
// Closing on it is the safe reading. The reference itself is dropped,
// because notes bind to command rows; put it on a command inside the body
// and the row picks it up (see RowCueName).
static bool IsBlockClose(const std::string& bodyLine,
	const std::vector<CueNote>& cues){
	if(bodyLine == "}") return true;
	if(bodyLine.empty() || bodyLine[0] != '}') return false;
	std::string probe = bodyLine;
	if(ExtractCueRef(probe, cues).empty()) return false;
	return Trim(probe) == "}";
}

static Command ParseCommandLine(const std::string& line, int lineNumber,
	const std::vector<CueNote>& cues){
	// (1.8.4) A `\Name` cue reference belongs to the whole navigator row,
	// so strip it here — before the `&` split — and stamp it on whatever
	// command comes out. On `clear & play x.mp3 \Cue1` the note lands on
	// the Compound, which is the row the notes column draws beside.
	std::string body = line;
	const std::string cueName = ExtractCueRef(body, cues);

	// (1.8.2) Split on `&` only where it is structural. Until 1.8.2 this
	// used `std::getline(stream, segment, '&')`, which knew nothing about
	// quoting — so `text "Tom & Jerry"` was chopped into `text "Tom ` and
	// `Jerry"` and dispatched as a two-command Compound. Routing the split
	// through FindStructural gives `&` the same quote/paren awareness the
	// `#` comment marker has.
	if(FindStructural(body, '&') == std::string::npos){
		Command single = ParseSingleCommand(body, lineNumber);
		single.cueName = cueName;
		return single;
	}

	std::vector<Command> parts;
	size_t start = 0;
	while(true){
		size_t amp = FindStructural(body, '&', start);
		std::string segment = Trim(amp == std::string::npos
			? body.substr(start)
			: body.substr(start, amp - start));
		if(!segment.empty()){
			parts.push_back(ParseSingleCommand(segment, lineNumber));
		}
		if(amp == std::string::npos) break;
		start = amp + 1;
	}

	if(parts.size() == 1){
		parts[0].cueName = cueName;
		return std::move(parts[0]);
	}

	Command compound;
	compound.type = CommandType::Compound;
	compound.lineNumber = lineNumber;
	compound.subCommands = std::move(parts);
	compound.cueName = cueName;
	return compound;
}

// Recursively tag a command (and any sub-commands of a Compound) with the
// given group name. Used when parsing `group NAME{ … }` blocks so the slave
// can later clear that group selectively.
static void TagCommandWithGroup(Command& cmd, const std::string& groupName){
	cmd.groupName = groupName;
	for(auto& sub : cmd.subCommands){
		TagCommandWithGroup(sub, groupName);
	}
	for(auto& b : cmd.loopBody){
		TagCommandWithGroup(b, groupName);
	}
}

static void ExpandLoop(int loopCount, int clearEvery,
	const std::vector<Command>& body, std::vector<Command>& out){
	for(int iter = 0; iter < loopCount; iter++){
		for(auto& cmd : body){
			out.push_back(cmd);
		}
		if(clearEvery > 0 && ((iter + 1) % clearEvery == 0)){
			Command clearCmd;
			clearCmd.type = CommandType::Clear;
			clearCmd.lineNumber = body.empty() ? 0 : body[0].lineNumber;
			out.push_back(clearCmd);
		}
	}
}

// ── Scene file parser ───────────────────────────────────────

// Process one "logical command" starting at allLines[i]. Handles:
//   - run{ … }       → emits a single Compound
//   - loop(N[,K]){…} → emits N copies of the body (with optional clears)
//   - macro NAME     → inlines the macro body
//   - plain line     → single command or &-compound
// The caller has already stripped comments and trimmed the starting `line`
// (which is guaranteed non-empty). `comment` is the original trailing `#…`
// comment for the plain-line case. May advance `i` past body lines.
// Does NOT recognise `group{…}` — groups exist only at the top level and
// their body delegates back to this helper.
static void ProcessLogicalCommand(
	const std::vector<std::string>& allLines, size_t& i,
	const std::string& line, const std::string& comment,
	std::unordered_map<std::string, std::vector<Command>>& macros,
	const std::vector<CueNote>& cues,
	std::vector<Command>& out){

	int lineNumber = (int)i + 1;

	// ── run{ … } ──────────────────────────────────────────
	bool isRunBlock = false;
	if(line.rfind("run", 0) == 0){
		std::string rest = Trim(line.substr(3));
		if(rest.empty()){
			size_t j = i + 1;
			while(j < allLines.size() && Trim(allLines[j]).empty()) j++;
			if(j < allLines.size() && !Trim(allLines[j]).empty()
				&& Trim(allLines[j])[0] == '{'){
				isRunBlock = true;
			}
		} else if(rest[0] == '{'){
			isRunBlock = true;
		}
	}
	if(isRunBlock){
		bool foundBrace = (line.find('{') != std::string::npos);
		if(!foundBrace){
			i++;
			while(i < allLines.size() && Trim(allLines[i]).empty()) i++;
			if(i < allLines.size() && Trim(allLines[i])[0] == '{') foundBrace = true;
		}

		std::vector<Command> body;
		if(foundBrace){
			i++;
			while(i < allLines.size()){
				std::string bodyLine = allLines[i];
				auto cp = FindCommentStart(bodyLine);
				if(cp != std::string::npos) bodyLine = bodyLine.substr(0, cp);
				bodyLine = Trim(bodyLine);
				if(IsBlockClose(bodyLine, cues)) break;
				if(!bodyLine.empty()){
					Command inner = ParseCommandLine(bodyLine, (int)i + 1, cues);
					if(inner.type == CommandType::Compound){
						for(auto& sub : inner.subCommands){
							body.push_back(std::move(sub));
						}
					} else{
						body.push_back(std::move(inner));
					}
				}
				i++;
			}
		}

		Command compound;
		compound.type = CommandType::Compound;
		compound.lineNumber = lineNumber;
		compound.subCommands = std::move(body);
		out.push_back(std::move(compound));
		return;
	}

	// ── loop(N[,K]){ … } ──────────────────────────────────
	if(line.rfind("loop(", 0) == 0){
		auto parenClose = line.find(')');
		if(parenClose == std::string::npos) return;
		std::string params = line.substr(5, parenClose - 5);

		int loopCount = 1, clearEvery = 0;
		auto comma = params.find(',');
		if(comma != std::string::npos){
			try{ loopCount = std::stoi(Trim(params.substr(0, comma))); } catch(...){}
			try{ clearEvery = std::stoi(Trim(params.substr(comma + 1))); } catch(...){}
		} else{
			try{ loopCount = std::stoi(Trim(params)); } catch(...){}
		}

		bool foundBrace = (line.find('{') != std::string::npos);
		if(!foundBrace){
			i++;
			if(i < allLines.size() && Trim(allLines[i]) == "{") foundBrace = true;
		}

		std::vector<Command> body;
		if(foundBrace){
			i++;
			while(i < allLines.size()){
				std::string bodyLine = allLines[i];
				auto cp = FindCommentStart(bodyLine);
				if(cp != std::string::npos) bodyLine = bodyLine.substr(0, cp);
				bodyLine = Trim(bodyLine);
				if(IsBlockClose(bodyLine, cues)) break;
				if(!bodyLine.empty()){
					body.push_back(ParseCommandLine(bodyLine, (int)i + 1, cues));
				}
				i++;
			}
		}
		ExpandLoop(loopCount, clearEvery, body, out);
		return;
	}

	// ── macro NAME ────────────────────────────────────────
	if(line.rfind("macro ", 0) == 0 || line.rfind("macro\t", 0) == 0){
		std::string name = Trim(line.substr(6));
		auto it = macros.find(name);
		if(it != macros.end()){
			for(auto& m : it->second){
				out.push_back(m);
			}
		}
		return;
	}

	// ── plain line ────────────────────────────────────────
	Command cmd = ParseCommandLine(line, lineNumber, cues);
	cmd.comment = comment;
	out.push_back(std::move(cmd));
}

Scene SceneParser::ParseFile(const std::string& scenePath){
	Scene scene;

	// (1.6.6) Open through Platform::Utf8ToPath so scene filenames
	// containing non-ASCII UTF-8 bytes (e.g. æøå) resolve correctly on
	// Windows, where the narrow-string std::ifstream overload would
	// otherwise interpret the path through the system codepage.
	std::ifstream file(Platform::Utf8ToPath(scenePath));
	if(!file.is_open()) return scene;

	auto pos = scenePath.find_last_of("/\\");
	auto dot = scenePath.find_last_of('.');
	scene.fileName = scenePath.substr(pos + 1);
	scene.name = scenePath.substr(pos + 1, dot - pos - 1);

	enum class Section{ None, Options, Preload, Macro, Cue, Commands, Backstage };
	Section currentSection = Section::None;

	// Macros collected from the [Macro] section. Referenced by name from
	// [Commands] via `macro <Name>`, which expands to the stored body at
	// parse time. Simple expansion only — no parameters, no nesting.
	std::unordered_map<std::string, std::vector<Command>> macros;

	std::vector<std::string> allLines;
	std::string rawLine;
	while(std::getline(file, rawLine)){
		allLines.push_back(rawLine);
	}
	scene.sourceLines = allLines;

	// (1.8.4) [Cue] is collected in a pre-pass. A command's `\Name`
	// reference is only recognised when the name is actually defined, so
	// the notes have to be known before any command is parsed — regardless
	// of whether the author put [Cue] above or below [Commands].
	{
		bool inCue = false;
		for(size_t i = 0; i < allLines.size(); i++){
			std::string line = allLines[i];
			auto cp = FindCommentStart(line);
			if(cp != std::string::npos) line = line.substr(0, cp);
			line = Trim(line);
			if(line.empty()) continue;

			// Any `[...]` header ends the section — checked generically so
			// an unknown section can't be swallowed as cue notes.
			if(line.size() >= 2 && line.front() == '[' && line.back() == ']'){
				inCue = (line == "[Cue]");
				continue;
			}
			if(!inCue) continue;

			// `Name  the note text…` — first token names the cue.
			auto sp = line.find_first_of(" \t");
			if(sp == std::string::npos){
				Scene::Warning w;
				w.lineNumber = (int)i + 1;
				w.message = "Cue note '" + line + "' has a name but no text";
				scene.warnings.push_back(std::move(w));
				continue;
			}
			CueNote note;
			note.name = line.substr(0, sp);
			note.text = Trim(line.substr(sp + 1));
			note.lineNumber = (int)i + 1;
			if(note.name.empty() || note.text.empty()) continue;

			bool duplicate = false;
			for(const auto& existing : scene.cueNotes){
				if(existing.name == note.name){ duplicate = true; break; }
			}
			if(duplicate){
				Scene::Warning w;
				w.lineNumber = note.lineNumber;
				w.message = "Duplicate cue note '" + note.name
					+ "' — the first definition is used";
				scene.warnings.push_back(std::move(w));
				continue;
			}
			scene.cueNotes.push_back(std::move(note));
		}
	}

	for(size_t i = 0; i < allLines.size(); i++){
		std::string line = allLines[i];
		int lineNumber = (int)i + 1;

		auto commentPos = (currentSection == Section::Options)
			? FindOptionCommentStart(line) : FindCommentStart(line);
		std::string comment;
		if(commentPos != std::string::npos){
			comment = Trim(line.substr(commentPos + 1));
			line = line.substr(0, commentPos);
		}

		line = Trim(line);
		if(line.empty()) continue;

		if(line == "[Options]"){ currentSection = Section::Options; continue; }
		if(line == "[Preload]"){ currentSection = Section::Preload; continue; }
		if(line == "[Macro]"){ currentSection = Section::Macro; continue; }
		if(line == "[Cue]"){ currentSection = Section::Cue; continue; }
		if(line == "[Commands]"){ currentSection = Section::Commands; continue; }
		// (2.0.2) Any case, since the roadmap and the ticker's own docs both
		// write it lower-case.
		{
			std::string lower = line;
			std::transform(lower.begin(), lower.end(), lower.begin(),
				[](unsigned char ch){ return (char)std::tolower(ch); });
			if(lower == "[backstage]"){
				currentSection = Section::Backstage;
				scene.backstage.present = true;
				continue;
			}
		}

		switch(currentSection){
			case Section::Backstage:{
				// (2.0.2) `Key = value`, see core/Backstage.hpp. A list may
				// span lines the way TOML writes it:
				//     Actors = [
				//         "Actor 1",
				//         "Actor 2"
				//     ]
				auto eq = line.find('=');
				if(eq == std::string::npos){
					Scene::Warning w;
					w.lineNumber = lineNumber;
					w.message = "[Backstage] expects 'Key = value', got '" + line + "'";
					scene.warnings.push_back(std::move(w));
					break;
				}
				std::string key = Trim(line.substr(0, eq));
				std::string val = Trim(line.substr(eq + 1));
				if(!val.empty() && val.front() == '[' && val.find(']') == std::string::npos){
					while(i + 1 < allLines.size()){
						std::string more = allLines[++i];
						auto cp = FindCommentStart(more);
						if(cp != std::string::npos) more = more.substr(0, cp);
						more = Trim(more);
						if(more.empty()) continue;
						val += " " + more;
						if(more.find(']') != std::string::npos) break;
					}
				}
				std::string warning;
				Backstage::ApplyKey(scene.backstage, key, val, warning);
				if(!warning.empty()){
					Scene::Warning w;
					w.lineNumber = lineNumber;
					w.message = warning;
					scene.warnings.push_back(std::move(w));
				}
				break;
			}
			case Section::Options:{
				auto eq = line.find('=');
				if(eq != std::string::npos){
					auto key = Trim(line.substr(0, eq));
					auto val = Trim(line.substr(eq + 1));
					// (2.0.1) A quoted value means its contents, so `bc = "#102030"`
					// and font paths with spaces both work. Unquoted values are
					// read exactly as before.
					if(val.size() >= 2 &&
					   ((val.front() == '"' && val.back() == '"') ||
					    (val.front() == '\'' && val.back() == '\''))){
						val = val.substr(1, val.size() - 2);
					}
					// (2.0.1) Option names come from the shared table in
					// OptionKeys: the same name works here and in schema.toml,
					// case does not matter, and the old spellings (`bc`, `cap`,
					// `font`, ...) still work. A scene honours the options below;
					// the rest (TextOutline, Subtitle*) are project-wide.
					const OptionKey opt = LookupOptionKey(key);
					if(opt == OptionKey::BackgroundColour){
						// Unrecognised colours leave the background as it was.
						Colour c;
						if(ParseColour(val, c)) scene.backgroundColor = c;
					} else if(opt == OptionKey::TextColour){
						// (2.0.1) Unrecognised colours leave it unset, so the
						// project's TextColour (or white) still applies.
						Colour c;
						if(ParseColour(val, c)) scene.textColour = c;
					} else if(opt == OptionKey::FontSize){
						try{ scene.fontSize = std::stoi(val); } catch(...){}
					} else if(opt == OptionKey::Font){
						scene.fontPath = val;
					} else if(opt == OptionKey::FontItalic){
						scene.fontPathItalic = val;
					} else if(opt == OptionKey::FontBold){
						scene.fontPathBold = val;
					} else if(opt == OptionKey::FontBoldItalic){
						scene.fontPathBoldItalic = val;
					} else if(opt == OptionKey::Capitalize){
						scene.capitalize = ParseOptionBool(val);
					} else if(opt == OptionKey::AudioFade){
						// (2.0.0) Opt-in audio fading for `play` fade times.
						scene.audioFade = ParseOptionBool(val);
					} else if(opt == OptionKey::FixedTextCentre){
						// (2.0.2) Either way, overriding the project.
						scene.fixedTextCentre = ParseOptionBool(val) ? 1 : 0;
					} else if(opt == OptionKey::Unknown){
						// (1.4) Particle tunables: <TYPE>.SPEED, <TYPE>.DENS,
						// <TYPE>.X.DIST = NAME(p1, p2). Case-insensitive type.
						auto dot = key.find('.');
						if(dot != std::string::npos){
							std::string typeName = key.substr(0, dot);
							std::string rest = key.substr(dot + 1);
							std::transform(typeName.begin(), typeName.end(),
								typeName.begin(), ::tolower);
							std::transform(rest.begin(), rest.end(),
								rest.begin(), ::toupper);
							ParticleType pt;
							if(ParseParticleTypeStrict(typeName, pt)){
								auto& tune = scene.particleTunings[(int)pt];
								if(rest == "SPEED"){
									try{ tune.speed = std::stof(val); tune.haveSpeed = true; } catch(...){}
								} else if(rest == "DENS" || rest == "DENSITY"){
									try{ tune.density = std::stof(val); tune.haveDensity = true; } catch(...){}
								} else if(rest == "X.DIST" || rest == "X"){
									if(ParseDistSpec(val, tune.xDist)) tune.haveXDist = true;
								} else if(rest == "VX.DIST" || rest == "VX"){
									if(ParseDistSpec(val, tune.vxDist)) tune.haveVxDist = true;
								} else if(rest == "VY.DIST" || rest == "VY"){
									if(ParseDistSpec(val, tune.vyDist)) tune.haveVyDist = true;
								}
							}
						}
					}
				}
				break;
			}
			case Section::Preload:{
				scene.preloadPaths.push_back(line);
				break;
			}
			case Section::Macro:{
				// Macro definitions look like:
				//   Name{          (brace on same line)
				//       command...
				//   }
				// or
				//   Name           (brace on the next non-empty line)
				//   {
				//       command...
				//   }
				// The name is any non-whitespace identifier before `{`. Body
				// lines are each parsed through ParseCommandLine — no nested
				// macros, loops, or run{} are supported (simple expansion).
				std::string header = line;
				std::string name;
				auto bracePos = header.find('{');
				if(bracePos != std::string::npos){
					name = Trim(header.substr(0, bracePos));
				} else{
					name = Trim(header);
				}
				if(name.empty()){ break; }

				// Find the opening brace on this line or the next non-blank.
				bool foundBrace = (bracePos != std::string::npos);
				if(!foundBrace){
					size_t j = i + 1;
					while(j < allLines.size() && Trim(allLines[j]).empty()) j++;
					if(j < allLines.size() && Trim(allLines[j])[0] == '{'){
						i = j;
						foundBrace = true;
					}
				}
				if(!foundBrace) break;

				std::vector<Command> body;
				i++;
				while(i < allLines.size()){
					std::string bodyLine = allLines[i];
					auto cp = FindCommentStart(bodyLine);
					if(cp != std::string::npos) bodyLine = bodyLine.substr(0, cp);
					bodyLine = Trim(bodyLine);
					if(IsBlockClose(bodyLine, scene.cueNotes)) break;
					if(!bodyLine.empty()){
						body.push_back(ParseCommandLine(bodyLine, (int)i + 1,
							scene.cueNotes));
					}
					i++;
				}
				macros[name] = std::move(body);
				break;
			}
			case Section::Commands:{
				// Detect `group NAME{ … }` block form. Every command inside
				// the body is tagged with NAME so later `clear NAME` can drop
				// this group selectively. The block itself produces no runtime
				// entry — its body is flattened directly into scene.commands.
				bool isGroupBlock = false;
				std::string groupName;
				if(line.rfind("group ", 0) == 0 || line.rfind("group\t", 0) == 0){
					std::string rest = Trim(line.substr(5));
					auto bracePos = rest.find('{');
					if(bracePos != std::string::npos){
						groupName = Trim(rest.substr(0, bracePos));
						if(!groupName.empty()) isGroupBlock = true;
					} else if(!rest.empty()){
						// Name on this line, brace on next non-blank.
						size_t j = i + 1;
						while(j < allLines.size() && Trim(allLines[j]).empty()) j++;
						if(j < allLines.size() && Trim(allLines[j])[0] == '{'){
							groupName = rest;
							isGroupBlock = true;
						}
					}
				}
				if(isGroupBlock){
					// Find opening brace on this line or next non-blank.
					bool foundBrace = (line.find('{') != std::string::npos);
					if(!foundBrace){
						i++;
						while(i < allLines.size() && Trim(allLines[i]).empty()) i++;
						if(i < allLines.size() && Trim(allLines[i])[0] == '{') foundBrace = true;
					}
					if(foundBrace){
						// Record how many commands existed before the body so we
						// can tag just the freshly added ones afterwards. This
						// delegates to ProcessLogicalCommand so every construct
						// supported at the top level (run{}, loop(){}, macro,
						// plain) is also supported inside `group{}`.
						size_t startCount = scene.commands.size();
						i++;
						while(i < allLines.size()){
							std::string bodyLine = allLines[i];
							auto cp = FindCommentStart(bodyLine);
							std::string bodyComment;
							if(cp != std::string::npos){
								bodyComment = Trim(bodyLine.substr(cp + 1));
								bodyLine = bodyLine.substr(0, cp);
							}
							bodyLine = Trim(bodyLine);
							if(IsBlockClose(bodyLine, scene.cueNotes)) break;
							if(!bodyLine.empty()){
								ProcessLogicalCommand(allLines, i, bodyLine,
									bodyComment, macros, scene.cueNotes, scene.commands);
							}
							i++;
						}
						// Tag every command added during the body with the
						// group name (recurses into sub-commands of Compounds).
						for(size_t k = startCount; k < scene.commands.size(); k++){
							TagCommandWithGroup(scene.commands[k], groupName);
						}
					}
					break;
				}

				// All other top-level command forms — run{}, loop(){}, macro,
				// plain — share one helper with the group-body loop above.
				ProcessLogicalCommand(allLines, i, line, comment, macros,
					scene.cueNotes, scene.commands);
				break;
			}
			default: break;
		}
	}

	// (1.6.3) Lift any per-command `unknownKeyword` markers into the
	// scene's warnings list so Overview Debug can render them without
	// re-walking commands. Done after parsing so deeply-nested commands
	// (compound bodies, loop bodies, group bodies) all get covered by
	// recursing through subCommands and loopBody.
	std::function<void(const Command&)> walk;
	walk = [&](const Command& c){
		if(!c.unknownKeyword.empty()){
			Scene::Warning w;
			w.lineNumber = c.lineNumber;
			w.message = "Unknown keyword: '" + c.unknownKeyword + "'";
			scene.warnings.push_back(std::move(w));
		}
		for(const auto& sub : c.subCommands) walk(sub);
		for(const auto& sub : c.loopBody)    walk(sub);
	};
	for(const auto& c : scene.commands) walk(c);

	// (1.8.4) A cue note that nothing references is almost always a typo in
	// the command's `\Name`, and it is the only way to catch one: an
	// unrecognised reference is deliberately left alone so it can't collide
	// with a LaTeX macro (see ExtractCueRef), which means the mistake shows
	// up here, on the note, rather than on the command.
	if(!scene.cueNotes.empty()){
		std::function<void(const Command&, std::vector<std::string>&)> collect;
		collect = [&](const Command& c, std::vector<std::string>& used){
			if(!c.cueName.empty()) used.push_back(c.cueName);
			for(const auto& sub : c.subCommands) collect(sub, used);
			for(const auto& sub : c.loopBody)    collect(sub, used);
		};
		std::vector<std::string> used;
		for(const auto& c : scene.commands) collect(c, used);

		for(const auto& note : scene.cueNotes){
			bool referenced = false;
			for(const auto& u : used){
				if(u == note.name){ referenced = true; break; }
			}
			if(!referenced){
				Scene::Warning w;
				w.lineNumber = note.lineNumber;
				w.message = "Cue note '" + note.name
					+ "' is never referenced — check the \\" + note.name
					+ " on the command";
				scene.warnings.push_back(std::move(w));
			}
		}
	}

	// (2.0.2) A [Backstage] block without a Name shows the scene's own name.
	if(scene.backstage.present && scene.backstage.name.empty()){
		scene.backstage.name = scene.name;
	}

	return scene;
}

Command SceneParser::ParseCommand(const std::string& line, int lineNumber){
	return ParseSingleCommand(line, lineNumber);
}

std::vector<Command> SceneParser::ParseLoop(
	const std::vector<std::string>& lines, size_t& index, int lineNumber){
	(void)lines; (void)index; (void)lineNumber;
	return {};
}

} // namespace SatyrAV
