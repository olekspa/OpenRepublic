#include "openrepublic/runtime/native_economy_hud.h"

#include <algorithm>
#include <limits>
#include <map>

namespace openrepublic::runtime
{
	namespace
	{
		void Add(std::uint64_t& value, std::uint64_t amount) noexcept
		{
			value = amount > std::numeric_limits<std::uint64_t>::max() - value
				? std::numeric_limits<std::uint64_t>::max() : value + amount;
		}
		const economy::BuildingSnapshot* FindBuilding(const economy::WorldSnapshot& snapshot, economy::BuildingId id)
		{
			const auto it = std::find_if(snapshot.buildings.begin(), snapshot.buildings.end(), [id](const auto& item) { return item.id == id; });
			return it == snapshot.buildings.end() ? nullptr : &*it;
		}
		const economy::HaulOrderSnapshot* FindOrder(const economy::WorldSnapshot& snapshot, economy::OrderId id)
		{
			const auto it = std::find_if(snapshot.orders.begin(), snapshot.orders.end(), [id](const auto& item) { return item.id == id; });
			return it == snapshot.orders.end() ? nullptr : &*it;
		}
		const char* StallName(economy::StallReason reason)
		{
			switch (reason)
			{
			case economy::StallReason::MissingInput: return "input shortage";
			case economy::StallReason::OutputFull: return "output full";
			case economy::StallReason::MissingHeating: return "heating shortage";
			case economy::StallReason::Disabled: return "disabled";
			case economy::StallReason::MissingWater: return "water shortage";
			default: return "stalled";
			}
		}
	}
	HudSummary BuildHudSummary(const economy::WorldSnapshot& snapshot, const economy::Catalog& catalog)
	{
		HudSummary out;
		out.tick = snapshot.tick; out.treasury = snapshot.treasury; out.population = snapshot.population;
		out.happyResidents = snapshot.happyResidents; out.happiness = snapshot.happiness;
		out.calendarYear = snapshot.calendarYear; out.calendarDay = snapshot.calendarDay;
		out.calendarSeason = snapshot.calendarSeason;
		out.hasCalendar = catalog.Calendar().ticksPerDay != 0;
		out.populationAvailable = catalog.CitizensEnabled() || catalog.Food().resource != economy::InvalidResourceId ||
			std::any_of(catalog.BuildingTypes().begin(), catalog.BuildingTypes().end(), [](const auto& type) { return type.housing != 0 || type.workers != 0; });
		std::map<economy::ResourceId, std::uint64_t> totals;
		for (const auto& resource : catalog.Resources()) if (!resource.utility) totals.emplace(resource.id, 0);
		for (const auto& building : snapshot.buildings)
			for (const auto& stock : building.inventory)
				if (auto it = totals.find(stock.resource); it != totals.end()) Add(it->second, stock.amount);
		for (const auto& [id, amount] : totals)
		{
			const auto* resource = catalog.Resource(id);
			out.storedResources.emplace_back(resource ? resource->name : "resource " + std::to_string(id), amount);
		}
		return out;
	}
	std::vector<HudEvent> DiffHudEvents(const economy::WorldSnapshot& previous,
		const economy::WorldSnapshot& current, const economy::Catalog& catalog)
	{
		std::vector<HudEvent> out;
		if (current.tick <= previous.tick) return out;
		for (const auto& old : previous.buildings)
		{
			if (!old.constructionSite) continue;
			const auto* now = FindBuilding(current, old.id);
			if (now && (!now->constructionSite || now->constructionState == 2))
			{
				const auto* type = catalog.BuildingType(now->type);
				out.push_back({current.tick, HudEventKind::ConstructionCompleted,
					"Construction completed: " + std::string(type ? type->name : "building") + " OR-" + std::to_string(now->id)});
			}
		}
		for (const auto& now : current.buildings)
		{
			if (now.stall.reason != economy::StallReason::MissingInput &&
				now.stall.reason != economy::StallReason::MissingHeating &&
				now.stall.reason != economy::StallReason::MissingWater) continue;
			const auto* old = FindBuilding(previous, now.id);
			if (old && old->stall.reason == now.stall.reason && old->stall.resource == now.stall.resource) continue;
			const auto* type = catalog.BuildingType(now.type);
			const auto* resource = catalog.Resource(now.stall.resource);
			std::string text = std::string(type ? type->name : "Building") + " OR-" + std::to_string(now.id) + ": " + StallName(now.stall.reason);
			if (resource) text += " (" + resource->name + ")";
			out.push_back({current.tick, HudEventKind::Shortage, std::move(text)});
		}
		for (const auto& old : previous.orders)
		{
			if (old.cargo == 0) continue;
			const auto* now = FindOrder(current, old.id);
			if (now && now->cargo == 0)
			{
				const auto* resource = catalog.Resource(now->resource);
				out.push_back({current.tick, HudEventKind::Delivery,
					"Delivery recorded: " + std::to_string(old.cargo) + " " + (resource ? resource->name : "units") + " on haul #" + std::to_string(now->id)});
			}
		}
		if (current.treasury != previous.treasury)
			out.push_back({current.tick, HudEventKind::BorderTrade, "Border trade treasury: " + std::to_string(previous.treasury) + " -> " + std::to_string(current.treasury)});
		if (current.population != previous.population)
		{
			if (catalog.CitizensEnabled() && current.population > previous.population)
				out.push_back({current.tick, HudEventKind::Immigration, "Immigration: +" + std::to_string(current.population - previous.population) + " residents (OpenRepublic synthetic citizens)"});
			else
				out.push_back({current.tick, HudEventKind::PopulationChanged, "Population changed: " + std::to_string(previous.population) + " -> " + std::to_string(current.population)});
		}
		return out;
	}
	HudEventHistory::HudEventHistory(std::size_t capacity) noexcept : capacity_(capacity) {}
	void HudEventHistory::Reset() noexcept { previous_.reset(); events_.clear(); }
	std::vector<HudEvent> HudEventHistory::Observe(const economy::WorldSnapshot& snapshot, const economy::Catalog& catalog)
	{
		if (!previous_ || snapshot.tick < previous_->tick) { Reset(); previous_ = snapshot; return {}; }
		if (snapshot.tick == previous_->tick) return {};
		auto added = DiffHudEvents(*previous_, snapshot, catalog);
		previous_ = snapshot;
		for (const auto& event : added)
		{
			if (capacity_ == 0) break;
			if (events_.size() == capacity_) events_.pop_front();
			events_.push_back(event);
		}
		return added;
	}
}
