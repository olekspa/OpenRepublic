#include "openrepublic/economy/economy.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace openrepublic::economy;

namespace {
void Check(bool ok, const char *message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
Catalog FixtureCatalog(std::uint64_t depotCapacity = 100) {
    CatalogBuilder b;
    Catalog c;
    CatalogErrorInfo e;
    ResourceId grain{}, other{};
    BuildingTypeId source{}, depot{}, incompatible{};
    Check(b.AddResource("grain", &grain, e) && b.AddResource("other", &other, e) &&
              b.AddBuildingType("source", 1, 1, &source, e) &&
              b.AddBuildingType("depot", 1, 1, &depot, e) &&
              b.AddBuildingType("incompatible", 1, 1, &incompatible, e) &&
              b.AddStorage(source, grain, 100, e) && b.AddStorage(depot, grain, depotCapacity, e) &&
              b.AddStorage(incompatible, other, 100, e) &&
              b.AddRecipe(source, {"harvest", 1, {}, {{grain, 1}}}, e) &&
              b.AddVehicleType("truck", 4, 1, nullptr, e) && b.Build(c, e),
          "create synthetic retarget catalog");
    return c;
}
void Setup(World &w, bool loaded = true) {
    for (int x = 1; x < 16; ++x)
        w.BuildRoad({x, 1}, {x + 1, 1});
    w.PlaceBuilding(0, {1, 2});
    w.PlaceBuilding(1, {9, 2});
    w.PlaceBuilding(1, {15, 2});
    w.PlaceBuilding(1, {21, 2}); // Disconnected initially.
    w.PlaceBuilding(2, {25, 2}); // Cannot store grain.
    Check(w.Step(12).accepted, "fixture produces grain");
    w.SetBuildingEnabled(1, false);
    Check(w.Step().accepted && w.Snapshot().buildings.size() == 5, "freeze production for accounting");
    if (loaded) {
        w.CreateHaulOrder(1, 2, 0, 0);
        w.Step();
        Check(w.Snapshot().orders.size() == 1 && w.Snapshot().orders[0].cargo == 4,
              "fixture starts loaded between endpoints");
    }
}
std::uint64_t Total(const WorldSnapshot &s) {
    std::uint64_t total = 0;
    for (const auto &b : s.buildings)
        for (const auto &i : b.inventory)
            if (i.resource == 0)
                total += i.amount;
    for (const auto &v : s.vehicles)
        total += v.cargo;
    return total;
}
void TickConserved(World &w, std::uint32_t ticks, std::uint64_t total) {
    for (std::uint32_t i = 0; i < ticks; ++i) {
        Check(w.Step().accepted, "step accepted");
        const auto s = w.Snapshot();
        Check(Total(s) == total, "stored plus onboard cargo conserved each tick");
        for (const auto &b : s.buildings)
            for (const auto &slot : b.inventory)
                Check(slot.amount <= slot.capacity, "inventory stays within capacity");
    }
}
void Rejected(World &w, CommandId command, CommandFailure failure) {
    w.Step();
    const auto result = w.GetCommandResult(command);
    Check(result.state == CommandState::Rejected && result.failure == failure,
          "invalid retarget rejected at tick boundary");
}
void Replay(World &w, const Catalog &c, unsigned ticks) {
    World restored(c, 1, 1);
    ParseError e;
    Check(LoadWorld(SaveWorld(w), c, restored, e), "load retarget save");
    Check(w.Snapshot().stateHash == restored.Snapshot().stateHash, "retarget save hash matches");
    for (unsigned i = 0; i < ticks; ++i) {
        w.Step();
        restored.Step();
        Check(w.Snapshot().stateHash == restored.Snapshot().stateHash,
              "retarget save continuation matches at every tick");
    }
}
void ReplaceOnce(std::string &text, const std::string &from, const std::string &to) {
    const auto at = text.find(from);
    Check(at != std::string::npos, "save fixture token exists");
    text.replace(at, from.size(), to);
}
}

int main() {
    const auto c = FixtureCatalog();
    {
        World w(c, 28, 8);
        Setup(w);
        const auto before = w.Snapshot();
        const auto command = w.RetargetHaulOrder(1, 3);
        Replay(w, c, 1); // Includes a persisted, unapplied retarget.
        const auto after = w.Snapshot();
        Check(w.GetCommandResult(command).state == CommandState::Accepted &&
                  w.GetCommandResult(command).order == 1 && after.orders[0].destination == 3 &&
                  after.orders[0].vehicle == before.orders[0].vehicle &&
                  after.orders[0].cargo == before.orders[0].cargo &&
                  after.vehicles[0].cell == Cell{3, 1},
              "loaded retarget preserves identity and moves from current cell");
        const auto cancel = w.CancelHaulOrder(1);
        Rejected(w, cancel, CommandFailure::CargoInTransit);
        Check(w.Snapshot().orders.size() == 1 && w.Snapshot().vehicles.size() == 1 &&
                  Total(w.Snapshot()) == Total(before), "loaded cancellation preserves cargo and truck");
        Replay(w, c, 1); // Includes persisted CargoInTransit outcome and active retarget.
        TickConserved(w, 10, Total(before));
        Check(w.Snapshot().buildings[1].inventory[0].amount == 0 &&
                  w.Snapshot().buildings[2].inventory[0].amount == 4,
              "cargo reaches new destination and bypasses previous destination");
        TickConserved(w, 200, Total(before));
    }
    {
        const auto small = FixtureCatalog(4);
        World w(small, 28, 8);
        Setup(w);
        TickConserved(w, 7, 12); // Fill original destination, then cancel its empty truck.
        const auto cancel = w.CancelHaulOrder(1);
        w.Step();
        Check(w.GetCommandResult(cancel).state == CommandState::Accepted, "cancel after delivery succeeds");
        w.CreateHaulOrder(1, 3, 0, 0);
        w.Step();
        w.RetargetHaulOrder(2, 2); // Already carrying cargo, so retain it when destination is full.
        TickConserved(w, 8, 12);
        Check(w.Snapshot().orders[0].cargo == 4 &&
                  w.Snapshot().orders[0].failure == HaulFailure::DestinationFull &&
                  w.Snapshot().orders[0].vehicleCell == Cell{9, 1},
              "retarget to a full destination waits with its cargo intact");
        Replay(w, small, 3);
        w.RetargetHaulOrder(2, 3);
        TickConserved(w, 7, 12);
        Check(w.Snapshot().buildings[2].inventory[0].amount == 4 && w.Snapshot().orders[0].cargo == 0,
              "waiting loaded truck can be redirected to available storage");
    }
    {
        World w(c, 28, 8);
        Setup(w);
        const auto total = Total(w.Snapshot());
        auto retarget = w.RetargetHaulOrder(1, 4);
        w.Step();
        Check(w.GetCommandResult(retarget).state == CommandState::Accepted &&
                  w.Snapshot().orders[0].state == HaulState::NoRoute &&
                  w.Snapshot().orders[0].cargo == 4, "disconnected retarget retains cargo and reports no route");
        TickConserved(w, 4, total);
        Replay(w, c, 5);
        for (int x = 16; x < 21; ++x)
            w.BuildRoad({x, 1}, {x + 1, 1});
        TickConserved(w, 20, total);
        Check(w.Snapshot().buildings[3].inventory[0].amount == 4,
              "retarget recovers after road reconnection");
    }
    {
        World w(c, 28, 8);
        Setup(w);
        const auto total = Total(w.Snapshot());
        TickConserved(w, 6, total); // Truck at x8, still loaded.
        TickConserved(w, 1, total); // Unloads at x9, returns to x8 in the same tick.
        Check(w.Snapshot().orders[0].cargo == 0 && w.Snapshot().vehicles[0].cell == Cell{8, 1},
              "fixture reaches empty return leg");
        w.RetargetHaulOrder(1, 3);
        TickConserved(w, 1, total);
        Check(w.Snapshot().orders[0].cargo == 0 && w.Snapshot().vehicles[0].cell == Cell{7, 1},
              "empty retarget keeps returning to source before loading");
        w.SetRoadEnabled(6, false); // Close x6-x7 while truck is at x7.
        TickConserved(w, 3, total);
        Check(w.Snapshot().orders[0].state == HaulState::NoRoute &&
                  w.Snapshot().orders[0].cargo == 0 && w.Snapshot().vehicles[0].cell == Cell{7, 1},
              "blocked empty return cannot load remotely");
        Replay(w, c, 3);
        w.SetRoadEnabled(6, true);
        TickConserved(w, 7, total);
        Check(w.Snapshot().orders[0].cargo > 0 && w.Snapshot().vehicles[0].cell == Cell{2, 1},
              "reconnected empty return reaches source before loading");
    }
    {
        World w(c, 28, 8);
        Setup(w, false);
        const auto created = w.CreateHaulOrder(1, 2, 0, 0);
        const auto retarget = w.RetargetHaulOrder(1, 3);
        w.Step();
        Check(w.GetCommandResult(created).state == CommandState::Accepted &&
                  w.GetCommandResult(retarget).state == CommandState::Accepted &&
                  w.Snapshot().orders[0].destination == 3 && w.Snapshot().orders[0].cargo == 0,
              "retarget can follow creation in the same tick without loading away from source");
        TickConserved(w, 1, 12);
        Check(w.Snapshot().orders[0].cargo == 4, "idle retarget loads at source on its next tick");
    }
    {
        World w(c, 28, 8);
        Setup(w);
        Rejected(w, w.RetargetHaulOrder(999, 3), CommandFailure::UnknownOrder);
        Rejected(w, w.RetargetHaulOrder(1, 999), CommandFailure::InvalidOrderEndpoint);
        Rejected(w, w.RetargetHaulOrder(1, 1), CommandFailure::InvalidOrderEndpoint);
        Rejected(w, w.RetargetHaulOrder(1, 5), CommandFailure::InvalidOrderEndpoint);
        const auto other = w.CreateHaulOrder(1, 3, 0, 0);
        const auto duplicate = w.RetargetHaulOrder(1, 3);
        w.Step();
        Check(w.GetCommandResult(other).state == CommandState::Accepted &&
                  w.GetCommandResult(duplicate).failure == CommandFailure::DuplicateOrder &&
                  w.Snapshot().orders[0].destination == 2,
              "duplicate destination rejected using earlier same-tick commands");
        const auto noOp = w.RetargetHaulOrder(1, 2);
        w.Step();
        Check(w.GetCommandResult(noOp).state == CommandState::Accepted,
              "same destination is an accepted no-op");
    }
    for (bool cancelFirst : {false, true}) {
        World w(c, 28, 8);
        Setup(w, false);
        w.CreateHaulOrder(1, 2, 0, 0);
        w.CancelHaulOrder(1); // Created/cancelled before its first haul phase.
        w.Step();
        Check(w.Snapshot().orders.empty() && w.Snapshot().vehicles.empty(), "empty order cancels safely");
        w.CreateHaulOrder(1, 2, 0, 0);
        CommandId cancelled{}, retarget{};
        if (cancelFirst) {
            cancelled = w.CancelHaulOrder(2);
            retarget = w.RetargetHaulOrder(2, 3);
        } else {
            retarget = w.RetargetHaulOrder(2, 3);
            cancelled = w.CancelHaulOrder(2);
        }
        w.Step();
        Check(w.GetCommandResult(cancelled).state == CommandState::Accepted &&
                  w.GetCommandResult(retarget).state ==
                      (cancelFirst ? CommandState::Rejected : CommandState::Accepted) &&
                  w.Snapshot().orders.empty() && Total(w.Snapshot()) == 12,
              "cancel and retarget follow submission order without losing inventory");
    }
    {
        // Retained v1 layout with a queued placement, independent of today's writer.
        const std::string legacy =
            "OPENREPUBLIC_ECONOMY_WORLD 1\nGRID 28 8\nTICK 0\nREVISION 0\nNEXT 1 1 1 1 2\n"
            "BUILDINGS 0\nROADS 0\nORDERS 0\nPENDING 1\nQ 1 1 0 0 1 2 0 0 0 0 0 0 0 0 0\n"
            "RESULTS 1\nC 1 1 0 0 0 0\nEND\n";
        World w(c, 1, 1);
        ParseError e;
        Check(LoadWorld(legacy, c, w, e) && w.Step().accepted && w.Snapshot().buildings.size() == 1,
              "v1 queued command still loads and executes");
        Check(SaveWorld(w).find("OPENREPUBLIC_ECONOMY_WORLD 2\n") == 0, "writer emits v2");
        World loaded(c, 28, 8);
        Setup(loaded);
        auto v1Loaded = SaveWorld(loaded);
        ReplaceOnce(v1Loaded, "WORLD 2\n", "WORLD 1\n");
        Check(LoadWorld(v1Loaded, c, w, e), "v1 loaded truck still migrates");
        loaded.RetargetHaulOrder(1, 3);
        auto wrongVersion = SaveWorld(loaded);
        ReplaceOnce(wrongVersion, "WORLD 2\n", "WORLD 1\n");
        const auto hash = w.Snapshot().stateHash;
        Check(!LoadWorld(wrongVersion, c, w, e) && w.Snapshot().stateHash == hash,
              "v1 rejects the new command kind transactionally");
        auto badCargo = v1Loaded;
        ReplaceOnce(badCargo, "O 1 1 2 0 0 1 1 0 4 2 1 1", "O 1 1 2 0 0 1 1 0 5 2 1 1");
        Check(!LoadWorld(badCargo, c, w, e) && w.Snapshot().stateHash == hash,
              "oversized persisted cargo rejected transactionally");
        auto badCell = v1Loaded;
        ReplaceOnce(badCell, "4 2 1 1\nPENDING", "4 99 1 1\nPENDING");
        Check(!LoadWorld(badCell, c, w, e) && w.Snapshot().stateHash == hash,
              "out-of-grid persisted truck rejected transactionally");
    }
    std::cout << "Haul retarget, cargo conservation, and save migration checks passed.\n";
}
