#pragma once

#include "openrepublic/sim/simulation.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace openrepublic::scripting
{
	class LuaRulesLoader final
	{
	public:
		// Rules are data-only Lua chunks. Every load uses an isolated state with no libraries.
		static bool LoadFile(const std::filesystem::path& path, sim::SimulationRules& out, std::string* error = nullptr);
		static bool LoadText(std::string_view text, sim::SimulationRules& out, std::string* error = nullptr);
	};
}
