#include "openrepublic/runtime/objective_planner.h"

#include <algorithm>
#include <tuple>

namespace openrepublic::runtime
{
	namespace
	{
		bool Occupied(const economy::WorldSnapshot& world, const economy::Catalog& catalog, economy::Cell cell)
		{
			for (const auto& b : world.buildings)
			{
				const auto* type = catalog.BuildingType(b.type);
				if (!type) continue;
				if (cell.x >= b.origin.x && cell.z >= b.origin.z &&
					static_cast<std::uint64_t>(cell.x - b.origin.x) < type->width &&
					static_cast<std::uint64_t>(cell.z - b.origin.z) < type->depth) return true;
			}
			return std::any_of(world.roads.begin(), world.roads.end(), [&](const auto& r) {
				return (r.a.x == cell.x && r.a.z == cell.z) || (r.b.x == cell.x && r.b.z == cell.z);
			});
		}

		std::optional<economy::Cell> FreeCell(const economy::WorldSnapshot& world,
			const economy::Catalog& catalog, const economy::BuildingTypeDef& type)
		{
			// Stable center-out search; bounded to 64x64 even on a larger map.
			const auto maxX = static_cast<std::int32_t>(std::min<std::uint32_t>(world.gridWidth, 64));
			const auto maxZ = static_cast<std::int32_t>(std::min<std::uint32_t>(world.gridDepth, 64));
			const auto cx = maxX / 2, cz = maxZ / 2;
			for (std::int32_t radius = 0; radius < std::max(maxX, maxZ); ++radius)
				for (std::int32_t z = 0; z < maxZ; ++z)
					for (std::int32_t x = 0; x < maxX; ++x)
					{
						if (std::max(std::abs(x - cx), std::abs(z - cz)) != radius) continue;
						if (x + static_cast<std::int32_t>(type.width) > maxX || z + static_cast<std::int32_t>(type.depth) > maxZ) continue;
						bool free = true;
						for (std::uint32_t dz = 0; dz < type.depth && free; ++dz)
							for (std::uint32_t dx = 0; dx < type.width; ++dx)
								if (Occupied(world, catalog, { x + static_cast<std::int32_t>(dx), z + static_cast<std::int32_t>(dz) })) { free = false; break; }
						if (free) return economy::Cell{ x, z };
					}
			return std::nullopt;
		}

		bool Matches(const economy::BuildingTypeDef& type, const NativeObjective& objective)
		{
			if (!objective.buildingType.empty()) return type.name == objective.buildingType;
			if (objective.metric == ObjectiveMetric::ConstructionSitesComplete || objective.metric == ObjectiveMetric::ConstructionSitesCount ||
				(objective.metric == ObjectiveMetric::BuildingCountAtLeast && objective.buildingType.empty()))
				return type.constructionWorkTicks != 0;
			if (objective.metric == ObjectiveMetric::PopulationAtLeast) return type.housing != 0;
			return false;
		}
	}

	std::optional<PlannerCommand> NextObjectivePlannerCommand(const economy::WorldSnapshot& snapshot,
		const economy::Catalog& catalog, const NativeObjectiveSet& objectives)
	{
		const auto progress = EvaluateNativeObjectives(objectives, snapshot, catalog);
		for (std::size_t index = 0; index < objectives.objectives.size(); ++index)
		{
			const auto& objective = objectives.objectives[index];
			if (progress.complete[index]) continue;
			if (objective.metric == ObjectiveMetric::ConstructionSitesComplete ||
				objective.metric == ObjectiveMetric::ConstructionSitesCount ||
				objective.metric == ObjectiveMetric::BuildingCountAtLeast ||
				objective.metric == ObjectiveMetric::PopulationAtLeast)
			{
				for (const auto& type : catalog.BuildingTypes())
				{
					if (!Matches(type, objective)) continue;
					const auto cell = FreeCell(snapshot, catalog, type);
					if (!cell) continue;
					return PlannerCommand{ type.constructionWorkTicks ? PlannerCommandKind::PlaceConstructionSite : PlannerCommandKind::PlaceBuilding,
						type.name, *cell, {} };
				}
			}
			// Inventory, production and utility goals require mission-specific
			// topology/resources. Stay idle rather than inventing a rule.
			return std::nullopt;
		}
		return std::nullopt;
	}
}
