// Not a test: prints the cost of one tick with serial and threaded production on a large world.
#include "openrepublic/economy/economy.h"

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
        std::cout << threads << " thread(s): " << buildings << " buildings, " << ms / 100 << " ms/tick\n";
    }
    return 0;
}
