#include "core/Types.hpp"
#include <cctype>
#include <algorithm>

namespace SatyrAV{

bool ParseColour(const std::string& text, Colour& out){
	size_t a = 0, b = text.size();
	while(a < b && std::isspace((unsigned char)text[a])) a++;
	while(b > a && std::isspace((unsigned char)text[b - 1])) b--;
	std::string s = text.substr(a, b - a);
	std::string lower = s;
	std::transform(lower.begin(), lower.end(), lower.begin(),
		[](unsigned char c){ return (char)std::tolower(c); });

	struct Named{ const char* name; uint32_t rgb; };
	static const Named kNamed[] = {
		{"red",    0xFF0000}, {"green",  0x00FF00}, {"blue",   0x0000FF},
		{"white",  0xFFFFFF}, {"black",  0x000000}, {"yellow", 0xFFFF00},
		{"orange", 0xFFA500}, {"purple", 0x800080}, {"pink",   0xFF69B4},
		{"cyan",   0x00FFFF}, {"lime",   0x32CD32}, {"grey",   0x808080},
		{"gray",   0x808080},
	};
	for(const auto& n : kNamed){
		if(lower == n.name){ out = Colour::FromHex(n.rgb); return true; }
	}

	if(s.size() != 7 && s.size() != 4) return false;
	if(s[0] != '#') return false;
	for(size_t i = 1; i < s.size(); i++){
		if(!std::isxdigit((unsigned char)s[i])) return false;
	}
	std::string hex = s.substr(1);
	if(hex.size() == 3){
		hex = std::string(2, hex[0]) + std::string(2, hex[1]) + std::string(2, hex[2]);
	}
	out = Colour::FromHex((uint32_t)std::stoul(hex, nullptr, 16));
	return true;
}

const char* RevyTypeName(RevyType type){
	switch(type){
		case RevyType::MatRevy:    return "MatRevy";
		case RevyType::BioRevy:    return "BioRevy";
		case RevyType::KemiRevy:   return "KemiRevy";
		case RevyType::MBKRevy:    return "MBKRevy";
		case RevyType::PsykoRevy:  return "PsykoRevy";
		case RevyType::FysikRevy:  return "FysikRevy";
		case RevyType::GeoRevy:    return "GeoRevy";
		case RevyType::DIKURevy:   return "DIKURevy";
		case RevyType::SaTyR:      return "SaTyR";
		case RevyType::Other:      return "Other";
	}
	return "Unknown";
}

const std::string& RowCueName(const Command& cmd){
	if(!cmd.cueName.empty()) return cmd.cueName;
	// A run{} body collapses into one Compound row; a reference written on
	// one of its commands is the author asking for a note on that row.
	for(const auto& sub : cmd.subCommands){
		if(!sub.cueName.empty()) return sub.cueName;
	}
	static const std::string none;
	return none;
}

const CueNote* FindCueNote(const Scene& scene, const std::string& name){
	if(name.empty()) return nullptr;
	for(const auto& note : scene.cueNotes){
		if(note.name == name) return &note;
	}
	return nullptr;
}

} // namespace SatyrAV
