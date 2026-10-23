// Not a test: prints the cost of one tick with serial and threaded production on a large world.
#include "openrepublic/economy/economy.h"
#include "openrepublic/economy_api/economy_api.h"

#ifdef OPENREPUBLIC_ECONOMY_HOST_AVAILABLE
#include "economy_protocol.h"
#endif

#include <chrono>
#include <iostream>

using namespace openrepublic::economy;

int main() {
    const Catalog catalog = MakeSyntheticCatalog();
    for (const unsigned threads : {1u, 2u, 4u, 8u}) {
        SetProductionThreads(threads);
        World w(catalog, 1100, 1100);
        for (int gx = 0; gx < 250; ++gx)
            for (int gz = 0; gz < 160; ++gz)
                w.PlaceBuilding(static_cast<BuildingTypeId>((gx + gz) % 2 == 0 ? 0 : 1), {2 + gx * 4, 3 + gz * 6});
        w.Step();
        const auto buildings = w.Snapshot().buildings.size();
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 100; ++i)
            w.Step();
        const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::cout << threads << " thread(s): " << buildings << " buildings, " << ms / 100
                  << " ms/tick, hash " << w.Snapshot().stateHash << '\n';
        if (threads == 1) {
            openrepublic::economy_api::EconomyApi api(w, {true, true});
            const auto apiStart = std::chrono::steady_clock::now();
            for (int i = 0; i < 20; ++i)
                api.Step();
            const auto apiMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - apiStart).count();
            const auto queryStart = std::chrono::steady_clock::now();
            for (int i = 0; i < 5; ++i)
                api.QueryWorld();
            const auto queryMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - queryStart).count();
            std::cout << "API 40000-building step: " << apiMs / 20 << " ms/tick; QueryWorld: "
                      << queryMs / 5 << " ms/query, hash " << w.Snapshot().stateHash << '\n';
            const auto revision = w.Revision();
            const auto pageStart = std::chrono::steady_clock::now();
            for (int i = 0; i < 100; ++i)
                api.QueryWorld(revision, "buildings", static_cast<std::size_t>(i) * 32, 32);
            const auto pageMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - pageStart).count();
            std::cout << "API 40000-building page(32): " << pageMs / 100 << " ms/query\n";
#ifdef OPENREPUBLIC_ECONOMY_HOST_AVAILABLE
            openrepublic::economy_host::Dispatcher host(openrepublic::economy_host::Role::Controller, w);
            const auto fullJsonStart = std::chrono::steady_clock::now();
            const auto fullJson = host.HandleLine("{\"id\":\"full\",\"protocolVersion\":0,\"method\":\"world\"}");
            const auto fullJsonMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - fullJsonStart).count();
            const std::string pageRequest = "{\"id\":\"page\",\"protocolVersion\":0,\"method\":\"world\",\"revision\":\"" +
                std::to_string(revision) + "\",\"kind\":\"buildings\",\"offset\":0,\"limit\":32}";
            const auto pageJsonStart = std::chrono::steady_clock::now();
            const auto pageJson = host.HandleLine(pageRequest);
            const auto pageJsonMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - pageJsonStart).count();
            std::cout << "Host 40000-building world JSON: " << fullJsonMs << " ms, " << fullJson.size()
                      << " bytes; page(32): " << pageJsonMs << " ms, " << pageJson.size() << " bytes\n";
#endif
        }
    }
    // Haul movement: one road row with farm/mill pairs and a truck per order.
    for (const int pairs : {100, 400, 1500, 5000}) {
        World w(catalog, 12500, 8);
        for (int x = 1; x < 4 * pairs * 2; ++x)
            w.BuildRoad({x, 1}, {x + 1, 1});
        for (int i = 0; i < pairs * 2; ++i)
            w.PlaceBuilding(static_cast<BuildingTypeId>(i % 2), {2 + i * 4, 2});
        w.Step();
        ResourceId grain{};
        catalog.FindResource("grain", grain);
        for (int i = 0; i < pairs; ++i)
            w.CreateHaulOrder(static_cast<BuildingId>(2 * i + 1), static_cast<BuildingId>(2 * i + 2), grain, 0);
        for (int i = 0; i < 20; ++i)
            w.Step();
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 200; ++i)
            w.Step();
        const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::cout << pairs << " haul orders: " << ms / 200 << " ms/tick, hash "
                  << w.Snapshot().stateHash << '\n';
    }
    return 0;
}
