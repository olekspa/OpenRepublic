#include "openrepublic/economy/economy.h"
#include <iostream>
#include <string>
using namespace openrepublic::economy;
namespace { int failures=0; void Check(bool ok,const char *message){if(!ok){std::cerr<<"FAIL: "<<message<<'\n';++failures;}} }
int main(){
 const std::string text=R"(openrepublic-economy 1
resource coal
resource heat
building depot 1 1
  depot
  fleet truck 2
  handling 2 1
end
building border 1 1
  store coal 50
  recipe mine every 1 out coal:2
end
building storage 1 1
  store coal 50
end
building heating 1 1
  store coal 50
  store heat 50
  recipe heat every 1 in coal:1 out heat:1
end
vehicle truck capacity 4 speed 1
)";
 Catalog catalog; ParseError error; Check(ParseCatalogText(text,catalog,error),"parse depot catalog");
 if(failures)return 1;
 World emptyDepotCatalog(catalog,20,8); auto legacyEmpty=MakeSyntheticCatalog(); World emptyLegacyCatalog(legacyEmpty,20,8);
 Check(SaveWorld(emptyDepotCatalog).rfind("OPENREPUBLIC_ECONOMY_WORLD 2\n",0)==0&&emptyDepotCatalog.Snapshot().stateHash==emptyLegacyCatalog.Snapshot().stateHash,"empty depot catalog preserves legacy save version and hash until a depot exists");
 World w(catalog,20,8);
 for(int x=1;x<14;++x)w.BuildRoad({x,2},{x+1,2});
 w.PlaceBuilding(0,{1,3});w.PlaceBuilding(1,{5,3});w.PlaceBuilding(2,{9,3});w.PlaceBuilding(3,{13,3});
 Check(w.Step().accepted,"place roads and buildings");
 auto s=w.Snapshot(); Check(s.vehicles.size()==2,"fleet spawns two vehicles");
 Check(s.vehicles[0].id==1&&s.vehicles[0].ownerDepot==1&&s.vehicles[0].leg==VehicleLeg::None,"stable parked truck snapshot");
 const auto parkedSave=SaveWorld(w); Check(parkedSave.rfind("OPENREPUBLIC_ECONOMY_WORLD 9\n",0)==0,"depot save uses opt-in v9 alongside rail v8");
 World parkedReload(catalog,1,1); const auto parkedLoaded=LoadWorld(parkedSave,catalog,parkedReload,error); if(!parkedLoaded)std::cerr<<"parked load error: "<<error.message<<'\n'; Check(parkedLoaded,"parked depot fleet reloads");
 Check(parkedReload.Snapshot().stateHash==s.stateHash,"parked fleet state hash survives reload");
 w.DispatchTruck(1,2,3,0,2); w.DispatchTruck(2,3,4,0,2); Check(w.Step().accepted,"queue dispatches");
 s=w.Snapshot(); Check(s.vehicles[0].leg==VehicleLeg::ToSource,"truck records source leg");
 const auto activeSave=SaveWorld(w); World activeReload(catalog,1,1); const auto activeLoaded=LoadWorld(activeSave,catalog,activeReload,error); if(!activeLoaded)std::cerr<<"active load error: "<<error.message<<'\n'; Check(activeLoaded,"active depot order reloads");
 Check(activeReload.Snapshot().stateHash==w.Snapshot().stateHash,"active depot state hash survives reload");
 const auto busy=w.DispatchTruck(1,2,3,0); w.Step(); Check(w.GetCommandResult(busy).failure==CommandFailure::DuplicateOrder,"busy truck dispatch is rejected at tick boundary");
 bool produced=false, seenLoad=false, seenDestination=false, seenUnload=false, seenReturn=false;
 for(int i=0;i<100;++i){w.Step();s=w.Snapshot(); for(const auto &v:s.vehicles){seenLoad|=v.leg==VehicleLeg::Loading;seenDestination|=v.leg==VehicleLeg::ToDestination;seenUnload|=v.leg==VehicleLeg::Unloading;seenReturn|=v.leg==VehicleLeg::Returning;} for(const auto &b:s.buildings)if(b.id==4&&!b.inventory.empty()&&b.inventory[1].amount)produced=true;}
 Check(produced,"storage truck delivers coal and heating recipe produces heat");
 Check(seenLoad&&seenDestination&&seenUnload&&seenReturn,"snapshot exposes every handling and return leg");
 s=w.Snapshot(); Check(s.vehicles[0].ownerDepot==1,"vehicle ownership persists in snapshots");
 std::string canonical=WriteCatalogText(catalog); Catalog round; Check(ParseCatalogText(canonical,round,error),"canonical depot catalog parses");
 Check(WriteCatalogText(round)==canonical,"depot catalog canonical round trip");
 return failures?1:0;
}
