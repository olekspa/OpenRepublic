#include "openrepublic/runtime/objective_planner.h"

#include <cstdlib>
#include <iostream>

int main()
{
	using namespace openrepublic;
	const auto catalog = economy::MakeSyntheticConstructionCatalog();
	economy::World world(catalog, 64, 64);
	const auto objectives = runtime::BuiltinConstructionObjectives();
	const auto first = runtime::NextObjectivePlannerCommand(world.Snapshot(), catalog, objectives);
	if (!first || first->kind != runtime::PlannerCommandKind::PlaceConstructionSite || first->buildingType.empty())
		return std::cerr << "planner must place a catalog-declared construction site\n", EXIT_FAILURE;
	const auto repeated = runtime::NextObjectivePlannerCommand(world.Snapshot(), catalog, objectives);
	if (!repeated || repeated->a != first->a || repeated->buildingType != first->buildingType)
		return std::cerr << "planner output must be deterministic for the same snapshot\n", EXIT_FAILURE;
	auto completed = world.Snapshot();
	completed.buildings.push_back({});
	completed.buildings.back().id = 1;
	completed.buildings.back().origin = first->a;
	const auto* type = [&]() -> const economy::BuildingTypeDef* {
		for (const auto& candidate : catalog.BuildingTypes()) if (candidate.name == first->buildingType) return &candidate;
		return nullptr;
	}();
	if (!type) return EXIT_FAILURE;
	completed.buildings.back().type = type->id;
	completed.buildings.back().constructionSite = true;
	const auto next = runtime::NextObjectivePlannerCommand(completed, catalog, objectives);
	if (!next || next->kind != runtime::PlannerCommandKind::BuildRoad || next->roads.empty())
		return std::cerr << "planner must connect a placed building access cell to a road\n", EXIT_FAILURE;
	for (const auto& edge : next->roads)
		completed.roads.push_back({ static_cast<economy::RoadId>(completed.roads.size() + 1), edge.first, edge.second, true });
	const auto subsequent = runtime::NextObjectivePlannerCommand(completed, catalog, objectives);
	if (!subsequent || subsequent->kind != runtime::PlannerCommandKind::PlaceConstructionSite || subsequent->a == first->a)
		return std::cerr << "planner must avoid occupied footprints while satisfying remaining count\n", EXIT_FAILURE;
	std::cout << "Objective planner checks passed\n";
	return EXIT_SUCCESS;
}
