#include "openrepublic/runtime/objective_planner.h"

#include "openrepublic/runtime/native_construction.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <tuple>

namespace openrepublic::runtime
{
	namespace
	{
		std::uint64_t Stock(const economy::BuildingSnapshot& building, economy::ResourceId resource)
		{
			std::uint64_t value = 0;
			for (const auto& slot : building.inventory) if (slot.resource == resource) value += slot.amount;
			return value;
		}
		const economy::BuildingSnapshot* Building(const economy::WorldSnapshot& world, economy::BuildingId id)
		{
			const auto it = std::find_if(world.buildings.begin(), world.buildings.end(), [id](const auto& b) { return b.id == id; });
			return it == world.buildings.end() ? nullptr : &*it;
		}
		bool HasType(const economy::BuildingSnapshot& b, const economy::Catalog& catalog, const std::string& name)
		{
			const auto* t = catalog.BuildingType(b.type);
			return t && (name.empty() || t->name == name);
		}
		const economy::BuildingTypeDef* TypeNamed(const economy::Catalog& catalog, const std::string& name)
		{
			for (const auto& t : catalog.BuildingTypes()) if (t.name == name) return &t;
			return nullptr;
		}
		bool OnFootprint(const economy::BuildingTypeDef& type, economy::Cell origin, economy::Cell c)
		{
			return c.x >= origin.x && c.z >= origin.z &&
				static_cast<std::int64_t>(c.x) < static_cast<std::int64_t>(origin.x) + type.width &&
				static_cast<std::int64_t>(c.z) < static_cast<std::int64_t>(origin.z) + type.depth;
		}
		bool RoadEndpoint(const economy::WorldSnapshot& world, economy::Cell c)
		{
			return std::any_of(world.roads.begin(), world.roads.end(), [&](const auto& r) { return r.a == c || r.b == c; });
		}
		std::uint64_t Manhattan(economy::Cell a, economy::Cell b)
		{
			return static_cast<std::uint64_t>(std::abs(a.x - b.x)) + static_cast<std::uint64_t>(std::abs(a.z - b.z));
		}

		std::optional<economy::Cell> FreeCell(const economy::WorldSnapshot& world,
			const economy::Catalog& catalog, const economy::BuildingTypeDef& type)
		{
			std::optional<economy::Cell> best;
			std::tuple<unsigned, std::uint64_t, std::int32_t, std::int32_t> bestScore{
				std::numeric_limits<unsigned>::max(), UINT64_MAX, INT32_MAX, INT32_MAX };
			for (std::int32_t z = 1; z < static_cast<std::int32_t>(world.gridDepth); ++z)
				for (std::int32_t x = 0; x + static_cast<std::int32_t>(type.width) <= static_cast<std::int32_t>(world.gridWidth); ++x)
				{
					const economy::Cell origin{ x, z };
					if (!ValidateNativeBuildingPlacement(world, catalog, type.id, origin).Valid()) continue;
					const economy::Cell access{ x + static_cast<std::int32_t>((type.width - 1) / 2), z - 1 };
					std::uint64_t roadDistance = world.roads.empty() ? 999 : UINT64_MAX;
					for (const auto& road : world.roads) roadDistance = std::min({ roadDistance, Manhattan(access, road.a), Manhattan(access, road.b) });
					const auto score = std::make_tuple(roadDistance == 0 ? 0u : 1u, roadDistance,
						static_cast<std::int32_t>(std::abs(x - static_cast<std::int32_t>(world.gridWidth / 2))),
						static_cast<std::int32_t>(std::abs(z - static_cast<std::int32_t>(world.gridDepth / 2))));
					if (score < bestScore) { bestScore = score; best = origin; }
				}
			return best;
		}

