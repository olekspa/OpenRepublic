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

std::string AsVersionFiveSave(std::string data) {
    const auto header=data.find("OPENREPUBLIC_SIM 6");
    if (header==std::string::npos) return {};
    data.replace(header,std::string("OPENREPUBLIC_SIM 6").size(),"OPENREPUBLIC_SIM 5");
    std::istringstream input(data);
    std::ostringstream output;
    std::string line;
    while (std::getline(input,line)) {
        if (line.rfind("ROAD_GRAPH ",0)==0) {
            std::istringstream record(line); std::string tag; std::uint32_t nextNode=0,nextRoad=0;
            std::uint64_t nodes=0,edges=0; record>>tag>>nextNode>>nextRoad>>nodes;
            for(std::uint64_t i=0;i<nodes*3;++i) { std::string ignored; record>>ignored; }
            record>>edges; line="ROADS";
            for(std::uint64_t i=0;i<edges;++i) {
                std::uint32_t id=0,a=0,b=0; unsigned enabled=0; record>>id>>a>>b>>enabled;
                if(i<4) line+=" "+std::to_string(enabled);
            }
        }
        else if (line.rfind("CONSTRUCTION_QUEUE ",0)==0) {
            std::istringstream record(line); std::string tag; std::uint64_t next=0,count=0;
            record>>tag>>next>>count; line="PLACEMENT_QUEUE "+std::to_string(next)+" "+std::to_string(count);
            for(std::uint64_t i=0;i<count;++i) {
                std::uint64_t id=0; unsigned type=0,facilityType=0; int x=0,z=0,bx=0,bz=0;
                record>>id>>type>>facilityType>>x>>z>>bx>>bz;
                if(type!=1) return {};
                line+=" "+std::to_string(id)+" "+std::to_string(facilityType)+" "+std::to_string(x)+" "+std::to_string(z);
            }
        } else if (line.rfind("CONSTRUCTION_RESULTS ",0)==0) {
            std::istringstream record(line); std::string tag; std::uint64_t count=0; record>>tag>>count;
            line="PLACEMENT_RESULTS "+std::to_string(count);
            for(std::uint64_t i=0;i<count;++i) {
                std::uint64_t id=0; unsigned type=0,accepted=0; std::uint32_t entity=0;
                record>>id>>type>>accepted>>entity;
                if(type!=1) return {};
                line+=" "+std::to_string(id)+" "+std::to_string(accepted)+" "+std::to_string(entity);
            }
        }
        output<<line<<'\n';
    }
    return output.str();
}

std::string AsVersionOneSave(std::string data) {
    data=AsVersionFiveSave(std::move(data));
    const auto header = data.find("OPENREPUBLIC_SIM 5");
    if (header == std::string::npos) return {};
    data.replace(header, std::string("OPENREPUBLIC_SIM 5").size(), "OPENREPUBLIC_SIM 1");
    data = RemoveSaveRecord(std::move(data), "PROTOTYPE_STORAGES ");
    data = RemoveSaveRecord(std::move(data), "RULES ");
    data = RemoveSaveRecord(std::move(data), "FACILITIES ");
    data = RemoveSaveRecord(std::move(data), "PLACEMENT_QUEUE ");
    return RemoveSaveRecord(std::move(data), "PLACEMENT_RESULTS ");
}

std::string AsVersionTwoSave(std::string data) {
    data=AsVersionFiveSave(std::move(data));
    const auto header = data.find("OPENREPUBLIC_SIM 5");
    if (header == std::string::npos) return {};
    data.replace(header, std::string("OPENREPUBLIC_SIM 5").size(), "OPENREPUBLIC_SIM 2");
    data = RemoveSaveRecord(std::move(data), "RULES ");
    data = RemoveSaveRecord(std::move(data), "FACILITIES ");
    data = RemoveSaveRecord(std::move(data), "PLACEMENT_QUEUE ");
    return RemoveSaveRecord(std::move(data), "PLACEMENT_RESULTS ");
}

std::string AsVersionThreeSave(std::string data) {
    data=AsVersionFiveSave(std::move(data));
    const auto header = data.find("OPENREPUBLIC_SIM 5");
    if (header == std::string::npos) return {};
    data.replace(header, std::string("OPENREPUBLIC_SIM 5").size(), "OPENREPUBLIC_SIM 3");
    data = RemoveSaveRecord(std::move(data), "FACILITIES ");
    data = RemoveSaveRecord(std::move(data), "PLACEMENT_QUEUE ");
    return RemoveSaveRecord(std::move(data), "PLACEMENT_RESULTS ");
}

