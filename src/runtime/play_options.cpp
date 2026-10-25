#include "openrepublic/runtime/play_options.h"

#include <system_error>

namespace openrepublic::runtime
{
	PlayOptions ResolvePlayOptions(const std::vector<std::wstring>& arguments)
	{
		PlayOptions result;
		std::wstring preset;
		std::vector<std::wstring> passthrough;
		for (std::size_t i = 0; i < arguments.size(); ++i)
		{
			if (arguments[i] != L"--play")
			{
				passthrough.push_back(arguments[i]);
				continue;
			}
			if (!preset.empty() || i + 1 >= arguments.size())
			{
				result.error = "--play requires exactly one preset: coal, sites, or town";
				return result;
			}
			preset = arguments[++i];
		}
		if (preset.empty())
		{
			result.arguments = arguments;
			return result;
		}
		result.selected = true;
		for (const auto& argument : passthrough)
		{
			if (argument == L"--native-economy" || argument == L"--native-coal-demo" ||
				argument == L"--native-construction-sites" || argument == L"--native-objectives" ||
				argument == L"--native-town-demo" || argument == L"--native-terrain" || argument == L"--ambient-traffic")
			{
				result.error = "--play cannot be combined with options it supplies";
				return result;
			}
		}
		result.arguments = { L"--native-economy", L"--native-terrain", L"--ambient-traffic" };
		if (preset == L"coal")
		{
			result.arguments.push_back(L"--native-coal-demo");
			result.arguments.push_back(L"--native-objectives");
			result.arguments.push_back(L"builtin:coal_demo");
		}
		else if (preset == L"sites")
		{
			result.arguments.push_back(L"--native-construction-sites");
			result.arguments.push_back(L"--native-objectives");
			result.arguments.push_back(L"builtin:construction_sites_demo");
		}
		else if (preset == L"town")
		{
			result.arguments.push_back(L"--native-town-demo");
			result.arguments.push_back(L"--native-objectives");
			result.arguments.push_back(L"builtin:growth_town");
		}
		else
		{
			result.error = "unknown --play preset; expected coal, sites, or town";
			result.arguments.clear();
			return result;
		}
		result.arguments.insert(result.arguments.end(), passthrough.begin(), passthrough.end());
		return result;
	}

	std::filesystem::path ResolveApplicationDataPath(const std::filesystem::path& requested,
		const std::filesystem::path& workingDirectory, const std::filesystem::path& executablePath)
	{
		if (requested.is_absolute()) return requested;
		std::error_code error;
		const auto workingCandidate = workingDirectory / requested;
		if (std::filesystem::is_regular_file(workingCandidate, error) && !error) return workingCandidate;
		auto directory = executablePath.parent_path();
		while (!directory.empty())
		{
			error.clear();
			const auto candidate = directory / requested;
			if (std::filesystem::is_regular_file(candidate, error) && !error) return candidate;
			const auto parent = directory.parent_path();
			if (parent == directory) break;
			directory = parent;
		}
		return workingCandidate;
	}

	std::filesystem::path ResolveEngineWorkingDirectory(const std::filesystem::path& executablePath)
	{
		auto directory = executablePath.parent_path();
		if (directory.empty()) return {};
		const auto executableDirectory = directory;
		while (!directory.empty())
		{
			std::error_code error;
			const auto hasShaderDirectory = [&](const std::filesystem::path& relative)
			{
				error.clear();
				return std::filesystem::is_directory(directory / relative, error) && !error;
			};
			if (hasShaderDirectory("WickedEngine/shaders") ||
				hasShaderDirectory("third_party/WickedEngine/WickedEngine/shaders") ||
				hasShaderDirectory("shaders")) return directory;
			const auto parent = directory.parent_path();
			if (parent == directory) break;
			directory = parent;
		}
		return executableDirectory;
	}
}
