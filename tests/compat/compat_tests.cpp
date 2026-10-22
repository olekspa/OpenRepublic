#include "openrepublic/compat/catalog_import.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

using namespace openrepublic;

namespace {

int failures = 0;

void Check(bool condition, const char *message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n";
    }
}

content::BuildingResourceRate Rate(const char *resource, double value,
                                   bool workersBefore = false) {
    return {resource, value, 1, workersBefore};
}

content::BuildingDefinition MakeFarm() {
    content::BuildingDefinition definition;
    definition.production.push_back(Rate("plants", 25.0));
    definition.production.push_back(Rate("water", 5.0));
    return definition;
}

content::BuildingDefinition MakeFactory() {
    content::BuildingDefinition definition;
    definition.workersNeeded = 100.0;
    definition.consumption.push_back(Rate("plants", 0.25, true));
    definition.consumption.push_back(Rate("water", 0.05, true));
    definition.production.push_back(Rate("food", 0.12, true));
    return definition;
}

const economy::RecipeAmount *FindAmount(const std::vector<economy::RecipeAmount> &amounts,
                                        economy::ResourceId resource) {
    for (const auto &amount : amounts)
        if (amount.resource == resource)
            return &amount;
    return nullptr;
}

const economy::InventorySnapshot *FindInventory(const economy::BuildingSnapshot &building,
                                                economy::ResourceId resource) {
    for (const auto &item : building.inventory)
        if (item.resource == resource)
            return &item;
    return nullptr;
}

} // namespace

