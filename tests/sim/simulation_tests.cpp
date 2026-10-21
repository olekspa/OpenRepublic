#include "openrepublic/sim/simulation.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

using openrepublic::sim::Resource;
using openrepublic::sim::Simulation;
using openrepublic::sim::SimulationRules;
using openrepublic::sim::TruckStage;

namespace {
int failures = 0;
void Check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

std::string WithTruckRecord(std::string data, const std::string& record) {
    const auto begin = data.find("TRUCK ");
    if (begin == std::string::npos) return {};
    const auto end = data.find('\n', begin);
    if (end == std::string::npos) return {};
    data.replace(begin, end - begin, record);
    return data;
}

std::string RemoveSaveRecord(std::string data, const std::string& prefix) {
    std::istringstream input(data);
    std::ostringstream output;
    std::string line;
    while (std::getline(input, line)) {
        if (line.compare(0, prefix.size(), prefix) != 0) output << line << '\n';
    }
    return output.str();
}

std::string AsVersionOneSave(std::string data) {
    const auto header = data.find("OPENREPUBLIC_SIM 3");
    if (header == std::string::npos) return {};
    data.replace(header, std::string("OPENREPUBLIC_SIM 3").size(), "OPENREPUBLIC_SIM 1");
    data = RemoveSaveRecord(std::move(data), "PROTOTYPE_STORAGES ");
    return RemoveSaveRecord(std::move(data), "RULES ");
}

std::string AsVersionTwoSave(std::string data) {
    const auto header = data.find("OPENREPUBLIC_SIM 3");
    if (header == std::string::npos) return {};
    data.replace(header, std::string("OPENREPUBLIC_SIM 3").size(), "OPENREPUBLIC_SIM 2");
    return RemoveSaveRecord(std::move(data), "RULES ");
}
}

