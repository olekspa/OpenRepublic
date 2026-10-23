#include "openrepublic/economy/economy.h"

#include <cstdlib>
#include <iostream>

using namespace openrepublic::economy;

namespace {
int failures = 0;
void Check(bool value, const char *message) {
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

Catalog MakeWorkforceCatalog(std::uint64_t housing, std::uint64_t workers) {
    const std::string source =
        "openrepublic-economy 1\n"
        "resource ore\n"
        "resource goods\n"
        "building dorm 1 1\n"
        " housing " + std::to_string(housing) + "\n"
        "end\n"
        "building mine 1 1\n"
        " workers " + std::to_string(workers) + "\n"
        " store ore 1000000\n"
        " store goods 1000000\n"
        " recipe ore every 1 out ore:1\n"
        " recipe goods every 1 out goods:2\n"
        "end\n";
    Catalog catalog;
    ParseError error;
    Check(ParseCatalogText(source, catalog, error), "multi-recipe workforce catalog parses");
    return catalog;
}

void CheckDisabledWorkplaceDoesNotReserveResidents() {
    const auto catalog = MakeWorkforceCatalog(4, 4);
    World world(catalog, 16, 16);
    const auto housing = world.PlaceBuilding(0, {1, 1});
    const auto disabledMine = world.PlaceBuilding(1, {5, 5});
    const auto activeMine = world.PlaceBuilding(1, {9, 9});
    Check(housing == 1 && disabledMine == 2 && activeMine == 3, "test buildings receive stable ids");
    world.SetBuildingEnabled(disabledMine, false);
    Check(world.Step().accepted, "disabled workplace setup applies");

    const auto after = world.Snapshot();
    Check(after.population == 4, "housing residents remain available while workplace is disabled");
    Check(after.workersRequired == 8, "required workers include disabled workplaces");
    Check(after.workersAssigned == 4, "active workplace receives residents skipped by disabled workplace");
    Check(after.buildings.size() == 3 && after.buildings[2].inventory[0].amount == 1 &&
              after.buildings[2].inventory[1].amount == 2,
          "enabled workplace runs each recipe after disabled workplace releases residents");

    world.SetBuildingEnabled(disabledMine, true);
    Check(world.Step().accepted, "disabled workplace can be re-enabled");
    const auto reenabled = world.Snapshot();
    Check(reenabled.workersAssigned == 4 && reenabled.workersRequired == 8,
          "re-enabled workplace participates in deterministic allocation");
    Check(reenabled.buildings[1].inventory[0].amount == 1 &&
              reenabled.buildings[1].inventory[1].amount == 2 &&
              reenabled.buildings[2].inventory[0].amount == 1 &&
              reenabled.buildings[2].inventory[1].amount == 2,
          "re-enabled earlier workplace receives workers and later workplace is starved");
}

void PopulateLargeWorld(World &world) {
    constexpr int width = 128;
    constexpr int depth = 128;
    world.PlaceBuilding(0, {1, 1});
    for (int x = 0; x < width; ++x) {
        for (int z = 0; z < depth; ++z) {
            if (x == 0 && z == 0)
                continue;
            world.PlaceBuilding(1, {2 + x * 4, 2 + z * 4});
        }
    }
}

void CheckLargeWorldDeterminism() {
    constexpr int width = 128;
    constexpr int depth = 128;
    constexpr std::size_t workplaceCount = width * depth - 1;
    const auto catalog = MakeWorkforceCatalog(workplaceCount * 2, 2);
    World serial(catalog, 520, 520);
    World parallel(catalog, 520, 520);
    PopulateLargeWorld(serial);
    PopulateLargeWorld(parallel);
    Check(serial.Step().accepted && parallel.Step().accepted, "scale world placements apply");
    Check(serial.Snapshot().buildings.size() >= 16384,
          "workforce scale world exceeds parallel production threshold");
    Check(serial.Snapshot().stateHash == parallel.Snapshot().stateHash,
          "equivalently constructed scale worlds start bit-identically");

    SetProductionThreads(1);
    Check(serial.Step(12).accepted, "serial long step succeeds");

    SetProductionThreads(8);
    Check(parallel.Step(3).accepted && parallel.Step(2).accepted && parallel.Step(7).accepted,
          "parallel batched steps succeed");
    SetProductionThreads(0);

    const auto serialSnapshot = serial.Snapshot();
    const auto parallelSnapshot = parallel.Snapshot();
    Check(serialSnapshot.stateHash == parallelSnapshot.stateHash,
          "serial long run matches parallel batched run hash");
    Check(serialSnapshot.workersAssigned == workplaceCount * 2 &&
              parallelSnapshot.workersAssigned == serialSnapshot.workersAssigned,
          "large workforce assignment totals remain deterministic");
    Check(serialSnapshot.buildings.size() == parallelSnapshot.buildings.size(),
          "large production building count remains deterministic");
    if (serialSnapshot.buildings.size() == parallelSnapshot.buildings.size()) {
        bool sameRecipes = true;
        for (std::size_t i = 0; i < serialSnapshot.buildings.size(); ++i) {
            const auto &a = serialSnapshot.buildings[i].recipes;
            const auto &b = parallelSnapshot.buildings[i].recipes;
            if (a.size() != b.size() || (serialSnapshot.buildings[i].type == 1 && a.size() != 2) ||
                (serialSnapshot.buildings[i].type == 0 && !a.empty())) {
                sameRecipes = false;
                break;
            }
            for (std::size_t recipe = 0; recipe < a.size(); ++recipe)
                if (a[recipe].remainingTicks != b[recipe].remainingTicks ||
                    a[recipe].completedCycles != b[recipe].completedCycles) {
                    sameRecipes = false;
                    break;
                }
            if (!sameRecipes)
                break;
        }
        Check(sameRecipes, "both recipes match per building in large parallel world");
    }
}

void CheckPartialStaffingCheckpointContinuation() {
    const auto catalog = MakeWorkforceCatalog(2, 4);
    World uninterrupted(catalog, 16, 16);
    uninterrupted.PlaceBuilding(0, {1, 1});
    uninterrupted.PlaceBuilding(1, {5, 5});
    Check(uninterrupted.Step().accepted, "partial staffing buildings place");
    auto first = uninterrupted.Snapshot();
    Check(first.population == 2 && first.workersAssigned == 2 && first.workersRequired == 4,
          "partial workforce is reported accurately");
    Check(first.buildings[1].recipes[0].completedCycles == 0 &&
              first.buildings[1].recipes[1].completedCycles == 0,
          "partial staffing preserves current tick-modulo schedule");
    Check(uninterrupted.Step().accepted, "partial staffing scheduled tick succeeds");
    first = uninterrupted.Snapshot();
    Check(first.buildings[1].recipes[0].completedCycles == 1 &&
              first.buildings[1].recipes[1].completedCycles == 1,
          "multiple recipes run on the scheduled partial-staffing tick");

    const auto checkpoint = SaveWorld(uninterrupted);
    Check(!checkpoint.empty(), "small partial-staffing checkpoint fits native save bounds");
    World restored(catalog, 16, 16);
    ParseError error;
    Check(LoadWorld(checkpoint, catalog, restored, error), "partial-staffing checkpoint loads");
    Check(uninterrupted.Step(6).accepted, "uninterrupted continuation succeeds");
    Check(restored.Step(2).accepted && restored.Step(4).accepted,
          "batched checkpoint continuation succeeds");
    Check(uninterrupted.Snapshot().stateHash == restored.Snapshot().stateHash,
          "partial-staffing checkpoint continuation matches uninterrupted hash");
}

void CheckSaturatingAssignmentAndUtilityPhase() {
    const auto huge = MakeWorkforceCatalog(UINT64_MAX, UINT64_MAX);
    World world(huge, 16, 16);
    world.PlaceBuilding(0, {1, 1});
    world.PlaceBuilding(0, {3, 1});
    world.PlaceBuilding(1, {5, 5});
    world.PlaceBuilding(1, {9, 9});
    Check(world.Step().accepted, "maximum uint64 population fixture places");
    const auto before = world.Snapshot();
    Check(before.population == UINT64_MAX && before.workersRequired == UINT64_MAX &&
              before.workersAssigned == UINT64_MAX,
          "overflowing aggregate population and worker totals saturate");
    Check(before.buildings[2].inventory[0].amount == 1 && before.buildings[3].inventory[0].amount == 0,
          "saturated population is not allocated twice");
    world.SetBuildingEnabled(3, false);
    world.Step();
    Check(world.Snapshot().buildings[3].inventory[0].amount == 1,
          "maximum allocation is released by disabled workplace without overflow");

    Catalog utility;
    ParseError error;
    Check(ParseCatalogText(
        "openrepublic-economy 1\nutility power\nresource ore\n"
        "building dorm 1 1\nhousing 4\nend\n"
        "building generator 1 1\nworkers 2\nrecipe generate every 1 out power:2\nend\n"
        "building mine 1 1\nworkers 2\nstore ore 100\n"
        "recipe mine every 1 in power:2 out ore:1\nend\n", utility, error),
        "utility workforce fixture parses");
    World powered(utility, 16, 16);
    powered.PlaceBuilding(0, {1, 1});
    powered.PlaceBuilding(1, {4, 4});
    powered.PlaceBuilding(1, {7, 7});
    powered.PlaceBuilding(2, {10, 10});
    powered.SetBuildingEnabled(2, false);
    powered.ConnectUtility(0, 3, 4);
    Check(powered.Step().accepted, "utility workforce setup applies");
    Check(powered.Snapshot().buildings[3].inventory[0].amount == 1,
          "utility generator and consumer share enabled-only assignment plan");
    Check(powered.QueryUtilityDiagnostics().available,
          "workforce utility tick retains diagnostic receipt");
    const auto save = SaveWorld(powered);
    World resumed(utility, 16, 16);
    Check(!save.empty() && LoadWorld(save, utility, resumed, error), "workforce utility save reloads");
    powered.Step(5);
    resumed.Step(5);
    Check(SaveWorld(powered) == SaveWorld(resumed), "workforce utility continuation matches exactly");
}
} // namespace

int main() {
    CheckDisabledWorkplaceDoesNotReserveResidents();
    CheckLargeWorldDeterminism();
    CheckPartialStaffingCheckpointContinuation();
    CheckSaturatingAssignmentAndUtilityPhase();
    SetProductionThreads(0);
    if (failures)
        return EXIT_FAILURE;
    std::cout << "OpenRepublic workforce scale tests passed\n";
    return EXIT_SUCCESS;
}
