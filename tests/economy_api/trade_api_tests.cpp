#include "openrepublic/economy_api/economy_api.h"

#include <cstdlib>
#include <iostream>

using namespace openrepublic;

namespace {
void Check(bool value, const char* label) {
    if (!value) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}
economy::Catalog Catalog() {
    const char* text =
        "openrepublic-economy 1\n"
        "resource grain\ntrade grain buy 5 sell 3\n"
        "resource coal\ntrade coal buy 2 sell 1\n"
        "building station 1 1\n  store grain 10\n  store coal 10\n"
        "  recipe make_grain every 1 out grain:4\n  tradepost\nend\n";
    economy::Catalog result;
    economy::ParseError error;
    Check(economy::ParseCatalogText(text, result, error), "parse trade catalog");
    return result;
}
}

int main() {
    auto catalog = Catalog();
    economy::World world(catalog, 8, 8);
    economy_api::EconomyApi controller(world, {true, true});
    economy_api::EconomyApi observer(world, {true, false});
    auto defs = observer.DescribeCatalog();
    Check(defs.resources[0].tradeDeclared && defs.resources[0].sellPrice == 3,
          "observer can inspect declared trade prices");
    Check(defs.buildingTypes[0].tradePost, "catalog exposes trade post designation");
    auto placed = controller.PlaceBuilding(world.Revision(), "station", {2, 2});
    Check(placed.error == economy_api::ErrorCode::None, "queue trade station");
    Check(controller.Step() == economy_api::ErrorCode::None, "place and produce at trade station");
    auto current = controller.QueryWorld();
    Check(current.world.buildings.size() == 1 && current.world.buildings[0].inventory[0].amount == 4,
          "station has synthetic grain stock");
    const auto building = current.world.buildings[0].id;
    auto stopped = controller.SetBuildingEnabled(current.world.revision, building, false);
    Check(stopped.error == economy_api::ErrorCode::None &&
          controller.Step() == economy_api::ErrorCode::None, "disable station recipe before trade");
    current = controller.QueryWorld();
    Check(observer.TradeExport(current.world.revision, building, "grain", 4, 2).error ==
              economy_api::ErrorCode::PermissionDenied,
          "observer cannot mutate trade");
    auto trade = controller.TradeExport(current.world.revision, building, "grain", 4, 2);
    Check(trade.error == economy_api::ErrorCode::None, "controller queues export");
    Check(controller.Step() == economy_api::ErrorCode::None, "apply export at tick boundary");
    auto poll = controller.PollCommand(trade.handle);
    Check(poll.error == economy_api::ErrorCode::None && poll.result.state == economy::CommandState::Accepted,
          "export command accepted");
    current = observer.QueryWorld();
    Check(current.world.treasury == 6 && current.world.buildings[0].inventory[0].amount == 2,
          "export obeys per-tick rate cap and credits treasury");
    trade = controller.TradeImport(current.world.revision, building, "coal", 3, 9);
    Check(trade.error == economy_api::ErrorCode::None, "queue affordable import");
    Check(controller.Step() == economy_api::ErrorCode::None, "apply import");
    current = controller.QueryWorld();
    Check(current.world.treasury == 0 && current.world.buildings[0].inventory[1].amount == 3,
          "import debits treasury and credits inventory atomically");
    return 0;
}