int main() {
    std::string error;
    Simulation defaultRulesSim;
    const auto defaultRules = defaultRulesSim.GetRules();
    Check(defaultRules.farmGrainPerHarvest == 2 && defaultRules.farmHarvestIntervalTicks == 2 &&
        defaultRules.millGrainPerTick == 1, "default rules preserve prior synthetic behavior");
    Check(defaultRulesSim.GetSnapshot().rules.farmGrainPerHarvest == 2 &&
        defaultRulesSim.GetSnapshot().rules.farmHarvestIntervalTicks == 2 &&
        defaultRulesSim.GetSnapshot().rules.millGrainPerTick == 1, "snapshot exposes active default rules");

    Simulation defaultParity;
    defaultParity.Step(5);
    Check(defaultParity.GetSnapshot().mill.grain == 2, "default farm harvest feeds two grain to mill after five ticks");
    defaultParity.Step();
    const auto defaultMillResult = defaultParity.GetSnapshot();
    Check(defaultMillResult.truckCargo == Resource::Flour && defaultMillResult.truckCargoAmount == 1,
        "default mill throughput remains one grain per tick");

    Simulation customRulesSim;
    SimulationRules customRules{3, 1, 2};
    Check(customRulesSim.SetRules(customRules, &error), "valid custom rules are accepted");
    Check(error.empty(), "successful rules update clears error string");
    customRulesSim.Step();
    Check(customRulesSim.GetSnapshot().farm.grain == 1 && customRulesSim.GetSnapshot().truckCargoAmount == 2,
        "custom harvest amount and interval produce three grain before the truck takes two");
    customRulesSim.Step(3); // Truck reaches and unloads at the mill.
    Check(customRulesSim.GetSnapshot().mill.grain == 2, "custom-rule grain reaches the mill");
    Check(customRulesSim.SetRoadEnabled(203, false) && customRulesSim.SetRoadEnabled(204, false),
        "mill throughput fixture isolates the mill from flour export");
    customRulesSim.Step();
    Check(customRulesSim.GetSnapshot().mill.grain == 0 && customRulesSim.GetSnapshot().mill.flour == 2,
        "custom mill throughput processes two grain into one-to-one flour");
    const auto customSave = customRulesSim.Save();
    Check(customSave.find("OPENREPUBLIC_SIM 3\n") == 0, "rules are persisted in version 3 saves");
    Simulation customRestored;
    Check(customRestored.Load(customSave, &error), "version 3 custom-rules save loads");
    Check(customRestored.GetRules().farmGrainPerHarvest == 3 && customRestored.GetRules().farmHarvestIntervalTicks == 1 &&
        customRestored.GetRules().millGrainPerTick == 2, "v3 load restores active rules");
    Check(customRestored.Save() == customSave, "v3 custom-rules save/load is canonical");
    customRulesSim.Step(120);
    customRestored.Step(120);
    Check(customRestored.Save() == customRulesSim.Save(), "v3 loaded custom rules produce the same future trajectory");
    const auto customBeforeInvalid = customRulesSim.Save();
    SimulationRules invalidRules{0, 1, 1};
    Check(!customRulesSim.SetRules(invalidRules, &error) && !error.empty(), "invalid rules are rejected with an error");
    Check(customRulesSim.Save() == customBeforeInvalid, "rejected rules update leaves active state unchanged");
    invalidRules = SimulationRules{1, 10001, 1};
    Check(!customRulesSim.SetRules(invalidRules, &error), "harvest interval above bound is rejected");
    invalidRules = SimulationRules{1, 1, 5};
    Check(!customRulesSim.SetRules(invalidRules, &error), "mill throughput above bound is rejected");

    // Repeatability under canonical fixed-step updates and stable IDs/order.
    Simulation first = Simulation::CreateSyntheticDemo();
    Simulation second = Simulation::CreateSyntheticDemo();
    first.Step(240);
    second.Step(240);
    Check(first.Save() == second.Save(), "same scenario and tick count produce identical canonical state");

    // End-to-end synthetic chain: farm yield -> mill recipe -> truck cargo -> warehouse.
    Simulation progression = Simulation::CreateSyntheticDemo();
    progression.Step(500);
    const auto progressed = progression.GetSnapshot();
    Check(progressed.tick == 500, "fixed tick count advances exactly");
    Check(progressed.warehouse.flour > 0, "flour reaches the independent warehouse");
    Check(progressed.deliveredFlourTotal >= progressed.warehouse.flour, "snapshot reports cumulative delivered flour");
    Check(progressed.warehouse.flour == 5, "warehouse reaches but never exceeds its independent capacity");
    Check(progressed.farm.grain <= 12, "farm storage never exceeds capacity");
    Check(progressed.mill.grain <= 4 && progressed.mill.flour <= 4, "mill inventories never exceed capacity");

    // Construction is an OpenRepublic-only grid prototype with stable IDs and bounded cells.
    Simulation placement = Simulation::CreateSyntheticDemo();
    Check(placement.PlacePrototypeStorage(-16, -16) == Simulation::FirstPrototypeStorageId,
        "prototype storage accepts inclusive minimum grid bounds");
    Check(placement.PlacePrototypeStorage(16, 16) == Simulation::FirstPrototypeStorageId + 1,
        "prototype storage accepts inclusive maximum grid bounds");
    Check(placement.PlacePrototypeStorage(-16, -16) == 0, "duplicate prototype storage cell is rejected");
    Check(placement.PlacePrototypeStorage(-17, 0) == 0, "prototype storage rejects x below grid bounds");
    Check(placement.PlacePrototypeStorage(0, 17) == 0, "prototype storage rejects z above grid bounds");
    for (std::int32_t i = 0; i < 6; ++i)
        Check(placement.PlacePrototypeStorage(i, 0) == Simulation::FirstPrototypeStorageId + 2 + i,
            "valid prototype storage receives next stable ID");
    Check(placement.PlacePrototypeStorage(10, 10) == 0, "ninth prototype storage is rejected");
    const auto placed = placement.GetSnapshot().prototypeStorages;
    Check(placed.size() == Simulation::MaxPrototypeStorages, "snapshot exposes no more than eight prototype storages");
    Check(placed.size() >= 2 && placed[0].gridX == -16 && placed[0].gridZ == -16 &&
        placed[1].gridX == 16 && placed[1].gridZ == 16, "snapshot exposes stable IDs and grid coordinates in ID order");

    // Save/load preserves construction and replays the next placement identically.
    const auto placedSave = placement.Save();
    Check(placedSave.find("OPENREPUBLIC_SIM 3\n") == 0, "construction uses the current version 3 save format");
    Simulation placementRestored;
    Check(placementRestored.Load(placedSave, &error), "version 3 save with construction loads");
    Check(placementRestored.Save() == placedSave, "construction save/load round-trip is canonical");
    Check(placement.PlacePrototypeStorage(0, 12) == placementRestored.PlacePrototypeStorage(0, 12),
        "next stable structure ID matches after save/load");
    Check(placement.Save() == placementRestored.Save(), "future construction state replays after save/load");

    // Enabled road states are visible and persist through version 2 saves.
    Simulation roadState;
    Check(roadState.SetRoadEnabled(202, false), "road state can close a synthetic road edge");
    const auto roadSave = roadState.Save();
    Simulation roadRestored;
    Check(roadRestored.Load(roadSave, &error), "version 3 road state save loads");
    const auto roadSnapshot = roadRestored.GetSnapshot();
    const auto closedRoad = std::find_if(roadSnapshot.roads.begin(), roadSnapshot.roads.end(),
        [](const openrepublic::sim::RoadView& road) { return road.id == 202; });
    Check(closedRoad != roadSnapshot.roads.end() && !closedRoad->enabled,
        "closed road is exposed in loaded snapshot");
    Check(roadRestored.Save() == roadSave, "road state save/load remains canonical");

    // Version 1 predates construction, and v1/v2 predate configurable rules.
    Simulation v1Source;
    v1Source.Step(23);
    const auto v1Save = AsVersionOneSave(v1Source.Save());
    Check(!v1Save.empty(), "test fixture represents the version 1 schema");
    Simulation v1Target;
    Check(v1Target.PlacePrototypeStorage(3, -4) != 0, "migration target starts with construction to replace");
    Check(v1Target.Load(v1Save, &error), "version 1 save loads through migration path");
    const auto migrated = v1Target.GetSnapshot();
    Check(migrated.tick == 23 && migrated.prototypeStorages.empty(),
        "version 1 migration preserves simulation state and supplies empty construction state");
    Check(std::all_of(migrated.roads.begin(), migrated.roads.end(),
        [](const openrepublic::sim::RoadView& road) { return road.enabled; }),
        "version 1 default-road fixture migrates with all roads open");
    Check(v1Target.Save().find("OPENREPUBLIC_SIM 3\n") == 0, "migrated state writes current save version");
    Check(v1Target.GetRules().farmGrainPerHarvest == 2 && v1Target.GetRules().farmHarvestIntervalTicks == 2 &&
        v1Target.GetRules().millGrainPerTick == 1, "version 1 migration supplies default rules");
    const auto v2Save = AsVersionTwoSave(v1Source.Save());
    Check(!v2Save.empty(), "test fixture represents the version 2 schema");
    Simulation v2Target;
    Check(v2Target.SetRules(SimulationRules{4, 3, 2}), "version 2 target starts with non-default rules");
    Check(v2Target.Load(v2Save, &error), "version 2 save loads through migration path");
    const auto v2Migrated = v2Target.GetSnapshot();
    Check(v2Migrated.tick == 23 && v2Migrated.prototypeStorages.empty(),
        "version 2 migration preserves state and construction collection");
    Check(v2Migrated.rules.farmGrainPerHarvest == 2 && v2Migrated.rules.farmHarvestIntervalTicks == 2 &&
        v2Migrated.rules.millGrainPerTick == 1, "version 2 migration supplies default rules");
    Check(v2Target.Save().find("OPENREPUBLIC_SIM 3\n") == 0, "v2 migrated state writes current save version");

    const auto v2ConstructionSave = AsVersionTwoSave(placedSave);
    Simulation v2ConstructionTarget;
    Check(v2ConstructionTarget.Load(v2ConstructionSave, &error), "version 2 construction save remains loadable");
    const auto v2ConstructionSnapshot = v2ConstructionTarget.GetSnapshot();
    Check(v2ConstructionSnapshot.prototypeStorages.size() == Simulation::MaxPrototypeStorages,
        "version 2 migration preserves all saved construction");
    Check(v2ConstructionSnapshot.rules.farmGrainPerHarvest == 2 &&
        v2ConstructionSnapshot.rules.farmHarvestIntervalTicks == 2 &&
        v2ConstructionSnapshot.rules.millGrainPerTick == 1, "version 2 construction migration supplies default rules");

    // Invalid version 3 construction and rule records fail transactionally.
    Simulation malformedTarget;
    malformedTarget.PlacePrototypeStorage(5, 6);
    const auto malformedBefore = malformedTarget.Save();
    auto badConstruction = placedSave;
    const auto storageLine = badConstruction.find("PROTOTYPE_STORAGES ");
    const auto storageEnd = badConstruction.find('\n', storageLine);
    badConstruction.replace(storageLine, storageEnd - storageLine, "PROTOTYPE_STORAGES 1 3001 -17 0");
    Check(!malformedTarget.Load(badConstruction, &error), "out-of-bounds saved structure is rejected");
    Check(malformedTarget.Save() == malformedBefore, "rejected construction save leaves existing state unchanged");
    auto badRules = customRulesSim.Save();
    const auto rulesLine = badRules.find("RULES ");
    const auto rulesEnd = badRules.find('\n', rulesLine);
    badRules.replace(rulesLine, rulesEnd - rulesLine, "RULES 0 1 1");
    Check(!customRulesSim.Load(badRules, &error), "out-of-bounds v3 rule record is rejected");
    Check(customRulesSim.Save() == customBeforeInvalid, "rejected v3 rules save leaves state unchanged");
    auto malformedRules = customRulesSim.Save();
    const auto malformedRulesLine = malformedRules.find("RULES ");
    const auto malformedRulesEnd = malformedRules.find('\n', malformedRulesLine);
    malformedRules.replace(malformedRulesLine, malformedRulesEnd - malformedRulesLine, "RULES 2 x 1");
    Check(!customRulesSim.Load(malformedRules, &error), "malformed v3 rule record is rejected");
    Check(customRulesSim.Save() == customBeforeInvalid, "malformed rules record rejection is transactional");

    // A disconnected road graph cannot dispatch cargo; local production remains bounded.
    Simulation disconnected;
    Check(disconnected.SetRoadEnabled(202, false), "known synthetic road edge can be disabled");
    disconnected.Step(100);
    const auto isolated = disconnected.GetSnapshot();
    Check(isolated.farm.grain == 12, "disconnected farm production saturates at its storage capacity");
    Check(isolated.truckNode == Simulation::FarmNodeId, "truck stays at farm when route is disconnected");
    Check(isolated.truckCargoAmount == 0, "truck does not load cargo without a route");
    Check(isolated.warehouse.flour == 0, "disconnected graph produces no warehouse delivery");

    // Save/load during active operation and compare future trajectory.
    Simulation saved;
    saved.Step(17);
    const auto bytes = saved.Save();
    Simulation restored;
    Check(restored.Load(bytes, &error), "valid versioned save loads");
    Check(error.empty(), "successful load clears error string");
    Check(restored.Save() == bytes, "load/save round-trip preserves canonical representation");
    saved.Step(160);
    restored.Step(160);
    Check(saved.Save() == restored.Save(), "restored simulation follows identical deterministic trajectory");

    // Closing a passed road segment is allowed. Its historical presence in the
    // active route must not make a later save impossible to load.
    Simulation passedRoad;
    passedRoad.Step(3); // Truck is at node 2 on route [1, 2, 3].
    Check(passedRoad.SetRoadEnabled(201, false), "a road behind the truck can be disabled");
    const auto passedRoadBytes = passedRoad.Save();
    Simulation passedRoadRestored;
    Check(passedRoadRestored.Load(passedRoadBytes, &error), "save with a disabled road behind the truck loads");
    Check(passedRoadRestored.Save() == passedRoadBytes, "disabled historical route edge survives canonical round-trip");
    passedRoad.Step(80);
    passedRoadRestored.Step(80);
    Check(passedRoadRestored.Save() == passedRoad.Save(), "reloaded truck keeps deterministic behavior with a closed road behind it");

    // Invalid loads must not partially mutate the existing object.
    const auto beforeBadLoad = restored.Save();
    auto badVersion = bytes;
    badVersion.replace(badVersion.find("OPENREPUBLIC_SIM 3"), std::string("OPENREPUBLIC_SIM 3").size(), "OPENREPUBLIC_SIM 9");
    Check(!restored.Load(badVersion, &error), "unsupported save version is rejected");
    Check(restored.Save() == beforeBadLoad, "rejected version leaves state unchanged");
    auto negativeTick = bytes;
    const auto tickLine = negativeTick.find("TICK ");
    const auto tickEnd = negativeTick.find('\n', tickLine);
    negativeTick.replace(tickLine, tickEnd - tickLine, "TICK -1");
    Check(!restored.Load(negativeTick, &error), "negative tick record is rejected");
    Check(restored.Save() == beforeBadLoad, "rejected negative tick leaves state unchanged");
    auto badRecord = bytes;
    const auto farmLine = badRecord.find("FARM ");
    badRecord.replace(farmLine, std::string("FARM 2").size(), "FARM 999");
    Check(!restored.Load(badRecord, &error), "over-capacity malformed record is rejected");
    Check(restored.Save() == beforeBadLoad, "rejected record leaves state unchanged");
    Check(!restored.Load(bytes + "GARBAGE\n", &error), "unexpected trailing data is rejected");
    Check(restored.Save() == beforeBadLoad, "trailing-data rejection leaves state unchanged");

    // The save parser must not accept delivery states the simulation cannot
    // produce, even when every listed road segment is individually valid.
    const auto activeRouteBytes = passedRoadBytes;
    const auto wrongOrigin = WithTruckRecord(activeRouteBytes, "TRUCK 2 1 1 2 0 2 2 3");
    Check(!restored.Load(wrongOrigin, &error), "grain delivery route with a non-farm origin is rejected");
    Check(restored.Save() == beforeBadLoad, "wrong-origin route rejection leaves state unchanged");
    const auto loopingRoute = WithTruckRecord(activeRouteBytes, "TRUCK 2 1 1 2 1 5 1 2 1 2 3");
    Check(!restored.Load(loopingRoute, &error), "repeated-node delivery route is rejected");
    Check(restored.Save() == beforeBadLoad, "looping route rejection leaves state unchanged");

    if (failures) {
        std::cerr << failures << " simulation checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All OpenRepublic simulation checks passed.\n";
    return EXIT_SUCCESS;
}