		bool HasUtility(const economy::WorldSnapshot& world, const economy::Catalog& catalog, const std::string& resource,
			economy::BuildingId a, economy::BuildingId b)
		{
			economy::ResourceId resourceId{};
			if (!catalog.FindResource(resource, resourceId)) return false;
			return std::any_of(world.utilityConnections.begin(), world.utilityConnections.end(), [&](const auto& e) {
				return e.resource == resourceId && ((e.a == a && e.b == b) || (e.a == b && e.b == a));
			});
		}

		std::optional<PlannerCommand> ConnectRecipeUtilities(const economy::WorldSnapshot& world,
			const economy::Catalog& catalog, const economy::BuildingSnapshot& target)
		{
			const auto* type = catalog.BuildingType(target.type);
			if (!type) return std::nullopt;
			for (const auto& recipe : type->recipes) for (const auto& input : recipe.inputs)
			{
				const auto* resource = catalog.Resource(input.resource);
				if (!resource || !resource->utility) continue;
				for (const auto& source : world.buildings)
				{
					if (source.id == target.id) continue;
					const auto* sourceType = catalog.BuildingType(source.type);
					if (!sourceType) continue;
					const bool produces = std::any_of(sourceType->recipes.begin(), sourceType->recipes.end(), [&](const auto& r) {
						return std::any_of(r.outputs.begin(), r.outputs.end(), [&](const auto& out) { return out.resource == input.resource; });
					});
					if (produces && !HasUtility(world, catalog, resource->name, source.id, target.id))
					{
						PlannerCommand cmd; cmd.kind = PlannerCommandKind::ConnectUtility; cmd.source = source.id; cmd.destination = target.id; cmd.resource = resource->name; return cmd;
					}
				}
			}
			return std::nullopt;
		}

		std::optional<PlannerCommand> Supply(const economy::WorldSnapshot& world, const economy::Catalog& catalog,
			const economy::BuildingSnapshot& destination, const std::string& resourceName, std::uint64_t desired)
		{
			economy::ResourceId resource{};
			if (!catalog.FindResource(resourceName, resource)) return std::nullopt;
			const auto held = Stock(destination, resource);
			if (held >= desired) return std::nullopt;
			const auto missing = std::max<std::uint64_t>(1, desired - held);
			for (const auto& source : world.buildings)
			{
				if (source.id == destination.id || Stock(source, resource) == 0) continue;
				const auto* sourceType = catalog.BuildingType(source.type);
				if (!sourceType) continue;
				for (const auto& vehicle : world.vehicles)
				{
					if (vehicle.state != economy::HaulState::Idle || vehicle.order != 0) continue;
					const auto* depot = Building(world, vehicle.ownerDepot);
					const auto* depotType = depot ? catalog.BuildingType(depot->type) : nullptr;
					const auto* vehicleType = catalog.VehicleType(vehicle.type);
					if (!depotType || !depotType->depot || !vehicleType) continue;
					PlannerCommand cmd; cmd.kind = PlannerCommandKind::DispatchTruck; cmd.vehicle = vehicle.id;
					cmd.source = source.id; cmd.destination = destination.id; cmd.resource = resourceName;
					cmd.amount = std::min({ missing, Stock(source, resource), vehicleType->capacity }); return cmd;
				}
			}
			const bool storedElsewhere = std::any_of(world.buildings.begin(), world.buildings.end(), [&](const auto& b) {
				return b.id != destination.id && Stock(b, resource) != 0;
			});
			const bool hasDepot = std::any_of(world.buildings.begin(), world.buildings.end(), [&](const auto& b) {
				const auto* t = catalog.BuildingType(b.type); return t && t->depot;
			});
			if (storedElsewhere && !hasDepot)
				if (const auto* depot = TypeNamed(catalog, "road_depot"))
					if (const auto cell = FreeCell(world, catalog, *depot))
					{ PlannerCommand cmd; cmd.kind = PlannerCommandKind::PlaceBuilding; cmd.buildingType = depot->name; cmd.a = *cell; return cmd; }
			for (const auto& post : world.buildings)
			{
				const auto* type = catalog.BuildingType(post.type);
				if (!type || !type->tradePost) continue;
				const auto* declared = catalog.Resource(resource);
				if (declared && declared->tradeDeclared && !declared->utility && world.treasury >= declared->buyPrice * missing)
				{
					PlannerCommand cmd; cmd.kind = PlannerCommandKind::TradeImport; cmd.building = post.id;
					cmd.resource = resourceName; cmd.amount = missing; return cmd;
				}
				if (world.treasury < (declared ? declared->buyPrice * missing : UINT64_MAX))
				{
					for (const auto& exportSlot : post.inventory)
					{
						const auto* exportResource = catalog.Resource(exportSlot.resource);
						if (!exportResource || !exportResource->tradeDeclared || exportResource->sellPrice == 0 || exportSlot.amount == 0) continue;
						PlannerCommand cmd; cmd.kind = PlannerCommandKind::TradeExport; cmd.building = post.id;
						cmd.resource = exportResource->name; cmd.amount = std::min<std::uint64_t>(exportSlot.amount,
							std::max<std::uint64_t>(1, (declared ? declared->buyPrice * missing : 1) / exportResource->sellPrice)); return cmd;
					}
				}
			}
			return std::nullopt;
		}

