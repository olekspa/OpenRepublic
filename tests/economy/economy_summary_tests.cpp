#include "openrepublic/economy_api/economy_summary.h"

#include <cstdlib>
#include <iostream>

namespace {
void Check(bool value, const char* label) {
    if (!value) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}
}

int main() {
    using namespace openrepublic;
    economy::CatalogBuilder builder;
    economy::Catalog catalog;
    economy::CatalogErrorInfo error;
    economy::ResourceId grain{}, flour{};
    economy::BuildingTypeId mill{};
    const bool built = builder.AddResource("grain", &grain, error) &&
        builder.AddResource("flour", &flour, error) &&
        builder.AddBuildingType("mill", 1, 1, &mill, error) &&
        builder.AddStorage(mill, grain, 10, error) &&
        builder.AddStorage(mill, flour, 10, error) &&
        builder.AddRecipe(mill, {"mill", 1, {{grain, 3}}, {{flour, 2}}}, error) &&
        builder.Build(catalog, error);
    Check(built, "build summary fixture catalog");

    economy::World world(catalog, 8, 8);
    world.PlaceBuilding(mill, {2, 2});
    Check(world.Step().accepted, "place summary fixture mill");
    Check(world.Step().accepted, "stall fixture mill for missing input");
    const auto summary = economy_api::MakeEconomySummary(catalog, world.Snapshot());
    Check(summary.tick == 2 && summary.vehicles == 0, "summary captures tick and vehicle count");
    Check(summary.stock.size() == 2 && summary.stock[0].resource == "grain" &&
          summary.stock[0].amount == 0 && summary.stock[1].resource == "flour" &&
          summary.stock[1].amount == 0, "stock is complete and catalog ordered");
    Check(summary.unservedInputs.size() == 1 && summary.unservedInputs[0].resource == "grain" &&
          summary.unservedInputs[0].held == 0 && summary.unservedInputs[0].needed == 3,
          "missing-input stall reports held and per-cycle need");
    Check(summary.text == "Tick: 2\nPopulation: unavailable\nWorkers: unavailable\nStock:\n"
          "  grain: 0\n  flour: 0\nUnserved inputs (top 5):\n  grain at mill#1: 0/3\nVehicles: 0",
          "summary text is deterministic and labels unavailable population/workers");
    const auto again = economy_api::MakeEconomySummary(catalog, world.Snapshot());
    Check(again.text == summary.text, "repeated snapshot summary is byte stable");

    economy::CatalogBuilder populationBuilder;
    economy::Catalog populationCatalog;
    economy::BuildingTypeId home{}, workplace{};
    Check(populationBuilder.AddBuildingType("home", 1, 1, &home, error) &&
          populationBuilder.SetHousing(home, 20, error) &&
          populationBuilder.AddBuildingType("workplace", 1, 1, &workplace, error) &&
          populationBuilder.SetWorkers(workplace, 10, error) &&
          populationBuilder.Build(populationCatalog, error),
          "build opt-in population summary catalog");
    auto populationSnapshot = world.Snapshot();
    populationSnapshot.population = 14;
    populationSnapshot.workersAssigned = 6;
    populationSnapshot.workersRequired = 10;
    const auto populationSummary = economy_api::MakeEconomySummary(populationCatalog, populationSnapshot);
    Check(populationSummary.populationEnabled && populationSummary.workforceEnabled &&
          populationSummary.population == 14 && populationSummary.workersAssigned == 6 &&
          populationSummary.workersRequired == 10,
          "summary copies population and workforce totals from the snapshot");
    Check(populationSummary.text.find("Population: 14\nWorkers: 6/10") != std::string::npos,
          "opt-in summary text reports residents and assigned/required workers");
    economy::Catalog citizenCatalog;
    economy::ParseError citizenParse;
    Check(economy::ParseCatalogText("openrepublic-economy 1\nresource bread\nfood bread 1 every 1\ncitizens\nbuilding home 1 1\n housing 2\n end\n",
                                    citizenCatalog, citizenParse), "citizen summary catalog parses");
    populationSnapshot.happiness = 82;
    populationSnapshot.happyResidents = 11;
    const auto citizenSummary = economy_api::MakeEconomySummary(citizenCatalog, populationSnapshot);
    Check(citizenSummary.citizensEnabled && citizenSummary.happiness == 82 &&
          citizenSummary.happyResidents == 11 && citizenSummary.text.find("82/100") != std::string::npos,
          "summary exposes happiness and happy residents only for opt-in citizens");
    std::cout << "Economy summary contracts passed\n";
}
