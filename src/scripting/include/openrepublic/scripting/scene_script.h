#pragma once

#include "openrepublic/scene_control.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace openrepublic::scripting
{
	// A persistent, isolated Lua scene script. The environment contains only `or`;
	// each Tick runs on_frame(dt) with a fresh bounded instruction budget.
	class SceneScript final
	{
	public:
		static constexpr std::size_t MaximumSourceBytes = 64 * 1024;
		static constexpr std::size_t MaximumMemoryBytes = 8 * 1024 * 1024;
		static constexpr std::size_t MaximumLogMessages = 64;
		static constexpr std::size_t MaximumLogMessageBytes = 256;

		SceneScript();
		~SceneScript();
		SceneScript(SceneScript&&) noexcept;
		SceneScript& operator=(SceneScript&&) noexcept;
		SceneScript(const SceneScript&) = delete;
		SceneScript& operator=(const SceneScript&) = delete;

		bool LoadText(std::string_view source, std::string* error = nullptr);
		bool LoadFile(const std::string& path, std::string* error = nullptr);
		// A failed frame disables the script. The error is retained by LastError().
		bool Tick(double dt, scenectl::SceneControlState& scene);
		bool Enabled() const noexcept;
		const std::string& LastError() const noexcept;
		// Returns and clears bounded log output emitted by `or.log`.
		std::string TakeLogs();

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}
