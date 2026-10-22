#include "openrepublic/economy/economy.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

using namespace openrepublic::economy;

namespace {
int failures = 0;
void Check(bool condition, const char *message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

struct Fixture {
    Catalog catalog;
    ResourceId fuel{}, stock{}, product{}, power{}, heat{};
    BuildingTypeId generator{}, consumer{}, relay{}, material{};
};

Fixture MakeFixture(bool stockInput = false, std::uint64_t outputCapacity = 8,
                    bool twoUtilities = false, std::uint64_t powerOutput = 4,
                    std::uint64_t heatOutput = 1) {
    Fixture f;
    CatalogBuilder b;
    CatalogErrorInfo e;
    bool ok = b.AddResource("fuel", &f.fuel, e) && b.AddResource("stock", &f.stock, e) &&
              b.AddResource("product", &f.product, e) && b.AddUtilityResource("power", &f.power, e) &&
              (!twoUtilities || b.AddUtilityResource("heat", &f.heat, e)) &&
              b.AddBuildingType("generator", 1, 1, &f.generator, e) &&
              b.AddBuildingType("consumer", 1, 1, &f.consumer, e) &&
              b.AddBuildingType("relay", 1, 1, &f.relay, e) &&
              b.AddBuildingType("material", 1, 1, &f.material, e) &&
              b.AddStorage(f.generator, f.fuel, 8, e) && b.AddStorage(f.consumer, f.product, outputCapacity, e) &&
              b.AddStorage(f.consumer, f.stock, 8, e) &&
              b.AddRecipe(f.generator, {"generate", 1, {}, {{f.power, powerOutput}},}, e);
    if (twoUtilities)
        ok = ok && b.AddRecipe(f.generator, {"generate_heat", 1, {}, {{f.heat, heatOutput}},}, e);
    std::vector<RecipeAmount> inputs;
    if (stockInput)
        inputs.push_back({f.stock, 1});
    inputs.push_back({f.power, 2});
    if (twoUtilities)
        inputs.push_back({f.heat, 2});
    ok = ok && b.AddRecipe(f.consumer, {"work", 1, inputs, {{f.product, 1}}}, e) &&
         b.AddStorage(f.material, f.product, 8, e) && b.AddRecipe(f.material, {"material", 1, {}, {{f.product, 1}}}, e) &&
         b.AddVehicleType("truck", 4, 1, nullptr, e) && b.Build(f.catalog, e);
    if (!ok)
        std::cerr << "fixture error: " << e.message << '\n';
    Check(ok, "utility diagnostics fixture catalog builds");
    return f;
}

const UtilityNetworkDiagnostic *Network(const UtilityDiagnostics &d, ResourceId resource, BuildingId component) {
    for (const auto &entry : d.networks)
        if (entry.resource == resource && entry.component == component)
            return &entry;
    return nullptr;
}
bool Is(UtilityAmount amount, std::uint64_t low, std::uint64_t high = 0) {
    return amount.low == low && amount.high == high;
}

void FormatBoundaries() {
    Check(FormatUtilityAmount({0, 0}) == "0", "zero formats as decimal zero");
    Check(FormatUtilityAmount({UINT64_MAX, 0}) == "18446744073709551615",
          "maximum low limb formats exactly");
    Check(FormatUtilityAmount({0, 1}) == "18446744073709551616",
          "one high limb formats exactly");
    Check(FormatUtilityAmount({UINT64_MAX, UINT64_MAX}) ==
              "340282366920938463463374607431768211455",
          "maximum unsigned 128-bit value formats exactly without compiler extensions");
}

void ComponentFlowAndTopology() {
    const auto f = MakeFixture();
    World world(f.catalog, 20, 10);
    world.PlaceBuilding(f.generator, {1, 1});
    world.PlaceBuilding(f.consumer, {4, 1});
    world.PlaceBuilding(f.consumer, {7, 1});
    world.PlaceBuilding(f.material, {10, 1});
    Check(!world.QueryUtilityDiagnostics().available, "receipt starts unavailable before a utility tick");
    world.Step();
    auto receipt = world.QueryUtilityDiagnostics();
    Check(receipt.available && receipt.tick == 1, "first utility phase publishes a tick-marked receipt");
    const auto *connected = Network(receipt, f.power, 1);
    const auto *isolated = Network(receipt, f.power, 3);
    Check(connected && Is(connected->generated, 4) && Is(connected->requested, 0) &&
              Is(connected->consumed, 0) && Is(connected->unused, 4),
          "isolated generator receipt reports its committed offer as unused");
    Check(isolated && Is(isolated->requested, 2) && Is(isolated->consumed, 0) &&
              isolated->starvedRecipes == 1,
          "disconnected consumer demand is measured on its own component");
    Check(receipt.networks.size() == 3, "recipe-used isolated components are included");
    bool unrelatedIncluded = false;
    for (const auto &entry : receipt.networks)
        for (const auto member : entry.members)
            unrelatedIncluded = unrelatedIncluded || member == 4;
    Check(!unrelatedIncluded, "unrelated isolated material-only building is omitted from utility membership");

    world.ConnectUtility(f.power, 1, 2);
    world.Step();
    receipt = world.QueryUtilityDiagnostics();
    connected = Network(receipt, f.power, 1);
    isolated = Network(receipt, f.power, 3);
    Check(connected && connected->members == std::vector<BuildingId>({1, 2}) &&
              Is(connected->generated, 4) && Is(connected->requested, 2) &&
              Is(connected->consumed, 2) && Is(connected->unused, 2),
          "connected component reports exact generated, requested, consumed, and unused amounts");
    Check(isolated && isolated->starvedRecipes == 1, "disconnected component remains independently starved");

    world.ConnectUtility(f.power, 2, 3);
    world.Step();
    receipt = world.QueryUtilityDiagnostics();
    connected = Network(receipt, f.power, 1);
    Check(receipt.networks.size() == 1 && connected &&
              connected->members == std::vector<BuildingId>({1, 2, 3}) &&
              Is(connected->requested, 4) && Is(connected->consumed, 4) &&
              Is(connected->unused, 0),
          "link merge combines demand and includes every connected building");

    world.DisconnectUtility(f.power, 1, 2);
    world.Step();
    receipt = world.QueryUtilityDiagnostics();
    const auto *generatorNetwork = Network(receipt, f.power, 1);
    const auto *consumerNetwork = Network(receipt, f.power, 2);
    Check(receipt.networks.size() == 2 && generatorNetwork && generatorNetwork->members.size() == 1 &&
              consumerNetwork && consumerNetwork->members == std::vector<BuildingId>({2, 3}) &&
              Is(consumerNetwork->requested, 4) && Is(consumerNetwork->consumed, 0) &&
              consumerNetwork->starvedRecipes == 2,
          "split components preserve relay membership and count short recipes");
}

void WideAggregateConservation() {
    const auto f = MakeFixture(false, 8, false, UINT64_MAX);
    World world(f.catalog, 16, 8);
    world.PlaceBuilding(f.generator, {1, 1});
    world.PlaceBuilding(f.generator, {4, 1});
    world.PlaceBuilding(f.consumer, {7, 1});
    world.ConnectUtility(f.power, 1, 3);
    world.ConnectUtility(f.power, 2, 3);
    world.Step();
    const auto receipt = world.QueryUtilityDiagnostics();
    const auto *network = Network(receipt, f.power, 1);
    Check(network && network->generated.low == UINT64_MAX - 1 && network->generated.high == 1 &&
              Is(network->requested, 2) && Is(network->consumed, 2) &&
              network->unused.low == UINT64_MAX - 3 && network->unused.high == 1,
          "component diagnostics conserve aggregate generation above 64-bit range");
}

void EligibilityAndAtomicShortage() {
    const auto stockFixture = MakeFixture(true);
    World stockWorld(stockFixture.catalog, 12, 8);
    stockWorld.PlaceBuilding(stockFixture.generator, {1, 1});
    stockWorld.PlaceBuilding(stockFixture.consumer, {4, 1});
    stockWorld.ConnectUtility(stockFixture.power, 1, 2);
    stockWorld.Step();
    auto receipt = stockWorld.QueryUtilityDiagnostics();
    Check(Is(Network(receipt, stockFixture.power, 1)->requested, 0) &&
              Network(receipt, stockFixture.power, 1)->starvedRecipes == 0,
          "missing stock input excludes demand and starvation from the receipt");

    const auto fullFixture = MakeFixture(false, 1);
    World fullWorld(fullFixture.catalog, 12, 8);
    fullWorld.PlaceBuilding(fullFixture.generator, {1, 1});
    fullWorld.PlaceBuilding(fullFixture.consumer, {4, 1});
    fullWorld.ConnectUtility(fullFixture.power, 1, 2);
    fullWorld.Step();
    receipt = fullWorld.QueryUtilityDiagnostics();
    const auto *firstFullNetwork = Network(receipt, fullFixture.power, 1);
    Check(firstFullNetwork && Is(firstFullNetwork->requested, 2),
          "consumer with output room contributes eligible demand");
    fullWorld.Step();
    receipt = fullWorld.QueryUtilityDiagnostics();
    const auto *fullNetwork = Network(receipt, fullFixture.power, 1);
    Check(fullNetwork && Is(fullNetwork->requested, 0) && Is(fullNetwork->consumed, 0) &&
              fullNetwork->starvedRecipes == 0,
          "full output storage excludes demand and starvation");

    const auto multi = MakeFixture(false, 8, true, 3, 1);
    World multiWorld(multi.catalog, 12, 8);
    multiWorld.PlaceBuilding(multi.generator, {1, 1});
    multiWorld.PlaceBuilding(multi.consumer, {4, 1});
    multiWorld.ConnectUtility(multi.power, 1, 2);
    multiWorld.ConnectUtility(multi.heat, 1, 2);
    multiWorld.Step();
    receipt = multiWorld.QueryUtilityDiagnostics();
    const auto *power = Network(receipt, multi.power, 1);
    const auto *heat = Network(receipt, multi.heat, 1);
    Check(power && heat && Is(power->requested, 2) && Is(heat->requested, 2) &&
              Is(power->consumed, 0) && Is(heat->consumed, 0) && power->starvedRecipes == 0 &&
              heat->starvedRecipes == 1,
          "atomic multi-utility recipe records eligible demand but debits neither network when one is short");

    const auto bothShort = MakeFixture(false, 8, true, 1, 1);
    World bothShortWorld(bothShort.catalog, 12, 8);
    bothShortWorld.PlaceBuilding(bothShort.generator, {1, 1});
    bothShortWorld.PlaceBuilding(bothShort.consumer, {4, 1});
    bothShortWorld.ConnectUtility(bothShort.power, 1, 2);
    bothShortWorld.ConnectUtility(bothShort.heat, 1, 2);
    bothShortWorld.Step();
    receipt = bothShortWorld.QueryUtilityDiagnostics();
    const auto *shortPower = Network(receipt, bothShort.power, 1);
    const auto *shortHeat = Network(receipt, bothShort.heat, 1);
    Check(shortPower && shortHeat && shortPower->starvedRecipes == 1 && shortHeat->starvedRecipes == 1 &&
              bothShortWorld.Explain(2).resource == bothShort.power,
          "multiple shortages are counted on both networks while the stall keeps the first input resource");
}

void DisabledFuelStarvationAndRelays() {
    const auto f = MakeFixture();
    World world(f.catalog, 16, 8);
    world.PlaceBuilding(f.generator, {1, 1});
    world.PlaceBuilding(f.relay, {4, 1});
    world.PlaceBuilding(f.consumer, {7, 1});
    world.ConnectUtility(f.power, 1, 2);
    world.ConnectUtility(f.power, 2, 3);
    world.Step();
    auto receipt = world.QueryUtilityDiagnostics();
    const auto *network = Network(receipt, f.power, 1);
    Check(network && network->members == std::vector<BuildingId>({1, 2, 3}) &&
              Is(network->generated, 4) && Is(network->consumed, 2),
          "relay appears in a used network and carries supply");
    const auto disable = world.SetBuildingEnabled(3, false);
    world.Step();
    receipt = world.QueryUtilityDiagnostics();
    network = Network(receipt, f.power, 1);
    Check(world.GetCommandResult(disable).state == CommandState::Accepted && network &&
              Is(network->requested, 0) && Is(network->consumed, 0),
          "disabled consumer contributes no demand or debit");

    CatalogBuilder b;
    CatalogErrorInfo e;
    ResourceId fuel{}, product{}, power{};
    BuildingTypeId generator{}, consumer{};
    bool ok = b.AddResource("fuel", &fuel, e) && b.AddResource("product", &product, e) &&
              b.AddUtilityResource("power", &power, e) && b.AddBuildingType("generator", 1, 1, &generator, e) &&
              b.AddBuildingType("consumer", 1, 1, &consumer, e) && b.AddStorage(generator, fuel, 4, e) &&
              b.AddStorage(consumer, product, 4, e) &&
              b.AddRecipe(generator, {"generate", 1, {{fuel, 1}}, {{power, 4}}}, e) &&
              b.AddRecipe(consumer, {"use", 1, {{power, 2}}, {{product, 1}}}, e);
    Catalog fuelCatalog;
    ok = ok && b.Build(fuelCatalog, e);
    Check(ok, "fuel starvation fixture builds");
    World fuelWorld(fuelCatalog, 12, 8);
    fuelWorld.PlaceBuilding(generator, {1, 1});
    fuelWorld.PlaceBuilding(consumer, {4, 1});
    fuelWorld.ConnectUtility(power, 1, 2);
    fuelWorld.Step();
    receipt = fuelWorld.QueryUtilityDiagnostics();
    network = Network(receipt, power, 1);
    Check(network && Is(network->generated, 0) && Is(network->requested, 2) &&
              Is(network->consumed, 0) && network->starvedRecipes == 1,
          "fuel-starved generator offers nothing and connected recipe shortage is visible");
}

void ObservationLifecycle() {
    const auto f = MakeFixture();
    World world(f.catalog, 12, 8);
    world.PlaceBuilding(f.generator, {1, 1});
    world.PlaceBuilding(f.consumer, {4, 1});
    world.ConnectUtility(f.power, 1, 2);
    world.Step();
    const auto originalSave = SaveWorld(world);
    const auto originalHash = world.Snapshot().stateHash;
    Check(world.QueryUtilityDiagnostics().available, "completed utility tick exposes receipt");

    World restored(f.catalog, 1, 1);
    ParseError error;
    Check(LoadWorld(originalSave, f.catalog, restored, error) &&
              !restored.QueryUtilityDiagnostics().available && SaveWorld(restored) == originalSave &&
              restored.Snapshot().stateHash == originalHash,
          "loading clears observations without altering canonical save or hash");
    std::string replaceError;
    Check(world.ReplaceCatalog(f.catalog, replaceError) &&
              !world.QueryUtilityDiagnostics().available,
          "successful catalog replacement clears the previous tick receipt");

    CatalogBuilder materialBuilder;
    CatalogErrorInfo catalogError;
    ResourceId grain{};
    BuildingTypeId farm{};
    Catalog materialCatalog;
    bool ok = materialBuilder.AddResource("grain", &grain, catalogError) &&
              materialBuilder.AddBuildingType("farm", 1, 1, &farm, catalogError) &&
              materialBuilder.AddStorage(farm, grain, 4, catalogError) &&
              materialBuilder.AddRecipe(farm, {"grow", 1, {}, {{grain, 1}}}, catalogError) &&
              materialBuilder.Build(materialCatalog, catalogError);
    Check(ok, "material-only diagnostics fixture builds");
    World materialWorld(materialCatalog, 8, 8);
    materialWorld.Step(3);
    Check(!materialWorld.QueryUtilityDiagnostics().available,
          "material-only world never publishes utility receipt");
}
} // namespace

int main() {
    FormatBoundaries();
    ComponentFlowAndTopology();
    WideAggregateConservation();
    EligibilityAndAtomicShortage();
    DisabledFuelStarvationAndRelays();
    ObservationLifecycle();
    if (failures) {
        std::cerr << failures << " utility diagnostics test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All utility diagnostics tests passed\n";
    return EXIT_SUCCESS;
}
