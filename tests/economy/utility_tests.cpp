#include "openrepublic/economy/economy.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

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
    ResourceId fuel{}, feed{}, product{}, exhaust{}, power{};
    BuildingTypeId fuelMaker{}, generator{}, consumer{};
};

Fixture MakeFixture(std::uint64_t generation = 2, bool fuelled = true, bool exhaustOutput = false) {
    Fixture f;
    CatalogBuilder b;
    CatalogErrorInfo e;
    bool ok = b.AddResource("fuel", &f.fuel, e) && b.AddResource("feed", &f.feed, e) &&
              b.AddResource("product", &f.product, e) && b.AddResource("exhaust", &f.exhaust, e) &&
              b.AddUtilityResource("electricity", &f.power, e) &&
              b.AddBuildingType("fuel_maker", 1, 1, &f.fuelMaker, e) &&
              b.AddBuildingType("generator", 1, 1, &f.generator, e) &&
              b.AddBuildingType("consumer", 1, 1, &f.consumer, e);
    ok = ok && b.AddStorage(f.fuelMaker, f.fuel, 128, e) && b.AddStorage(f.generator, f.fuel, 128, e) &&
         b.AddStorage(f.generator, f.exhaust, 1, e) && b.AddStorage(f.consumer, f.feed, 128, e) &&
         b.AddStorage(f.consumer, f.product, 128, e);
    ok = ok && b.AddRecipe(f.fuelMaker, {"make_fuel", 1, {}, {{f.fuel, 2}}}, e);
    RecipeDef generatorRecipe{"generate", 1, {}, {{f.power, generation}}};
    if (fuelled)
        generatorRecipe.inputs.push_back({f.fuel, 1});
    if (exhaustOutput)
        generatorRecipe.outputs.push_back({f.exhaust, 1});
    ok = ok && b.AddRecipe(f.generator, generatorRecipe, e) &&
         b.AddRecipe(f.consumer, {"make_feed", 1, {}, {{f.feed, 1}}}, e) &&
         b.AddRecipe(f.consumer, {"work", 1, {{f.feed, 1}, {f.power, 1}}, {{f.product, 1}}}, e) &&
         b.AddVehicleType("truck", 8, 1, nullptr, e) && b.Build(f.catalog, e);
    if (!ok)
        std::cerr << "fixture error: " << e.message << '\n';
    Check(ok, "utility fixture catalog builds");
    return f;
}

void Place(World &world, std::uint32_t type, Cell cell) { world.PlaceBuilding(type, cell); }
void CheckQueuedAccepted(const World &world, CommandId id, const char *message) {
    Check(id != 0 && world.GetCommandResult(id).state == CommandState::Queued, message);
}
const BuildingSnapshot *Building(const WorldSnapshot &snapshot, BuildingId id) {
    const auto it = std::find_if(snapshot.buildings.begin(), snapshot.buildings.end(),
                                 [&](const BuildingSnapshot &b) { return b.id == id; });
    return it == snapshot.buildings.end() ? nullptr : &*it;
}
std::uint64_t Stock(const BuildingSnapshot *building, ResourceId resource) {
    if (!building)
        return 0;
    const auto it = std::find_if(building->inventory.begin(), building->inventory.end(),
                                 [&](const InventorySnapshot &slot) { return slot.resource == resource; });
    return it == building->inventory.end() ? 0 : it->amount;
}

