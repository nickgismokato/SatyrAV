#include "core/SchemaParser.hpp"
#include <toml++/toml.hpp>
#include <algorithm>

namespace SatyrAV{

// (1.4) Parse a distribution spec written in TOML — three keys
// `<prefix>Dist = "normal"`, `<prefix>DistP1 = ...`, `<prefix>DistP2 = ...`.
// Returns true if a recognised name was found. Prefix is `X`, `VX`, or `VY`.
static bool ParseDistFromToml(const toml::table* tbl, const char* prefix, DistSpec& out){
	if(!tbl) return false;
	std::string distKey = std::string(prefix) + "Dist";
	std::string p1Key   = std::string(prefix) + "DistP1";
	std::string p2Key   = std::string(prefix) + "DistP2";

	auto distVal = (*tbl)[distKey].value_or(std::string(""));
	if(distVal.empty()) return false;

	std::string lower = distVal;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
	if(lower == "uniform")        out.type = Distribution::Uniform;
	else if(lower == "normal")    out.type = Distribution::Normal;
	else if(lower == "lognormal") out.type = Distribution::LogNormal;
	else if(lower == "binomial")  out.type = Distribution::Binomial;
	else if(lower == "bernoulli") out.type = Distribution::Bernoulli;
	else if(lower == "gamma")     out.type = Distribution::Gamma;
	else return false;

	out.p1 = (float)(*tbl)[p1Key].value_or((double)out.p1);
	out.p2 = (float)(*tbl)[p2Key].value_or((double)out.p2);
	return true;
}

static void ParseParticleTuningsFromToml(const toml::table& root, RevySchema& schema){
	auto particle = root["Particle"].as_table();
	if(!particle) return;

	static const std::pair<const char*, ParticleType> kTypes[] = {
		{"RAIN",        ParticleType::Rain},
		{"SNOW",        ParticleType::Snow},
		{"CONFETTI",    ParticleType::Confetti},
		{"RISE",        ParticleType::Rise},
		{"SCATTER",     ParticleType::Scatter},
		{"BROWNIAN",    ParticleType::Brownian},
		{"OSCILLATION", ParticleType::Oscillation},
		{"ORBIT",       ParticleType::Orbit},
		{"NOISE",       ParticleType::Noise},
		{"LIFECURVE",   ParticleType::LifeCurve},
	};

	for(auto [name, type] : kTypes){
		auto sub = (*particle)[name].as_table();
		if(!sub) continue;

		ParticleTuning tune;
		if(auto v = (*sub)["Speed"].value<double>()){
			tune.speed = (float)*v;
			tune.haveSpeed = true;
		}
		if(auto v = (*sub)["Density"].value<double>()){
			tune.density = (float)*v;
			tune.haveDensity = true;
		}
		if(ParseDistFromToml(sub, "X", tune.xDist)){
			tune.haveXDist = true;
		}
		if(ParseDistFromToml(sub, "VX", tune.vxDist)){
			tune.haveVxDist = true;
		}
		if(ParseDistFromToml(sub, "VY", tune.vyDist)){
			tune.haveVyDist = true;
		}
		if(tune.haveSpeed || tune.haveDensity || tune.haveXDist
			|| tune.haveVxDist || tune.haveVyDist){
			schema.particleTunings[(int)type] = tune;
		}
	}
}

RevySchema SchemaParser::ParseFile(const std::string& schemaPath){
	RevySchema schema;

	try{
		auto tbl = toml::parse_file(schemaPath);

		schema.fontSize = tbl["Options"]["FontSize"].value_or(25);
		schema.textOutline = tbl["Options"]["TextOutline"].value_or(4);
		schema.fontPath = tbl["Options"]["Font"].value_or(std::string(""));
		// (1.6.1) Optional per-style font faces. Empty falls back to the
		// global Config path; missing-at-draw-time falls back to regular.
		schema.fontPathItalic     = tbl["Options"]["FontItalic"].value_or(std::string(""));
		schema.fontPathBold       = tbl["Options"]["FontBold"].value_or(std::string(""));
		schema.fontPathBoldItalic = tbl["Options"]["FontBoldItalic"].value_or(std::string(""));
		schema.capitalize = tbl["Options"]["Capitalize"].value_or(false);

		auto bc = tbl["Options"]["BackgroundColour"].value_or(std::string(""));
		if(bc == "black")      schema.backgroundColor = Colours::BLACK;
		else if(bc == "blue")  schema.backgroundColor = {0x00, 0x00, 0xFF, 0xFF};
		else if(bc == "red")   schema.backgroundColor = {0xFF, 0x00, 0x00, 0xFF};
		else if(bc == "green") schema.backgroundColor = {0x00, 0xFF, 0x00, 0xFF};
		else if(bc == "white") schema.backgroundColor = Colours::WHITE;

		// (1.6.1) Subtitle defaults for `textD`. Each key is optional; missing
		// keys leave the schema's built-in default in place.
		schema.subtitleItalic = tbl["Options"]["SubtitleItalic"].value_or(true);
		auto sc = tbl["Options"]["SubtitleColour"].value_or(std::string(""));
		if(!sc.empty()){
			if(sc.size() >= 7 && sc[0] == '#'){
				try{
					unsigned long hex = std::stoul(sc.substr(1), nullptr, 16);
					schema.subtitleColour = Colour::FromHex((uint32_t)hex);
				} catch(...){}
			} else if(sc == "black")       schema.subtitleColour = Colours::BLACK;
			else if(sc == "white")         schema.subtitleColour = Colours::WHITE;
			else if(sc == "grey" || sc == "gray")
			                                schema.subtitleColour = {0xAA, 0xAA, 0xAA, 0xFF};
		}
		// Stored as percent in TOML (0–100); kept as a 0–1 internal value
		// so the renderer's `transparency` field doesn't need scaling.
		auto stPct = tbl["Options"]["SubtitleTransparency"].value_or(0.0);
		schema.subtitleTransparency = 1.0f - (float)(stPct / 100.0);
		if(schema.subtitleTransparency < 0.0f) schema.subtitleTransparency = 0.0f;
		if(schema.subtitleTransparency > 1.0f) schema.subtitleTransparency = 1.0f;
		schema.subtitlePosY = (float)tbl["Options"]["SubtitlePosY"].value_or(90.0);

		schema.revyName = tbl["RevyData"]["Revy"].value_or(std::string("Unnamed Revy"));
		schema.aktCount = tbl["Structure"]["akter"].value_or(0);

		// [Display] — targeted display rect. Missing keys leave zeros,
		// which is the "fill physical display on first load" sentinel.
		schema.targetedDisplay.width    = tbl["Display"]["Width"].value_or(0);
		schema.targetedDisplay.height   = tbl["Display"]["Height"].value_or(0);
		schema.targetedDisplay.centerX  = tbl["Display"]["CenterX"].value_or(0);
		schema.targetedDisplay.centerY  = tbl["Display"]["CenterY"].value_or(0);
		schema.targetedDisplay.rotation = (float)tbl["Display"]["Rotation"].value_or(0.0);

		// (1.4) Project-level particle tunables from [Particle.<TYPE>] tables.
		ParseParticleTuningsFromToml(tbl, schema);

		for(int i = 1; i <= schema.aktCount; i++){
			std::string key = "Structure.Akt" + std::to_string(i);
			Akt akt;
			akt.number = i;

			if(auto arr = tbl.at_path(key + ".schema").as_array()){
				for(auto& elem : *arr){
					if(auto name = elem.value<std::string>()){
						akt.sceneNames.push_back(*name);
					}
				}
			}

			schema.akts.push_back(std::move(akt));
		}
	} catch(const toml::parse_error& e){
		// TODO: Proper error reporting
		(void)e;
	}

	return schema;
}

} // namespace SatyrAV
