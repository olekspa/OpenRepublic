#include "NativeEconomyHud.h"

#include <algorithm>
#include <sstream>

namespace openrepublic::preview
{
	void NativeEconomyHud::Create(wi::gui::GUI& gui, std::function<void()> toggleOverview)
	{
		topBar_.Create("NativeEconomyHudTopBar");
		topBar_.SetSize(XMFLOAT2(1150, 34));
		gui.AddWidget(&topBar_);
		resourceTicker_.Create("NativeEconomyHudResourceTicker");
		resourceTicker_.SetSize(XMFLOAT2(1150, 42));
		gui.AddWidget(&resourceTicker_);
		eventLog_.Create("NativeEconomyHudEventLog");
		eventLog_.SetSize(XMFLOAT2(920, 150));
		eventLog_.scrollbar.SetEnabled(true);
		gui.AddWidget(&eventLog_);
		overviewButton_.Create("NativeEconomyHudOverviewButton");
		overviewButton_.SetText("Overview map");
		overviewButton_.SetTooltip("Toggle a top-down view of the native economy world.");
		overviewButton_.SetSize(XMFLOAT2(150, 30));
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

	void NativeEconomyHud::Layout(float width, float height, float top)
	{
		if (!created_) return;
		const float w = std::max(240.0f, width - 36.0f);
		topBar_.SetPos(XMFLOAT2(18, top)); topBar_.SetSize(XMFLOAT2(w - 166.0f, 32));
		overviewButton_.SetPos(XMFLOAT2(std::max(18.0f, width - 168.0f), top));
		resourceTicker_.SetPos(XMFLOAT2(18, top + 34.0f)); resourceTicker_.SetSize(XMFLOAT2(w, 48));
		const float logWidth = std::min(500.0f, std::max(300.0f, width - 36.0f));
		eventLog_.SetPos(XMFLOAT2(std::max(18.0f, width - logWidth - 18.0f), std::max(top + 90.0f, height - 170.0f)));
		eventLog_.SetSize(XMFLOAT2(logWidth, 150));
	}
}