void CatalogRules() {
    CatalogBuilder b;
    CatalogErrorInfo ce;
    ResourceId fuel{}, power{};
    BuildingTypeId plant{};
    Check(b.AddResource("fuel", &fuel, ce) && b.AddUtilityResource("power", &power, ce),
          "ordinary and utility resources can be authored");
    Check(b.AddBuildingType("plant", 1, 1, &plant, ce), "utility test building type is added");
    Check(!b.AddStorage(plant, power, 1, ce) && ce.code == CatalogError::UtilityHasStorage,
          "utility resources cannot receive storage slots");
    Check(b.AddStorage(plant, fuel, 8, ce), "fuel storage can be added");
    Check(!b.AddRecipe(plant, {"slow", 2, {}, {{power, 1}}}, ce) &&
              ce.code == CatalogError::InvalidUtilityRecipe,
          "utility recipe intervals other than one are rejected");
    Check(!b.AddRecipe(plant, {"self_powered", 1, {{power, 1}}, {{power, 1}}}, ce) &&
              ce.code == CatalogError::InvalidUtilityRecipe,
          "utility producers cannot consume utility resources");

    Catalog catalog;
    Check(b.AddRecipe(plant, {"generate", 1, {{fuel, 1}}, {{power, 2}}}, ce) && b.Build(catalog, ce),
          "valid generator recipe builds without a power inventory slot");
    const auto serialized = WriteCatalogText(catalog);
    Check(serialized.find("utility power\n") != std::string::npos &&
              serialized.find("resource fuel\n") != std::string::npos,
          "catalog text preserves resource versus utility declarations");
    Catalog restored;
    ParseError parse;
    Check(ParseCatalogText(serialized, restored, parse) && WriteCatalogText(restored) == serialized &&
              restored.Resource(power)->utility,
          "utility catalog text round trips canonically");
}

void ConnectionAndAllocation() {
    const auto f = MakeFixture(1, false);
    World world(f.catalog, 16, 8);
    Place(world, f.generator, {1, 2});
    Place(world, f.consumer, {5, 2});
    Place(world, f.consumer, {9, 2});
    world.Step();
    auto snapshot = world.Snapshot();
    Check(snapshot.buildings.size() == 3, "all utility endpoints are live before linking");
    const auto gen = snapshot.buildings[0].id, first = snapshot.buildings[1].id, second = snapshot.buildings[2].id;
    world.Step();
    snapshot = world.Snapshot();
    Check(Building(snapshot, first)->stall.reason == StallReason::MissingInput &&
              Building(snapshot, first)->stall.resource == f.power,
          "disconnected consumer stalls for the utility resource");
    const auto link1 = world.ConnectUtility(f.power, gen, first);
    const auto link2 = world.ConnectUtility(f.power, first, second);
    CheckQueuedAccepted(world, link1, "first utility link queues");
    CheckQueuedAccepted(world, link2, "second utility link queues");
    world.Step();
    Check(world.GetCommandResult(link1).state == CommandState::Accepted &&
              world.GetCommandResult(link2).state == CommandState::Accepted,
          "utility links are accepted at the command boundary");
    snapshot = world.Snapshot();
    Check(snapshot.utilityConnections.size() == 2 && snapshot.utilityConnections[0].a == gen &&
              snapshot.utilityConnections[0].b == first && snapshot.utilityConnections[1].a == first &&
              snapshot.utilityConnections[1].b == second,
          "utility connection snapshots use canonical endpoints and ordering");
    const auto firstConsumer = Building(snapshot, first);
    const auto secondConsumer = Building(snapshot, second);
    Check(firstConsumer && secondConsumer && firstConsumer->recipes[1].completedCycles > 0 &&
              secondConsumer->recipes[1].completedCycles == 0,
          "one unit of utility is allocated to the lower building ID first");
    Check(secondConsumer && secondConsumer->stall.reason == StallReason::MissingInput &&
              secondConsumer->stall.resource == f.power && Stock(secondConsumer, f.feed) > 0,
          "utility shortage stalls the later consumer without consuming stock inputs");
    const auto duplicate = world.ConnectUtility(f.power, first, gen);
    world.Step();
    Check(world.GetCommandResult(duplicate).state == CommandState::Rejected &&
              world.GetCommandResult(duplicate).failure == CommandFailure::DuplicateUtilityConnection,
          "duplicate undirected utility link is rejected");
    const auto remove = world.RemoveBuilding(gen);
    world.Step();
    Check(world.GetCommandResult(remove).failure == CommandFailure::ReferencedByUtilityConnection,
          "connected buildings cannot be removed before disconnecting links");
    const auto disconnect = world.DisconnectUtility(f.power, first, gen);
    const auto removeAfter = world.RemoveBuilding(gen);
    world.Step();
    Check(world.GetCommandResult(disconnect).state == CommandState::Accepted &&
              world.GetCommandResult(removeAfter).state == CommandState::Accepted,
          "disconnect applies before removal in the same command batch");
    const auto disconnectedSave = SaveWorld(world);
    World disconnectedReload(f.catalog, 1, 1);
    ParseError reloadError;
    Check(disconnectedSave.rfind("OPENREPUBLIC_ECONOMY_WORLD 4\n", 0) == 0 &&
              LoadWorld(disconnectedSave, f.catalog, disconnectedReload, reloadError),
          "completed connect/disconnect history remains loadable after the last edge is removed");
}

