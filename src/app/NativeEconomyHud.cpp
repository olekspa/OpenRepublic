#include "NativeEconomyHud.h"
#include "openrepublic/runtime/panel_layout.h"

#include <algorithm>
#include <sstream>

namespace openrepublic::preview
{
	void NativeEconomyHud::Create(wi::gui::GUI& gui, std::function<void()> toggleOverview)
	{
		topBar_.Create("NativeEconomyHudTopBar");
		topBar_.SetSize(XMFLOAT2(1150, 34));
		topBar_.SetColor(wi::Color(20, 29, 38, 232));
		topBar_.font.params.color = wi::Color(245, 248, 250, 255);
		topBar_.margin_left = 10;
		topBar_.margin_top = 7;
		gui.AddWidget(&topBar_);
		resourceTicker_.Create("NativeEconomyHudResourceTicker");
		resourceTicker_.SetSize(XMFLOAT2(1150, 42));
		resourceTicker_.SetColor(wi::Color(20, 29, 38, 232));
		resourceTicker_.font.params.color = wi::Color(245, 248, 250, 255);
		resourceTicker_.margin_left = 10;
		resourceTicker_.margin_top = 7;
		gui.AddWidget(&resourceTicker_);
		eventLog_.Create("NativeEconomyHudEventLog");
		eventLog_.SetSize(XMFLOAT2(920, 150));
		eventLog_.SetColor(wi::Color(20, 29, 38, 232));
		eventLog_.font.params.color = wi::Color(245, 248, 250, 255);
		eventLog_.margin_left = 10;
		eventLog_.margin_top = 8;
		eventLog_.scrollbar.SetEnabled(true);
		gui.AddWidget(&eventLog_);
		overviewButton_.Create("NativeEconomyHudOverviewButton");
		overviewButton_.SetText("Overview map");
		overviewButton_.SetTooltip("Toggle a top-down view of the native economy world.");
		overviewButton_.SetSize(XMFLOAT2(150, 30));
		overviewButton_.SetColor(wi::Color(36, 60, 80, 255), wi::gui::IDLE);
		overviewButton_.SetColor(wi::Color(52, 88, 116, 255), wi::gui::FOCUS);
		overviewButton_.SetColor(wi::Color(28, 46, 62, 255), wi::gui::ACTIVE);
		overviewButton_.font.params.color = wi::Color(242, 247, 255, 255);
		overviewButton_.OnClick([toggleOverview](const wi::gui::EventArgs&) { if (toggleOverview) toggleOverview(); });
		gui.AddWidget(&overviewButton_);
		created_ = true;
	}

	void NativeEconomyHud::Update(const economy::WorldSnapshot& snapshot, const economy::Catalog& catalog,
		bool running, std::uint32_t speed, const economy::UtilityDiagnostics& utilities,
		const std::string& hoveredObject)
	{
		if (!created_) return;
		const auto summary = runtime::BuildHudSummary(snapshot, catalog);
		std::ostringstream top;
		top << "Treasury " << summary.treasury << "   |   Tick " << summary.tick;
		if (summary.hasCalendar)
			top << "   |   Year " << summary.calendarYear << " day " << summary.calendarDay
				<< (summary.calendarSeason.empty() ? "" : " " + summary.calendarSeason);
		if (summary.populationAvailable) top << "   |   Residents " << summary.population << "   |   Happy " << summary.happyResidents << " (" << summary.happiness << "%)";
		top << "   |   " << (running ? "Running" : "Paused") << "   |   speed x" << speed;
		topBar_.SetText(top.str());

		std::ostringstream stocks;
		stocks << "Stored: ";
		for (std::size_t i = 0; i < summary.storedResources.size(); ++i)
		{
			if (i) stocks << "   |   ";
			stocks << summary.storedResources[i].first << " " << summary.storedResources[i].second;
		}
		resourceTicker_.SetText(stocks.str());
		if (utilities.available && !utilities.networks.empty())
		{
			stocks << "\nUtilities: ";
			for (std::size_t i = 0; i < utilities.networks.size(); ++i)
			{
				const auto& network = utilities.networks[i];
				const auto* resource = catalog.Resource(network.resource);
				if (i) stocks << "   |   ";
				stocks << (resource ? resource->name : "utility") << " used "
					<< economy::FormatUtilityAmount(network.consumed) << "/"
					<< economy::FormatUtilityAmount(network.requested) << " requested, generated "
					<< economy::FormatUtilityAmount(network.generated) << ", unused "
					<< economy::FormatUtilityAmount(network.unused);
			}
			resourceTicker_.SetText(stocks.str());
		}

		const auto added = events_.Observe(snapshot, catalog);
		(void)added;
		std::ostringstream log;
		log << "Recent events";
		if (events_.Events().empty()) log << "\nNo notable snapshot changes yet.";
		for (const auto& event : events_.Events()) log << "\n[T" << event.tick << "] " << event.text;
		if (!hoveredObject.empty()) log << "\nHover: " << hoveredObject;
		eventLog_.SetText(log.str());
	}

	void NativeEconomyHud::Layout(float width, float height, float top, bool modalPanelOpen)
	{
		if (!created_) return;
		eventLog_.SetVisible(!modalPanelOpen);
		const float available = std::max(240.0f, width - 36.0f);
		const float topBarWidth = std::min(620.0f, std::max(80.0f, available - 158.0f));
		const float logWidth = std::min(300.0f, std::max(200.0f, width * 0.28f));
		const auto layout = runtime::LayoutPanels(width, height, 1.0f, {
			{"toolbar-status", {18.0f, top, topBarWidth, 32.0f}, runtime::PanelDock::Top, 100, true, false},
			{"overview-button", {18.0f + topBarWidth + 8.0f, top, 150.0f, 30.0f}, runtime::PanelDock::Top, 95, true, false},
			{"resources", {18.0f, top + 34.0f, std::min(620.0f, available), 48.0f}, runtime::PanelDock::Top, 90, true, true},
			{"events", {std::max(18.0f, width - logWidth - 18.0f), std::max(top + 90.0f, height - 170.0f), logWidth, 150.0f}, runtime::PanelDock::Bottom, 50, !modalPanelOpen, true},
		});
		for (const auto& panel : layout.panels) if (panel.visible) {
			const XMFLOAT2 pos(panel.rect.x, panel.rect.y), size(panel.rect.width, panel.rect.height);
			if (panel.id == "toolbar-status") { topBar_.SetPos(pos); topBar_.SetSize(size); }
			else if (panel.id == "overview-button") overviewButton_.SetPos(pos);
			else if (panel.id == "resources") { resourceTicker_.SetPos(pos); resourceTicker_.SetSize(size); }
			else if (panel.id == "events") { eventLog_.SetPos(pos); eventLog_.SetSize(size); }
		}
	}
}
