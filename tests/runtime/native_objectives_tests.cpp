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
	auto constructionCatalog = economy::MakeSyntheticConstructionCatalog();
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
	auto heatGoal = runtime::NativeObjectiveSet{ "snapshot_goals", "Snapshot goals", {
		{ "heat", "Store heat", "Keep going", runtime::ObjectiveMetric::InventoryAtLeast, "heating_plant", "heat", 100 },
		{ "population", "Grow", "Keep going", runtime::ObjectiveMetric::PopulationAtLeast, "", "", 100 }
	} };
	progress = runtime::EvaluateNativeObjectives(heatGoal, completedSnapshot, catalog);
	Check(progress.finished && progress.completedCount == 2 && progress.fraction == 1.0,
		"inventory and population targets must evaluate from snapshot data");
	economy::BuildingTypeId mill{};
	economy::BuildingTypeId farm{};
	Check(constructionCatalog.FindBuildingType("mill", mill) &&
		constructionCatalog.FindBuildingType("farm", farm), "construction catalog must expose mill and farm types");
	economy::WorldSnapshot constructionSnapshot;
	economy::BuildingSnapshot finishedMill;
	finishedMill.id = 1;
	finishedMill.type = mill;
	finishedMill.constructionSite = false;
	finishedMill.constructionState = 2;
	constructionSnapshot.buildings.push_back(finishedMill);
	economy::BuildingSnapshot ordinaryFarm;
	ordinaryFarm.id = 2;
	ordinaryFarm.type = farm;
	ordinaryFarm.constructionSite = false;
	ordinaryFarm.constructionState = 2;
	constructionSnapshot.buildings.push_back(ordinaryFarm);
	const runtime::NativeObjectiveSet completedSiteGoal{ "completed_site", "Completed site", {
		{ "site_done", "Complete site", "Keep going", runtime::ObjectiveMetric::ConstructionSitesComplete, "mill", "", 1 }
	} };
	progress = runtime::EvaluateNativeObjectives(completedSiteGoal, constructionSnapshot, constructionCatalog);
	Check(progress.finished && progress.values.front() == 1,
		"completed construction is counted after its active-site flag clears, without counting ordinary buildings");
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
