#pragma once
#include "core/Types.hpp"
#include <string>

namespace SatyrAV{

class SchemaParser{
public:
	static RevySchema ParseFile(const std::string& schemaPath);
};

} // namespace SatyrAV
