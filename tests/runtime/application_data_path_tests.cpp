#include "openrepublic/runtime/play_options.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
	void Require(bool value, const char* message)
	{
		if (!value) { std::cerr << message << '\n'; std::exit(1); }
	}
	void WriteFile(const std::filesystem::path& path)
	{
		std::filesystem::create_directories(path.parent_path());
		std::ofstream output(path, std::ios::binary);
		output << "test";
		Require(static_cast<bool>(output), "could not create resolver fixture");
	}
}

int main()
{
	const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
	const auto root = std::filesystem::temp_directory_path() /
		("OpenRepublicApplicationDataPathTests-" + std::to_string(unique));
	const auto relative = std::filesystem::path("data") / "objectives" / "coal_demo.json";
	const auto cwd = root / "unrelated";
	const auto executable = root / "build" / "bin" / "OpenRepublic.exe";
	const auto packaged = root / relative;
	try
	{
		WriteFile(packaged);
		const auto fallback = openrepublic::runtime::ResolveApplicationDataPath(relative, cwd, executable);
		Require(fallback == packaged, "resolver did not find packaged data by walking up from executable directory");

		const auto cwdFile = cwd / relative;
		WriteFile(cwdFile);
		const auto preferred = openrepublic::runtime::ResolveApplicationDataPath(relative, cwd, executable);
		Require(preferred == cwdFile, "resolver did not prefer a working-directory file");

		const std::filesystem::path absolute = packaged;
		Require(openrepublic::runtime::ResolveApplicationDataPath(absolute, cwd, executable) == absolute,
			"resolver changed an absolute path");
		std::filesystem::remove_all(root);
	}
	catch (...)
	{
		std::filesystem::remove_all(root);
		throw;
	}
	return 0;
}
