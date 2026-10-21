#include "openrepublic/scripting/lua_rules_loader.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	int failures = 0;

	void Check(bool condition, const std::string& name)
	{
		if (!condition)
		{
			std::cerr << "FAIL: " << name << '\n';
			++failures;
		}
	}

	constexpr std::string_view kValidRules = R"lua(
return {
    schema_version = 1,
    farmGrainPerHarvest = 7,
    farmHarvestIntervalTicks = 13,
    millGrainPerTick = 3,
}
)lua";

	openrepublic::sim::SimulationRules SentinelRules()
	{
		return {11, 37, 4};
	}

	bool Equals(const openrepublic::sim::SimulationRules& a, const openrepublic::sim::SimulationRules& b)
	{
		return a.farmGrainPerHarvest == b.farmGrainPerHarvest &&
			a.farmHarvestIntervalTicks == b.farmHarvestIntervalTicks &&
			a.millGrainPerTick == b.millGrainPerTick;
	}

	void CheckRejected(std::string_view name, std::string_view source)
	{
		auto out = SentinelRules();
		std::string error = "stale error";
		const bool loaded = openrepublic::scripting::LuaRulesLoader::LoadText(source, out, &error);
		Check(!loaded, std::string(name) + " rejects");
		Check(!error.empty(), std::string(name) + " reports an actionable error");
		Check(Equals(out, SentinelRules()), std::string(name) + " preserves output on failure");
	}
}

int main()
{
	using openrepublic::scripting::LuaRulesLoader;
	using openrepublic::sim::SimulationRules;
	std::string error;
	auto out = SimulationRules{};
	Check(LuaRulesLoader::LoadFile(OPENREPUBLIC_DEMO_RULES_PATH, out, &error), "demo rules file loads");
	Check(error.empty(), "successful file load clears prior error");
	Check(Equals(out, SimulationRules{2, 2, 1}), "demo rules preserve simulation defaults");

	out = {};
	Check(LuaRulesLoader::LoadText(kValidRules, out, &error), "valid custom values load");
	Check(Equals(out, SimulationRules{7, 13, 3}), "custom values are applied");

	const std::string noAmbientGlobals = R"lua(
if _G ~= nil or math ~= nil or string ~= nil or require ~= nil then
    return false
end
return {
    schema_version = 1,
    farmGrainPerHarvest = 2,
    farmHarvestIntervalTicks = 2,
    millGrainPerTick = 1,
}
)lua";
	out = {};
	Check(LuaRulesLoader::LoadText(noAmbientGlobals, out, &error), "chunk executes with no ambient globals");

	CheckRejected("non-table return", "return false");
	CheckRejected("missing field", R"lua(return {schema_version=1, farmGrainPerHarvest=2, farmHarvestIntervalTicks=2})lua");
	CheckRejected("extra field", R"lua(return {schema_version=1, farmGrainPerHarvest=2, farmHarvestIntervalTicks=2, millGrainPerTick=1, bonus=1})lua");
	CheckRejected("non-string key", R"lua(return {[1]=1, schema_version=1, farmGrainPerHarvest=2, farmHarvestIntervalTicks=2, millGrainPerTick=1})lua");
	CheckRejected("string instead of integer", R"lua(return {schema_version=1, farmGrainPerHarvest='2', farmHarvestIntervalTicks=2, millGrainPerTick=1})lua");
	CheckRejected("fraction instead of integer", R"lua(return {schema_version=1, farmGrainPerHarvest=2.5, farmHarvestIntervalTicks=2, millGrainPerTick=1})lua");
	CheckRejected("boolean instead of integer", R"lua(return {schema_version=1, farmGrainPerHarvest=true, farmHarvestIntervalTicks=2, millGrainPerTick=1})lua");
	CheckRejected("wrong schema version", R"lua(return {schema_version=2, farmGrainPerHarvest=2, farmHarvestIntervalTicks=2, millGrainPerTick=1})lua");
	CheckRejected("farm harvest below range", R"lua(return {schema_version=1, farmGrainPerHarvest=0, farmHarvestIntervalTicks=2, millGrainPerTick=1})lua");
	CheckRejected("farm harvest above range", R"lua(return {schema_version=1, farmGrainPerHarvest=13, farmHarvestIntervalTicks=2, millGrainPerTick=1})lua");
	CheckRejected("harvest interval below range", R"lua(return {schema_version=1, farmGrainPerHarvest=2, farmHarvestIntervalTicks=0, millGrainPerTick=1})lua");
	CheckRejected("harvest interval above range", R"lua(return {schema_version=1, farmGrainPerHarvest=2, farmHarvestIntervalTicks=10001, millGrainPerTick=1})lua");
	CheckRejected("mill throughput below range", R"lua(return {schema_version=1, farmGrainPerHarvest=2, farmHarvestIntervalTicks=2, millGrainPerTick=0})lua");
	CheckRejected("mill throughput above range", R"lua(return {schema_version=1, farmGrainPerHarvest=2, farmHarvestIntervalTicks=2, millGrainPerTick=5})lua");
	CheckRejected("instruction limit", "while true do end");
	CheckRejected("allocation limit", "local value = 'x'; while true do value = value .. value end");
	CheckRejected("syntax failure", "return {");
	CheckRejected("runtime failure", "error('deliberate')");

	const std::string oversized(65537, ' ');
	CheckRejected("oversize source", oversized);
	CheckRejected("binary chunk", std::string_view("\x1bLua", 4));
	CheckRejected("unknown ambient global", R"lua(return {schema_version=unknown_name, farmGrainPerHarvest=2, farmHarvestIntervalTicks=2, millGrainPerTick=1})lua");

	if (failures != 0)
	{
		std::cerr << failures << " Lua rule loader check(s) failed\n";
		return EXIT_FAILURE;
	}

	std::cout << "Lua rule loader checks passed\n";
	return EXIT_SUCCESS;
}