std::string AsVersionFourSave(std::string data) {
    data=AsVersionFiveSave(std::move(data));
    const auto header = data.find("OPENREPUBLIC_SIM 5");
    if (header == std::string::npos) return {};
    data.replace(header, std::string("OPENREPUBLIC_SIM 5").size(), "OPENREPUBLIC_SIM 4");
    return RemoveSaveRecord(std::move(data), "PLACEMENT_RESULTS ");
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
    Check(customSave.find("OPENREPUBLIC_SIM 6\n") == 0, "rules are persisted in version 6 saves");
    Simulation customRestored;
    Check(customRestored.Load(customSave, &error), "version 6 custom-rules save loads");
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
    Check(placedSave.find("OPENREPUBLIC_SIM 6\n") == 0, "construction uses the current version 6 save format");
    Simulation placementRestored;
    Check(placementRestored.Load(placedSave, &error), "version 3 save with construction loads");
    Check(placementRestored.Save() == placedSave, "construction save/load round-trip is canonical");
    Check(placement.PlacePrototypeStorage(0, 12) == placementRestored.PlacePrototypeStorage(0, 12),
        "next stable structure ID matches after save/load");
    Check(placement.Save() == placementRestored.Save(), "future construction state replays after save/load");

    // Typed facilities are simulation-owned commands. Requests validate at a
    // tick boundary, reserve cells in request order, and affect synthetic rates.
    using openrepublic::sim::FacilityType;
    Simulation construction;
    const auto farmCommand = construction.QueueFacilityPlacement(FacilityType::Farm, 2, 3);
    const auto duplicateCommand = construction.QueueFacilityPlacement(FacilityType::Mill, 2, 3);
    const auto invalidTypeCommand = construction.QueueFacilityPlacement(static_cast<FacilityType>(99), 4, 3);
    Check(farmCommand == 1 && duplicateCommand == 2 && invalidTypeCommand == 3,
        "facility placement commands receive stable submission IDs");
    Check(construction.GetSnapshot().facilities.empty() && construction.GetSnapshot().facilityPlacementResults.empty(),
        "queued facilities are not visible before a fixed tick boundary");
    const auto queuedSave = construction.Save();
    Simulation queuedRestored;
    Check(queuedRestored.Load(queuedSave, &error), "save preserves queued facility commands");
    Check(queuedRestored.Save() == queuedSave, "queued facility save/load is canonical");
    construction.Step();
    queuedRestored.Step();
    auto constructionSnapshot = construction.GetSnapshot();
    Check(constructionSnapshot.facilityPlacementResults.size() == 3,
        "snapshot reports every placement result from the tick boundary");
    Check(constructionSnapshot.facilityPlacementResults[0].accepted &&
        constructionSnapshot.facilityPlacementResults[0].facilityId == Simulation::FirstFacilityId,
        "valid farm placement is accepted with canonical stable ID");
    Check(!constructionSnapshot.facilityPlacementResults[1].accepted &&
        constructionSnapshot.facilityPlacementResults[1].facilityId == 0,
        "later command targeting an occupied cell is rejected");
    Check(!constructionSnapshot.facilityPlacementResults[2].accepted,
        "unknown facility type is rejected at the tick boundary");
    Check(constructionSnapshot.facilities.size() == 1 && constructionSnapshot.facilities[0].type == FacilityType::Farm,
        "snapshot exposes the accepted typed facility");
    Check(construction.Save() == queuedRestored.Save(), "queued placement continuation is deterministic after reload");
    Check(construction.PlacePrototypeStorage(2, 3) == 0,
        "prototype storage cannot overlap a constructed facility");

    Simulation acknowledged;
    const auto firstAckCommand = acknowledged.QueueFacilityPlacement(FacilityType::Farm, 1, 1);
    acknowledged.Step();
    const auto secondAckCommand = acknowledged.QueueFacilityPlacement(FacilityType::Mill, 2, 1);
    acknowledged.Step();
    auto acknowledgedSnapshot = acknowledged.GetSnapshot();
    Check(acknowledgedSnapshot.facilityPlacementResults.size() == 2 &&
        acknowledgedSnapshot.facilityPlacementResults[0].commandId == firstAckCommand &&
        acknowledgedSnapshot.facilityPlacementResults[1].commandId == secondAckCommand,
        "unacknowledged results remain ordered and visible across later ticks");
    const auto acknowledgmentsSave = acknowledged.Save();
    Simulation acknowledgmentsRestored;
    Check(acknowledgmentsRestored.Load(acknowledgmentsSave, &error),
        "save/load preserves placement results before caller retrieval");
    Check(acknowledgmentsRestored.GetSnapshot().facilityPlacementResults.size() == 2 &&
        acknowledgmentsRestored.Save() == acknowledgmentsSave,
        "restored placement acknowledgments retain their canonical order");
    Check(acknowledgmentsRestored.AcknowledgeFacilityPlacementResult(firstAckCommand),
        "caller can acknowledge a retrieved placement result");
    Check(!acknowledgmentsRestored.AcknowledgeFacilityPlacementResult(firstAckCommand),
        "acknowledging the same result twice reports it is absent");
    Simulation acknowledgedAgain;
    Check(acknowledgedAgain.Load(acknowledgmentsRestored.Save(), &error) &&
        acknowledgedAgain.GetSnapshot().facilityPlacementResults.size() == 1 &&
        acknowledgedAgain.GetSnapshot().facilityPlacementResults[0].commandId == secondAckCommand,
        "acknowledged result stays removed across save/load");

    Simulation manyAcknowledgments;
    std::size_t completedAcknowledgments = 0;
    for (std::int32_t i = 0; i < 20; ++i) {
        const auto commandId = manyAcknowledgments.QueueFacilityPlacement(
            FacilityType::Farm, Simulation::PrototypeGridMax + 1, i);
        manyAcknowledgments.Step();
        const auto resultSnapshot = manyAcknowledgments.GetSnapshot().facilityPlacementResults;
        const auto result = std::find_if(resultSnapshot.begin(), resultSnapshot.end(),
            [commandId](const auto& item) { return item.commandId == commandId; });
        Check(commandId != 0 && result != resultSnapshot.end() && !result->accepted,
            "placement acknowledgment stream remains usable beyond sixteen total outcomes");
        if (commandId != 0 && result != resultSnapshot.end() &&
            manyAcknowledgments.AcknowledgeFacilityPlacementResult(commandId)) ++completedAcknowledgments;
    }
    Check(completedAcknowledgments == 20 && manyAcknowledgments.GetSnapshot().facilityPlacementResults.empty(),
        "caller can process and acknowledge more than sixteen placement outcomes over time");
    Simulation invalidCellCommand;
    Check(invalidCellCommand.QueueFacilityPlacement(FacilityType::Farm, Simulation::PrototypeGridMax + 1, 0) != 0,
        "out-of-bounds request enters the command stream for boundary validation");
    const auto invalidCellSave = invalidCellCommand.Save();
    Simulation invalidCellRestored;
    Check(invalidCellRestored.Load(invalidCellSave, &error), "save preserves queued invalid-cell request");
    invalidCellRestored.Step();
    Check(!invalidCellRestored.GetSnapshot().facilityPlacementResults[0].accepted,
        "out-of-bounds request is rejected at the tick boundary");

    Simulation accelerated;
    Check(accelerated.QueueFacilityPlacement(FacilityType::Farm, -3, 0) != 0 &&
        accelerated.QueueFacilityPlacement(FacilityType::Mill, 3, 0) != 0,
        "synthetic farm and mill requests queue");
    accelerated.Step();
    Check(accelerated.GetSnapshot().facilities.size() == 2,
        "both non-overlapping facility types are committed at the boundary");
    accelerated.Step(5);
    Check(accelerated.GetSnapshot().mill.grain == 0 &&
        accelerated.GetSnapshot().truckCargo == Resource::Flour && accelerated.GetSnapshot().truckCargoAmount == 2,
        "placed mill rate processes the delivered grain at its synthetic throughput");
    accelerated.Step(4);
    Check(accelerated.GetSnapshot().deliveredFlourTotal > 0,
        "the truck consumes facility-produced flour in the active delivery chain");
    const auto activeFacilitiesSave = accelerated.Save();
    Simulation facilitiesRestored;
    Check(facilitiesRestored.Load(activeFacilitiesSave, &error), "save with typed facilities loads");
    Check(facilitiesRestored.Save() == activeFacilitiesSave, "typed facility save/load is canonical");
    accelerated.Step(140);
    facilitiesRestored.Step(140);
    Check(facilitiesRestored.Save() == accelerated.Save(), "typed facility simulation resumes deterministically after reload");

    Simulation fullGrid;
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(Simulation::MaxFacilities); ++i) {
        Check(fullGrid.QueueFacilityPlacement(FacilityType::Farm, i - 8, -8) != 0,
            "bounded facility requests fit in command queue");
    }
    Check(fullGrid.QueueFacilityPlacement(FacilityType::Farm, 9, -8) == 0,
        "facility command queue rejects overflow before enqueue");
    fullGrid.Step();
    const auto fullGridResults = fullGrid.GetSnapshot().facilityPlacementResults;
    Check(std::count_if(fullGridResults.begin(), fullGridResults.end(), [](const auto& r) { return r.accepted; }) == 16,
        "facility collection accepts its documented capacity");
    Check(fullGrid.QueueFacilityPlacement(FacilityType::Mill, 9, -8) == 0,
        "unacknowledged result capacity bounds further construction commands");
    Check(fullGrid.AcknowledgeFacilityPlacementResult(fullGridResults[0].commandId),
        "acknowledging a result frees command/result capacity");
    Check(fullGrid.QueueFacilityPlacement(FacilityType::Mill, 9, -8) != 0,
        "queue can accept commands after prior tick is applied and space is available");
    fullGrid.Step();
    const auto fullGridFinalResults = fullGrid.GetSnapshot().facilityPlacementResults;
    Check(fullGridFinalResults.size() == 16,
        "result history retains the placement outcome from each tick");
    Check(std::any_of(fullGridFinalResults.begin(), fullGridFinalResults.end(), [](const auto& r) { return !r.accepted; }),
        "facility collection rejects placement beyond its capacity");

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
    Check(v1Target.Save().find("OPENREPUBLIC_SIM 6\n") == 0, "migrated state writes current save version");
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
    Check(v2Target.Save().find("OPENREPUBLIC_SIM 6\n") == 0, "v2 migrated state writes current save version");

    const auto v2ConstructionSave = AsVersionTwoSave(placedSave);
    Simulation v2ConstructionTarget;
    Check(v2ConstructionTarget.Load(v2ConstructionSave, &error), "version 2 construction save remains loadable");
    const auto v2ConstructionSnapshot = v2ConstructionTarget.GetSnapshot();
    Check(v2ConstructionSnapshot.prototypeStorages.size() == Simulation::MaxPrototypeStorages,
        "version 2 migration preserves all saved construction");
    Check(v2ConstructionSnapshot.rules.farmGrainPerHarvest == 2 &&
        v2ConstructionSnapshot.rules.farmHarvestIntervalTicks == 2 &&
        v2ConstructionSnapshot.rules.millGrainPerTick == 1, "version 2 construction migration supplies default rules");
    const auto v3Save = AsVersionThreeSave(placedSave);
    Check(!v3Save.empty(), "test fixture represents the version 3 schema");
    Simulation v3Target;
    Check(v3Target.Load(v3Save, &error), "version 3 save loads through migration path");
    Check(v3Target.GetSnapshot().facilities.empty() && v3Target.GetSnapshot().prototypeStorages.size() == Simulation::MaxPrototypeStorages,
        "version 3 migration supplies empty typed facility state while preserving storage");
    Check(v3Target.Save().find("OPENREPUBLIC_SIM 6\n") == 0, "v3 migrated state writes current save version");
    const auto v4Save = AsVersionFourSave(activeFacilitiesSave);
    Check(!v4Save.empty(), "test fixture represents the version 4 schema");
    Simulation v4Target;
    Check(v4Target.Load(v4Save, &error), "version 4 save loads through migration path");
    Check(v4Target.GetSnapshot().facilityPlacementResults.empty(), "version 4 migration starts with no placement acknowledgments");
    Check(v4Target.Save().find("OPENREPUBLIC_SIM 6\n") == 0, "v4 migrated state writes current save version");
    Simulation v5Source;
    Check(v5Source.SetRoadEnabled(202,false), "v5 migration fixture closes a legacy seed edge");
    const auto v5Save=AsVersionFiveSave(v5Source.Save());
    Simulation v5Target;
    Check(!v5Save.empty()&&v5Target.Load(v5Save,&error), "version 5 saves migrate to the dynamic graph");
    const auto migratedV5Roads=v5Target.GetSnapshot().roads;
    const auto migratedClosed=std::find_if(migratedV5Roads.begin(),migratedV5Roads.end(),[](const auto& road){return road.id==202;});
    Check(migratedClosed!=migratedV5Roads.end()&&!migratedClosed->enabled, "v5 migration preserves seed edge state and node coordinates");
    Check(v5Target.Save().find("OPENREPUBLIC_SIM 6\n")==0, "v5 migrated state writes current save version");

    // Reject serialized enum values before converting them to the enum's
    // uint8_t representation; 257 must not wrap into FacilityType::Farm.
    Simulation malformedEnumTarget;
    Check(malformedEnumTarget.Load(activeFacilitiesSave, &error), "enum validation target starts with valid facilities");
    const auto malformedEnumBefore = malformedEnumTarget.Save();
    auto badFacilityType = AsVersionFiveSave(activeFacilitiesSave);
    const auto facilityTypeRecord = badFacilityType.find("FACILITIES 2 4001 ");
    Check(facilityTypeRecord != std::string::npos, "facility enum fixture is found");
    if (facilityTypeRecord != std::string::npos) {
        const auto rawType = facilityTypeRecord + std::string("FACILITIES 2 4001 ").size();
        badFacilityType.replace(rawType, 1, "257");
    }
    Check(!malformedEnumTarget.Load(badFacilityType, &error), "serialized facility type 257 is rejected before narrowing");
    Check(malformedEnumTarget.Save() == malformedEnumBefore, "invalid facility enum load is transactional");
    auto badQueuedType = AsVersionFiveSave(queuedSave);
    const auto queueRecord = badQueuedType.find("PLACEMENT_QUEUE ");
    Check(queueRecord != std::string::npos, "queued enum fixture is found");
    if (queueRecord != std::string::npos) {
        const auto firstEntry = badQueuedType.find("1 1 2 3", queueRecord);
        Check(firstEntry != std::string::npos, "queued placement enum entry is found");
        if (firstEntry != std::string::npos) badQueuedType.replace(firstEntry + 2, 1, "257");
    }
    Check(!malformedEnumTarget.Load(badQueuedType, &error), "serialized queued type 257 is rejected before narrowing");
    Check(malformedEnumTarget.Save() == malformedEnumBefore, "invalid queued enum load is transactional");

    Simulation mixedOutstandingIds;
    const auto completedBeforeQueue = mixedOutstandingIds.QueueFacilityPlacement(FacilityType::Farm, 1, 2);
    mixedOutstandingIds.Step();
    const auto retainedResultId = mixedOutstandingIds.QueueFacilityPlacement(FacilityType::Mill, 2, 2);
    mixedOutstandingIds.Step();
    Check(mixedOutstandingIds.AcknowledgeFacilityPlacementResult(completedBeforeQueue),
        "mixed-ID fixture acknowledges its earlier result");
    const auto queuedAfterResultId = mixedOutstandingIds.QueueFacilityPlacement(FacilityType::Farm, 3, 2);
    const auto mixedOutstandingSave = AsVersionFiveSave(mixedOutstandingIds.Save());
    Simulation mixedIdLoadTarget;
    mixedIdLoadTarget.Step(9);
    const auto mixedIdTargetBefore = mixedIdLoadTarget.Save();
    auto overlappingOutstandingIds = mixedOutstandingSave;
    const auto outstandingQueueTag = overlappingOutstandingIds.find("PLACEMENT_QUEUE ");
    Check(outstandingQueueTag != std::string::npos, "mixed outstanding queue record is found");
    if (outstandingQueueTag != std::string::npos) {
        const auto queueEntry = overlappingOutstandingIds.find("PLACEMENT_QUEUE 4 1 ", outstandingQueueTag);
        Check(queueEntry != std::string::npos, "mixed outstanding queue entry is found");
        if (queueEntry != std::string::npos) {
            const auto idStart = queueEntry + std::string("PLACEMENT_QUEUE 4 1 ").size();
            overlappingOutstandingIds.replace(idStart, std::to_string(queuedAfterResultId).size(),
                std::to_string(retainedResultId));
        }
    }
    Check(!mixedOutstandingSave.empty() &&
        !mixedIdLoadTarget.Load(overlappingOutstandingIds, &error),
        "overlap between result and queue IDs is rejected");
    Check(mixedIdLoadTarget.Save() == mixedIdTargetBefore,
        "overlapping outstanding IDs fail transactionally");

    auto invertedOutstandingIds = mixedOutstandingSave;
    const auto inversionTag = invertedOutstandingIds.find("PLACEMENT_QUEUE ");
    if (inversionTag != std::string::npos) {
        const auto queueEntry = invertedOutstandingIds.find("PLACEMENT_QUEUE 4 1 ", inversionTag);
        if (queueEntry != std::string::npos) {
            const auto idStart = queueEntry + std::string("PLACEMENT_QUEUE 4 1 ").size();
            invertedOutstandingIds.replace(idStart, std::to_string(queuedAfterResultId).size(),
                std::to_string(completedBeforeQueue));
        }
    }
    Check(!mixedIdLoadTarget.Load(invertedOutstandingIds, &error),
        "out-of-order result-to-queue IDs are rejected");
    Check(mixedIdLoadTarget.Save() == mixedIdTargetBefore,
        "out-of-order outstanding IDs fail transactionally");

    Simulation queuedSuffix;
    queuedSuffix.QueueFacilityPlacement(FacilityType::Farm, 0, 6);
    queuedSuffix.QueueFacilityPlacement(FacilityType::Mill, 1, 6);
    queuedSuffix.QueueFacilityPlacement(FacilityType::Farm, 2, 6);
    const auto queuedSuffixSave = AsVersionFiveSave(queuedSuffix.Save()); // Queue IDs 1,2,3; next ID 4.
    Simulation queuedSuffixLoadTarget;
    queuedSuffixLoadTarget.Step(13);
    const auto queuedSuffixTargetBefore = queuedSuffixLoadTarget.Save();
    auto queueWithGap = queuedSuffixSave;
    const auto gapTag = queueWithGap.find("PLACEMENT_QUEUE 4 3 ");
    Check(gapTag != std::string::npos, "queued suffix fixture is found");
    if (gapTag != std::string::npos) {
        queueWithGap.replace(gapTag, std::string("PLACEMENT_QUEUE 4 3 ").size(), "PLACEMENT_QUEUE 4 2 ");
        const auto secondEntry = queueWithGap.find("2 2 1 6 ", gapTag);
        Check(secondEntry != std::string::npos, "second queued suffix entry is found");
        if (secondEntry != std::string::npos) queueWithGap.erase(secondEntry, std::string("2 2 1 6 ").size());
    }
    Check(!queuedSuffixLoadTarget.Load(queueWithGap, &error),
        "gap within persisted queued command suffix is rejected");
    Check(queuedSuffixLoadTarget.Save() == queuedSuffixTargetBefore,
        "queued command gap rejection is transactional");

    Simulation queuedNotLatest;
    queuedNotLatest.QueueFacilityPlacement(FacilityType::Farm, 0, 6);
    queuedNotLatest.QueueFacilityPlacement(FacilityType::Mill, 1, 6);
    auto queueNotLatest = AsVersionFiveSave(queuedNotLatest.Save()); // Queue IDs 1,2; next ID 3.
    const auto latestTag = queueNotLatest.find("PLACEMENT_QUEUE 3 2 ");
    if (latestTag != std::string::npos) queueNotLatest.replace(latestTag, std::string("PLACEMENT_QUEUE 3 2 ").size(),
        "PLACEMENT_QUEUE 4 2 ");
    Check(!queuedSuffixLoadTarget.Load(queueNotLatest, &error),
        "queue whose final ID does not precede next ID is rejected");
    Check(queuedSuffixLoadTarget.Save() == queuedSuffixTargetBefore,
        "non-latest queued suffix rejection is transactional");

    Simulation acceptedResultIds;
    acceptedResultIds.QueueFacilityPlacement(FacilityType::Farm, -2, 4);
    acceptedResultIds.Step();
    acceptedResultIds.QueueFacilityPlacement(FacilityType::Mill, 2, 4);
    acceptedResultIds.Step();
    const auto acceptedResultSave = AsVersionFiveSave(acceptedResultIds.Save());
    Simulation facilityResultLoadTarget;
    facilityResultLoadTarget.Step(11);
    const auto facilityResultTargetBefore = facilityResultLoadTarget.Save();
    auto duplicateFacilityResult = acceptedResultSave;
    const auto resultRecord = duplicateFacilityResult.find("PLACEMENT_RESULTS 2 ");
    Check(resultRecord != std::string::npos, "two accepted-result fixture is found");
    if (resultRecord != std::string::npos) {
        const auto secondResult = duplicateFacilityResult.find("2 1 4002", resultRecord);
        Check(secondResult != std::string::npos, "second accepted-result record is found");
        if (secondResult != std::string::npos)
            duplicateFacilityResult.replace(secondResult + std::string("2 1 ").size(), 4, "4001");
    }
    Check(!facilityResultLoadTarget.Load(duplicateFacilityResult, &error),
        "duplicate accepted facility references are rejected");
    Check(facilityResultLoadTarget.Save() == facilityResultTargetBefore,
        "duplicate accepted facility references fail transactionally");

    auto reversedFacilityResults = acceptedResultSave;
    const auto reversedRecord = reversedFacilityResults.find("PLACEMENT_RESULTS 2 ");
    if (reversedRecord != std::string::npos) {
        const auto firstFacility = reversedFacilityResults.find("1 1 4001", reversedRecord);
        const auto secondFacility = reversedFacilityResults.find("2 1 4002", reversedRecord);
        Check(firstFacility != std::string::npos && secondFacility != std::string::npos,
            "accepted facility result IDs are located for reversal fixture");
        if (firstFacility != std::string::npos && secondFacility != std::string::npos) {
            reversedFacilityResults.replace(secondFacility + std::string("2 1 ").size(), 4, "XXXX");
            reversedFacilityResults.replace(firstFacility + std::string("1 1 ").size(), 4, "4002");
            const auto placeholder = reversedFacilityResults.find("XXXX", secondFacility);
            if (placeholder != std::string::npos) reversedFacilityResults.replace(placeholder, 4, "4001");
        }
    }
    Check(!facilityResultLoadTarget.Load(reversedFacilityResults, &error),
        "reversed accepted facility-result mapping is rejected");
    Check(facilityResultLoadTarget.Save() == facilityResultTargetBefore,
        "reversed accepted facility-result mapping fails transactionally");

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

    // Player-built road segments use the same command IDs and tick boundary as
    // facility placement. A detour around closed edge 202 carries deliveries.
    using openrepublic::sim::ConstructionCommandType;
    Simulation newEndpointFirst;
    const auto newEndpointCommand=newEndpointFirst.QueueRoadSegment(0,1,0,0); // A is new; B is seed node 3.
    newEndpointFirst.Step();
    const auto newEndpointSnapshot=newEndpointFirst.GetSnapshot();
    Check(newEndpointCommand==1&&newEndpointSnapshot.roadNodes.size()==6&&newEndpointSnapshot.roads.size()==5,
        "new A endpoint followed by existing B remains valid when node storage grows");
    Check(newEndpointSnapshot.constructionResults.size()==1&&newEndpointSnapshot.constructionResults[0].accepted&&
        newEndpointSnapshot.roads.back().aGridX==0&&newEndpointSnapshot.roads.back().aGridZ==1&&
        newEndpointSnapshot.roads.back().bGridX==0&&newEndpointSnapshot.roads.back().bGridZ==0,
        "road segment preserves both endpoint coordinates after appending its new first endpoint");
    Simulation detour;
    const auto detourOne=detour.QueueRoadSegment(-1,0,-1,-1);
    const auto detourTwo=detour.QueueRoadSegment(-1,-1,0,-1);
    const auto detourThree=detour.QueueRoadSegment(0,-1,0,0);
    Check(detourOne==1&&detourTwo==2&&detourThree==3,"road commands share the monotonically increasing construction ID namespace");
    const auto beforeRoadTick=detour.GetSnapshot();
    Check(beforeRoadTick.roads.size()==4&&beforeRoadTick.roadNodes.size()==5,"queued road construction is invisible before Step");
    Check(detour.SetRoadEnabled(202,false),"edge 202 can be closed before route dispatch");
    detour.Step();
    auto detourSnapshot=detour.GetSnapshot();
    Check(detourSnapshot.roads.size()==7&&detourSnapshot.roadNodes.size()==7,"three valid segments apply together at fixed tick boundary");
    Check(detourSnapshot.constructionResults.size()==3&&detourSnapshot.constructionResults[0].type==ConstructionCommandType::RoadSegment&&
        detourSnapshot.constructionResults[0].accepted,"typed road command result is exposed to callers");
    const auto invalidBeforeNodes=detourSnapshot.roadNodes.size();
    const auto invalidBeforeEdges=detourSnapshot.roads.size();
    const auto invalidDiagonal=detour.QueueRoadSegment(5,5,6,6);
    const auto duplicateSegment=detour.QueueRoadSegment(-1,0,-1,-1);
    const auto outOfBounds=detour.QueueRoadSegment(16,0,17,0);
    Check(invalidDiagonal==4&&duplicateSegment==5&&outOfBounds==6,"invalid road requests are accepted into bounded validation queue");
    const auto queuedRoadSave=detour.Save();
    Simulation queuedRoadRestore;
    Check(queuedRoadRestore.Load(queuedRoadSave,&error),"save/load preserves queued road requests");
    detour.Step(); queuedRoadRestore.Step();
    detourSnapshot=detour.GetSnapshot();
    Check(detourSnapshot.roadNodes.size()==invalidBeforeNodes&&detourSnapshot.roads.size()==invalidBeforeEdges,
        "diagonal, duplicate and out-of-bounds segments reject without partial graph mutation");
    Check(detourSnapshot.constructionResults.size()==6&&
        !detourSnapshot.constructionResults[3].accepted&&!detourSnapshot.constructionResults[4].accepted&&!detourSnapshot.constructionResults[5].accepted,
        "invalid road requests return explicit rejected results");
    Check(detour.Save()==queuedRoadRestore.Save(),"save/load with road results and graph state is canonical");
    Check(detour.AcknowledgeConstructionResult(detourOne)&&!detour.AcknowledgeConstructionResult(detourOne),
        "generic construction result acknowledgment removes a result once");
    detour.Step(3); // Grain dispatch begins and the truck traverses the detour.
    detourSnapshot=detour.GetSnapshot();
    Check(detourSnapshot.truckStage==TruckStage::DeliverGrainToMill&&detourSnapshot.activeRoute.size()==5&&
        std::find(detourSnapshot.activeRoute.begin(),detourSnapshot.activeRoute.end(),detourSnapshot.roadNodes[5].id)!=detourSnapshot.activeRoute.end(),
        "deterministic BFS selects the player-built detour around closed edge 202");
    const auto midDetour=detour.Save();
    Simulation midDetourRestored;
    Check(midDetourRestored.Load(midDetour,&error),"mid-detour save restores route and dynamic graph");
    detour.Step(220); midDetourRestored.Step(220);
    Check(detour.Save()==midDetourRestored.Save(),"mid-detour delivery continues deterministically after save/load");
    const auto delivered=detour.GetSnapshot();
    Check(delivered.deliveredFlourTotal>0&&delivered.warehouse.flour>0,"truck completes delivery over the player-built detour");
    Check(delivered.deliveredFlourTotal==delivered.warehouse.flour,
        "cumulative flour delivery reconciles exactly with warehouse stock because the prototype has no consumption");

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
    badVersion.replace(badVersion.find("OPENREPUBLIC_SIM 6"), std::string("OPENREPUBLIC_SIM 6").size(), "OPENREPUBLIC_SIM 9");
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

    Simulation terminalTick;
    auto terminalSave = terminalTick.Save();
    const auto tickRecord = terminalSave.find("TICK 0");
    Check(tickRecord != std::string::npos, "terminal tick fixture is found");
    if (tickRecord != std::string::npos)
        terminalSave.replace(tickRecord, std::string("TICK 0").size(), "TICK 18446744073709551615");
    Check(terminalTick.Load(terminalSave, &error), "maximum-tick save loads");
    const auto terminalBeforeQueue = terminalTick.Save();
    Check(terminalTick.QueueFacilityPlacement(FacilityType::Farm, 0, 0) == 0,
        "facility request is rejected at the terminal tick");
    terminalTick.Step();
    Check(terminalTick.Save() == terminalBeforeQueue, "terminal tick does not advance or mutate state");

    // The save parser must not accept delivery states the simulation cannot
    // produce, even when every listed road segment is individually valid.
    const auto activeRouteBytes = passedRoadBytes;
    const auto wrongOrigin = WithTruckRecord(activeRouteBytes, "TRUCK 2 1 1 2 0 2 2 3");
    Check(!restored.Load(wrongOrigin, &error), "grain delivery route with a non-farm origin is rejected");
    Check(restored.Save() == beforeBadLoad, "wrong-origin route rejection leaves state unchanged");
    const auto loopingRoute = WithTruckRecord(activeRouteBytes, "TRUCK 2 1 1 2 1 5 1 2 1 2 3");
    Check(!restored.Load(loopingRoute, &error), "repeated-node delivery route is rejected");
    Check(restored.Save() == beforeBadLoad, "looping route rejection leaves state unchanged");

    // Dynamic graph records are validated transactionally, including geometry
    // and duplicate endpoint/edge invariants.
    Simulation graphLoadTarget;
    graphLoadTarget.Step(9);
    const auto graphTargetBefore=graphLoadTarget.Save();
    auto nonAdjacentGraph=midDetour;
    const auto insertedNode=nonAdjacentGraph.find(" 6 -1 -1");
    Check(insertedNode!=std::string::npos,"dynamic road node fixture is located");
    if(insertedNode!=std::string::npos) nonAdjacentGraph.replace(insertedNode,std::string(" 6 -1 -1").size()," 6 5 5");
    Check(!graphLoadTarget.Load(nonAdjacentGraph,&error),"non-adjacent saved road segment is rejected");
    Check(graphLoadTarget.Save()==graphTargetBefore,"invalid graph geometry load is transactional");
    auto duplicateNode=midDetour;
    const auto duplicateNodeRecord=duplicateNode.find(" 6 -1 -1");
    if(duplicateNodeRecord!=std::string::npos) duplicateNode.replace(duplicateNodeRecord,std::string(" 6 -1 -1").size()," 6 -1 0");
    Check(!graphLoadTarget.Load(duplicateNode,&error),"duplicate saved road-node coordinates are rejected");
    Check(graphLoadTarget.Save()==graphTargetBefore,"duplicate-node rejection is transactional");
    auto duplicateEdge=midDetour;
    const auto dynamicEdgeId=duplicateEdge.find(" 205 2 6 1");
    Check(dynamicEdgeId!=std::string::npos,"dynamic edge fixture is located");
    if(dynamicEdgeId!=std::string::npos) duplicateEdge.replace(dynamicEdgeId,std::string(" 205 2 6 1").size()," 201 2 6 1");
    Check(!graphLoadTarget.Load(duplicateEdge,&error),"noncanonical or duplicate saved road-edge IDs are rejected");
    Check(graphLoadTarget.Save()==graphTargetBefore,"duplicate-edge rejection is transactional");
    Simulation seedRemapSource;
    seedRemapSource.QueueRoadSegment(-2,0,-2,1);
    seedRemapSource.Step();
    auto remappedSeedEdge=seedRemapSource.Save();
    const auto seedEdgeRecord=remappedSeedEdge.find(" 201 1 2 1");
    Check(seedEdgeRecord!=std::string::npos,"canonical seed edge record is found for remap fixture");
    if(seedEdgeRecord!=std::string::npos) remappedSeedEdge.replace(seedEdgeRecord,std::string(" 201 1 2 1").size()," 201 1 6 1");
    Check(!graphLoadTarget.Load(remappedSeedEdge,&error),"orthogonal seed-edge remap is rejected even when graph geometry is valid");
    Check(graphLoadTarget.Save()==graphTargetBefore,"seed-edge remap rejection is transactional");

    auto overflowCommandEnum=queuedSave;
    const auto commandQueueTag=overflowCommandEnum.find("CONSTRUCTION_QUEUE ");
    const auto commandFields=overflowCommandEnum.find(" 1 1 1 ",commandQueueTag);
    Check(commandQueueTag!=std::string::npos&&commandFields!=std::string::npos,"v6 queued-command enum fixture is found");
    if(commandFields!=std::string::npos) overflowCommandEnum.replace(commandFields+3,1,"257");
    Check(!graphLoadTarget.Load(overflowCommandEnum,&error),"v6 command type 257 is rejected before enum narrowing");
    Check(graphLoadTarget.Save()==graphTargetBefore,"command enum overflow rejection is transactional");
    auto overflowResultEnum=newEndpointFirst.Save();
    const auto resultTag=overflowResultEnum.find("CONSTRUCTION_RESULTS ");
    const auto resultFields=overflowResultEnum.find(" 1 2 1 ",resultTag);
    Check(resultTag!=std::string::npos&&resultFields!=std::string::npos,"v6 construction-result enum fixture is found");
    if(resultFields!=std::string::npos) overflowResultEnum.replace(resultFields+3,1,"257");
    Check(!graphLoadTarget.Load(overflowResultEnum,&error),"v6 result type 257 is rejected before enum narrowing");
    Check(graphLoadTarget.Save()==graphTargetBefore,"result enum overflow rejection is transactional");

    if (failures) {
        std::cerr << failures << " simulation checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All OpenRepublic simulation checks passed.\n";
    return EXIT_SUCCESS;
}
