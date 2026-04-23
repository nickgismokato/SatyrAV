#include "core/SceneParser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace SatyrAV{

static std::string Trim(const std::string& s){
	auto lt = s.find_first_not_of(" \t");
	if(lt == std::string::npos) return "";
	auto rt = s.find_last_not_of(" \t");
	return s.substr(lt, rt - lt + 1);
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

// Split arguments at top-level commas (not inside parens or brackets)
static std::vector<std::string> SplitArgs(const std::string& s){
	std::vector<std::string> args;
	int parenDepth = 0, bracketDepth = 0;
	bool inQuote = false;
	size_t start = 0;

	for(size_t i = 0; i < s.size(); i++){
		char c = s[i];
		if(c == '"') inQuote = !inQuote;
		if(inQuote) continue;
		if(c == '(') parenDepth++;
		else if(c == ')') parenDepth--;
		else if(c == '[') bracketDepth++;
		else if(c == ']') bracketDepth--;
		else if(c == ',' && parenDepth == 0 && bracketDepth == 0){
			args.push_back(Trim(s.substr(start, i - start)));
			start = i + 1;
		}
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

// Strip quotes from a string argument
static std::string StripQuotes(const std::string& s){
	if(s.size() >= 2 && s.front() == '"' && s.back() == '"'){
		return s.substr(1, s.size() - 2);
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
static Colour ParseNamedColour(const std::string& name){
	std::string lower = name;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
	if(lower == "red")    return {0xFF, 0x00, 0x00, 0xFF};
	if(lower == "green")  return {0x00, 0xFF, 0x00, 0xFF};
	if(lower == "blue")   return {0x00, 0x00, 0xFF, 0xFF};
	if(lower == "white")  return {0xFF, 0xFF, 0xFF, 0xFF};
	if(lower == "black")  return {0x00, 0x00, 0x00, 0xFF};
	if(lower == "yellow") return {0xFF, 0xFF, 0x00, 0xFF};
	if(lower == "orange") return {0xFF, 0xA5, 0x00, 0xFF};
	if(lower == "purple") return {0x80, 0x00, 0x80, 0xFF};
	if(lower == "pink")   return {0xFF, 0x69, 0xB4, 0xFF};
	if(lower == "cyan")   return {0x00, 0xFF, 0xFF, 0xFF};
	if(lower == "lime")   return {0x32, 0xCD, 0x32, 0xFF};
	if(lower == "grey" || lower == "gray") return {0x80, 0x80, 0x80, 0xFF};
	// Try hex: #RRGGBB
	if(name.size() == 7 && name[0] == '#'){
		try{
			unsigned long hex = std::stoul(name.substr(1), nullptr, 16);
			return Colour::FromHex((uint32_t)hex);
		} catch(...){}
	}
	return {0xFF, 0xFF, 0xFF, 0xFF}; // default white
}

// Returns the final "content" (filename or text) and fills mods.
static std::string ParseModifiers(const std::string& input, RenderModifiers& mods){
	std::string s = Trim(input);

	// color(COLOUR, content)
	if(s.rfind("color(", 0) == 0){
		auto close = FindMatchingParen(s, 5);
		if(close != std::string::npos){
			std::string inner = s.substr(6, close - 6);
			auto args = SplitArgs(inner);
			if(args.size() >= 2){
				mods.textColour = ParseNamedColour(Trim(args[0]));
				return ParseModifiers(args[1], mods);
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
static std::vector<std::string> SplitPlus(const std::string& s){
	std::vector<std::string> out;
	int parenDepth = 0, bracketDepth = 0;
	bool inQuote = false;
	size_t start = 0;
	for(size_t i = 0; i < s.size(); i++){
		char c = s[i];
		if(c == '"') inQuote = !inQuote;
		if(inQuote) continue;
		if(c == '(') parenDepth++;
		else if(c == ')') parenDepth--;
		else if(c == '[') bracketDepth++;
		else if(c == ']') bracketDepth--;
		else if(c == '+' && parenDepth == 0 && bracketDepth == 0){
			out.push_back(Trim(s.substr(start, i - start)));
			start = i + 1;
		}
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
		run.text = content;
		if(segMods.textColour.a > 0) run.colour = segMods.textColour;
		run.transparency = segMods.transparency;
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

	auto spacePos = line.find(' ');
	std::string keyword = (spacePos != std::string::npos)
		? line.substr(0, spacePos) : line;
	std::string argRaw = (spacePos != std::string::npos)
		? Trim(line.substr(spacePos + 1)) : "";

	if(keyword == "text")              cmd.type = CommandType::Text;
	else if(keyword == "textCont")     cmd.type = CommandType::TextCont;
	else if(keyword == "clear")        cmd.type = CommandType::Clear;
	else if(keyword == "clearText")    cmd.type = CommandType::ClearText;
	else if(keyword == "clearImages")  cmd.type = CommandType::ClearImages;
	else if(keyword == "clearAll")     cmd.type = CommandType::ClearAll;
	else if(keyword == "play")         cmd.type = CommandType::Play;
	else if(keyword == "stop")         cmd.type = CommandType::Stop;
	else if(keyword == "stopParticleCont") cmd.type = CommandType::StopParticleCont;
	else if(keyword == "show")         cmd.type = CommandType::Show;
	else if(keyword == "comment")      cmd.type = CommandType::Comment;
	else                               cmd.type = CommandType::Text;

	// Parse modifiers from the argument
	if(cmd.type == CommandType::Comment){
		// Comments: take the quoted text verbatim; no modifier parsing.
		cmd.argument = StripQuotes(argRaw);
	} else if(cmd.type == CommandType::Clear){
		// clear may take an optional group name (bareword, no quotes).
		cmd.argument = Trim(argRaw);
	} else if(cmd.type == CommandType::ClearText
		|| cmd.type == CommandType::ClearImages
		|| cmd.type == CommandType::ClearAll){
		// These take no arguments — any trailing text is ignored.
		cmd.argument = "";
	} else if(cmd.type == CommandType::Text || cmd.type == CommandType::TextCont){
		// Text and textCont always go through the run-producing path so the
		// `+` concatenation operator is supported uniformly. Single-segment
		// lines produce a single run (identical behaviour to pre-1.4).
		cmd.runs = ParseTextRuns(argRaw, cmd.mods);
		// Keep `argument` filled with the concatenated plain text so debug
		// output, logs, and any legacy consumer that reads `argument` still
		// see something sensible.
		std::string concat;
		for(auto& r : cmd.runs) concat += r.text;
		cmd.argument = concat;
	} else if(cmd.type == CommandType::StopParticleCont){
		// Optional group name as a bareword; no modifier parsing.
		cmd.argument = Trim(argRaw);
	} else if(!argRaw.empty() && cmd.type != CommandType::Stop){
		cmd.argument = ParseModifiers(argRaw, cmd.mods);
	} else{
		cmd.argument = StripQuotes(argRaw);
	}

	return cmd;
}

static Command ParseCommandLine(const std::string& line, int lineNumber){
	if(line.find('&') == std::string::npos){
		return ParseSingleCommand(line, lineNumber);
	}

	std::vector<Command> parts;
	std::istringstream stream(line);
	std::string segment;
	while(std::getline(stream, segment, '&')){
		segment = Trim(segment);
		if(segment.empty()) continue;
		parts.push_back(ParseSingleCommand(segment, lineNumber));
	}

	if(parts.size() == 1) return std::move(parts[0]);

	Command compound;
	compound.type = CommandType::Compound;
	compound.lineNumber = lineNumber;
	compound.subCommands = std::move(parts);
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
				auto cp = bodyLine.find('#');
				if(cp != std::string::npos) bodyLine = bodyLine.substr(0, cp);
				bodyLine = Trim(bodyLine);
				if(bodyLine == "}") break;
				if(!bodyLine.empty()){
					Command inner = ParseCommandLine(bodyLine, (int)i + 1);
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
				auto cp = bodyLine.find('#');
				if(cp != std::string::npos) bodyLine = bodyLine.substr(0, cp);
				bodyLine = Trim(bodyLine);
				if(bodyLine == "}") break;
				if(!bodyLine.empty()){
					body.push_back(ParseCommandLine(bodyLine, (int)i + 1));
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
	Command cmd = ParseCommandLine(line, lineNumber);
	cmd.comment = comment;
	out.push_back(std::move(cmd));
}

Scene SceneParser::ParseFile(const std::string& scenePath){
	Scene scene;

	std::ifstream file(scenePath);
	if(!file.is_open()) return scene;

	auto pos = scenePath.find_last_of("/\\");
	auto dot = scenePath.find_last_of('.');
	scene.fileName = scenePath.substr(pos + 1);
	scene.name = scenePath.substr(pos + 1, dot - pos - 1);

	enum class Section{ None, Options, Preload, Macro, Commands };
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

	for(size_t i = 0; i < allLines.size(); i++){
		std::string line = allLines[i];
		int lineNumber = (int)i + 1;

		auto commentPos = line.find('#');
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
		if(line == "[Commands]"){ currentSection = Section::Commands; continue; }

		switch(currentSection){
			case Section::Options:{
				auto eq = line.find('=');
				if(eq != std::string::npos){
					auto key = Trim(line.substr(0, eq));
					auto val = Trim(line.substr(eq + 1));
					if(key == "bc"){
						if(val == "black")      scene.backgroundColor = Colours::BLACK;
						else if(val == "blue")   scene.backgroundColor = {0x00, 0x00, 0xFF, 0xFF};
						else if(val == "red")    scene.backgroundColor = {0xFF, 0x00, 0x00, 0xFF};
						else if(val == "green")  scene.backgroundColor = {0x00, 0xFF, 0x00, 0xFF};
						else if(val == "white")  scene.backgroundColor = Colours::WHITE;
					} else if(key == "FontSize"){
						try{ scene.fontSize = std::stoi(val); } catch(...){}
					} else if(key == "font"){
						scene.fontPath = val;
					} else if(key == "cap"){
						scene.capitalize = (val == "true" || val == "1");
					} else{
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
					auto cp = bodyLine.find('#');
					if(cp != std::string::npos) bodyLine = bodyLine.substr(0, cp);
					bodyLine = Trim(bodyLine);
					if(bodyLine == "}") break;
					if(!bodyLine.empty()){
						body.push_back(ParseCommandLine(bodyLine, (int)i + 1));
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
							auto cp = bodyLine.find('#');
							std::string bodyComment;
							if(cp != std::string::npos){
								bodyComment = Trim(bodyLine.substr(cp + 1));
								bodyLine = bodyLine.substr(0, cp);
							}
							bodyLine = Trim(bodyLine);
							if(bodyLine == "}") break;
							if(!bodyLine.empty()){
								ProcessLogicalCommand(allLines, i, bodyLine,
									bodyComment, macros, scene.commands);
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
				ProcessLogicalCommand(allLines, i, line, comment, macros, scene.commands);
				break;
			}
			default: break;
		}
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