int main() {
    std::map<std::string, content::BuildingDefinition> definitions;
    auto farmDefinition = MakeFarm();
    farmDefinition.storage.push_back({"$STORAGE_MISC", "opaque", 50.0, "storage_only", 7});
    definitions.emplace("farm", farmDefinition);
    definitions.emplace("food_factory", MakeFactory());

    content::BuildingDefinition electricity;
    electricity.consumptionPerSecond.push_back(Rate("electricity", 0.01));
    definitions.emplace("electric_consumer", electricity);

    content::BuildingDefinition empty;
    definitions.emplace("decorative", empty);

    content::BuildingDefinition invalid;
    invalid.production.push_back(Rate("invalid resource", 1.0));
    definitions.emplace("bad_definition", invalid);

    content::BuildingDefinition tiny;
    tiny.production.push_back(Rate("dust", 0.00000001));
    definitions.emplace("tiny_producer", tiny);

    const compat::CatalogImportOptions options{24, 4, 10.0};
    economy::Catalog catalog;
    compat::ImportReport report;
    Check(compat::ImportCatalogFromDefinitions(definitions, options, catalog, report),
          "synthetic definitions import");
    Check(report.success && report.definitionsSeen == definitions.size() &&
              report.buildingsImported == 4 && report.buildingsSkipped == 2,
          "report counts imported and skipped buildings");
    Check(report.skippedBuildings.size() == 2 &&
              report.skippedBuildings[0].name == "bad_definition" &&
              report.skippedBuildings[1].name == "decorative",
          "skipped buildings are deterministic and include reasons");
    Check(report.resourceSpecificStorageEntriesSeen == 1,
          "resource-specific explicit storage entries are counted");
    Check(catalog.Resources().size() == 6 && catalog.BuildingTypes().size() == 4 &&
              catalog.VehicleTypes().size() == 1,
          "catalog includes sorted valid resources, economic buildings, and default truck");

    economy::ResourceId plants{}, water{}, food{}, electricityId{}, dust{}, storageOnly{};
    Check(catalog.FindResource("plants", plants) && catalog.FindResource("water", water) &&
              catalog.FindResource("food", food) &&
              catalog.FindResource("electricity", electricityId) &&
              catalog.FindResource("dust", dust) && catalog.FindResource("storage_only", storageOnly),
          "all valid economic resources are imported");
    Check(!catalog.FindResource("invalid resource", dust),
          "invalid resource name is excluded and its building is skipped");

    economy::BuildingTypeId farmId{}, factoryId{}, electricId{}, tinyId{};
    Check(catalog.FindBuildingType("farm", farmId), "farm building type is imported");
    Check(catalog.FindBuildingType("food_factory", factoryId) &&
              catalog.FindBuildingType("electric_consumer", electricId) &&
              catalog.FindBuildingType("tiny_producer", tinyId),
          "economic definitions are addressable as building types");
    const auto *factory = catalog.BuildingType(factoryId);
    const auto *electric = catalog.BuildingType(electricId);
    const auto *tinyType = catalog.BuildingType(tinyId);
    Check(factory && factory->recipes.size() == 1 && factory->recipes[0].name == "operate" &&
              factory->recipes[0].intervalTicks == 1,
          "each imported building has one operate recipe per tick");
    const auto *plantInput = factory ? FindAmount(factory->recipes[0].inputs, plants) : nullptr;
    const auto *waterInput = factory ? FindAmount(factory->recipes[0].inputs, water) : nullptr;
    const auto *foodOutput = factory ? FindAmount(factory->recipes[0].outputs, food) : nullptr;
    Check(plantInput && plantInput->amount == 1041667 && waterInput && waterInput->amount == 208333 &&
              foodOutput && foodOutput->amount == 500000,
          "worker-scaled daily rates convert to rounded micro-units per tick");
    const auto *electricInput = electric ? FindAmount(electric->recipes[0].inputs, electricityId)
                                         : nullptr;
    Check(electricInput && electricInput->amount == 25000,
          "loader-converted per-second consumption is multiplied by 60 per day");
    const auto *dustOutput = tinyType ? FindAmount(tinyType->recipes[0].outputs, dust) : nullptr;
    Check(dustOutput && dustOutput->amount == 1 && !report.warnings.empty(),
          "positive sub-micro tick rates round up and are reported");
    Check(factory && factory->width == 4 && factory->depth == 4,
          "footprints use configured OpenRepublic default");
    const auto factoryPlantStorage = factory
                                         ? std::find_if(factory->storage.begin(), factory->storage.end(),
                                                        [plants](const economy::StorageDef &slot) {
                                                            return slot.resource == plants;
                                                        })
                                         : std::vector<economy::StorageDef>::const_iterator{};
    Check(factory && factoryPlantStorage != factory->storage.end() &&
              factoryPlantStorage->capacity == 250000000,
          "inferred storage fallback uses the larger daily rate times configured days");
    economy::BuildingTypeId farmIdForStorage{};
    catalog.FindBuildingType("farm", farmIdForStorage);
    const auto *farmType = catalog.BuildingType(farmIdForStorage);
    Check(farmType && farmType->storage.size() == 2,
          "opaque explicit storage entries do not become capacity slots");
    Check(catalog.VehicleTypes()[0].name == "truck" &&
              catalog.VehicleTypes()[0].capacity == 20000000 &&
              catalog.VehicleTypes()[0].cellsPerTick == 2,
          "OpenRepublic default truck has specified capacity and speed");

    const std::string firstText = economy::WriteCatalogText(catalog);
    economy::Catalog secondCatalog;
    compat::ImportReport secondReport;
    Check(compat::ImportCatalogFromDefinitions(definitions, options, secondCatalog, secondReport) &&
              economy::WriteCatalogText(secondCatalog) == firstText,
          "repeated import produces identical catalog text");
    economy::Catalog parsed;
    economy::ParseError parseError;
    Check(economy::ParseCatalogText(firstText, parsed, parseError) &&
              economy::WriteCatalogText(parsed) == firstText,
          "catalog text round-trips canonically");

    auto storageEntry = content::BuildingStorageEntry{"$STORAGE_X", "opaque", 99.0, "plants", 9};
    definitions["farm"].storage.push_back(storageEntry);
    economy::Catalog withStorage;
    compat::ImportReport storageReport;
    Check(compat::ImportCatalogFromDefinitions(definitions, options, withStorage, storageReport) &&
              storageReport.resourceSpecificStorageEntriesSeen == 2,
          "report counts resource-bearing storage entries but does not map them");

    economy::World world(catalog, 20, 10);
    economy::ResourceId foodPlants{}, foodWater{};
    catalog.FindResource("plants", foodPlants);
    catalog.FindResource("water", foodWater);
    world.PlaceBuilding(farmId, {2, 3});
    world.PlaceBuilding(factoryId, {10, 3});
    world.Step();
    for (std::int32_t x = 3; x < 11; ++x)
        world.BuildRoad({x, 2}, {x + 1, 2});
    world.Step();
    const auto orderPlants = world.CreateHaulOrder(1, 2, foodPlants, 0);
    const auto orderWater = world.CreateHaulOrder(1, 2, foodWater, 0);
    world.Step();
    Check(world.GetCommandResult(orderPlants).state == economy::CommandState::Accepted &&
              world.GetCommandResult(orderWater).state == economy::CommandState::Accepted,
          "farm-to-factory haul orders are accepted");
    world.Step(800);
    const auto snapshot = world.Snapshot();
    const auto *factoryState = snapshot.buildings.size() > 1 ? &snapshot.buildings[1] : nullptr;
    const auto *foodInventory = factoryState ? FindInventory(*factoryState, food) : nullptr;
    Check(factoryState && factoryState->recipes[0].completedCycles > 0 && foodInventory &&
              foodInventory->amount > 0,
          "bounded world run hauls farm resources and produces food");

    economy::Catalog untouched = catalog;
    compat::CatalogImportOptions invalidOptions{0, 4, 10.0};
    compat::ImportReport invalidReport;
    Check(!compat::ImportCatalogFromDefinitions(definitions, invalidOptions, untouched, invalidReport) &&
              economy::WriteCatalogText(untouched) == firstText && !invalidReport.error.empty(),
          "invalid options fail without changing the output catalog");

    const std::string description = compat::DescribeImport(report);
    Check(description.find("OBSERVED in one reviewed build") != std::string::npos &&
              description.find("UNKNOWN") != std::string::npos,
          "human-readable report preserves evidence boundaries");
    compat::ImportReport preciseOptions;
    preciseOptions.options.storageDaysFallback = 1.00000001;
    const std::string preciseDescription = compat::DescribeImport(preciseOptions);
    const std::string fallbackLabel = "storageDaysFallback=";
    const std::size_t fallbackStart = preciseDescription.find(fallbackLabel);
    const std::size_t fallbackEnd = preciseDescription.find('\n', fallbackStart);
    const bool fallbackRoundTrips = fallbackStart != std::string::npos &&
                                    std::stod(preciseDescription.substr(
                                        fallbackStart + fallbackLabel.size(),
                                        fallbackEnd - fallbackStart - fallbackLabel.size())) ==
                                        preciseOptions.options.storageDaysFallback;
    Check(fallbackRoundTrips,
          "human-readable report preserves the configured fallback value");
    return failures == 0 ? 0 : 1;
}
