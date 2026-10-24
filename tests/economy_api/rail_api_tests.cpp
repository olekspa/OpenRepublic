#include "openrepublic/economy_api/economy_api.h"

#include <cstdlib>
#include <iostream>

using namespace openrepublic;

namespace {
void Check(bool value,const char *label){if(!value){std::cerr<<"FAIL: "<<label<<'\n';std::exit(1);}}
economy::Catalog MakeCatalog(){
    const char *text="openrepublic-economy 1\nresource grain\n"
        "building origin 1 1\n station\n store grain 1000\n end\n"
        "building destination 1 1\n station\n store grain 1000\n end\n"
        "vehicle truck capacity 10 speed 1\nvehicle train capacity 1000 speed 4 mode rail\n";
    economy::Catalog catalog; economy::ParseError error;
    Check(economy::ParseCatalogText(text,catalog,error),"parse API rail catalog"); return catalog;
}
}

int main(){
    auto catalog=MakeCatalog(); economy::World world(catalog,8,8);
    economy_api::EconomyApi api(world,{true,true});
    const auto definitions=api.DescribeCatalog();
    Check(definitions.buildingTypes[0].station,"catalog API exposes station flag");
    Check(definitions.vehicleTypes[1].railOnly,"catalog API exposes rail-only train flag");
    auto a=api.PlaceBuilding(world.Revision(),"origin",{1,2});
    auto b=api.PlaceBuilding(world.Revision(),"destination",{5,2});
    auto r1=api.BuildRail(world.Revision(),{1,1},{2,1});
    auto r2=api.BuildRail(world.Revision(),{2,1},{3,1});
    auto r3=api.BuildRail(world.Revision(),{3,1},{4,1});
    auto r4=api.BuildRail(world.Revision(),{4,1},{5,1});
    Check(a.error==economy_api::ErrorCode::None && b.error==economy_api::ErrorCode::None &&
          r1.error==economy_api::ErrorCode::None && r2.error==economy_api::ErrorCode::None &&
          r3.error==economy_api::ErrorCode::None && r4.error==economy_api::ErrorCode::None,
          "API accepts rail and station setup commands");
    Check(api.Step()==economy_api::ErrorCode::None,"API applies setup");
    auto snapshot=api.QueryWorld();
    Check(snapshot.error==economy_api::ErrorCode::None && snapshot.world.rails.size()==4,
          "world API exposes rail edges");
    auto page=api.QueryWorld(snapshot.world.revision,"rails",0,8);
    Check(page.error==economy_api::ErrorCode::None && page.page.total==4 && page.page.rails.size()==4,
          "paged API exposes rail edges");
    auto order=api.CreateHaulOrder(snapshot.world.revision,1,2,"grain","train",economy::HaulMode::Rail);
    Check(order.error==economy_api::ErrorCode::None,"API accepts explicit rail haul mode");
    Check(api.Step()==economy_api::ErrorCode::None,"API applies rail haul order");
    snapshot=api.QueryWorld();
    Check(snapshot.world.orders.size()==1 && snapshot.world.orders[0].mode==economy::HaulMode::Rail,
          "world API preserves selected haul mode");
    const auto result=api.PollCommand(order.handle);
    Check(result.error==economy_api::ErrorCode::None && result.result.state==economy::CommandState::Accepted,
          "API reports accepted rail order");
    return 0;
}
