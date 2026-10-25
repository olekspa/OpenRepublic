#include "openrepublic/scripting/lua_rules_loader.h"
#include "openrepublic/scripting/economy_rules.h"

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
}

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <utility>

namespace openrepublic::scripting
{
	namespace
	{
		constexpr std::size_t kMaximumSourceBytes = 64 * 1024;
		constexpr std::size_t kMaximumLuaMemoryBytes = 8 * 1024 * 1024;
		constexpr int kHookInterval = 1000;
		constexpr int kMaximumHookCalls = 100;
		char kInstructionBudgetRegistryKey;

		struct LuaStateDeleter
		{
			void operator()(lua_State* state) const noexcept
			{
				if (state != nullptr)
				{
					lua_close(state);
				}
			}
		};

		class StackGuard final
		{
		public:
			explicit StackGuard(lua_State* state) noexcept : state_(state), top_(lua_gettop(state)) {}
			~StackGuard() { lua_settop(state_, top_); }
			StackGuard(const StackGuard&) = delete;
			StackGuard& operator=(const StackGuard&) = delete;

		private:
			lua_State* state_;
			int top_;
		};

		struct LuaMemoryBudget
		{
			std::size_t allocatedBytes{};
		};

		void* BoundedLuaAllocator(void* userData, void* pointer, std::size_t oldSize, std::size_t newSize)
		{
			auto* budget = static_cast<LuaMemoryBudget*>(userData);
			const std::size_t trackedOldSize = pointer == nullptr ? 0 : std::min(oldSize, budget->allocatedBytes);
			if (newSize == 0)
			{
				std::free(pointer);
				budget->allocatedBytes -= trackedOldSize;
				return nullptr;
			}

			const std::size_t retainedBytes = budget->allocatedBytes - trackedOldSize;
			if (newSize > kMaximumLuaMemoryBytes - retainedBytes)
			{
				return nullptr;
			}

			void* replacement = std::realloc(pointer, newSize);
			if (replacement != nullptr)
			{
				budget->allocatedBytes = retainedBytes + newSize;
			}
			return replacement;
		}

		struct InstructionBudget
		{
			int hookCallsRemaining{kMaximumHookCalls};
		};

		void SetError(std::string* error, std::string message)
		{
			if (error != nullptr)
			{
				*error = std::move(message);
			}
		}

		void InstructionLimitHook(lua_State* state, lua_Debug*)
		{
			lua_rawgetp(state, LUA_REGISTRYINDEX, &kInstructionBudgetRegistryKey);
			auto* budget = static_cast<InstructionBudget*>(lua_touserdata(state, -1));
			lua_pop(state, 1);
			if (budget == nullptr || --budget->hookCallsRemaining <= 0)
			{
				luaL_error(state, "Lua rule script exceeded its instruction limit");
			}
		}

		bool Reject(std::string* error, const std::string& message)
		{
			SetError(error, message);
			return false;
		}

		std::string LuaErrorMessage(lua_State* state, const char* fallback)
		{
			size_t length = 0;
			const char* message = lua_tolstring(state, -1, &length);
			return message != nullptr ? std::string(message, length) : std::string(fallback);
		}

		bool ReadIntegerField(lua_State* state, int tableIndex, const char* name,
			lua_Integer minimum, lua_Integer maximum, std::uint32_t& value, std::string* error)
		{
			tableIndex = lua_absindex(state, tableIndex);
			lua_pushstring(state, name);
			lua_rawget(state, tableIndex);
			if (!lua_isinteger(state, -1))
			{
				const bool present = !lua_isnil(state, -1);
				lua_pop(state, 1);
				return Reject(error, std::string("rule field '") + name + (present ? "' must be an integer" : "' is required"));
			}

			const lua_Integer integer = lua_tointeger(state, -1);
			lua_pop(state, 1);
			if (integer < minimum || integer > maximum)
			{
				std::ostringstream message;
				message << "rule field '" << name << "' must be between " << minimum << " and " << maximum;
				return Reject(error, message.str());
			}

			value = static_cast<std::uint32_t>(integer);
			return true;
		}

