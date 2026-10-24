#include "openrepublic/compat/catalog_import.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
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
    empty.other.push_back({"$OPAQUE_TEST", {"value"}, 1});
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
    Check(report.directiveCoverage.at("$PRODUCTION").parsedTyped == 4 &&
              report.directiveCoverage.at("$CONSUMPTION_PER_SECOND").parsedTyped == 1 &&
              report.directiveCoverage.at("$STORAGE_MISC").parsedTyped == 1 &&
              report.directiveCoverage.at("$OPAQUE_TEST").ignored == 1,
          "directive coverage counts typed declarations and importer-ignored directives");
    Check(catalog.Resources().size() == 6 && catalog.BuildingTypes().size() == 4 &&
              catalog.VehicleTypes().size() == 1,
          "catalog includes sorted valid resources, economic buildings, and default truck");

    // Construction-cost opt-in is diagnostic-only until the native simulation
    // has construction-site state. It must not perturb the imported catalog.
    auto withRawCosts = definitions;
    withRawCosts["food_factory"].economicRaw = {
        {"$COST_RESOURCE_AUTO", {"opaque_auto", "2"}, {std::nullopt, 2.0}, 40},
        {"$COST_WORK", {"100"}, {100.0}, 41},
        {"$COST_WORK_BUILDING_NODE", {"3"}, {3.0}, 42},
        {"$COST_WORK_BUILDING_KEYWORD", {"keyword"}, {std::nullopt}, 43},
        {"$COST_WORK_BUILDING_ALL", {}, {}, 44},
        {"$COST_ALL_MULTIPLY", {"1.5"}, {1.5}, 45},
        {"$COST_RESOURCE", {"steel", "4"}, {std::nullopt, 4.0}, 46}};
    economy::Catalog legacyCostCatalog;
    compat::ImportReport legacyCostReport;
    Check(compat::ImportCatalogFromDefinitions(withRawCosts, options, legacyCostCatalog,
                                                legacyCostReport),
          "raw construction directives preserve default catalog import");
    auto diagnosticOptions = options;
    diagnosticOptions.constructionCosts = true;
    economy::Catalog diagnosticCostCatalog;
    compat::ImportReport diagnosticCostReport;
    Check(compat::ImportCatalogFromDefinitions(withRawCosts, diagnosticOptions,
                                                diagnosticCostCatalog, diagnosticCostReport),
          "construction-cost diagnostic import succeeds");
    Check(economy::WriteCatalogText(legacyCostCatalog) == economy::WriteCatalogText(catalog) &&
              economy::WriteCatalogText(diagnosticCostCatalog) == economy::WriteCatalogText(catalog),
          "construction cost option leaves canonical catalog bytes unchanged");
    Check(legacyCostReport.constructionCostDirectivesObserved == 0 &&
              diagnosticCostReport.constructionCostDirectivesObserved == 7 &&
              std::count_if(diagnosticCostReport.warnings.begin(), diagnosticCostReport.warnings.end(),
                            [](const std::string &warning) {
                                return warning.find("construction-site material delivery/work is unsupported") !=
                                       std::string::npos;
                            }) == 7,
          "opt-in reports every retained cost directive without guessing conversions");
    Check(diagnosticCostReport.directiveCoverage.at("$COST_RESOURCE_AUTO").retainedRaw == 1 &&
              diagnosticCostReport.directiveCoverage.at("$COST_WORK").retainedRaw == 1,
          "directive coverage identifies raw-retained economic declarations");

    economy::ResourceId plants{}, water{}, food{}, electricityId{}, dust{}, storageOnly{};
    Check(catalog.FindResource("plants", plants) && catalog.FindResource("water", water) &&
              catalog.FindResource("food", food) &&
              catalog.FindResource("electricity", electricityId) &&
              catalog.FindResource("dust", dust) && catalog.FindResource("storage_only", storageOnly),
          "all valid economic resources are imported");
    Check(!catalog.FindResource("invalid resource", dust),
          "invalid resource name is excluded and its building is skipped");

    content::BuildingDefinition exactResourceNames;
    exactResourceNames.production = {Rate("eletric", 1.0), Rate("electric", 2.0),
                                     Rate("Electric", 3.0)};
    economy::Catalog exactNameCatalog;
    compat::ImportReport exactNameReport;
    Check(compat::ImportCatalogFromDefinitions({{"exact_names", exactResourceNames}}, options,
                                                exactNameCatalog, exactNameReport) &&
              exactNameCatalog.FindResource("eletric", dust) &&
              exactNameCatalog.FindResource("electric", dust) &&
              exactNameCatalog.FindResource("Electric", dust) &&
              exactNameCatalog.Resources().size() == 3,
          "resource-name normalization preserves exact spelling and case");

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

    // Reviewed explicit-storage profiles are opt-in, exact (class, resource)
    // mappings. These synthetic cases exercise only adapter behavior.
    const compat::CatalogImportOptions profiledOptions{
        24, 4, 10.0, {{{"RESOURCE_TRANSPORT_COVERED", "grain"}, 3.0},
                      {{"RESOURCE_TRANSPORT_OPEN", "grain"}, 9.0},
                      {{"RESOURCE_TRANSPORT_COVERED", "ore"}, 2.5},
                      {{"RESOURCE_TRANSPORT_COVERED", "unknown"}, 1.0}}};
    content::BuildingDefinition profiled;
    profiled.production.push_back(Rate("grain", 0.0001));
    profiled.storage.push_back({"$STORAGE_IMPORT_SPECIAL", "RESOURCE_TRANSPORT_COVERED",
                                2.1, "grain", 10});
    content::BuildingDefinition unmatched;
    unmatched.production.push_back(Rate("grain", 1.0));
    unmatched.storage.push_back({"$STORAGE_EXPORT_SPECIAL", "UNPROFILED_CLASS", 5.0,
                                 "grain", 11});
    content::BuildingDefinition unsupported;
    unsupported.production.push_back(Rate("grain", 1.0));
    unsupported.storage.push_back({"$STORAGE_IMPORT", "RESOURCE_TRANSPORT_COVERED", 5.0,
                                  "grain", 111});
    content::BuildingDefinition duplicated;
    duplicated.production.push_back(Rate("ore", 1.0));
    duplicated.storage.push_back({"$STORAGE_SPECIAL", "RESOURCE_TRANSPORT_COVERED", 3.0,
                                  "ore", 12});
    duplicated.storage.push_back({"$STORAGE_IMPORT_SPECIAL", "RESOURCE_TRANSPORT_COVERED", 4.0,
                                  "ore", 13});
    content::BuildingDefinition shared;
    shared.production.push_back(Rate("grain", 1.0));
    shared.storage.push_back({"$STORAGE_SPECIAL", "RESOURCE_TRANSPORT_COVERED", 2.0,
                              "grain", 14});
    shared.storage.push_back({"$STORAGE", "RESOURCE_TRANSPORT_COVERED", 100.0, "", 15});
    content::BuildingDefinition storageOnlyDefinition;
    storageOnlyDefinition.storage.push_back({"$STORAGE_EXPORT_SPECIAL", "RESOURCE_TRANSPORT_COVERED",
                                             3.0, "ore", 16});
    std::map<std::string, content::BuildingDefinition> profileDefinitions{
        {"duplicate", duplicated}, {"profiled", profiled}, {"shared", shared},
        {"storage_only", storageOnlyDefinition}, {"unmatched", unmatched},
        {"unsupported", unsupported}};
    economy::Catalog profiledCatalog;
    compat::ImportReport profiledReport;
    Check(compat::ImportCatalogFromDefinitions(profileDefinitions, profiledOptions,
                                                profiledCatalog, profiledReport),
          "explicit storage profile definitions import");
    economy::BuildingTypeId profiledId{}, unmatchedId{}, duplicateId{}, sharedId{}, storageOnlyId{},
        unsupportedId{};
    profiledCatalog.FindBuildingType("profiled", profiledId);
    profiledCatalog.FindBuildingType("unmatched", unmatchedId);
    profiledCatalog.FindBuildingType("duplicate", duplicateId);
    profiledCatalog.FindBuildingType("shared", sharedId);
    profiledCatalog.FindBuildingType("storage_only", storageOnlyId);
    profiledCatalog.FindBuildingType("unsupported", unsupportedId);
    const auto *profiledType = profiledCatalog.BuildingType(profiledId);
    const auto *unmatchedType = profiledCatalog.BuildingType(unmatchedId);
    const auto *duplicateType = profiledCatalog.BuildingType(duplicateId);
    const auto *sharedType = profiledCatalog.BuildingType(sharedId);
    const auto *storageOnlyType = profiledCatalog.BuildingType(storageOnlyId);
    const auto *unsupportedType = profiledCatalog.BuildingType(unsupportedId);
    economy::ResourceId grainId{}, oreId{};
    profiledCatalog.FindResource("grain", grainId);
    profiledCatalog.FindResource("ore", oreId);
    auto capacityFor = [](const economy::BuildingTypeDef *type, economy::ResourceId resource) {
        if (!type)
            return std::uint64_t{};
        for (const auto &slot : type->storage)
            if (slot.resource == resource)
                return slot.capacity;
        return std::uint64_t{};
    };
    Check(capacityFor(profiledType, grainId) == 7,
          "explicit profile capacity uses exact class/resource mapping and rounds up");
    Check(capacityFor(unmatchedType, grainId) == 10000000,
          "unknown class retains inferred rate fallback");
    Check(capacityFor(unsupportedType, grainId) == 10000000,
          "unreviewed storage directives retain rate fallback");
    Check(capacityFor(duplicateType, oreId) == 10000000 &&
              profiledReport.ambiguousStorageResources == 1,
          "duplicate resource rows are ambiguous and use rate fallback without summing");
    Check(capacityFor(sharedType, grainId) == 10000000,
          "any general storage row disables explicit mappings for that definition");
    Check(storageOnlyType && storageOnlyType->recipes.empty() &&
              capacityFor(storageOnlyType, oreId) == 8 &&
              profiledReport.storageOnlyBuildingsImported == 1,
          "mapped storage-only definition imports without a recipe and rounds capacity up");
    Check(profiledReport.storageEntriesMapped == 2 &&
              profiledReport.storageEntriesUnmapped == 5,
          "profile report counts mapped and unmapped resource-bearing rows");
    Check(std::is_sorted(profiledReport.storageDiagnostics.begin(),
                         profiledReport.storageDiagnostics.end()),
          "storage diagnostics are sorted deterministically");

    content::BuildingDefinition binary64TenTenths;
    binary64TenTenths.storage.push_back({"$STORAGE_SPECIAL", "CLASS_A", 10.0,
                                         "binary_a", 20});
    content::BuildingDefinition binary64HundredHundredths;
    binary64HundredHundredths.storage.push_back({"$STORAGE_SPECIAL", "CLASS_B", 100.0,
                                                  "binary_b", 21});
    const compat::CatalogImportOptions binary64Options{
        24, 4, 10.0, {{{"CLASS_A", "binary_a"}, 0.1}, {{"CLASS_B", "binary_b"}, 0.01}}};
    economy::Catalog binary64Catalog;
    compat::ImportReport binary64Report;
    Check(compat::ImportCatalogFromDefinitions(
              {{"binary_a", binary64TenTenths}, {"binary_b", binary64HundredHundredths}},
              binary64Options, binary64Catalog, binary64Report),
          "binary64 storage product fixtures import");
    economy::BuildingTypeId binaryAId{}, binaryBId{};
    economy::ResourceId binaryAResource{}, binaryBResource{};
    binary64Catalog.FindBuildingType("binary_a", binaryAId);
    binary64Catalog.FindBuildingType("binary_b", binaryBId);
    binary64Catalog.FindResource("binary_a", binaryAResource);
    binary64Catalog.FindResource("binary_b", binaryBResource);
    Check(capacityFor(binary64Catalog.BuildingType(binaryAId), binaryAResource) == 1 &&
              capacityFor(binary64Catalog.BuildingType(binaryBId), binaryBResource) == 1,
          "explicit amount-factor products round to binary64 before ceiling");
    economy::Catalog profiledAgain;
    compat::ImportReport profiledAgainReport;
    Check(compat::ImportCatalogFromDefinitions(profileDefinitions, profiledOptions,
                                                profiledAgain, profiledAgainReport) &&
              economy::WriteCatalogText(profiledAgain) == economy::WriteCatalogText(profiledCatalog) &&
              compat::DescribeImport(profiledAgainReport) == compat::DescribeImport(profiledReport),
          "profiled catalog and diagnostics are deterministic");

    content::BuildingDefinition invalidAmount;
    invalidAmount.storage.push_back({"$STORAGE_SPECIAL", "RESOURCE_TRANSPORT_COVERED", 0.0,
                                     "grain", 17});
    content::BuildingDefinition overflowAmount;
    overflowAmount.storage.push_back({"$STORAGE_SPECIAL", "RESOURCE_TRANSPORT_COVERED",
                                      std::numeric_limits<double>::max(), "grain", 18});
    const compat::CatalogImportOptions overflowingProfile{
        24, 4, 10.0, {{{"RESOURCE_TRANSPORT_COVERED", "grain"},
                      std::numeric_limits<double>::max()}}};
    economy::Catalog rejectionCatalog;
    compat::ImportReport rejectionReport;
    Check(compat::ImportCatalogFromDefinitions(
              {{"invalid_amount", invalidAmount}}, profiledOptions, rejectionCatalog,
              rejectionReport) &&
              rejectionReport.buildingsSkipped == 1 &&
              rejectionReport.skippedBuildings[0].reason.find("invalid or overflowing") !=
                  std::string::npos &&
              rejectionCatalog.BuildingTypes().empty(),
          "invalid explicit amount skips before adding a building to the catalog");
    Check(compat::ImportCatalogFromDefinitions(
              {{"overflow_amount", overflowAmount}}, overflowingProfile, rejectionCatalog,
              rejectionReport) &&
              rejectionReport.buildingsSkipped == 1 && rejectionCatalog.BuildingTypes().empty(),
          "overflowing explicit capacity skips before builder mutation");

    content::BuildingDefinition laterInvalid;
    laterInvalid.storage.push_back({"$STORAGE_SPECIAL", "RESOURCE_TRANSPORT_COVERED", 1.0,
                                    "grain", 181});
    laterInvalid.storage.push_back({"$STORAGE_SPECIAL", "RESOURCE_TRANSPORT_COVERED", 0.0,
                                    "ore", 182});
    const compat::CatalogImportOptions laterInvalidProfile{
        24, 4, 10.0, {{{"RESOURCE_TRANSPORT_COVERED", "grain"}, 1.0},
                      {{"RESOURCE_TRANSPORT_COVERED", "ore"}, 1.0}}};
    Check(compat::ImportCatalogFromDefinitions({{"later_invalid", laterInvalid}},
                                                laterInvalidProfile, rejectionCatalog,
                                                rejectionReport) &&
              rejectionReport.buildingsSkipped == 1 &&
              rejectionReport.storageEntriesMapped == 0 &&
              std::none_of(rejectionReport.storageDiagnostics.begin(),
                           rejectionReport.storageDiagnostics.end(),
                           [](const std::string &diagnostic) {
                               return diagnostic.find("mapped to") != std::string::npos;
                           }),
          "a later invalid row discards earlier provisional mappings from the report");

    content::BuildingDefinition undersized;
    undersized.production.push_back(Rate("grain", 1.0));
    undersized.storage.push_back({"$STORAGE_SPECIAL", "RESOURCE_TRANSPORT_COVERED", 1.0,
                                  "grain", 19});
    const compat::CatalogImportOptions undersizedProfile{
        24, 4, 10.0, {{{"RESOURCE_TRANSPORT_COVERED", "grain"}, 1.0}}};
    Check(compat::ImportCatalogFromDefinitions({{"undersized", undersized}}, undersizedProfile,
                                                rejectionCatalog, rejectionReport) &&
              rejectionReport.buildingsSkipped == 1 && rejectionCatalog.BuildingTypes().empty() &&
              rejectionReport.storageEntriesMapped == 0 &&
              std::none_of(rejectionReport.storageDiagnostics.begin(),
                           rejectionReport.storageDiagnostics.end(),
                           [](const std::string &diagnostic) {
                               return diagnostic.find("mapped to") != std::string::npos;
                           }) &&
              rejectionReport.skippedBuildings[0].reason.find("below one recipe tick") !=
                  std::string::npos,
          "explicit capacity below one recipe tick skips before builder mutation");
    economy::Catalog unchanged = catalog;
    compat::ImportReport invalidProfileReport;
    const compat::CatalogImportOptions invalidProfile{
        24, 4, 10.0, {{{"invalid class", "grain"}, 1.0}}};
    Check(!compat::ImportCatalogFromDefinitions({}, invalidProfile, unchanged,
                                                 invalidProfileReport) &&
              economy::WriteCatalogText(unchanged) == firstText,
          "invalid profile key fails transactionally without changing output catalog");

    economy::Catalog noProfileCatalog;
    compat::ImportReport noProfileReport;
    Check(compat::ImportCatalogFromDefinitions({{"profiled", profiled}}, options,
                                                noProfileCatalog, noProfileReport) &&
              noProfileReport.storageEntriesMapped == 0 &&
              noProfileCatalog.BuildingTypes().size() == 1 &&
              noProfileCatalog.BuildingTypes()[0].storage[0].capacity == 1000,
          "without a profile the legacy rate-based storage behavior remains active");

    content::BuildingDefinition powerConsumer;
    powerConsumer.consumptionPerSecond.push_back(Rate("electricity", 0.01));
    powerConsumer.storage.push_back({"$STORAGE_IMPORT_SPECIAL", "RESOURCE_TRANSPORT_COVERED",
                                     100.0, "electricity", 22});
    content::BuildingDefinition utilityGenerator;
    utilityGenerator.consumption.push_back(Rate("power", 2.0));
    utilityGenerator.production.push_back(Rate("power", 5.0));
    content::BuildingDefinition materialProducer;
    materialProducer.production.push_back(Rate("food", 1.0));
    std::map<std::string, content::BuildingDefinition> utilityDefinitions{
        {"material_producer", materialProducer}, {"power_consumer", powerConsumer},
        {"utility_generator", utilityGenerator}};
    compat::CatalogImportOptions utilityOptions;
    utilityOptions.utilityResources = {"electricity", "power"};
    economy::Catalog utilityCatalog;
    compat::ImportReport utilityReport;
    Check(compat::ImportCatalogFromDefinitions(utilityDefinitions, utilityOptions,
                                                utilityCatalog, utilityReport),
          "utility-marked catalog import");
    economy::ResourceId electricityResource{}, powerResource{}, foodResource{};
    utilityCatalog.FindResource("electricity", electricityResource);
    utilityCatalog.FindResource("power", powerResource);
    utilityCatalog.FindResource("food", foodResource);
    const auto *electricityDefinition = utilityCatalog.Resource(electricityResource);
    const auto *powerDefinition = utilityCatalog.Resource(powerResource);
    economy::BuildingTypeId powerConsumerId{}, materialProducerId{}, utilityGeneratorId{};
    utilityCatalog.FindBuildingType("power_consumer", powerConsumerId);
    utilityCatalog.FindBuildingType("material_producer", materialProducerId);
    const bool generatorWasImported =
        utilityCatalog.FindBuildingType("utility_generator", utilityGeneratorId);
    const auto *powerConsumerType = utilityCatalog.BuildingType(powerConsumerId);
    const auto *materialProducerType = utilityCatalog.BuildingType(materialProducerId);
    Check(electricityDefinition && electricityDefinition->utility &&
              powerDefinition && powerDefinition->utility &&
              utilityReport.utilityResourcesImported == 2,
          "requested discovered resources are created as utilities");
    Check(powerConsumerType && powerConsumerType->storage.empty() &&
              powerConsumerType->recipes.size() == 1 &&
              powerConsumerType->recipes[0].inputs.size() == 1 &&
              powerConsumerType->recipes[0].inputs[0].amount == 25000,
          "utility recipe rate conversion remains unchanged while utility storage is omitted");
    Check(utilityReport.utilityStorageEntriesIgnored == 1 &&
              std::any_of(utilityReport.storageDiagnostics.begin(),
                          utilityReport.storageDiagnostics.end(),
                          [](const std::string &diagnostic) {
                              return diagnostic.find("utility resource 'electricity' ignored") !=
                                     std::string::npos;
                          }),
          "explicit storage rows for utilities are diagnosed and ignored");
    Check(!generatorWasImported && materialProducerType &&
              materialProducerType->storage.size() == 1 &&
              utilityReport.skippedBuildings.size() == 1 &&
              utilityReport.skippedBuildings[0].name == "utility_generator",
          "utility generator chains are rejected before the building is added");
    economy::Catalog utilityRoundTrip;
    economy::ParseError utilityParseError;
    const auto utilityCatalogText = economy::WriteCatalogText(utilityCatalog);
    Check(economy::ParseCatalogText(utilityCatalogText, utilityRoundTrip, utilityParseError) &&
              utilityRoundTrip.Resources().size() == utilityCatalog.Resources().size() &&
              utilityRoundTrip.Resources()[electricityResource].utility,
          "canonical catalog round-trip preserves the utility resource kind");

    economy::Catalog unmarkedUtilityCatalog;
    compat::ImportReport unmarkedUtilityReport;
    Check(compat::ImportCatalogFromDefinitions({{"power_consumer", powerConsumer}}, options,
                                                unmarkedUtilityCatalog, unmarkedUtilityReport),
          "unmarked electricity keeps legacy material catalog import");
    economy::ResourceId unmarkedElectricity{};
    unmarkedUtilityCatalog.FindResource("electricity", unmarkedElectricity);
    economy::BuildingTypeId unmarkedConsumerId{};
    unmarkedUtilityCatalog.FindBuildingType("power_consumer", unmarkedConsumerId);
    const auto *unmarkedConsumer = unmarkedUtilityCatalog.BuildingType(unmarkedConsumerId);
    Check(unmarkedUtilityCatalog.Resource(unmarkedElectricity) &&
              !unmarkedUtilityCatalog.Resource(unmarkedElectricity)->utility &&
              unmarkedConsumer && unmarkedConsumer->storage.size() == 1 &&
              unmarkedUtilityReport.utilityResourcesImported == 0,
          "unmarked electricity retains ordinary storage behavior");

    economy::Catalog unchangedForUtilityError = catalog;
    compat::CatalogImportOptions unknownUtilityOptions;
    unknownUtilityOptions.utilityResources.insert("eletricity_typo");
    compat::ImportReport unknownUtilityReport;
    Check(!compat::ImportCatalogFromDefinitions({{"power_consumer", powerConsumer}},
                                                 unknownUtilityOptions,
                                                 unchangedForUtilityError,
                                                 unknownUtilityReport) &&
              economy::WriteCatalogText(unchangedForUtilityError) == firstText,
          "unknown utility resource option fails without changing the output catalog");
    return failures == 0 ? 0 : 1;
}
