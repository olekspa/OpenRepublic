#include "openrepublic/runtime/native_coal_demo.h"
#include <cstdlib>
#include <iostream>
using namespace openrepublic;
int main() {
    economy::WorldSnapshot s;
    s.treasury = 10;
    for (std::uint32_t id = 1; id <= 4; ++id) { economy::BuildingSnapshot b; b.id=id; s.buildings.push_back(b); }
    s.buildings[0].inventory.push_back({1,0,80});
    s.buildings[2].inventory.push_back({1,0,80});
    s.vehicles.push_back({1,0,1,{},0,economy::HaulState::Idle,2,economy::VehicleLeg::None,0});
    s.vehicles.push_back({2,0,1,{},0,economy::HaulState::Idle,2,economy::VehicleLeg::None,0});
    auto a = runtime::NextCoalDemoAction(s,1,2,3,4,1,4);
    if (!a || a->kind != runtime::CoalDemoActionKind::ImportCoal) return 1;
    s.buildings[0].inventory[0].amount=4;
    a = runtime::NextCoalDemoAction(s,1,2,3,4,1,4);
    if (!a || a->kind != runtime::CoalDemoActionKind::DispatchToStorage || a->vehicle != 1) return 2;
    s.vehicles[0].order=9; s.vehicles[0].leg=economy::VehicleLeg::ToSource;
    s.buildings[2].inventory[0].amount=4;
    a = runtime::NextCoalDemoAction(s,1,2,3,4,1,4);
    if (!a || a->kind != runtime::CoalDemoActionKind::DispatchToPlant || a->vehicle != 2) return 3;
    s.vehicles[1].order=10; s.vehicles[1].leg=economy::VehicleLeg::ToSource;
    if (runtime::NextCoalDemoAction(s,1,2,3,4,1,4)) return 4;
    std::cout << "Native coal demo planner checks passed\n";
}