		bool ValidateRules(lua_State* state, sim::SimulationRules& candidate, std::string* error)
		{
			if (lua_type(state, -1) != LUA_TTABLE)
			{
				return Reject(error, "Lua rule script must return a table");
			}

			const int tableIndex = lua_absindex(state, -1);
			std::array<bool, 4> found{};
			std::size_t fieldCount = 0;
			lua_pushnil(state);
			while (lua_next(state, tableIndex) != 0)
			{
				++fieldCount;
				if (lua_type(state, -2) != LUA_TSTRING)
				{
					lua_pop(state, 2);
					return Reject(error, "Lua rule table keys must be schema field names");
				}

				size_t keyLength = 0;
				const char* key = lua_tolstring(state, -2, &keyLength);
				if (key == nullptr)
				{
					lua_pop(state, 2);
					return Reject(error, "Lua rule table contains an invalid field name");
				}

				constexpr std::array<const char*, 4> fieldNames = {
					"schema_version", "farmGrainPerHarvest", "farmHarvestIntervalTicks", "millGrainPerTick"
				};
				bool recognized = false;
				for (std::size_t i = 0; i < fieldNames.size(); ++i)
				{
					if (std::string_view(fieldNames[i]) == std::string_view(key, keyLength))
					{
						found[i] = true;
						recognized = true;
						break;
					}
				}
				if (!recognized)
				{
					lua_pop(state, 2);
					return Reject(error, std::string("unknown Lua rule field '") + key + "'");
				}
				lua_pop(state, 1);
			}

			if (fieldCount != found.size())
			{
				return Reject(error, "Lua rule table must contain exactly schema_version and the three supported rule fields");
			}

			std::uint32_t schemaVersion = 0;
			if (!ReadIntegerField(state, tableIndex, "schema_version", 1, 1, schemaVersion, error) ||
				!ReadIntegerField(state, tableIndex, "farmGrainPerHarvest", 1, 12, candidate.farmGrainPerHarvest, error) ||
				!ReadIntegerField(state, tableIndex, "farmHarvestIntervalTicks", 1, 10000, candidate.farmHarvestIntervalTicks, error) ||
				!ReadIntegerField(state, tableIndex, "millGrainPerTick", 1, 4, candidate.millGrainPerTick, error))
			{
				return false;
			}
			return true;
		}
	}

	bool LuaRulesLoader::LoadFile(const std::filesystem::path& path, sim::SimulationRules& out, std::string* error)
	{
		if (error != nullptr)
		{
			error->clear();
		}

		std::ifstream file(path, std::ios::binary);
		if (!file)
		{
			return Reject(error, "unable to open Lua rules file: " + path.string());
		}

		std::string source(kMaximumSourceBytes + 1, '\0');
		file.read(source.data(), static_cast<std::streamsize>(source.size()));
		const std::streamsize bytesRead = file.gcount();
		if (bytesRead < 0 || static_cast<std::size_t>(bytesRead) > kMaximumSourceBytes)
		{
			return Reject(error, "Lua rule source exceeds the 65536-byte limit");
		}
		if (!file.eof() && file.fail())
		{
			return Reject(error, "failed while reading Lua rules file: " + path.string());
		}
		source.resize(static_cast<std::size_t>(bytesRead));
		return LoadText(source, out, error);
	}

	bool RunSandboxedRules(std::string_view text, std::string* error, const std::function<bool(lua_State*)>& consume)
	{
		if (error != nullptr)
		{
			error->clear();
		}
		if (text.size() > kMaximumSourceBytes)
		{
			return Reject(error, "Lua rule source exceeds the 65536-byte limit");
		}

		LuaMemoryBudget memoryBudget;
		std::unique_ptr<lua_State, LuaStateDeleter> state(lua_newstate(BoundedLuaAllocator, &memoryBudget));
		if (state == nullptr)
		{
			return Reject(error, "unable to allocate an isolated Lua state");
		}
		StackGuard stackGuard(state.get());

		const int loadStatus = luaL_loadbufferx(state.get(), text.data(), text.size(), "OpenRepublic rules", "t");
		if (loadStatus != LUA_OK)
		{
			return Reject(error, "unable to compile Lua rules: " + LuaErrorMessage(state.get(), "unknown Lua compiler error"));
		}

		// Replace the chunk's _ENV with a fresh table. No libraries or ambient globals are exposed.
		lua_newtable(state.get());
		if (lua_setupvalue(state.get(), -2, 1) == nullptr)
		{
			return Reject(error, "compiled Lua rules do not have an _ENV upvalue");
		}

		InstructionBudget budget;
		lua_pushlightuserdata(state.get(), &budget);
		lua_rawsetp(state.get(), LUA_REGISTRYINDEX, &kInstructionBudgetRegistryKey);
		lua_sethook(state.get(), InstructionLimitHook, LUA_MASKCOUNT, kHookInterval);

		const int callStatus = lua_pcall(state.get(), 0, 1, 0);
		lua_sethook(state.get(), nullptr, 0, 0);
		if (callStatus != LUA_OK)
		{
			return Reject(error, "unable to execute Lua rules: " + LuaErrorMessage(state.get(), "unknown Lua runtime error"));
		}
		return consume(state.get());
	}