void FuelDeliveryAndPersistence() {
    const auto f = MakeFixture(4, true);
    World world(f.catalog, 16, 8);
    for (std::int32_t x = 1; x < 9; ++x)
        world.BuildRoad({x, 1}, {x + 1, 1});
    Place(world, f.fuelMaker, {1, 2});
    Place(world, f.generator, {5, 2});
    Place(world, f.consumer, {9, 2});
    world.Step();
    const auto gen = world.Snapshot().buildings[1].id;
    const auto consumer = world.Snapshot().buildings[2].id;
    const auto link = world.ConnectUtility(f.power, gen, consumer);
    const auto haul = world.CreateHaulOrder(1, gen, f.fuel, 0);
    const auto utilityHaul = world.CreateHaulOrder(gen, consumer, f.power, 0);
    world.Step();
    Check(world.GetCommandResult(link).state == CommandState::Accepted &&
              world.GetCommandResult(haul).state == CommandState::Accepted &&
              world.GetCommandResult(utilityHaul).failure == CommandFailure::InvalidOrderEndpoint,
          "fuel haul and utility connection apply while utility hauling rejects");
    world.Step(60);
    auto snapshot = world.Snapshot();
    Check(Building(snapshot, consumer)->recipes[1].completedCycles > 0,
          "hauled fuel powers a connected consumer through the generator");
    Check(Stock(Building(snapshot, consumer), f.product) > 0,
          "powered consumer stores its ordinary product");
    const auto saved = SaveWorld(world);
    Check(saved.rfind("OPENREPUBLIC_ECONOMY_WORLD 4\n", 0) == 0,
          "utility links select native save version four");
    World restored(f.catalog, 1, 1);
    ParseError error;
    Check(LoadWorld(saved, f.catalog, restored, error), "version four utility world loads");
    const auto restoredSnapshot = restored.Snapshot();
    Check(restoredSnapshot.stateHash == snapshot.stateHash &&
              restoredSnapshot.utilityConnections.size() == snapshot.utilityConnections.size() &&
              restoredSnapshot.utilityConnections[0].resource == snapshot.utilityConnections[0].resource &&
              restoredSnapshot.utilityConnections[0].a == snapshot.utilityConnections[0].a &&
              restoredSnapshot.utilityConnections[0].b == snapshot.utilityConnections[0].b,
          "utility world hash and canonical links survive save/load");
    for (int i = 0; i < 12; ++i) {
        world.Step();
        restored.Step();
        Check(world.Snapshot().stateHash == restored.Snapshot().stateHash,
              "saved utility world replays deterministic subsequent ticks");
    }
    auto malformed = saved;
    const auto edge = malformed.find("U ");
    if (edge != std::string::npos)
        malformed.replace(edge, malformed.find('\n', edge) - edge, "U 0 1 999");
    World unchanged(f.catalog, 1, 1);
    const auto before = SaveWorld(unchanged);
    Check(!LoadWorld(malformed, f.catalog, unchanged, error) && SaveWorld(unchanged) == before,
          "malformed utility endpoint is rejected transactionally");
    auto impossibleProgress = saved;
    const auto generatorProgress = impossibleProgress.find("P 2 1 0 ");
    Check(generatorProgress != std::string::npos, "saved generator has canonical progress row");
    if (generatorProgress != std::string::npos)
        impossibleProgress.replace(generatorProgress, std::string("P 2 1 0 ").size(), "P 2 1 1 ");
    Check(!LoadWorld(impossibleProgress, f.catalog, unchanged, error) && SaveWorld(unchanged) == before,
          "malformed nonzero progress for an atomic utility recipe is rejected transactionally");
}