		std::optional<PlannerCommand> PlanProducerInputs(const economy::WorldSnapshot& world,
			const economy::Catalog& catalog, economy::ResourceId output, unsigned depth = 0)
		{
			if (depth >= 8) return std::nullopt;
			for (const auto& producer : world.buildings)
			{
				const auto* type = catalog.BuildingType(producer.type);
				if (!type) continue;
				for (const auto& recipe : type->recipes)
				{
					if (std::none_of(recipe.outputs.begin(), recipe.outputs.end(), [&](const auto& item) { return item.resource == output; })) continue;
					for (const auto& input : recipe.inputs)
					{
						const auto* inputResource = catalog.Resource(input.resource);
						if (!inputResource) continue;
						if (inputResource->utility)
						{
							for (const auto& source : world.buildings)
							{
								if (source.id == producer.id) continue;
								const auto* sourceType = catalog.BuildingType(source.type);
								if (!sourceType) continue;
								const bool supplies = std::any_of(sourceType->recipes.begin(), sourceType->recipes.end(), [&](const auto& r) {
									return std::any_of(r.outputs.begin(), r.outputs.end(), [&](const auto& o) { return o.resource == input.resource; });
								});
								if (supplies && !HasUtility(world, catalog, inputResource->name, source.id, producer.id))
								{
									PlannerCommand cmd; cmd.kind = PlannerCommandKind::ConnectUtility; cmd.source = source.id;
									cmd.destination = producer.id; cmd.resource = inputResource->name; return cmd;
								}
							}
							continue;
						}
						if (Stock(producer, input.resource) >= input.amount) continue;
					if (const auto supply = Supply(world, catalog, producer, inputResource->name, input.amount * 2)) return supply;
					if (const auto recurse = PlanProducerInputs(world, catalog, input.resource, depth + 1)) return recurse;
				}
			}
			}
			return std::nullopt;
		}

		std::optional<PlannerCommand> ConnectHousingUtilities(const economy::WorldSnapshot& world,
			const economy::Catalog& catalog)
		{
			for (const auto& building : world.buildings)
			{
				const auto* type = catalog.BuildingType(building.type);
				if (!type || type->housing == 0) continue;
				if (const auto connection = ConnectRecipeUtilities(world, catalog, building)) return connection;
			}
			return std::nullopt;
		}

