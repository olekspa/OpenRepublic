#include "openrepublic/economy/economy.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace openrepublic::economy;

namespace {
int failures = 0;
void Check(bool condition, const char *message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n";
    }
}
const BuildingSnapshot *Building(const WorldSnapshot &s, BuildingId id) {
    for (const auto &b : s.buildings)
        if (b.id == id)
            return &b;
    return nullptr;
}
const InventorySnapshot *Inventory(const BuildingSnapshot &b, ResourceId r) {
    for (const auto &i : b.inventory)
        if (i.resource == r)
            return &i;
    return nullptr;
}
void PlaceChain(World &w) {
    for (int x = 1; x < 10; ++x)
        w.BuildRoad({x, 1}, {x + 1, 1});
    w.PlaceBuilding(0, {1, 2});
    w.PlaceBuilding(1, {4, 2});
    w.PlaceBuilding(2, {7, 2});
    w.PlaceBuilding(3, {10, 2});
    w.Step();
}
void QueueOrders(World &w, const Catalog &c) {
    ResourceId grain{}, flour{}, bread{};
    c.FindResource("grain", grain);
    c.FindResource("flour", flour);
    c.FindResource("bread", bread);
    w.CreateHaulOrder(1, 2, grain, 0);
    w.CreateHaulOrder(2, 3, flour, 0);
    w.CreateHaulOrder(3, 4, bread, 0);
    w.Step();
}
} // namespace