	bool LuaRulesLoader::LoadText(std::string_view text, sim::SimulationRules& out, std::string* error)
	{
		return RunSandboxedRules(text, error, [&](lua_State* state) {
			sim::SimulationRules candidate = out;
			if (!ValidateRules(state, candidate, error))
			{
				return false;
			}
			out = candidate;
			return true;
		});
	}
	namespace
	{
		struct BuildingTuning
		{
			std::uint32_t speedPercent = 100;
			std::uint32_t storagePercent = 100;
		};
		struct VehicleTuning
		{
			std::uint32_t capacityPercent = 100;
			std::uint32_t speedPercent = 100;
		};

		using TuningValues = std::array<std::uint32_t, 2>;

		// Reads a table of name -> { field = integer percent }; every field must be in 10..1000.
		bool ReadTuningTable(lua_State* state, const char* tableName, const std::array<const char*, 2>& fieldNames,
			const std::function<bool(const std::string&)>& knownName,
			const std::function<void(const std::string&, const TuningValues&)>& store, std::string* error)
		{
			lua_getfield(state, -1, tableName);
			if (lua_isnil(state, -1))
			{
				lua_pop(state, 1);
				return true;
			}
			if (lua_type(state, -1) != LUA_TTABLE)
			{
				return Reject(error, std::string("'") + tableName + "' must be a table");
			}
			const int tableIndex = lua_absindex(state, -1);
			lua_pushnil(state);
			while (lua_next(state, tableIndex) != 0)
			{
				if (lua_type(state, -2) != LUA_TSTRING || lua_type(state, -1) != LUA_TTABLE)
				{
					return Reject(error, std::string("'") + tableName + "' entries must be name = { fields }");
				}
				const std::string name = lua_tostring(state, -2);
				if (!knownName(name))
				{
					return Reject(error, std::string("unknown ") + tableName + " entry '" + name + "'");
				}
				TuningValues values{ 100, 100 };
				const int entryIndex = lua_absindex(state, -1);
				lua_pushnil(state);
				while (lua_next(state, entryIndex) != 0)
				{
					if (lua_type(state, -2) != LUA_TSTRING)
					{
						return Reject(error, "tuning field names must be strings");
					}
					const std::string field = lua_tostring(state, -2);
					std::size_t slot = fieldNames.size();
					for (std::size_t i = 0; i < fieldNames.size(); ++i)
					{
						if (field == fieldNames[i])
						{
							slot = i;
						}
					}
					if (slot == fieldNames.size())
					{
						return Reject(error, "unknown tuning field '" + field + "' on " + name);
					}
					if (!lua_isinteger(state, -1) || lua_tointeger(state, -1) < 10 || lua_tointeger(state, -1) > 1000)
					{
						return Reject(error, name + "." + field + " must be an integer percent between 10 and 1000");
					}
					values[slot] = static_cast<std::uint32_t>(lua_tointeger(state, -1));
					lua_pop(state, 1);
				}
				store(name, values);
				lua_pop(state, 1);
			}
			lua_pop(state, 1);
			return true;
		}
	}

