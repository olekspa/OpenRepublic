#include "openrepublic/runtime/native_objectives.h"

#include <algorithm>
#include <limits>

namespace openrepublic::runtime
{
	namespace
	{
		bool TypeMatches(const economy::BuildingSnapshot& building, const economy::Catalog& catalog,
			const std::string& name)
		{
			if (name.empty()) return true;
			const auto* type = catalog.BuildingType(building.type);
			return type != nullptr && type->name == name;
		}

		std::uint64_t Inventory(const economy::BuildingSnapshot& building, economy::ResourceId resource)
		{
			std::uint64_t total = 0;
			for (const auto& entry : building.inventory)
				if (entry.resource == resource)
					total = entry.amount > std::numeric_limits<std::uint64_t>::max() - total
						? std::numeric_limits<std::uint64_t>::max() : total + entry.amount;
			return total;
		}

		std::uint64_t Value(const NativeObjective& objective, const economy::WorldSnapshot& snapshot,
			const economy::Catalog& catalog)
		{
			if (objective.metric == ObjectiveMetric::PopulationAtLeast) return snapshot.population;
			if (objective.metric == ObjectiveMetric::ConstructionSitesCount)
				return static_cast<std::uint64_t>(std::count_if(snapshot.buildings.begin(), snapshot.buildings.end(),
					[&](const auto& building) { return TypeMatches(building, catalog, objective.buildingType) && building.constructionSite; }));
			if (objective.metric == ObjectiveMetric::ConstructionSitesComplete)
				return static_cast<std::uint64_t>(std::count_if(snapshot.buildings.begin(), snapshot.buildings.end(),
					[&](const auto& building) { return TypeMatches(building, catalog, objective.buildingType) &&
						building.constructionSite && building.constructionState == 2; }));
			if (objective.metric == ObjectiveMetric::BuildingCountAtLeast)
				return static_cast<std::uint64_t>(std::count_if(snapshot.buildings.begin(), snapshot.buildings.end(),
					[&](const auto& building) { return TypeMatches(building, catalog, objective.buildingType); }));

			economy::ResourceId resource = economy::InvalidResourceId;
			if (!objective.resource.empty() && !catalog.FindResource(objective.resource, resource)) return 0;
			if (objective.metric == ObjectiveMetric::InventoryAtLeast)
			{
				std::uint64_t total = 0;
				for (const auto& building : snapshot.buildings)
					if (TypeMatches(building, catalog, objective.buildingType))
					{
						const auto amount = Inventory(building, resource);
						total = amount > std::numeric_limits<std::uint64_t>::max() - total
							? std::numeric_limits<std::uint64_t>::max() : total + amount;
					}
				return total;
			}
			// The snapshot has no durable delivery history. Use stock currently held
			// at matching destinations as the only observable, repeatable proxy.
			std::uint64_t held = 0;
			for (const auto& building : snapshot.buildings)
				if (TypeMatches(building, catalog, objective.buildingType))
				{
					const auto amount = Inventory(building, resource);
					held = amount > std::numeric_limits<std::uint64_t>::max() - held
						? std::numeric_limits<std::uint64_t>::max() : held + amount;
				}
			return held;
		}
	}

	NativeObjectiveProgress EvaluateNativeObjectives(const NativeObjectiveSet& set,
		const economy::WorldSnapshot& snapshot, const economy::Catalog& catalog)
	{
		NativeObjectiveProgress result;
		result.values.reserve(set.objectives.size());
		result.complete.reserve(set.objectives.size());
		for (const auto& objective : set.objectives)
		{
			const auto value = Value(objective, snapshot, catalog);
			const bool complete = objective.target == 0 || value >= objective.target;
			result.values.push_back(value);
			result.complete.push_back(complete);
			if (complete) ++result.completedCount;
			else if (result.currentHint.empty()) result.currentHint = objective.hint;
		}
		result.finished = !set.objectives.empty() && result.completedCount == set.objectives.size();
		result.fraction = set.objectives.empty() ? 0.0 : static_cast<double>(result.completedCount) / set.objectives.size();
		return result;
	}

	NativeObjectiveSet BuiltinCoalDemoObjectives()
	{
		return { "coal_demo", "Coal to heat", {
			{ "buy_coal", "Buy coal at the border", "Use Buy coal to import coal at the border post.", ObjectiveMetric::InventoryAtLeast, "border_post", "coal", 1 },
			{ "dispatch_storage", "Dispatch coal to storage", "Dispatch a depot truck from the border post to storage.", ObjectiveMetric::DeliveredAtLeast, "storage", "coal", 1 },
			{ "deliver_plant", "Deliver coal to the heating plant", "Dispatch a second truck from storage to the heating plant.", ObjectiveMetric::DeliveredAtLeast, "heating_plant", "coal", 1 },
			{ "produce_heat", "Reach 100 heat units", "Keep the heating plant supplied until it stores 100 heat units.", ObjectiveMetric::InventoryAtLeast, "heating_plant", "heat", 100 }
		} };
	}

	NativeObjectiveSet BuiltinConstructionObjectives()
	{
		return { "construction_sites_demo", "Build the town", {
			{ "place_sites", "Start two construction sites", "Place two authored construction sites.", ObjectiveMetric::BuildingCountAtLeast, "", "", 2 },
			{ "complete_sites", "Complete two buildings", "Supply materials and wait for construction work to finish.", ObjectiveMetric::ConstructionSitesComplete, "", "", 2 }
		} };
	}

	NativeObjectiveSet BuiltinGrowthTownObjectives()
	{
		return { "growth_town", "Grow a town", {
			{ "population", "Reach 100 residents", "Provide housing and services to grow the population.", ObjectiveMetric::PopulationAtLeast, "", "", 100 },
			{ "housing", "Build three homes", "Place three residential buildings.", ObjectiveMetric::BuildingCountAtLeast, "residential_building", "", 3 }
		} };
	}
}
