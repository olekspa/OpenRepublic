// Opt-in synthetic workforce benchmark; never part of timing-sensitive CTest.
#include "openrepublic/economy/economy.h"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

using namespace openrepublic::economy;
using Clock = std::chrono::steady_clock;

int main(int argc, char **argv) {
    std::uint32_t count{};
    if (argc == 2) {
        const std::string token = argv[1];
        const auto parsed = std::from_chars(token.data(), token.data() + token.size(), count);
        if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) count = 0;
    }
    if (!count || count > 20000) {
        std::cerr << "Usage: OpenRepublicWorkforceBench <workplaces:1..20000>\n";
        return 2;
    }
    const std::string text =
        "openrepublic-economy 1\nresource ore\nresource goods\n"
        "building dorm 1 1\nhousing " + std::to_string(count) + "\nend\n"
        "building mine 1 1\nworkers 1\nstore ore 1000000000\nstore goods 1000000000\n"
        "recipe ore every 1 out ore:1\nrecipe goods every 1 out goods:1\nend\n";
    Catalog catalog;
    ParseError error;
    if (!ParseCatalogText(text, catalog, error)) {
        std::cerr << error.message << '\n';
        return 3;
    }
    World world(catalog, 64, ((count + 61) / 62) * 2 + 5);
    world.PlaceBuilding(0, {1, 1});
    for (std::uint32_t i = 0; i < count; ++i)
        if (!world.PlaceBuilding(1, {1 + static_cast<std::int32_t>(i % 62),
                                     3 + static_cast<std::int32_t>((i / 62) * 2)})) return 4;
    if (!world.Step().accepted || world.Snapshot().buildings.size() != count + 1) return 5;
    if (!world.Step().accepted) return 6; // warm-up; setup and warm-up are untimed
    const auto tickStart = Clock::now();
    if (!world.Step(20).accepted) return 7;
    const auto tickEnd = Clock::now();
    const auto snapshotStart = Clock::now();
    WorldSnapshot snapshot;
    for (int i = 0; i < 10; ++i) snapshot = world.Snapshot();
    const auto snapshotEnd = Clock::now();
    std::uint64_t ore = 0, goods = 0;
    for (const auto &building : snapshot.buildings)
        for (const auto &stock : building.inventory) {
            if (stock.resource == 0) ore += stock.amount;
            if (stock.resource == 1) goods += stock.amount;
        }
    if (snapshot.tick != 22 || snapshot.workersAssigned != count ||
        ore != std::uint64_t(count) * 22 || goods != ore) return 8;
    const auto tickMs = std::chrono::duration<double, std::milli>(tickEnd - tickStart).count();
    const auto snapshotMs = std::chrono::duration<double, std::milli>(snapshotEnd - snapshotStart).count();
    std::cout << "workplaces=" << count << " avg_tick_ms=" << tickMs / 20
              << " avg_snapshot_ms=" << snapshotMs / 10 << " tick=" << snapshot.tick
              << " stateHash=" << snapshot.stateHash << " ore=" << ore << " goods=" << goods << '\n';
}
