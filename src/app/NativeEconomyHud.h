#pragma once

#include "WickedEngine.h"
#include "openrepublic/runtime/native_economy_hud.h"

#include <functional>
#include <string>

namespace openrepublic::preview
{
	class NativeEconomyHud final
	{
	public:
		void Create(wi::gui::GUI& gui, std::function<void()> toggleOverview);
		void Update(const economy::WorldSnapshot& snapshot, const economy::Catalog& catalog,
			bool running, std::uint32_t speed, const economy::UtilityDiagnostics& utilities,
			const std::string& hoveredObject);
		void Layout(float width, float height, float top = 96.0f);
		void SetOverviewActive(bool active) { overviewButton_.SetText(active ? "Return to map view" : "Overview map"); }
		void ResetHistory() noexcept { events_.Reset(); }
	private:
		wi::gui::Label topBar_;
		wi::gui::Label resourceTicker_;
		wi::gui::Label eventLog_;
		wi::gui::Button overviewButton_;
		runtime::HudEventHistory events_{8};
		bool created_{};
	};
}
