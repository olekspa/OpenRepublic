#include "openrepublic/runtime/economy_overview.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace
{
	void Check(bool value, const char* message)
	{
		if (!value) { std::cerr << message << '\n'; std::exit(1); }
	}
}

int main()
{
	auto catalog = openrepublic::economy::MakeSyntheticCatalog();
	const auto& resources = catalog.Resources();
	Check(!resources.empty(), "synthetic catalog must declare resources");
	const auto resource = resources.front().id;
	openrepublic::economy::WorldSnapshot before, now;
	before.tick = 10;
	before.buildings.push_back({});
	before.buildings.back().inventory.push_back({ resource, 4, 20 });
	now.tick = 14;
	now.treasury = 99;
	now.population = 12;
	now.happyResidents = 9;
	now.happiness = 75;
	now.workersAssigned = 3;
	now.workersRequired = 7;
	now.buildings.push_back({});
	now.buildings.back().inventory.push_back({ resource, 20, 20 });
	now.buildings.back().constructionSite = true;
	now.buildings.back().constructionState = 1;
	now.buildings.back().constructionProgress = 0.5;
	now.vehicles.push_back({});
	now.orders.push_back({});
	now.orders.back().id = 8;
	now.orders.back().resource = resource;
	now.orders.back().source = 2;
	now.orders.back().destination = 3;
	now.orders.back().vehicle = 4;
	now.orders.back().cargo = 6;
	const auto overview = openrepublic::runtime::BuildEconomyOverview(now, catalog, before);
	Check(overview.tick == 14 && overview.treasury == 99, "tick and treasury copied");
	Check(overview.happyResidents == 9 && overview.happiness == 75 && !overview.citizensAvailable,
		"citizen summary values copy while catalog opt-in remains explicit");
	Check(overview.truckCount == 1 && overview.activeHauls.size() == 1, "truck and haul included");
	Check(overview.activeHauls.front().id == 8 && overview.activeHauls.front().cargo == 6, "haul details copied");
	Check(overview.constructionSites.size() == 1 && overview.constructionSites.front().progress == 0.5,
		"active construction site copied");
	const auto found = std::find_if(overview.resources.begin(), overview.resources.end(),
		[resource](const auto& item) { return item.id == resource; });
	Check(found != overview.resources.end() && found->stored == 20, "inventory aggregated");
	Check(found->perTickDelta && *found->perTickDelta == 4.0, "delta normalized by tick span");
	auto noBaseline = openrepublic::runtime::BuildEconomyOverview(now, catalog);
	Check(!noBaseline.resources.empty() && !noBaseline.resources.front().perTickDelta,
		"rate unavailable without baseline");
	auto coalCatalog = openrepublic::economy::MakeSyntheticCoalCatalog();
	openrepublic::economy::WorldSnapshot heated;
	openrepublic::economy::BuildingTypeId plantType{};
	Check(coalCatalog.FindBuildingType("heating_plant", plantType), "coal catalog has heating plant");
	openrepublic::economy::BuildingSnapshot plant; plant.type = plantType; plant.recipes.push_back({});
	plant.recipes.front().completedCycles = 7; heated.buildings.push_back(plant);
	auto heatOverview = openrepublic::runtime::BuildEconomyOverview(heated, coalCatalog);
	Check(heatOverview.heatProduced == 7, "overview totals completed synthetic heat output");
	std::cout << "Economy overview checks passed\n";
}
