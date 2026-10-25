#pragma once

#include "openrepublic/runtime/native_objectives.h"

#include <optional>

namespace openrepublic::runtime
{
	enum class PlannerCommandKind : std::uint8_t { PlaceBuilding, PlaceConstructionSite, BuildRoad };
	struct PlannerCommand
	{
		PlannerCommandKind kind{ PlannerCommandKind::PlaceBuilding };
		std::string buildingType;
		economy::Cell a{}, b{};
	};

	// Deterministic OpenRepublic-only planner. Emits at most one command for the
	// first unmet objective and never mutates the supplied snapshot or catalog.
	std::optional<PlannerCommand> NextObjectivePlannerCommand(
		const economy::WorldSnapshot& snapshot, const economy::Catalog& catalog,
		const NativeObjectiveSet& objectives);
}