void FuelAndCapacityGates() {
    const auto f = MakeFixture(1, true, true);
    World world(f.catalog, 12, 8);
    Place(world, f.generator, {1, 2});
    Place(world, f.consumer, {5, 2});
    world.Step();
    const auto gen = world.Snapshot().buildings[0].id;
    const auto consumer = world.Snapshot().buildings[1].id;
    const auto link = world.ConnectUtility(f.power, gen, consumer);
    world.Step();
    Check(world.GetCommandResult(link).state == CommandState::Accepted, "fuel gate topology connects");
    world.Step();
    auto snapshot = world.Snapshot();
    Check(Building(snapshot, consumer)->recipes[1].completedCycles == 0 &&
              Building(snapshot, consumer)->stall.resource == f.power,
          "generator with missing fuel supplies no phantom utility");

    const auto full = MakeFixture(1, false, true);
    World fullWorld(full.catalog, 12, 8);
    Place(fullWorld, full.generator, {1, 2});
    Place(fullWorld, full.consumer, {5, 2});
    fullWorld.Step(); // generator fills its sole ordinary output slot
    const auto fullGen = fullWorld.Snapshot().buildings[0].id;
    const auto fullConsumer = fullWorld.Snapshot().buildings[1].id;
    const auto fullLink = fullWorld.ConnectUtility(full.power, fullGen, fullConsumer);
    fullWorld.Step(); // full generator cannot offer electricity
    snapshot = fullWorld.Snapshot();
    Check(fullWorld.GetCommandResult(fullLink).state == CommandState::Accepted &&
              Building(snapshot, fullGen)->stall.reason == StallReason::OutputFull &&
              Building(snapshot, fullConsumer)->recipes[1].completedCycles == 0 &&
              Building(snapshot, fullConsumer)->stall.resource == full.power,
          "full generator output blocks supply without phantom utility");
}

void WideUtilityBudget() {
    CatalogBuilder b;
    CatalogErrorInfo e;
    ResourceId product{}, power{};
    BuildingTypeId generator{}, consumer{};
    bool ok = b.AddResource("product", &product, e) && b.AddUtilityResource("power", &power, e) &&
              b.AddBuildingType("generator", 1, 1, &generator, e) &&
              b.AddBuildingType("consumer", 1, 1, &consumer, e) &&
              b.AddStorage(consumer, product, 8, e) &&
              b.AddRecipe(generator, {"generate", 1, {}, {{power, UINT64_MAX}}}, e) &&
              b.AddRecipe(consumer, {"consume", 1, {{power, UINT64_MAX}}, {{product, 1}}}, e);
    Catalog catalog;
    ok = ok && b.Build(catalog, e);
    Check(ok, "wide budget catalog builds");
    if (!ok)
        return;
    World world(catalog, 12, 8);
    world.PlaceBuilding(generator, {1, 2});
    world.PlaceBuilding(generator, {3, 2});
    world.PlaceBuilding(consumer, {5, 2});
    world.PlaceBuilding(consumer, {7, 2});
    world.Step();
    world.ConnectUtility(power, 1, 3);
    world.ConnectUtility(power, 2, 3);
    world.ConnectUtility(power, 3, 4);
    world.Step();
    const auto snapshot = world.Snapshot();
    Check(Building(snapshot, 3)->recipes[0].completedCycles == 1 &&
              Building(snapshot, 4)->recipes[0].completedCycles == 1,
          "utility offers above 64-bit total are allocated without wrap or false shortage");
}

