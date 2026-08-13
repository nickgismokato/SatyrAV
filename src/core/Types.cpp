#include "core/Types.hpp"

namespace SatyrAV{

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
