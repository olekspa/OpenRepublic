#include "openrepublic/scripting/scene_script.h"

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
}

#include "json.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace openrepublic::scripting
{
	namespace
	{
		using Json = nlohmann::json;
		constexpr std::size_t kMemoryCap = SceneScript::MaximumMemoryBytes;
		constexpr int kHookInterval = 1000;
		constexpr int kHookCallsPerTick = 100;
		constexpr int kHookCallsOnLoad = 10000;
		constexpr std::size_t kMaximumTableDepth = 32;
		constexpr std::size_t kMaximumTableNodes = 4096;
		// Leave allocator headroom for Lua to construct and propagate its OOM error.
		constexpr std::size_t kLuaErrorReserve = 64 * 1024;
		char kContextKey;
		char kBudgetKey;
		char kEnvironmentKey;

		struct MemoryBudget { std::size_t bytes{}; };
		struct InstructionBudget { int calls{kHookCallsPerTick}; };
		struct Context
		{
			scenectl::SceneControlState* scene{};
			double time{};
			std::vector<std::string> logs;
			std::size_t logBytes{};
			std::string callbackError;
		};
		struct LuaCloser { void operator()(lua_State* state) const noexcept { if (state) lua_close(state); } };

		void* BoundedAllocator(void* user, void* pointer, std::size_t oldSize, std::size_t newSize)
		{
			auto* budget = static_cast<MemoryBudget*>(user);
			const std::size_t old = pointer ? std::min(oldSize, budget->bytes) : 0;
			if (newSize == 0) { std::free(pointer); budget->bytes -= old; return nullptr; }
			const std::size_t retained = budget->bytes - old;
		if (newSize > (kMemoryCap - kLuaErrorReserve) - retained) return nullptr;
			void* result = std::realloc(pointer, newSize);
			if (result) budget->bytes = retained + newSize;
			return result;
		}
		void Hook(lua_State* state, lua_Debug*)
		{
			lua_rawgetp(state, LUA_REGISTRYINDEX, &kBudgetKey);
			auto* budget = static_cast<InstructionBudget*>(lua_touserdata(state, -1));
			lua_pop(state, 1);
			if (!budget || --budget->calls <= 0) luaL_error(state, "scene script exceeded its instruction limit");
		}
		Context* GetContext(lua_State* state)
		{
			lua_rawgetp(state, LUA_REGISTRYINDEX, &kContextKey);
			auto* result = static_cast<Context*>(lua_touserdata(state, -1));
			lua_pop(state, 1);
			return result;
		}
		void PushLook(lua_State* state, const scenectl::SceneLook& l)
		{
			lua_createtable(state, 0, 15);
			auto number = [state](const char* key, double value) { lua_pushnumber(state, value); lua_setfield(state, -2, key); };
			auto boolean = [state](const char* key, bool value) { lua_pushboolean(state, value); lua_setfield(state, -2, key); };
			number("sunElevationDegrees", l.sunElevationDegrees); number("sunYawOffsetDegrees", l.sunYawOffsetDegrees);
			number("sunWarmthKelvin", l.sunWarmthKelvin); number("exposure", l.exposure);
			number("cloudCoverage", l.cloudCoverage); number("fogDensity", l.fogDensity);
			boolean("oceanEnabled", l.oceanEnabled); boolean("shadowsEnabled", l.shadowsEnabled);
			boolean("bloomEnabled", l.bloomEnabled); boolean("ssaoEnabled", l.ssaoEnabled);
			boolean("rtaoEnabled", l.rtaoEnabled); boolean("rayTracedReflectionsEnabled", l.rayTracedReflectionsEnabled);
			lua_createtable(state, 0, 3);
			lua_pushnumber(state, l.camera.yawDegrees); lua_setfield(state, -2, "yawDegrees");
			lua_pushnumber(state, l.camera.elevationDegrees); lua_setfield(state, -2, "elevationDegrees");
			lua_pushnumber(state, l.camera.distanceFactor); lua_setfield(state, -2, "distanceFactor");
			lua_setfield(state, -2, "camera");
		}
		int LookGet(lua_State* state)
		{
			auto* context = GetContext(state);
			if (!context || !context->scene) return luaL_error(state, "scene API is unavailable");
			PushLook(state, context->scene->CurrentLook());
			return 1;
		}
		// Returns an API error in a trivial buffer so no C++ object is alive across luaL_error's longjmp.
		bool ApplyCommand(Context* context, const Json& command, char* error, std::size_t errorSize)
		{
			try
			{
				const Json response = Json::parse(context->scene->Apply(command.dump()));
				if (response.value("ok", false)) return true;
				const std::string message = response.at("error").at("message").get<std::string>();
				std::snprintf(error, errorSize, "%s", message.c_str());
			}
			catch (...) { std::snprintf(error, errorSize, "scene API request failed"); }
			return false;
		}
		bool TableToJson(lua_State* state, int index, Json& result, char* error, std::size_t errorSize,
			std::size_t depth, std::size_t& nodes, std::vector<const void*> ancestors)
		{
			if (lua_type(state, index) != LUA_TTABLE) { std::snprintf(error,errorSize,"params must be an object"); return false; }
			if (depth > kMaximumTableDepth) { std::snprintf(error,errorSize,"scene parameters exceed the nesting limit"); return false; }
			index = lua_absindex(state, index);
			const void* identity = lua_topointer(state,index);
			if (std::find(ancestors.begin(),ancestors.end(),identity) != ancestors.end())
			{ std::snprintf(error,errorSize,"cyclic scene parameter tables are not allowed"); return false; }
			ancestors.push_back(identity);
			result = Json::object();
			lua_pushnil(state);
			while (lua_next(state, index) != 0)
			{
				if (++nodes > kMaximumTableNodes) { lua_pop(state,2); std::snprintf(error,errorSize,"scene parameters exceed the node limit"); return false; }
				if (lua_type(state,-2) != LUA_TSTRING) { lua_pop(state,2); std::snprintf(error,errorSize,"object keys must be strings"); return false; }
				const char* key = lua_tostring(state,-2);
				if (lua_type(state,-1) == LUA_TBOOLEAN) result[key] = lua_toboolean(state,-1) != 0;
				else if (lua_type(state,-1) == LUA_TNUMBER)
				{
					const double value = lua_tonumber(state,-1);
					if (!std::isfinite(value)) { lua_pop(state,2); std::snprintf(error,errorSize,"numeric fields must be finite"); return false; }
					result[key] = value;
				}
				else if (lua_type(state,-1) == LUA_TTABLE)
				{
					Json nested;
					if (!TableToJson(state,-1,nested,error,errorSize,depth+1,nodes,ancestors)) { lua_pop(state,2); return false; }
					result[key] = std::move(nested);
				}
				else { lua_pop(state,2); std::snprintf(error,errorSize,"fields must be numbers, booleans, or nested objects"); return false; }
				lua_pop(state,1);
			}
			return true;
		}
		enum class Setter { Look, Camera, Preset };
		bool SetImpl(lua_State* state, Context* context, Setter setter)
		{
			try
			{
				if (!context || !context->scene) return false;
				char error[512]{};
				Json params;
				if (setter == Setter::Preset)
				{
					if (lua_type(state,1) != LUA_TSTRING) { context->callbackError = "name must be a string"; return false; }
					params = Json{{"name",lua_tostring(state,1)}};
				}
				else
				{
					std::size_t nodes=0;
					if (!TableToJson(state,1,params,error,sizeof(error),0,nodes,{})) { context->callbackError = error; return false; }
				}
				const char* method = setter == Setter::Look ? "look.set" : setter == Setter::Camera ? "camera.set" : "look.preset";
				const bool applied = ApplyCommand(context, Json{{"id","lua"},{"protocolVersion",0},{"method",method},{"params",std::move(params)}},error,sizeof(error));
				if (!applied) context->callbackError = error;
				return applied;
			}
			catch (...)
			{
				if (context) context->callbackError = "scene API request failed";
				return false;
			}
		}
		int Set(lua_State* state)
		{
			auto* context = GetContext(state);
			const auto setter = static_cast<Setter>(lua_tointeger(state, lua_upvalueindex(1)));
			if (context) context->callbackError.clear();
			const bool success = context && SetImpl(state,context,setter);
			if (!success) return luaL_error(state,"%s",context && !context->callbackError.empty() ? context->callbackError.c_str() : "scene API is unavailable");
			return 0;
		}
		int Log(lua_State* state)
		{
			if (lua_type(state,1) != LUA_TSTRING) return luaL_error(state,"or.log expects a string");
			auto* context = GetContext(state);
			std::size_t length{}; const char* value = lua_tolstring(state,1,&length);
			if (context && context->logs.size() < SceneScript::MaximumLogMessages && length <= SceneScript::MaximumLogMessageBytes &&
				context->logBytes + length <= SceneScript::MaximumLogMessages * SceneScript::MaximumLogMessageBytes)
			{ try { context->logs.emplace_back(value,length); context->logBytes += length; } catch (...) {} }
			return 0;
		}
	}

	struct SceneScript::Impl
	{
		MemoryBudget memory;
		Context context;
		bool enabled{};
		std::string error;
		double time{};
		// Keep allocator/context storage alive until lua_close has freed the state.
		std::unique_ptr<lua_State,LuaCloser> state;
	};

	SceneScript::SceneScript() : impl_(std::make_unique<Impl>()) {}
	SceneScript::~SceneScript() = default;
	SceneScript::SceneScript(SceneScript&&) noexcept = default;
	SceneScript& SceneScript::operator=(SceneScript&&) noexcept = default;
	bool SceneScript::Enabled() const noexcept { return impl_ && impl_->enabled; }
	const std::string& SceneScript::LastError() const noexcept { static const std::string empty; return impl_ ? impl_->error : empty; }
	std::string SceneScript::TakeLogs()
	{
		if (!impl_) return {};
		std::string out;
		for (const auto& line : impl_->context.logs) { if (!out.empty()) out.push_back('\n'); out += line; }
		impl_->context.logs.clear(); impl_->context.logBytes = 0; return out;
	}
	bool SceneScript::LoadFile(const std::string& path, std::string* error)
	{
		std::ifstream file(path,std::ios::binary);
		if (!file) { if (error) *error = "unable to open scene script file"; return false; }
		std::string source(MaximumSourceBytes+1,'\0'); file.read(source.data(),static_cast<std::streamsize>(source.size()));
		const auto n=file.gcount(); if (n < 0 || static_cast<std::size_t>(n)>MaximumSourceBytes) { if(error)*error="scene script source exceeds the 65536-byte limit"; return false; }
		source.resize(static_cast<std::size_t>(n)); return LoadText(source,error);
	}
	bool SceneScript::LoadText(std::string_view source, std::string* error)
	{
		if(error) error->clear();
		if(!impl_) impl_=std::make_unique<Impl>();
		impl_->enabled=false; impl_->error.clear(); impl_->time=0; impl_->context={};
		if(source.size()>MaximumSourceBytes) { if(error)*error=impl_->error="scene script source exceeds the 65536-byte limit"; return false; }
		impl_->state.reset(lua_newstate(BoundedAllocator,&impl_->memory));
		if(!impl_->state) { if(error)*error=impl_->error="unable to allocate an isolated Lua state"; return false; }
		lua_State* state=impl_->state.get();
		lua_pushlightuserdata(state,&impl_->context); lua_rawsetp(state,LUA_REGISTRYINDEX,&kContextKey);
		const int status=luaL_loadbufferx(state,source.data(),source.size(),"OpenRepublic scene script","t");
		if(status!=LUA_OK) { const char* message=lua_tostring(state,-1); if(error)*error=impl_->error=message?message:"unable to compile scene script"; return false; }
		// Bind a fresh environment with only the explicit `or` API table.
		lua_newtable(state);
		lua_newtable(state);
		lua_newtable(state); lua_pushcfunction(state,LookGet); lua_setfield(state,-2,"get");
		for (const auto& entry : std::array<std::pair<const char*,Setter>,2>{{{"set",Setter::Look},{"preset",Setter::Preset}}})
		{ lua_pushinteger(state,static_cast<lua_Integer>(entry.second)); lua_pushcclosure(state,Set,1); lua_setfield(state,-2,entry.first); }
		lua_setfield(state,-2,"look");
		lua_newtable(state); lua_pushinteger(state,static_cast<lua_Integer>(Setter::Camera)); lua_pushcclosure(state,Set,1); lua_setfield(state,-2,"set"); lua_setfield(state,-2,"camera");
		lua_pushcfunction(state,Log); lua_setfield(state,-2,"log");
		lua_setfield(state,-2,"or");
		lua_pushvalue(state,-1); lua_rawsetp(state,LUA_REGISTRYINDEX,&kEnvironmentKey);
		if(lua_setupvalue(state,-2,1)==nullptr) { if(error)*error=impl_->error="compiled scene script has no _ENV"; return false; }
		InstructionBudget budget; budget.calls=kHookCallsOnLoad;
		lua_pushlightuserdata(state,&budget); lua_rawsetp(state,LUA_REGISTRYINDEX,&kBudgetKey); lua_sethook(state,Hook,LUA_MASKCOUNT,kHookInterval);
		const int run=lua_pcall(state,0,0,0); lua_sethook(state,nullptr,0,0);
		if(run!=LUA_OK) { const char* message=lua_tostring(state,-1); if(error)*error=impl_->error=message?message:"unable to initialize scene script"; return false; }
		lua_rawgetp(state,LUA_REGISTRYINDEX,&kEnvironmentKey); lua_getfield(state,-1,"on_frame"); lua_remove(state,-2);
		const bool callback=lua_isfunction(state,-1); lua_pop(state,1);
		if(!callback) { if(error)*error=impl_->error="scene script must define function on_frame(dt)"; return false; }
		impl_->enabled=true; return true;
	}
	bool SceneScript::Tick(double dt, scenectl::SceneControlState& scene)
	{
		if(!impl_ || !impl_->enabled || !std::isfinite(dt) || dt<0) return false;
		if (dt > std::numeric_limits<double>::max() - impl_->time)
		{
			impl_->error="dt would overflow scene script time"; impl_->enabled=false; return false;
		}
		impl_->context.scene=&scene;
		impl_->time += dt;
		lua_State* state=impl_->state.get();
		lua_rawgetp(state,LUA_REGISTRYINDEX,&kEnvironmentKey); lua_getfield(state,-1,"or");
		lua_pushnumber(state,impl_->time); lua_setfield(state,-2,"time"); lua_pop(state,1);
		lua_getfield(state,-1,"on_frame"); lua_remove(state,-2); lua_pushnumber(state,dt);
		InstructionBudget budget; lua_pushlightuserdata(state,&budget); lua_rawsetp(state,LUA_REGISTRYINDEX,&kBudgetKey); lua_sethook(state,Hook,LUA_MASKCOUNT,kHookInterval);
		const int status=lua_pcall(state,1,0,0); lua_sethook(state,nullptr,0,0);
		if(status==LUA_OK) return true;
		const char* message=lua_tostring(state,-1); impl_->error=message?message:"scene script frame failed"; impl_->enabled=false; lua_pop(state,1); return false;
	}
}
