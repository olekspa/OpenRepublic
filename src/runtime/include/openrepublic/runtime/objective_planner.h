#pragma once

#include "openrepublic/runtime/native_objectives.h"

#include <optional>

namespace openrepublic::runtime
{
enum class PlannerCommandKind : std::uint8_t { PlaceBuilding, PlaceConstructionSite, BuildRoad, TradeImport, TradeExport, DispatchTruck, ConnectUtility };
	struct PlannerCommand
	{
		PlannerCommandKind kind{ PlannerCommandKind::PlaceBuilding };
		std::string buildingType;
		economy::Cell a{}, b{};
	economy::BuildingId building{}, source{}, destination{};
	economy::VehicleId vehicle{};
	std::string resource;
	std::uint64_t amount{};
	std::vector<std::pair<economy::Cell, economy::Cell>> roads;
	};

	// Deterministic OpenRepublic-only planner. Emits at most one command for the
	// first unmet objective and never mutates the supplied snapshot or catalog.
	std::optional<PlannerCommand> NextObjectivePlannerCommand(
		const economy::WorldSnapshot& snapshot, const economy::Catalog& catalog,
		const NativeObjectiveSet& objectives);
}