	bool EconomyRules::ApplyText(std::string_view text, const economy::Catalog& base, economy::Catalog& out, std::string* error)
	{
		if (error != nullptr)
		{
			error->clear();
		}
		std::map<std::string, BuildingTuning> buildingTuning;
		std::map<std::string, VehicleTuning> vehicleTuning;
		const bool parsed = RunSandboxedRules(text, error, [&](lua_State* state) {
			if (lua_type(state, -1) != LUA_TTABLE)
			{
				return Reject(error, "Lua rule script must return a table");
			}
			lua_getfield(state, -1, "schema_version");
			if (!lua_isinteger(state, -1) || lua_tointeger(state, -1) != 1)
			{
				return Reject(error, "rule field 'schema_version' must be the integer 1");
			}
			lua_pop(state, 1);
			lua_pushnil(state);
			while (lua_next(state, -2) != 0)
			{
				const char* key = lua_type(state, -2) == LUA_TSTRING ? lua_tostring(state, -2) : nullptr;
				if (key == nullptr || (std::string(key) != "schema_version" && std::string(key) != "buildings" &&
					std::string(key) != "vehicles"))
				{
					return Reject(error, "unknown top-level rule field");
				}
				lua_pop(state, 1);
			}
			const std::array<const char*, 2> buildingFields = { "speed_percent", "storage_percent" };
			const std::array<const char*, 2> vehicleFields = { "capacity_percent", "speed_percent" };
			return ReadTuningTable(state, "buildings", buildingFields,
					[&](const std::string& n) { economy::BuildingTypeId id; return base.FindBuildingType(n, id); },
					[&](const std::string& n, const TuningValues& v) { buildingTuning[n] = { v[0], v[1] }; }, error) &&
				ReadTuningTable(state, "vehicles", vehicleFields,
					[&](const std::string& n) { economy::VehicleTypeId id; return base.FindVehicleType(n, id); },
					[&](const std::string& n, const TuningValues& v) { vehicleTuning[n] = { v[0], v[1] }; }, error);
		});
		if (!parsed)
		{
			return false;
		}

		economy::CatalogBuilder builder;
		economy::CatalogErrorInfo info;
		auto failed = [&]() { return Reject(error, "catalog rebuild failed: " + info.message); };
		for (const auto& r : base.Resources())
		{
			economy::ResourceId id;
			if (!(r.utility ? builder.AddUtilityResource(r.name, &id, info) : builder.AddResource(r.name, &id, info)))
			{
				return failed();
			}
			if (r.tradeDeclared && !builder.AddTradePrice({ id, r.buyPrice, r.sellPrice }, info))
			{
				return failed();
			}
		}
		if (base.Food().resource != economy::InvalidResourceId &&
			!builder.SetFood(base.Food().resource, base.Food().amount, base.Food().intervalTicks, info))
		{
			return failed();
		}
		if (base.CitizensEnabled() && !builder.EnableCitizens(info))
		{
			return failed();
		}
		for (const auto& t : base.BuildingTypes())
		{
			const auto found = buildingTuning.find(t.name);
			const BuildingTuning tuning = found != buildingTuning.end() ? found->second : BuildingTuning{};
			economy::BuildingTypeId id;
			if (!builder.AddBuildingType(t.name, t.width, t.depth, &id, info))
			{
				return failed();
			}
			if ((t.housing && !builder.SetHousing(id, t.housing, info)) ||
				(t.workers && !builder.SetWorkers(id, t.workers, info)) ||
				(t.tradePost && !builder.SetTradePost(id, true, info)))
			{
				return failed();
			}
			for (const auto& s : t.storage)
			{
				const std::uint64_t capacity = std::max<std::uint64_t>(1, s.capacity * tuning.storagePercent / 100);
				if (!builder.AddStorage(id, s.resource, capacity, info))
				{
					return failed();
				}
			}
			for (auto recipe : t.recipes)
			{
				recipe.intervalTicks = static_cast<std::uint32_t>(std::max<std::uint64_t>(
					1, static_cast<std::uint64_t>(recipe.intervalTicks) * 100 / tuning.speedPercent));
				if (!builder.AddRecipe(id, recipe, info))
				{
					return failed();
				}
			}
		}
		for (const auto& v : base.VehicleTypes())
		{
			const auto found = vehicleTuning.find(v.name);
			const VehicleTuning tuning = found != vehicleTuning.end() ? found->second : VehicleTuning{};
			economy::VehicleTypeId id;
			const std::uint64_t capacity = std::max<std::uint64_t>(1, v.capacity * tuning.capacityPercent / 100);
			const std::uint32_t speed = static_cast<std::uint32_t>(
				std::max<std::uint64_t>(1, static_cast<std::uint64_t>(v.cellsPerTick) * tuning.speedPercent / 100));
			if (!builder.AddVehicleType(v.name, capacity, speed, &id, info))
			{
				return failed();
			}
		}
		economy::Catalog rebuilt;
		if (!builder.Build(rebuilt, info))
		{
			return failed();
		}
		out = std::move(rebuilt);
		return true;
	}

	bool EconomyRules::ApplyFile(const std::filesystem::path& path, const economy::Catalog& base, economy::Catalog& out, std::string* error)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
		{
			return Reject(error, "unable to open Lua rules file: " + path.string());
		}
		std::string source(kMaximumSourceBytes + 1, '\0');
		file.read(source.data(), static_cast<std::streamsize>(source.size()));
		const std::streamsize bytesRead = file.gcount();
		if (bytesRead < 0 || static_cast<std::size_t>(bytesRead) > kMaximumSourceBytes)
		{
			return Reject(error, "Lua rule source exceeds the 65536-byte limit");
		}
		source.resize(static_cast<std::size_t>(bytesRead));
		return ApplyText(source, base, out, error);
	}
}
