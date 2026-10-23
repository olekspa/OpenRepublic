#include "openrepublic/scripting/scene_script.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace
{
	int failures = 0;
	void Check(bool value, const char* message)
	{
		if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
	}
	std::string ReadFile(const std::string& path)
	{
		std::ifstream file(path, std::ios::binary);
		return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	}
}

int main()
{
	using openrepublic::scenectl::SceneControlState;
	using openrepublic::scripting::SceneScript;
	SceneControlState scene;
	SceneScript cycle;
	std::string error;
	Check(cycle.LoadText(ReadFile(OPENREPUBLIC_DAY_CYCLE_PATH), &error), "day-cycle example loads");
	Check(cycle.Tick(0.5, scene), "day-cycle first frame runs");
	const double first = scene.CurrentLook().sunElevationDegrees;
	Check(cycle.Tick(0.5, scene), "day-cycle second frame runs");
	Check(scene.CurrentLook().sunElevationDegrees > first, "day-cycle sun elevation advances monotonically");
	Check(cycle.Tick(0.5, scene), "day-cycle third frame runs");
	Check(scene.CurrentLook().sunElevationDegrees > first, "day-cycle continues rising");

	SceneScript badValue;
	Check(badValue.LoadText("local api=_ENV['or']; function on_frame(dt) api.look.set{sunElevationDegrees=999} end", &error), "bad-value script compiles");
	Check(!badValue.Tick(0.1, scene), "out-of-range scene value is rejected");
	Check(!badValue.Enabled() && badValue.LastError().find("sunElevationDegrees is outside its allowed range") != std::string::npos,
		"typed scene API validation is reported and disables script");
	SceneScript cyclic;
	Check(cyclic.LoadText("local api=_ENV['or']; local x={}; x.camera=x; function on_frame(dt) api.look.set(x) end", &error),
		"cyclic input script compiles");
	Check(!cyclic.Tick(0.1,scene) && cyclic.LastError().find("cyclic") != std::string::npos,
		"cyclic Lua tables are rejected by bounded conversion");

	SceneScript infinite;
	Check(infinite.LoadText("function on_frame(dt) while true do end end", &error), "infinite script compiles");
	Check(!infinite.Tick(0.1, scene) && !infinite.Enabled(), "per-tick instruction budget disables runaway script");

	SceneScript noLibraries;
	Check(noLibraries.LoadText("if os ~= nil or io ~= nil or require ~= nil then _ENV['or'].log('ambient library found') end; function on_frame(dt) end", &error),
		"empty environment script loads without ambient globals");
	Check(noLibraries.Tick(0.1, scene), "os, io and require are unavailable");
	Check(noLibraries.TakeLogs().empty(), "sandbox exposes no os, io, or require values");

	SceneScript memory;
	Check(!memory.LoadText("local t={}; while true do t[#t+1]={1,2,3,4,5,6,7,8} end", &error) &&
		error.find("memory") != std::string::npos, "Lua allocator enforces the memory cap during initialization");

	SceneScript broken;
	Check(broken.LoadText("local api=_ENV['or']; function on_frame(dt) api.log('ready'); error('frame failure') end", &error), "error script loads");
	Check(!broken.Tick(0.1, scene) && !broken.Enabled(), "runtime error disables script");
	Check(!broken.Tick(0.1, scene), "disabled script remains inactive");

	SceneScript apiScript;
	Check(apiScript.LoadText("local api=_ENV['or']; function on_frame(dt) if api.time ~= 0.25 then error('bad time') end; local l=api.look.get(); api.camera.set{yawDegrees=30}; api.look.preset('dusk'); api.log('called') end", &error),
		"get, camera, preset and log API script loads");
	Check(apiScript.Tick(0.25, scene), "get, camera, preset and log calls run");
	Check(scene.CurrentLook().sunElevationDegrees == 3 && scene.CurrentLook().camera.yawDegrees == 30,
		"preset and camera changes reach SceneControlState");
	Check(apiScript.TakeLogs() == "called", "or.time is available and logs are retrievable");

	if (failures) return 1;
	std::cout << "SceneScript tests passed\n";
	return 0;
}
