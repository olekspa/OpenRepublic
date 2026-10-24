#include "openrepublic/economy/economy.h"

#include <iostream>
#include <string>

using namespace openrepublic::economy;

int main() {
    int failures=0;
    auto check=[&](bool ok,const char *message){if(!ok){++failures;std::cerr<<"FAIL: "<<message<<'\n';}};
    const std::string text=
        "openrepublic-economy 1\n"
        "resource grain\n"
        "building rail_source 1 1\n station\n store grain 1000\n recipe grow every 1 out grain:1000\n end\n"
        "building rail_sink 1 1\n station\n store grain 1000\n end\n"
        "building road_source 1 1\n store grain 1000\n recipe grow every 1 out grain:1000\n end\n"
        "building road_sink 1 1\n store grain 1000\n end\n"
        "vehicle truck capacity 10 speed 1\n"
        "vehicle train capacity 1000 speed 4 mode rail\n";
    Catalog catalog; ParseError parse;
    check(ParseCatalogText(text,catalog,parse),"rail catalog parses");
    const auto canonical=WriteCatalogText(catalog);
    Catalog roundTrip;
    check(ParseCatalogText(canonical,roundTrip,parse) && WriteCatalogText(roundTrip)==canonical,
          "station and rail-only catalog declarations round trip");
    VehicleTypeId truck{},train{}; BuildingTypeId rs{},rd{},ts{},td{}; ResourceId grain{};
    catalog.FindVehicleType("truck",truck); catalog.FindVehicleType("train",train); catalog.FindResource("grain",grain);
    catalog.FindBuildingType("rail_source",rs); catalog.FindBuildingType("rail_sink",rd);
    catalog.FindBuildingType("road_source",ts); catalog.FindBuildingType("road_sink",td);

    World baseline(MakeSyntheticCatalog(),8,8);
    check(SaveWorld(baseline).find("WORLD 2")!=std::string::npos,"world without rail keeps legacy save version");
    World w(catalog,28,8);
    const auto railSource=w.PlaceBuilding(rs,{2,2}); const auto railSink=w.PlaceBuilding(rd,{22,2});
    const auto roadSource=w.PlaceBuilding(ts,{2,5}); const auto roadSink=w.PlaceBuilding(td,{22,5});
    CommandId firstRail{};
    for(int x=2;x<22;++x) { const auto rail=w.BuildRail({x,1},{x+1,1}); if(!firstRail) firstRail=rail; w.BuildRoad({x,4},{x+1,4}); }
    const auto railOrder=w.CreateHaulOrder(1,2,grain,train,HaulMode::Rail);
    const auto roadOrder=w.CreateHaulOrder(3,4,grain,truck,HaulMode::Road);
    check(railSource&&railSink&&roadSource&&roadSink&&railOrder&&roadOrder,"commands queue");
    check(w.Step(1).accepted,"place networks and orders");
    check(w.GetCommandResult(railOrder).state==CommandState::Accepted,"rail order accepted for two stations");
    check(w.GetCommandResult(firstRail).rail==1,"rail build result exposes the allocated rail ID");
    check(w.GetCommandResult(roadOrder).state==CommandState::Accepted,"road order accepted for road vehicles");
    check(w.Snapshot().rails.size()==20,"rail segments appear in authoritative snapshot");
    std::uint64_t railArrival=0,roadArrival=0;
    for(int i=0;i<10000 && (!railArrival || !roadArrival);++i) {
        check(w.Step(1).accepted,"simulation step accepted");
        const auto snap=w.Snapshot();
        if(!railArrival && snap.buildings[1].inventory[0].amount==1000) railArrival=snap.tick;
        if(!roadArrival && snap.buildings[3].inventory[0].amount==1000) roadArrival=snap.tick;
    }
    check(railArrival>0 && roadArrival>0,"both modes deliver 1000 grain");
    check(railArrival<roadArrival,"train delivers 1000 grain in fewer ticks than truck");
    std::cout << "{\"scenario\":\"rail_vs_road\",\"evidence\":\"OpenRepublic synthetic economy test\",\"grainUnits\":1000,\"railTicks\":"
              << railArrival << ",\"roadTicks\":" << roadArrival << ",\"railFaster\":"
              << (railArrival>0 && railArrival<roadArrival ? "true" : "false") << ",\"stateHash\":"
              << w.Snapshot().stateHash << "}\n";
    const auto saved=SaveWorld(w);
    check(saved.find("WORLD 8")!=std::string::npos,"rail state opts into save version 8");
    World restored(catalog,1,1);
    const auto restoredOk=LoadWorld(saved,catalog,restored,parse);
    check(restoredOk,"rail world restores transactionally");
    if(!restoredOk) std::cerr << "load error: " << parse.message << '\n';
    check(restored.Snapshot().stateHash==w.Snapshot().stateHash,"rail save preserves state hash");
    check(restored.Snapshot().rails.size()==w.Snapshot().rails.size(),"rail save restores network geometry");

    World invalid(catalog,8,8);
    invalid.PlaceBuilding(ts,{2,2}); invalid.PlaceBuilding(td,{5,2}); invalid.Step(1);
    const auto bad=invalid.CreateHaulOrder(1,2,grain,train,HaulMode::Road); invalid.Step(1);
    check(invalid.GetCommandResult(bad).state==CommandState::Rejected,"rail-only train cannot use road mode");
    const auto badRail=invalid.CreateHaulOrder(1,2,grain,truck,HaulMode::Rail); invalid.Step(1);
    check(invalid.GetCommandResult(badRail).state==CommandState::Rejected,"rail mode requires stations and train");
    const auto badMode=invalid.CreateHaulOrder(1,2,grain,truck,static_cast<HaulMode>(9)); invalid.Step(1);
    check(invalid.GetCommandResult(badMode).state==CommandState::Rejected,"unknown haul modes are rejected");
    return failures?1:0;
}
