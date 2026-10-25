#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

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

	// Resolve a relative packaged data path from the working directory first,
	// then from the executable directory and its parents.
	std::filesystem::path ResolveApplicationDataPath(const std::filesystem::path& requested,
		const std::filesystem::path& workingDirectory, const std::filesystem::path& executablePath);

	// Find the nearest executable ancestor containing the engine shader resources.
	// Falls back to the executable directory if no resource tree is present.
	std::filesystem::path ResolveEngineWorkingDirectory(const std::filesystem::path& executablePath);
}