		std::optional<PlannerCommand> PlaceForObjective(const economy::WorldSnapshot& world,
			const economy::Catalog& catalog, const NativeObjective& objective)
		{
			const economy::BuildingTypeDef* selected = objective.buildingType.empty() ? nullptr : TypeNamed(catalog, objective.buildingType);
			for (const auto& type : catalog.BuildingTypes())
			{
				const bool matches = selected ? type.id == selected->id :
					((objective.metric == ObjectiveMetric::PopulationAtLeast && type.housing != 0) ||
					 ((objective.metric == ObjectiveMetric::ConstructionSitesComplete || objective.metric == ObjectiveMetric::ConstructionSitesCount ||
					   objective.metric == ObjectiveMetric::BuildingCountAtLeast) && type.constructionWorkTicks != 0));
				if (!matches) continue;
				const auto count = static_cast<std::uint64_t>(std::count_if(world.buildings.begin(), world.buildings.end(), [&](const auto& b) {
					return HasType(b, catalog, type.name) &&
						(objective.metric != ObjectiveMetric::ConstructionSitesComplete || b.constructionSite);
				}));
				if (objective.metric == ObjectiveMetric::PopulationAtLeast)
				{
					std::uint64_t capacity = 0;
					for (const auto& b : world.buildings) if (const auto* existing = catalog.BuildingType(b.type)) capacity += existing->housing;
					if (capacity >= objective.target) continue;
				}
				else if (count >= objective.target) continue;
				const auto cell = FreeCell(world, catalog, type);
				if (!cell) continue;
				PlannerCommand cmd; cmd.kind = type.constructionWorkTicks ? PlannerCommandKind::PlaceConstructionSite : PlannerCommandKind::PlaceBuilding;
				cmd.buildingType = type.name; cmd.a = *cell; return cmd;
			}
			return std::nullopt;
		}

		bool RoadCellBlocked(const economy::WorldSnapshot& world, const economy::Catalog& catalog, economy::Cell cell)
		{
			for (const auto& building : world.buildings)
			{
				const auto* type = catalog.BuildingType(building.type);
				if (type && OnFootprint(*type, building.origin, cell)) return true;
			}
			return false;
		}
		std::optional<PlannerCommand> RoadAccess(const economy::WorldSnapshot& world, const economy::Catalog& catalog)
		{
			for (const auto& building : world.buildings)
			{
				if (RoadEndpoint(world, building.access)) continue;
				PlannerCommand command; command.kind = PlannerCommandKind::BuildRoad;
				if (world.roads.empty())
				{
					for (const economy::Cell delta : { economy::Cell{1,0}, economy::Cell{0,1}, economy::Cell{-1,0}, economy::Cell{0,-1} })
					{
						const economy::Cell other{ building.access.x + delta.x, building.access.z + delta.z };
						if (other.x < 0 || other.z < 0 || static_cast<std::uint32_t>(other.x) >= world.gridWidth ||
							static_cast<std::uint32_t>(other.z) >= world.gridDepth || RoadCellBlocked(world, catalog, other)) continue;
						command.roads.emplace_back(building.access, other); return command;
					}
					continue;
				}
				economy::Cell nearest{}; std::uint64_t distance = UINT64_MAX;
				for (const auto& road : world.roads) for (const auto endpoint : { road.a, road.b })
					if (const auto candidate = Manhattan(building.access, endpoint); candidate < distance) { distance = candidate; nearest = endpoint; }
				if (distance == 0 || distance == UINT64_MAX) continue;
				auto route = [&](bool xFirst) {
					std::vector<economy::Cell> points{ building.access };
					auto current = building.access;
					const auto walk = [&](bool xAxis, economy::Cell& cursor, std::vector<economy::Cell>& path) {
						while ((xAxis ? cursor.x != nearest.x : cursor.z != nearest.z))
						{
							const economy::Cell next{ cursor.x + (xAxis ? (nearest.x > cursor.x ? 1 : -1) : 0),
								cursor.z + (!xAxis ? (nearest.z > cursor.z ? 1 : -1) : 0) };
							if ((next != nearest && RoadCellBlocked(world, catalog, next)) || next.x < 0 || next.z < 0 ||
								static_cast<std::uint32_t>(next.x) >= world.gridWidth || static_cast<std::uint32_t>(next.z) >= world.gridDepth) return false;
							cursor = next; path.push_back(next);
						}
						return true;
					};
					return walk(xFirst, current, points) && walk(!xFirst, current, points) ? points : std::vector<economy::Cell>{};
				};
			for (const bool xFirst : { true, false })
			{
				const auto points = route(xFirst);
				if (points.size() < 2) continue;
				for (std::size_t i = 1; i < points.size(); ++i) command.roads.emplace_back(points[i-1], points[i]);
				return command;
			}
			}
			return std::nullopt;
		}
	}

