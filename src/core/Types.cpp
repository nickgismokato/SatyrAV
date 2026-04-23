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

} // namespace SatyrAV