int main() {
    {
        CatalogBuilder b;
        CatalogErrorInfo e;
        ResourceId grain{};
        Check(b.AddResource("grain", &grain, e) && grain == 0, "catalog assigns dense resource IDs");
        Check(!b.AddResource("grain", nullptr, e) && e.code == CatalogError::DuplicateName &&
                  !e.message.empty(),
              "catalog rejects duplicate resource names");
        BuildingTypeId farm{};
        Check(b.AddBuildingType("farm", 2, 2, &farm, e), "catalog adds building");
        Check(!b.AddStorage(farm, 99, 1, e) && e.code == CatalogError::UnknownResource,
              "catalog rejects unknown storage resource");
        Check(b.AddStorage(farm, grain, 4, e), "catalog adds storage");
        Check(!b.AddStorage(farm, grain, 4, e) && e.code == CatalogError::DuplicateStorage,
              "catalog rejects duplicate storage");
        Check(!b.AddRecipe(farm, {"bad", 0, {}, {{grain, 1}}}, e) && e.code == CatalogError::InvalidInterval,
              "catalog rejects zero interval");
        Check(b.AddRecipe(farm, {"harvest", 1, {}, {{grain, 1}}}, e), "catalog adds extractor recipe");
        Catalog c;
        Check(b.Build(c, e) && c.Resources().size() == 1 && c.BuildingTypes()[0].recipes.size() == 1,
              "catalog build succeeds");
    }
    Catalog catalog = MakeSyntheticCatalog();
    Check(catalog.Resources().size() >= 4 && catalog.BuildingTypes().size() >= 4 &&
              catalog.VehicleTypes().size() == 1,
          "synthetic catalog contains chained economy");
    {
        const std::string text =
            "openrepublic-economy 1\n# comment\nresource grain\nresource flour\nbuilding farm 2 2\n store "
            "grain 20\n recipe harvest every 2 out grain:2\nend\nvehicle truck capacity 4 speed 1\n";
        Catalog parsed;
        ParseError e;
        Check(ParseCatalogText(text, parsed, e) && e.code == ParseErrorCode::None,
              "catalog text parses with comments and whitespace");
        const auto canonical = WriteCatalogText(parsed);
        Catalog round;
        Check(ParseCatalogText(canonical, round, e) && WriteCatalogText(round) == canonical,
              "catalog text round-trips canonically");
        Catalog untouched = MakeSyntheticCatalog();
        const auto before = WriteCatalogText(untouched);
        Check(!ParseCatalogText("openrepublic-economy 2\n", untouched, e) &&
                  WriteCatalogText(untouched) == before,
              "catalog parse failure is transactional");
    }
    {
        World w(catalog, 16, 8);
        PlaceChain(w);
        auto setup = w.Snapshot();
        Check(setup.buildings.size() == 4 && setup.roads.size() == 9,
              "queued placements and roads apply at one tick boundary");
        QueueOrders(w, catalog);
        for (std::uint32_t i = 0; i < 300; ++i)
            w.Step();
        auto s = w.Snapshot();
        const auto *market = Building(s, 4);
        const auto *bakery = Building(s, 3);
        Check(market && market->recipes[0].completedCycles > 0,
              "farm to mill to bakery to market chain reaches consumer sink");
        Check(bakery && bakery->recipes[0].completedCycles > 0, "bakery completes chained production");
        Check(s.stateHash != 0 && s.revision > s.tick,
              "snapshot exposes deterministic hash and monotonic revision");
        for (const auto &b : s.buildings)
            for (const auto &i : b.inventory)
                Check(i.amount <= i.capacity, "inventory never exceeds catalog capacity");
    }
    {
        World w(catalog, 8, 8);
        w.PlaceBuilding(1, {2, 2});
        w.Step();
        auto missing = w.Explain(1);
        Check(missing.reason == StallReason::MissingInput, "missing input explanation is structured");
        auto disable = w.SetBuildingEnabled(1, false);
        w.Step();
        Check(w.GetCommandResult(disable).state == CommandState::Accepted &&
                  w.Explain(1).reason == StallReason::Disabled,
              "disabled building explanation is structured");
        World output(catalog, 8, 8);
        output.PlaceBuilding(0, {2, 2});
        output.Step(30);
        Check(output.Explain(1).reason == StallReason::OutputFull, "full output explanation is structured");
        World noroute(catalog, 8, 8);
        noroute.PlaceBuilding(0, {2, 2});
        noroute.PlaceBuilding(1, {5, 2});
        noroute.Step(2);
        auto c = noroute.CreateHaulOrder(1, 2, 0, 0);
        noroute.Step();
        auto ns = noroute.Snapshot();
        Check(noroute.GetCommandResult(c).state == CommandState::Accepted &&
                  ns.orders[0].state == HaulState::NoRoute && ns.orders[0].failure == HaulFailure::NoRoute,
              "haul records no route");
        World disabled(catalog, 8, 8);
        disabled.BuildRoad({2, 1}, {3, 1});
        disabled.BuildRoad({3, 1}, {4, 1});
        disabled.BuildRoad({4, 1}, {5, 1});
        disabled.PlaceBuilding(0, {2, 2});
        disabled.PlaceBuilding(1, {5, 2});
        disabled.Step();
        disabled.CreateHaulOrder(1, 2, 0, 0);
        disabled.Step(2);
        auto close = disabled.SetRoadEnabled(1, false);
        disabled.Step();
        auto ds = disabled.Snapshot();
        Check(disabled.GetCommandResult(close).state == CommandState::Accepted && !ds.orders.empty() &&
                  (ds.orders[0].failure == HaulFailure::RoadDisabled ||
                   ds.orders[0].failure == HaulFailure::NoRoute),
              "road disabled mid-route is observable");
    }
    {
        World a(catalog, 16, 8), b(catalog, 16, 8);
        PlaceChain(a);
        PlaceChain(b);
        QueueOrders(a, catalog);
        QueueOrders(b, catalog);
        for (int i = 0; i < 80; ++i) {
            Check(a.Snapshot().stateHash == b.Snapshot().stateHash,
                  "identical command sequences have identical hashes");
            a.Step();
            b.Step();
        }
    }
    {
        World original(catalog, 16, 8);
        PlaceChain(original);
        QueueOrders(original, catalog);
        original.Step(40);
        const auto save = SaveWorld(original);
        World restored(catalog, 1, 1);
        ParseError e;
        Check(LoadWorld(save, catalog, restored, e) &&
                  restored.Snapshot().stateHash == original.Snapshot().stateHash,
              "mid-run save/load preserves state hash");
        for (int i = 0; i < 100; ++i) {
            original.Step();
            restored.Step();
            Check(original.Snapshot().stateHash == restored.Snapshot().stateHash,
                  "save/load continuation is deterministic");
        }
        World unchanged(catalog, 16, 8);
        PlaceChain(unchanged);
        auto before = unchanged.Snapshot().stateHash;
        Check(!LoadWorld("OPENREPUBLIC_ECONOMY_WORLD 9\n", catalog, unchanged, e) &&
                  unchanged.Snapshot().stateHash == before,
              "invalid world load is transactional");
    }
    {
        World w(catalog, 8, 8);
        auto invalid = w.PlaceBuilding(99, {1, 1});
        w.Step();
        Check(w.GetCommandResult(invalid).state == CommandState::Rejected &&
                  w.GetCommandResult(invalid).failure == CommandFailure::UnknownBuildingType,
              "invalid building command is rejected");
        auto badroad = w.BuildRoad({0, 0}, {2, 0});
        w.Step();
        Check(w.GetCommandResult(badroad).failure == CommandFailure::InvalidRoadGeometry,
              "diagonal or non-unit road is rejected");
        auto remove = w.RemoveBuilding(999);
        w.Step();
        Check(w.GetCommandResult(remove).failure == CommandFailure::UnknownBuilding,
              "unknown removal is rejected");
        Check(!w.Step(100001).accepted && w.Tick() == 3, "step bound rejects without advancing");
    }
    {
        // Parallel production must be bit-identical to the serial path.
        auto run = [&](unsigned threads) {
            SetProductionThreads(threads);
            World w(catalog, 520, 520);
            for (int gx = 0; gx < 128; ++gx)
                for (int gz = 0; gz < 128; ++gz)
                    w.PlaceBuilding(static_cast<BuildingTypeId>((gx + gz) % 2 == 0 ? 0 : 1), {2 + gx * 4, 3 + gz * 4});
            for (std::uint32_t i = 0; i < 40; ++i)
                w.Step();
            const auto snap = w.Snapshot();
            return std::make_pair(snap.stateHash, snap.buildings.size());
        };
        const auto serial = run(1);
        const auto two = run(2);
        const auto four = run(4);
        const auto parallel = run(8);
        SetProductionThreads(0);
        Check(serial.second >= 16384, "parallel determinism world exceeds the threading threshold");
        Check(serial == two && serial == four && serial == parallel,
              "parallel production matches serial hashes at 1/2/4/8 threads");
    }
    {
        // Rules swap: identical structure is accepted, a different one is not.
        const std::string base = WriteCatalogText(catalog);
        std::string text = base;
        const auto at = text.find("store ");
        Check(at != std::string::npos, "catalog text has a store line");
        const auto end = text.find('\n', at);
        const std::string line = text.substr(at, end - at);
        const auto lastSpace = line.rfind(' ');
        text.replace(at, end - at,
                     line.substr(0, lastSpace + 1) + std::to_string(std::stoull(line.substr(lastSpace + 1)) * 2));
        Catalog doubled, different;
        ParseError pe;
        Check(ParseCatalogText(text, doubled, pe), "doubled-capacity catalog parses");
        std::string moreResources = base;
        moreResources.insert(moreResources.find("resource "), "resource zzz_extra\n");
        Check(ParseCatalogText(moreResources, different, pe), "different catalog parses");
        World w(catalog, 16, 8);
        PlaceChain(w);
        std::string err;
        Check(w.ReplaceCatalog(doubled, err), "same-structure catalog with new capacity is accepted");
        Check(&w.GetCatalog() == &doubled, "world now references the replacement catalog");
        Check(!w.ReplaceCatalog(different, err) && !err.empty(), "different structure is rejected");
        Check(&w.GetCatalog() == &doubled, "a rejected swap leaves the catalog in place");
    }
    if (failures) {
        std::cerr << failures << " economy checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All OpenRepublic economy checks passed.\n";
    return EXIT_SUCCESS;
}
