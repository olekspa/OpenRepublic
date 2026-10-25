#pragma once

#include "openrepublic/economy/economy.h"

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace openrepublic::runtime
{
	enum class HudEventKind { ConstructionCompleted, Shortage, Delivery, BorderTrade, Immigration, PopulationChanged };
	struct HudEvent { std::uint64_t tick{}; HudEventKind kind{}; std::string text; };
	struct HudSummary
	{
		std::uint64_t tick{}, treasury{}, population{}, happyResidents{};
		std::uint32_t happiness{}, calendarYear{}, calendarDay{};
		std::string calendarSeason;
		bool hasCalendar{}, populationAvailable{};
		std::vector<std::pair<std::string, std::uint64_t>> storedResources;
	};
	/** Read-only presentation summary derived only from fields in a world snapshot. */
	HudSummary BuildHudSummary(const economy::WorldSnapshot& snapshot, const economy::Catalog& catalog);
	/** Snapshot transition detector; does not infer event causes absent from snapshot fields. */
	std::vector<HudEvent> DiffHudEvents(const economy::WorldSnapshot& previous,
		const economy::WorldSnapshot& current, const economy::Catalog& catalog);
	class HudEventHistory final
	{
	public:
		explicit HudEventHistory(std::size_t capacity = 8) noexcept;
		std::vector<HudEvent> Observe(const economy::WorldSnapshot& snapshot, const economy::Catalog& catalog);
		const std::deque<HudEvent>& Events() const noexcept { return events_; }
		void Reset() noexcept;
	private:
		std::size_t capacity_;
		std::optional<economy::WorldSnapshot> previous_;
		std::deque<HudEvent> events_;
	};
}
