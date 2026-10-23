#include "openrepublic/economy/economy.h"

#include <cstdlib>
#include <iostream>

using namespace openrepublic::economy;

int main() {
    int failures = 0;
    auto check = [&](bool value, const char *message) {
        if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };

    Catalog catalog;
    ParseError parse;
    const std::string text =
        "openrepublic-economy 1\n"
        "resource ore\n"
        "building dorm 1 1\n housing 2\n end\n"
        "building mine 1 1\n workers 4\n store ore 100\n"
        "recipe extract every 1 out ore:1\n end\n";
    check(ParseCatalogText(text, catalog, parse), "population declarations parse");
    check(WriteCatalogText(catalog).find("housing 2") != std::string::npos, "housing declaration canonicalizes");
    check(WriteCatalogText(catalog).find("workers 4") != std::string::npos, "worker declaration canonicalizes");

    const auto noPopulation = MakeSyntheticCatalog();
    World baseline(noPopulation, 8, 8);
    const auto before = SaveWorld(baseline);
    baseline.Step(1);
    World baselineAgain(noPopulation, 8, 8);
    ParseError loadError;
    check(LoadWorld(before, noPopulation, baselineAgain, loadError), "legacy save loads");
    baselineAgain.Step(1);
    check(baseline.Snapshot().stateHash == baselineAgain.Snapshot().stateHash,
          "undeclared population preserves deterministic baseline hash");
    check(SaveWorld(baseline).find("WORLD 2") != std::string::npos,
          "undeclared population preserves old save version");

    World world(catalog, 8, 8);
    world.PlaceBuilding(0, {2, 2});
    world.PlaceBuilding(1, {4, 2});
    check(world.Step(1).accepted, "population buildings place");
    check(world.Snapshot().population == 2, "residents fill authored housing");
    check(world.Snapshot().workersAssigned == 2 && world.Snapshot().workersRequired == 4,
          "workers distribute in stable building order");
    world.Step(1);
    check(world.Snapshot().buildings[1].inventory[0].amount == 1,
          "understaffed recipe runs at proportional rate");
    const auto saved = SaveWorld(world);
    check(saved.find("WORLD 6") != std::string::npos, "population state opts into save version 6");
    World restored(catalog, 8, 8);
    check(LoadWorld(saved, catalog, restored, loadError), "population save restores");
    check(restored.Snapshot().stateHash == world.Snapshot().stateHash, "population save restores hash");
    if (failures) return EXIT_FAILURE;
    std::cout << "OpenRepublic synthetic population tests passed\n";
    return EXIT_SUCCESS;
}
