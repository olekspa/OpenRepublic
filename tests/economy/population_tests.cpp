#include "openrepublic/economy/economy.h"

#include <cstdlib>
#include <iostream>
#include <string>

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

    // v6 must enforce the same recipe-progress bound as all earlier versions.
    // A food-only opt-in also writes v6 even when its recipes need no workers.
    auto corruptProgress = saved;
    const auto progress = corruptProgress.find("P 2 1 ");
    check(progress != std::string::npos, "progress mutation finds mine record");
    if (progress != std::string::npos) {
        const auto value = progress + std::string("P 2 1 ").size();
        corruptProgress.replace(value, corruptProgress.find(' ', value) - value, "4294967295");
        const auto beforeFailure = SaveWorld(restored);
        check(!LoadWorld(corruptProgress, catalog, restored, loadError) && !loadError.message.empty(),
              "v6 rejects progress above recipe interval");
        check(SaveWorld(restored) == beforeFailure, "bad v6 progress leaves destination unchanged");
    }

    auto populationCatalog = [&](unsigned housing, unsigned workers, const std::string &food) {
        Catalog result;
        std::string source = "openrepublic-economy 1\nresource ore\nresource food\nresource fruit\n";
        if (!food.empty()) source += "food " + food + " 1 every 2\n";
        source += "building dorm 1 1\n";
        if (housing) source += "housing " + std::to_string(housing) + "\n";
        source += "end\nbuilding mine 1 1\n";
        if (workers) source += "workers " + std::to_string(workers) + "\n";
        source += "store ore 100\nrecipe extract every 3 out ore:1\nend\n";
        check(ParseCatalogText(source, result, parse), "reload fixture parses");
        return result;
    };
    const auto original = populationCatalog(2, 4, "");
    const auto smallerHousing = populationCatalog(1, 4, "");
    const auto largerHousing = populationCatalog(3, 6, "");
    const auto noHousing = populationCatalog(0, 4, "");
    const auto noWorkers = populationCatalog(2, 0, "");
    const auto addedFood = populationCatalog(2, 4, "food");
    World reloadWorld(original, 8, 8);
    reloadWorld.PlaceBuilding(0, {2, 2});
    reloadWorld.PlaceBuilding(1, {4, 2});
    reloadWorld.Step(4);
    const auto reloadBefore = SaveWorld(reloadWorld);
    std::string reloadError;
    for (const auto *rejected : {&smallerHousing, &noHousing, &noWorkers, &addedFood}) {
        check(!reloadWorld.ReplaceCatalog(*rejected, reloadError) && !reloadError.empty(),
              "unsafe population catalog reload rejects with diagnostic");
        check(SaveWorld(reloadWorld) == reloadBefore && &reloadWorld.GetCatalog() == &original,
              "rejected population reload is transactional");
    }
    check(reloadWorld.ReplaceCatalog(largerHousing, reloadError),
          "same-structure housing and workforce tuning succeeds");
    World reloadRestored(largerHousing, 8, 8);
    check(LoadWorld(SaveWorld(reloadWorld), largerHousing, reloadRestored, loadError),
          "accepted population tuning remains loadable before another tick");
    check(SaveWorld(reloadRestored) == SaveWorld(reloadWorld), "tuned population state round trips exactly");
    reloadWorld.Step(20);
    reloadRestored.Step(20);
    check(SaveWorld(reloadRestored) == SaveWorld(reloadWorld), "tuned population continuation matches");

    const auto foodCatalog = populationCatalog(2, 4, "food");
    const auto otherFood = populationCatalog(2, 4, "fruit");
    World foodWorld(foodCatalog, 8, 8);
    const auto foodBefore = SaveWorld(foodWorld);
    check(!foodWorld.ReplaceCatalog(otherFood, reloadError), "food resource rewiring rejects");
    check(!foodWorld.ReplaceCatalog(original, reloadError), "removing food opt-in rejects");
    check(SaveWorld(foodWorld) == foodBefore && &foodWorld.GetCatalog() == &foodCatalog,
          "food binding rejection preserves world and catalog");

    Catalog citizenCatalog;
    const std::string citizenText =
        "openrepublic-economy 1\nresource bread\nutility heat\n"
        "food bread 1 every 2\ncitizens\n"
        "building home 1 1\n housing 4\n recipe warmth every 1 in heat:1\n end\n"
        "building bakery 1 1\n store bread 16\n recipe bake every 1 out bread:1\n end\n"
        "building boiler 1 1\n recipe heat every 1 out heat:2\n end\n"
        "building mine 1 1\n workers 2\n end\n";
    check(ParseCatalogText(citizenText, citizenCatalog, parse), "citizen needs catalog parses");
    check(citizenCatalog.CitizensEnabled() && WriteCatalogText(citizenCatalog).find("citizens\n") != std::string::npos,
          "citizen opt-in round trips through catalog text");
    World citizens(citizenCatalog, 8, 8);
    BuildingTypeId home{}, bakery{}, boiler{}, mine{};
    ResourceId heat{};
    citizenCatalog.FindResource("heat", heat);
    citizenCatalog.FindBuildingType("home", home);
    citizenCatalog.FindBuildingType("bakery", bakery);
    citizenCatalog.FindBuildingType("boiler", boiler);
    citizenCatalog.FindBuildingType("mine", mine);
    citizens.PlaceBuilding(home, {1, 2});
    citizens.PlaceBuilding(bakery, {3, 2});
    citizens.PlaceBuilding(boiler, {5, 2});
    citizens.PlaceBuilding(mine, {7, 2});
    check(citizens.Step(1).accepted, "citizen needs buildings place");
    const auto placed = citizens.Snapshot();
    check(citizens.ConnectUtility(heat, placed.buildings[0].id, placed.buildings[2].id) != 0,
          "citizen housing heat connection queues");
    citizens.Step(1);
    citizens.Step(8);
    auto thriving = citizens.Snapshot();
    check(thriving.population > 1 && thriving.happiness == 100 && thriving.happyResidents == thriving.population,
          "food and connected heat support happy residents and immigration");
    check(thriving.workersAssigned > 0, "happy residents remain available for jobs");
    const auto citizenSave = SaveWorld(citizens);
    check(citizenSave.find("WORLD 6") != std::string::npos && citizenSave.find("POP ") != std::string::npos,
          "citizen state persists through opt-in population records");
    World citizenRestored(citizenCatalog, 8, 8);
    check(LoadWorld(citizenSave, citizenCatalog, citizenRestored, loadError) &&
          citizenRestored.Snapshot().stateHash == thriving.stateHash,
          "citizen save restores exact deterministic state");
    citizens.SetBuildingEnabled(thriving.buildings[2].id, false);
    citizens.Step(2);
    const auto cold = citizens.Snapshot();
    check(cold.population < thriving.population && cold.happiness < thriving.happiness &&
          cold.workersAssigned <= thriving.workersAssigned,
          "missing heat lowers happiness and available workforce");
    if (failures) return EXIT_FAILURE;
    std::cout << "OpenRepublic synthetic population tests passed\n";
    return EXIT_SUCCESS;
}
