#pragma once

#include "openrepublic/economy/economy.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openrepublic::runtime
{
	enum class ObjectiveMetric : std::uint8_t
	{
		InventoryAtLeast,
		DeliveredAtLeast,
		ConstructionSitesComplete,
		ConstructionSitesCount,
		PopulationAtLeast,
		BuildingCountAtLeast
	};

	struct NativeObjective
	{
		std::string id;
		std::string title;
		std::string hint;
		ObjectiveMetric metric{ ObjectiveMetric::InventoryAtLeast };
		std::string buildingType;
		std::string resource;
		std::uint64_t target{};
	};

	struct NativeObjectiveSet
	{
		std::string id;
		std::string title;
		std::vector<NativeObjective> objectives;
	};

	struct NativeObjectiveProgress
	{
		std::vector<std::uint64_t> values;
		std::vector<bool> complete;
		std::size_t completedCount{};
		double fraction{};
		bool finished{};
		std::string currentHint;
	};

	// Pure read-only evaluation of authored OpenRepublic tutorial goals.
	NativeObjectiveProgress EvaluateNativeObjectives(const NativeObjectiveSet& set,
		const economy::WorldSnapshot& snapshot, const economy::Catalog& catalog);
	NativeObjectiveSet BuiltinCoalDemoObjectives();
	NativeObjectiveSet BuiltinConstructionObjectives();
	NativeObjectiveSet BuiltinGrowthTownObjectives();
}