	std::optional<PlannerCommand> NextObjectivePlannerCommand(const economy::WorldSnapshot& snapshot,
		const economy::Catalog& catalog, const NativeObjectiveSet& objectives)
	{
		const auto progress = EvaluateNativeObjectives(objectives, snapshot, catalog);
		if (auto road = RoadAccess(snapshot, catalog)) return road;
		for (std::size_t i = 0; i < objectives.objectives.size(); ++i)
		{
			if (progress.complete[i]) continue;
			const auto& objective = objectives.objectives[i];
			if (objective.metric == ObjectiveMetric::BuildingCountAtLeast || objective.metric == ObjectiveMetric::ConstructionSitesCount ||
				objective.metric == ObjectiveMetric::ConstructionSitesComplete || objective.metric == ObjectiveMetric::PopulationAtLeast)
			{
				if (auto place = PlaceForObjective(snapshot, catalog, objective)) return place;
			}
			if (objective.metric == ObjectiveMetric::InventoryAtLeast || objective.metric == ObjectiveMetric::DeliveredAtLeast)
			{
				const economy::BuildingSnapshot* target = nullptr;
				for (const auto& b : snapshot.buildings) if (HasType(b, catalog, objective.buildingType)) { target = &b; break; }
				if (!target && !objective.buildingType.empty())
				{
					NativeObjective placement = objective; placement.metric = ObjectiveMetric::BuildingCountAtLeast; placement.target = 1;
					if (auto place = PlaceForObjective(snapshot, catalog, placement)) return place;
				}
				if (target)
				{
					if (auto utility = ConnectRecipeUtilities(snapshot, catalog, *target)) return utility;
					// Bring declared recipe inputs to the target before waiting on local production.
					if (const auto* type = catalog.BuildingType(target->type))
						for (const auto& recipe : type->recipes) for (const auto& input : recipe.inputs)
						{
							const auto* resource = catalog.Resource(input.resource);
						if (!resource || resource->utility || input.amount == 0) continue;
						if (auto supply = Supply(snapshot, catalog, *target, resource->name, input.amount * 2)) return supply;
						}
					if (!objective.resource.empty())
						if (auto supply = Supply(snapshot, catalog, *target, objective.resource, objective.target)) return supply;
				}
			}
			if (objective.metric == ObjectiveMetric::PopulationAtLeast && catalog.Food().resource != economy::InvalidResourceId)
			{
				if (auto connection = ConnectHousingUtilities(snapshot, catalog)) return connection;
				const auto foodName = catalog.Resource(catalog.Food().resource)->name;
				if (auto chain = PlanProducerInputs(snapshot, catalog, catalog.Food().resource)) return chain;
				for (const auto& b : snapshot.buildings)
					if (HasType(b, catalog, "market"))
						if (auto supply = Supply(snapshot, catalog, b, foodName, 8)) return supply;
			}
			// A completed construction site consumes its authored materials before work progresses.
			for (const auto& site : snapshot.buildings)
			{
				if (!site.constructionSite) continue;
				const auto* type = catalog.BuildingType(site.type);
				if (!type) continue;
				for (const auto& material : site.constructionMaterials)
					if (material.held < material.required)
						if (const auto* resource = catalog.Resource(material.resource))
						{
							if (auto supply = Supply(snapshot, catalog, site, resource->name, material.required)) return supply;
							if (auto chain = PlanProducerInputs(snapshot, catalog, resource->id)) return chain;
						}
			}
			return std::nullopt;
		}
		return std::nullopt;
	}
}
