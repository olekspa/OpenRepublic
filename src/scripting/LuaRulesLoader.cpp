#include "openrepublic/scripting/lua_rules_loader.h"

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
}

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <limits>
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

	bool LuaRulesLoader::LoadText(std::string_view text, sim::SimulationRules& out, std::string* error)
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

		sim::SimulationRules candidate = out;
		if (!ValidateRules(state.get(), candidate, error))
		{
			return false;
		}

		out = candidate;
		return true;
	}
}
