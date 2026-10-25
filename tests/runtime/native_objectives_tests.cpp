#include "openrepublic/runtime/native_objectives.h"

#include <cstdlib>
#include <iostream>

namespace
{
	void Check(bool value, const char* message)
	{
		if (!value) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
	}
}

int main()
{
	using namespace openrepublic;
	auto catalog = economy::MakeSyntheticCoalCatalog();
	economy::World world(catalog, 64, 64);
	const auto snapshot = world.Snapshot();
	auto objectives = runtime::BuiltinCoalDemoObjectives();
	auto progress = runtime::EvaluateNativeObjectives(objectives, snapshot, catalog);
	Check(progress.completedCount == 0 && !progress.finished, "empty coal fixture must not complete goals");
	Check(progress.currentHint == objectives.objectives.front().hint, "first incomplete hint must be shown");
	Check(progress.fraction == 0.0, "empty fixture progress must be zero");

	// Exercise value lookup and ordered completion with snapshot-only data.
	economy::BuildingTypeId heating{};
	economy::ResourceId heat{};
	Check(catalog.FindBuildingType("heating_plant", heating), "coal catalog needs a heating plant");
	Check(catalog.FindResource("heat", heat), "coal catalog needs heat");
	auto completedSnapshot = snapshot;
	economy::BuildingSnapshot plant;
	plant.id = 1;
	plant.type = heating;
	plant.inventory.push_back({ heat, 100, 100 });
	completedSnapshot.buildings.push_back(plant);
	completedSnapshot.population = 100;
	plant.constructionSite = true;
	plant.constructionState = 2;
	completedSnapshot.buildings.back() = plant;
	auto heatGoal = runtime::NativeObjectiveSet{ "snapshot_goals", "Snapshot goals", {
		{ "heat", "Store heat", "Keep going", runtime::ObjectiveMetric::InventoryAtLeast, "heating_plant", "heat", 100 },
		{ "population", "Grow", "Keep going", runtime::ObjectiveMetric::PopulationAtLeast, "", "", 100 },
		{ "site_count", "Place site", "Keep going", runtime::ObjectiveMetric::ConstructionSitesCount, "heating_plant", "", 1 },
		{ "site_done", "Complete site", "Keep going", runtime::ObjectiveMetric::ConstructionSitesComplete, "heating_plant", "", 1 }
	} };
	progress = runtime::EvaluateNativeObjectives(heatGoal, completedSnapshot, catalog);
	Check(progress.finished && progress.completedCount == 4 && progress.fraction == 1.0,
		"inventory, population, and construction targets must evaluate from snapshot data");
	Check(progress.currentHint.empty(), "completed sets have no outstanding hint");

	const auto beforeHash = completedSnapshot.stateHash;
	(void)runtime::EvaluateNativeObjectives(heatGoal, completedSnapshot, catalog);
	Check(completedSnapshot.stateHash == beforeHash, "evaluation must not mutate snapshot state");

	const auto sites = runtime::BuiltinConstructionObjectives();
	const auto growth = runtime::BuiltinGrowthTownObjectives();
	Check(sites.objectives.size() == 2 && growth.objectives.size() == 2, "built-ins must have ordered goals");
	Check(runtime::EvaluateNativeObjectives({}, snapshot, catalog).fraction == 0.0, "empty objective list must be safe");
	std::cout << "Native objective evaluation checks passed.\n";
	return EXIT_SUCCESS;
}
