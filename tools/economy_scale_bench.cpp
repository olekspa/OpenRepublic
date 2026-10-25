// OpenRepublic synthetic combined scale workload. WRSR parity is UNKNOWN.
#include "openrepublic/economy/economy.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "Psapi.lib")
#elif defined(__linux__)
#include <sys/resource.h>
#endif

using namespace openrepublic::economy;
using Clock = std::chrono::steady_clock;

namespace {
std::size_t PeakMemoryMiB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    return GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))
        ? static_cast<std::size_t>(counters.PeakWorkingSetSize / (1024 * 1024)) : 0;
#elif defined(__linux__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) return static_cast<std::size_t>(usage.ru_maxrss / 1024);
    return 0;
#else
    return 0;
#endif
}

bool MakeCatalog(Catalog& catalog, std::string& error) {
    const std::string source =
        "openrepublic-economy 1\nresource goods\ntransit threshold 4 transfer 2\ncitizens\n"
        "building home 1 1\n housing 4\n end\n"
        "building workplace 1 1\n workers 2\n store goods 100\n recipe produce every 1 out goods:1\n end\n"
        "building station 1 1\n station\n end\n"
        "building depot 1 1\n depot\n fleet truck 8\n handling 1 1\n end\n"
        "building site 1 1\n build goods:1 work 3\n end\n"
        "vehicle bus capacity 16 speed 2\nvehicle truck capacity 8 speed 1\n";
    ParseError parse;
    if (ParseCatalogText(source, catalog, parse) && catalog.CitizensEnabled() &&
        catalog.Transit().declared) return true;
    if (parse.message.empty()) error = "citizens or transit declaration missing";
    else error = parse.message;
    return false;
}

bool BuildWorld(std::size_t count, const Catalog& catalog, World& world, std::string& error) {
    constexpr BuildingTypeId homeType = 0, workType = 1, stationType = 2, depotType = 3, siteType = 4;
    const auto pairs = count > 4 ? (count - 4) / 2 : 1;
    const auto roadEnd = static_cast<std::int32_t>(pairs * 4 + 2);
    for (std::int32_t x = 0; x < roadEnd; ++x)
        if (!world.BuildRoad({x, 2}, {x + 1, 2})) { error = "road command rejected"; return false; }

    const auto leftStation = world.PlaceBuilding(stationType, {0, 3});
    const auto rightStation = world.PlaceBuilding(stationType, {roadEnd, 3});
    if (!leftStation || !rightStation) { error = "station command rejected"; return false; }
    for (std::size_t i = 0; i < pairs; ++i) {
        const auto x = static_cast<std::int32_t>(i * 4 + 1);
        if (!world.PlaceBuilding(homeType, {x, 1}) || !world.PlaceBuilding(workType, {x + 2, 3})) {
            error = "building command rejected"; return false;
        }
    }
    const auto depotX = static_cast<std::int32_t>(roadEnd / 2);
    if (!world.PlaceBuilding(depotType, {depotX, 4}) || !world.PlaceConstructionSite(siteType, {depotX + 1, 3})) {
        error = "depot/site command rejected"; return false;
    }
    const auto setup = world.Step();
    if (!setup.accepted) { error = "initial roads/buildings did not apply"; return false; }
    const auto placed = world.Snapshot();
    if (placed.buildings.size() < 2) { error = "stations missing from snapshot"; return false; }
    const auto route = world.CreateTransitRoute({placed.buildings[0].id, placed.buildings[1].id}, 0);
    if (!route || !world.Step().accepted) { error = "transit route did not apply"; return false; }
    const auto ready = world.Snapshot();
    const auto expected = 2 * pairs + 4;
    const auto site = std::find_if(ready.buildings.begin(), ready.buildings.end(),
        [siteType](const BuildingSnapshot& building) { return building.type == siteType && building.constructionSite; });
    if (ready.buildings.size() != expected || ready.transitRoutes.size() != 1 ||
        ready.vehicles.size() != 8 || site == ready.buildings.end()) {
        error = "combined workload is missing a requested building, route, depot fleet, or construction site";
        return false;
    }
    (void)catalog;
    return true;
}

bool Run(std::size_t buildings, std::uint32_t ticks, unsigned threads, bool guard) {
    SetProductionThreads(threads);
    Catalog catalog;
    std::string error;
    if (!MakeCatalog(catalog, error)) { std::cerr << "catalog: " << error << '\n'; return false; }
    const auto pairs = std::max<std::size_t>(1, buildings > 4 ? (buildings - 4) / 2 : 1);
    World world(catalog, static_cast<std::int32_t>(pairs * 4 + 8), 8);
    if (!BuildWorld(buildings, catalog, world, error)) { std::cerr << "setup: " << error << '\n'; return false; }
    const auto actualBuildings = world.Snapshot().buildings.size();
    const auto start = Clock::now();
    for (std::uint32_t i = 0; i < ticks; ++i) world.Step();
    const auto seconds = std::chrono::duration<double>(Clock::now() - start).count();
    const auto tps = seconds > 0 ? ticks / seconds : 0;
    const auto peak = PeakMemoryMiB();
    if (guard) {
        if (actualBuildings != 1004 || seconds > 15.0) {
            std::cerr << "scale guard failed: buildings=" << actualBuildings << " seconds=" << seconds << '\n';
            return false;
        }
    } else {
        std::cout << buildings << ',' << threads << ',' << ticks << ',' << actualBuildings << ','
                  << seconds << ',' << tps << ',' << peak << ',' << world.Snapshot().stateHash << '\n';
    }
    return true;
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--guard") return Run(1004, 3, 1, true) ? 0 : 1;
    if (argc != 4) {
        std::cerr << "Usage: OpenRepublicEconomyCombinedScaleBench <buildings> <ticks> <threads>\n";
        return 2;
    }
    try {
        const auto buildings = static_cast<std::size_t>(std::stoull(argv[1]));
        const auto ticks = static_cast<std::uint32_t>(std::stoul(argv[2]));
        const auto threads = static_cast<unsigned>(std::stoul(argv[3]));
        if (buildings < 100 || buildings > 20000 || !ticks || ticks > 10000 ||
            (threads != 1 && threads != 2 && threads != 4 && threads != 8)) return 2;
        return Run(buildings, ticks, threads, false) ? 0 : 1;
    } catch (...) { return 2; }
}
