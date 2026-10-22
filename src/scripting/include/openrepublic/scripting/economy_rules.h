#pragma once

#include "openrepublic/economy/economy.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace openrepublic::scripting
{
	// Sandboxed Lua tuning of an economy catalog (same limits as LuaRulesLoader: no libraries,
	// 64 KiB source, 8 MiB memory, bounded instructions). The chunk returns
	//   { schema_version = 1,
	//     buildings = { <type> = { speed_percent = 10..1000, storage_percent = 10..1000 } },
	//     vehicles  = { <type> = { capacity_percent = 10..1000, speed_percent = 10..1000 } } }
	// Unknown names, fields or out-of-range values reject the whole bundle; `out` is only written on success.
	class EconomyRules final
	{
	public:
		static bool ApplyFile(const std::filesystem::path& path, const economy::Catalog& base,
			economy::Catalog& out, std::string* error = nullptr);
		static bool ApplyText(std::string_view text, const economy::Catalog& base,
			economy::Catalog& out, std::string* error = nullptr);
	};
}
