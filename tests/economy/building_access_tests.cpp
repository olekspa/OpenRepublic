#include "openrepublic/economy/economy.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace openrepublic::economy;

namespace {
int failures = 0;

void Check(bool ok, const std::string &message) {
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

Catalog MakeAccessCatalog() {
    CatalogBuilder builder;
    CatalogErrorInfo error;
    ResourceId resource{};
    BuildingTypeId small{}, square{}, wide{};
    VehicleTypeId truck{};
    Check(builder.AddResource("goods", &resource, error), "fixture adds resource");
    Check(builder.AddBuildingType("small", 1, 1, &small, error), "fixture adds 1x1 type");
    Check(builder.AddBuildingType("square", 2, 2, &square, error), "fixture adds 2x2 type");
    Check(builder.AddBuildingType("wide", 3, 2, &wide, error), "fixture adds 3x2 type");
    Check(builder.AddStorage(small, resource, 10, error) && builder.AddStorage(square, resource, 10, error) &&
              builder.AddStorage(wide, resource, 10, error),
          "fixture building types have compatible storage");
    Check(builder.AddVehicleType("truck", 4, 1, &truck, error), "fixture adds truck");
    Catalog catalog;
    Check(builder.Build(catalog, error), "access fixture catalog builds");
    return catalog;
}

void Advance(World &world) { (void)world.Step(); }

CommandResult Run(World &world, CommandId command) {
    Advance(world);
    return world.GetCommandResult(command);
}

std::vector<std::string> Lines(const std::string &text) {
    std::istringstream in(text);
    std::vector<std::string> result;
    for (std::string line; std::getline(in, line);)
        result.push_back(line);
    return result;
}

std::vector<std::string> Words(const std::string &line) {
    std::istringstream in(line);
    std::vector<std::string> result;
    for (std::string value; in >> value;)
        result.push_back(value);
    return result;
}

std::string Join(const std::vector<std::string> &words) {
    std::string out;
    for (const auto &word : words) {
        if (!out.empty())
            out += ' ';
        out += word;
    }
    return out;
}

std::string Rebuild(const std::vector<std::string> &lines) {
    std::string out;
    for (const auto &line : lines)
        out += line + '\n';
    return out;
}

// Record-aware mutation preserves all canonical IDs and child records. Only
// the building's origin changes, so any load failure exercises layout checks.
std::string MoveBuilding(const std::string &save, BuildingId id, std::int32_t x, std::int32_t z) {
    auto lines = Lines(save);
    for (auto &line : lines) {
        auto fields = Words(line);
        if (fields.size() == 6 && fields[0] == "B" && fields[1] == std::to_string(id)) {
            fields[3] = std::to_string(x);
            fields[4] = std::to_string(z);
            line = Join(fields);
            return Rebuild(lines);
        }
    }
    Check(false, "save fixture contains requested building record");
    return save;
}

std::string DisableRoad(const std::string &save, RoadId id) {
    auto lines = Lines(save);
    for (auto &line : lines) {
        auto fields = Words(line);
        if (fields.size() == 7 && fields[0] == "R" && fields[1] == std::to_string(id)) {
            fields[6] = "0";
            line = Join(fields);
            return Rebuild(lines);
        }
    }
    Check(false, "save fixture contains requested road record");
    return save;
}

bool LoadPreserving(World &target, const Catalog &catalog, const std::string &bad) {
    const auto before = SaveWorld(target);
    ParseError error;
    const bool loaded = LoadWorld(bad, catalog, target, error);
    Check(!loaded, "invalid building layout save is rejected");
    Check(error.code != ParseErrorCode::None, "invalid layout load reports a diagnostic");
    Check(SaveWorld(target) == before, "invalid layout load leaves destination world unchanged");
    return !loaded;
}

void TestPlacementGeometry(const Catalog &catalog) {
    {
        World world(catalog, 10, 8);
        auto place = world.PlaceBuilding(1, {4, 2});
        const auto result = Run(world, place);
        const auto snapshot = world.Snapshot();
        Check(result.state == CommandState::Accepted && snapshot.buildings.size() == 1 &&
                  snapshot.buildings[0].access == Cell{4, 1},
              "2x2 building uses the lower-z edge center with deterministic lower-cell rounding");
    }
    {
        World world(catalog, 10, 8);
        auto place = world.PlaceBuilding(2, {4, 2});
        const auto result = Run(world, place);
        const auto snapshot = world.Snapshot();
        Check(result.state == CommandState::Accepted && snapshot.buildings.size() == 1 &&
                  snapshot.buildings[0].access == Cell{5, 1},
              "3x2 building access is centered on its lower-z edge");
    }
    {
        World world(catalog, 10, 8);
        auto place = world.PlaceBuilding(0, {8, 6});
        const auto result = Run(world, place);
        Check(result.state == CommandState::Accepted && world.Snapshot().buildings.size() == 1,
              "building placement is allowed without any road connection");
    }
    {
        World world(catalog, 5, 5);
        auto outside = world.PlaceBuilding(1, {4, 2});
        const auto result = Run(world, outside);
        Check(result.state == CommandState::Rejected && result.failure == CommandFailure::OutOfBounds,
              "footprint extending beyond grid reports OutOfBounds");
    }
    {
        World world(catalog, 5, 5);
        auto accessOutside = world.PlaceBuilding(0, {2, 0});
        const auto result = Run(world, accessOutside);
        Check(result.state == CommandState::Rejected && result.failure == CommandFailure::AccessCellUnavailable,
              "in-bounds footprint with out-of-grid access reports AccessCellUnavailable");
    }
    {
        World world(catalog, 10, 8);
        auto first = world.PlaceBuilding(1, {2, 2});
        auto second = world.PlaceBuilding(0, {3, 3});
        Advance(world);
        Check(world.GetCommandResult(first).state == CommandState::Accepted &&
                  world.GetCommandResult(second).state == CommandState::Rejected &&
                  world.GetCommandResult(second).failure == CommandFailure::BuildingCollision,
              "overlapping 2x2 footprints report BuildingCollision");
    }
    {
        World world(catalog, 10, 8);
        auto first = world.PlaceBuilding(0, {4, 4}); // access (4,3), outside its own footprint
        auto second = world.PlaceBuilding(1, {4, 2}); // footprint covers first access
        Advance(world);
        Check(world.GetCommandResult(first).state == CommandState::Accepted &&
                  world.GetCommandResult(second).state == CommandState::Rejected &&
                  world.GetCommandResult(second).failure == CommandFailure::AccessCellUnavailable,
              "new footprint cannot cover existing access cell");
    }
    {
        // Same final geometry with the queue order reversed: the first accepted
        // footprint owns (4,3), so the later small building cannot use it as access.
        World world(catalog, 10, 8);
        auto first = world.PlaceBuilding(1, {4, 2});
        auto second = world.PlaceBuilding(0, {4, 4});
        Advance(world);
        Check(world.GetCommandResult(first).state == CommandState::Accepted &&
                  world.GetCommandResult(second).state == CommandState::Rejected &&
                  world.GetCommandResult(second).failure == CommandFailure::AccessCellUnavailable,
              "same-tick access reservation is enforced in reverse layout order");
    }
}

void TestRoadInteractions(const Catalog &catalog) {
    {
        World world(catalog, 10, 8);
        auto road = world.BuildRoad({4, 1}, {5, 1});
        auto place = world.PlaceBuilding(1, {4, 2});
        Advance(world);
        Check(world.GetCommandResult(road).state == CommandState::Accepted &&
                  world.GetCommandResult(place).state == CommandState::Accepted,
              "road at building access is allowed");
    }
    {
        World world(catalog, 10, 8);
        auto road = world.BuildRoad({2, 2}, {3, 2});
        Advance(world);
        auto disable = world.SetRoadEnabled(1, false);
        Advance(world);
        auto place = world.PlaceBuilding(1, {2, 2});
        const auto result = Run(world, place);
        Check(world.GetCommandResult(road).state == CommandState::Accepted &&
                  world.GetCommandResult(disable).state == CommandState::Accepted &&
                  result.state == CommandState::Rejected && result.failure == CommandFailure::RoadOnBuilding,
              "disabled road endpoint still reserves against building footprints");
    }
    {
        World world(catalog, 10, 8);
        auto place = world.PlaceBuilding(1, {2, 2});
        Advance(world);
        auto road = world.BuildRoad({2, 2}, {3, 2});
        const auto result = Run(world, road);
        Check(world.GetCommandResult(place).state == CommandState::Accepted &&
                  result.state == CommandState::Rejected && result.failure == CommandFailure::RoadOnBuilding,
              "road built after a building cannot use a footprint endpoint");
    }
    {
        World world(catalog, 10, 8);
        auto place = world.PlaceBuilding(0, {2, 2});
        Advance(world);
        auto remove = world.RemoveBuilding(1);
        Advance(world);
        auto replacement = world.PlaceBuilding(1, {2, 1}); // Covers the removed building's old access.
        const auto result = Run(world, replacement);
        Check(world.GetCommandResult(place).state == CommandState::Accepted &&
                  world.GetCommandResult(remove).state == CommandState::Accepted &&
                  result.state == CommandState::Accepted,
              "removing a building releases its access reservation");
    }
}

void TestBounds(const Catalog &catalog) {
    {
        World world(catalog, MaxWorldGridSize, MaxWorldGridSize);
        auto place = world.PlaceBuilding(0, {static_cast<std::int32_t>(MaxWorldGridSize - 1), 1});
        const auto result = Run(world, place);
        Check(result.state == CommandState::Accepted,
              "1x1 footprint at maximum representable grid coordinate is accepted");
    }
    {
        World world(catalog, MaxWorldGridSize, MaxWorldGridSize);
        auto place = world.PlaceBuilding(2, {static_cast<std::int32_t>(MaxWorldGridSize - 2), 1});
        const auto result = Run(world, place);
        Check(result.state == CommandState::Rejected && result.failure == CommandFailure::OutOfBounds,
              "3-wide footprint cannot overflow or exceed maximum grid edge");
    }
}

void TestSaveLayouts(const Catalog &catalog) {
    {
        World source(catalog, 12, 10);
        source.PlaceBuilding(0, {2, 4});
        source.PlaceBuilding(0, {6, 2});
        Advance(source);
        const auto valid = SaveWorld(source);
        for (const auto &bad : {MoveBuilding(valid, 2, 2, 3), MoveBuilding(valid, 1, 6, 3)}) {
            World destination(catalog, 3, 3);
            Check(LoadPreserving(destination, catalog, bad),
                  "access-only conflict rejects with either canonical ID orientation");
            ParseError error;
            Check(!LoadWorld(bad, catalog, destination, error) &&
                      error.message.find("access") != std::string::npos,
                  "nonoverlapping footprints reject specifically for access conflict");
            auto legacy = Lines(bad);
            legacy[0] = "OPENREPUBLIC_ECONOMY_WORLD 1";
            Check(LoadPreserving(destination, catalog, Rebuild(legacy)),
                  "legacy access conflict rejects instead of silently moving a building");
        }
    }
    // The canonical IDs are kept in increasing order. Each invalid fixture
    // changes only one origin, so record traversal order cannot hide overlap.
    {
        World source(catalog, 12, 10);
        source.PlaceBuilding(0, {2, 3});
        source.PlaceBuilding(1, {6, 3});
        Advance(source);
        const std::string validV2 = SaveWorld(source);
        World destination(catalog, 3, 3);
        ParseError error;
        Check(LoadWorld(validV2, catalog, destination, error) && SaveWorld(destination) == validV2,
              "valid v2 layout saves and loads canonically");
        Check(LoadPreserving(destination, catalog, MoveBuilding(validV2, 2, 2, 3)),
              "overlap is rejected when later canonical ID moves onto earlier footprint");
        Check(LoadPreserving(destination, catalog, MoveBuilding(validV2, 1, 6, 3)),
              "overlap is rejected when earlier canonical ID moves onto later footprint");
    }
    {
        World source(catalog, 12, 10);
        auto command = source.PlaceBuilding(2, {4, 3});
        Advance(source);
        Check(source.GetCommandResult(command).state == CommandState::Accepted,
              "v1 fixture source placement accepted");
        auto lines = Lines(SaveWorld(source));
        if (!lines.empty())
            lines[0] = "OPENREPUBLIC_ECONOMY_WORLD 1";
        const std::string validV1 = Rebuild(lines);
        World restored(catalog, 2, 2);
        ParseError error;
        Check(LoadWorld(validV1, catalog, restored, error) &&
                  restored.Snapshot().stateHash == source.Snapshot().stateHash,
              "valid v1 building layout survives save conversion/load");
        const auto roundTrip = SaveWorld(restored);
        World replay(catalog, 1, 1);
        Check(LoadWorld(roundTrip, catalog, replay, error) &&
                  replay.Snapshot().stateHash == restored.Snapshot().stateHash,
              "v1 layout conversion replays deterministically through v2");
    }
    {
        World source(catalog, 12, 10);
        auto road = source.BuildRoad({4, 3}, {5, 3});
        Advance(source);
        auto disable = source.SetRoadEnabled(1, false);
        Advance(source);
        auto building = source.PlaceBuilding(0, {7, 3});
        Advance(source);
        Check(source.GetCommandResult(road).state == CommandState::Accepted &&
                  source.GetCommandResult(disable).state == CommandState::Accepted &&
                  source.GetCommandResult(building).state == CommandState::Accepted,
              "disabled-road save fixture is created");
        const std::string roadSave = SaveWorld(source);
        World destination(catalog, 12, 10);
        ParseError error;
        Check(LoadWorld(roadSave, catalog, destination, error), "valid disabled road save loads");
        Check(LoadPreserving(destination, catalog, MoveBuilding(roadSave, 1, 4, 3)),
              "save rejects building footprint over disabled road endpoint");
        Check(!LoadWorld(MoveBuilding(roadSave, 1, 4, 3), catalog, destination, error) &&
                  error.message.find("road endpoint") != std::string::npos,
              "road overlap load failure identifies the conflicting footprint");
    }
    {
        // A queued same-tick placement is part of the saved transaction and is
        // replayed in command order after loading.
        World source(catalog, 10, 8);
        auto a = source.PlaceBuilding(1, {2, 2});
        auto b = source.PlaceBuilding(0, {2, 4});
        const auto queued = SaveWorld(source);
        World restored(catalog, 1, 1);
        ParseError error;
        Check(LoadWorld(queued, catalog, restored, error), "valid queued layout commands load");
        Advance(source);
        Advance(restored);
        Check(source.GetCommandResult(a).state == restored.GetCommandResult(a).state &&
                  source.GetCommandResult(b).state == restored.GetCommandResult(b).state &&
                  source.GetCommandResult(b).failure == CommandFailure::AccessCellUnavailable &&
                  source.Snapshot().stateHash == restored.Snapshot().stateHash,
              "queued v2 layout commands survive save and deterministic rejection replay");
    }
}

void TestHaulSurvivesRejectedPlacement() {
    const auto catalog = MakeSyntheticCatalog();
    World world(catalog, 12, 8);
    for (int x = 1; x < 6; ++x)
        world.BuildRoad({x, 1}, {x + 1, 1});
    world.PlaceBuilding(0, {1, 2});
    world.PlaceBuilding(1, {5, 2});
    world.Step(8);
    world.SetBuildingEnabled(1, false);
    world.SetBuildingEnabled(2, false);
    world.Step();
    world.CreateHaulOrder(1, 2, 0, 0);
    world.Step();
    const auto before = world.Snapshot();
    Check(before.orders.size() == 1 && before.orders[0].cargo > 0,
          "preserved-route fixture has cargo aboard");
    const auto placement = world.PlaceBuilding(0, {3, 1});
    world.Step();
    Check(world.GetCommandResult(placement).failure == CommandFailure::RoadOnBuilding &&
              world.Snapshot().buildings.size() == 2 && world.Snapshot().roads.size() == 5,
          "rejected placement preserves an existing working haul road");
    world.Step(3);
    const auto after = world.Snapshot();
    Check(after.orders.size() == 1 && after.vehicles[0].id == before.vehicles[0].id &&
              after.buildings[1].inventory[0].amount > 0,
          "same truck completes delivery after road-covering placement was rejected");
    World restored(catalog, 1, 1);
    ParseError error;
    Check(LoadWorld(SaveWorld(world), catalog, restored, error) &&
              restored.Snapshot().stateHash == after.stateHash,
          "working haul remains saveable after rejected placement");
}
} // namespace

int main() {
    const Catalog catalog = MakeAccessCatalog();
    TestPlacementGeometry(catalog);
    TestRoadInteractions(catalog);
    TestBounds(catalog);
    TestSaveLayouts(catalog);
    TestHaulSurvivesRejectedPlacement();
    if (failures != 0) {
        std::cerr << failures << " building access checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "building access checks passed\n";
    return EXIT_SUCCESS;
}
