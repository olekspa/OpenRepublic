#include "openrepublic/runtime/economy_overview.h"

#include <algorithm>
#include <limits>
#include <map>

namespace openrepublic::runtime
{
	namespace
	{
		void SaturatingAdd(std::uint64_t& target, std::uint64_t value) noexcept
		{
			target = value > std::numeric_limits<std::uint64_t>::max() - target
				? std::numeric_limits<std::uint64_t>::max() : target + value;
		}
	}

	EconomyOverview BuildEconomyOverview(const economy::WorldSnapshot& current,
		const economy::Catalog& catalog, const std::optional<economy::WorldSnapshot>& baseline)
	{
		EconomyOverview result;
		result.tick = current.tick;
		result.treasury = current.treasury;
		result.population = current.population;
		result.workersAssigned = current.workersAssigned;
		result.workersRequired = current.workersRequired;
		result.populationAvailable = catalog.Food().resource != economy::InvalidResourceId ||
			std::any_of(catalog.BuildingTypes().begin(), catalog.BuildingTypes().end(), [](const auto& type) {
				return type.housing != 0 || type.workers != 0;
			});
		result.truckCount = current.vehicles.size();
		for (const auto& building : current.buildings) {
			const auto* type = catalog.BuildingType(building.type);
			for (std::size_t i = 0; type && i < type->recipes.size() && i < building.recipes.size(); ++i)
				for (const auto& output : type->recipes[i].outputs) {
					const auto* resource = catalog.Resource(output.resource);
					if (!resource || resource->name != "heat") continue;
					const auto cycles = building.recipes[i].completedCycles;
					const auto produced = cycles && output.amount > std::numeric_limits<std::uint64_t>::max() / cycles
						? std::numeric_limits<std::uint64_t>::max() : cycles * output.amount;
					SaturatingAdd(result.heatProduced, produced);
				}
		}
		for (const auto& building : current.buildings)
		{
			if (building.constructionSite) continue;
			if (const auto* type = catalog.BuildingType(building.type))
				SaturatingAdd(result.housingCapacity, type->housing);
		}

		std::map<economy::ResourceId, std::uint64_t> totals;
		std::map<economy::ResourceId, std::uint64_t> previousTotals;
		for (const auto& resource : catalog.Resources())
			if (!resource.utility) totals.emplace(resource.id, 0);
		const auto collect = [&totals](const economy::WorldSnapshot& snapshot) {
			for (const auto& building : snapshot.buildings)
				for (const auto& inventory : building.inventory)
				{
					auto found = totals.find(inventory.resource);
					if (found != totals.end()) SaturatingAdd(found->second, inventory.amount);
				}
		};
		collect(current);
		const bool hasBaseline = baseline && current.tick > baseline->tick;
		const std::uint64_t tickSpan = hasBaseline ? current.tick - baseline->tick : 0;
		if (hasBaseline)
		{
			for (const auto& resource : catalog.Resources())
				if (!resource.utility) previousTotals.emplace(resource.id, 0);
			const auto savedTotals = std::move(totals);
			totals.clear();
			for (const auto& resource : catalog.Resources())
				if (!resource.utility) totals.emplace(resource.id, 0);
			collect(*baseline);
			previousTotals = std::move(totals);
			totals = savedTotals;
		}
		for (const auto& [id, amount] : totals)
		{
			const auto* def = catalog.Resource(id);
			EconomyOverviewResource item;
			item.id = id;
			item.name = def ? def->name : std::string("resource #") + std::to_string(id);
			item.stored = amount;
			if (hasBaseline)
			{
				const auto old = previousTotals.find(id);
				const auto oldAmount = old == previousTotals.end() ? 0 : old->second;
				item.perTickDelta = (static_cast<long double>(amount) - static_cast<long double>(oldAmount)) /
					static_cast<long double>(tickSpan);
			}
			result.resources.push_back(std::move(item));
		}

		for (const auto& building : current.buildings)
		{
			if (building.constructionSite)
			{
				const auto* type = catalog.BuildingType(building.type);
				result.constructionSites.push_back({ building.id,
					type ? type->name : std::string("building #") + std::to_string(building.type),
					building.constructionState, building.constructionProgress });
			}
		}
		for (const auto& order : current.orders)
		{
			const auto* resource = catalog.Resource(order.resource);
			result.activeHauls.push_back({ order.id, order.source, order.destination, order.vehicle,
				resource ? resource->name : std::string("resource #") + std::to_string(order.resource),
				order.cargo, order.state });
		}
		return result;
	}
}
