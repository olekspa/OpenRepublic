#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::runtime
{
	struct PlayOptions
	{
		std::vector<std::wstring> arguments;
		std::string error;
		bool selected = false;
	};

	// Expands --play <preset> into existing desktop options. With no --play,
	// arguments are returned byte-for-byte in their original order.
	PlayOptions ResolvePlayOptions(const std::vector<std::wstring>& arguments);
}