void LegacyAndPendingSaveRules() {
    const auto f = MakeFixture(1, false);
    CatalogBuilder b;
    CatalogErrorInfo ce;
    ResourceId grain{};
    BuildingTypeId farm{};
    Check(b.AddResource("grain", &grain, ce) && b.AddBuildingType("farm", 1, 1, &farm, ce) &&
              b.AddStorage(farm, grain, 4, ce), "legacy-only catalog constructs");
    Catalog legacy;
    Check(b.Build(legacy, ce), "legacy-only catalog builds");
    World oldWorld(legacy, 8, 8);
    const auto oldBytes = SaveWorld(oldWorld);
    Check(oldBytes.rfind("OPENREPUBLIC_ECONOMY_WORLD 2\n", 0) == 0 &&
              oldBytes == "OPENREPUBLIC_ECONOMY_WORLD 2\nGRID 8 8\nTICK 0\nREVISION 0\n"
                          "NEXT 1 1 1 1 1\nBUILDINGS 0\nROADS 0\nORDERS 0\nPENDING 0\n"
                          "RESULTS 0\nEND\n",
          "world without utility state retains exact legacy version-two bytes");
    World world(f.catalog, 12, 8);
    Place(world, f.generator, {1, 2});
    Place(world, f.consumer, {5, 2});
    world.Step();
    const auto queued = world.ConnectUtility(f.power, 1, 2);
    const auto pendingSave = SaveWorld(world);
    Check(queued && pendingSave.rfind("OPENREPUBLIC_ECONOMY_WORLD 4\n", 0) == 0 &&
              pendingSave.find("HISTORY 1\n") != std::string::npos,
          "pending utility command selects v4 with mandatory history floor");
    World restored(f.catalog, 1, 1);
    ParseError error;
    Check(LoadWorld(pendingSave, f.catalog, restored, error), "pending utility command survives reload");
    world.Step();
    restored.Step();
    Check(world.Snapshot().stateHash == restored.Snapshot().stateHash &&
              world.GetCommandResult(queued).state == restored.GetCommandResult(queued).state,
          "pending utility command applies identically after restore");
    auto malformed = pendingSave;
    const auto queuedRecord = malformed.find("Q ");
    if (queuedRecord != std::string::npos)
        malformed.replace(queuedRecord, malformed.find('\n', queuedRecord) - queuedRecord,
                          "Q 2 9 1 0 0 0 0 0 0 0 0 0 0 0 0");
    const auto before = SaveWorld(restored);
    Check(!LoadWorld(malformed, f.catalog, restored, error) && SaveWorld(restored) == before,
          "malformed pending utility payload is rejected transactionally");

    World invalidPending(f.catalog, 12, 8);
    Place(invalidPending, f.generator, {1, 2});
    Place(invalidPending, f.consumer, {5, 2});
    invalidPending.Step();
    const auto invalidEndpoint = invalidPending.ConnectUtility(f.power, 1, 999);
    const auto invalidEndpointSave = SaveWorld(invalidPending);
    World invalidRestored(f.catalog, 1, 1);
    Check(LoadWorld(invalidEndpointSave, f.catalog, invalidRestored, error),
          "queued invalid utility endpoint can be restored for deterministic rejection");
    invalidRestored.Step();
    Check(invalidRestored.GetCommandResult(invalidEndpoint).failure == CommandFailure::InvalidUtilityConnection,
          "restored invalid endpoint rejects at the same command boundary");
}
} // namespace

int main() {
    CatalogRules();
    ConnectionAndAllocation();
    FuelDeliveryAndPersistence();
    FuelAndCapacityGates();
    WideUtilityBudget();
    LegacyAndPendingSaveRules();
    if (failures) {
        std::cerr << failures << " utility test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All utility tests passed\n";
    return EXIT_SUCCESS;
}
