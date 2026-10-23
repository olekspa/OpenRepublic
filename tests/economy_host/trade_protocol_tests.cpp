#include "economy_protocol.h"
#include "json.hpp"

#include <cstdlib>
#include <iostream>

using namespace openrepublic;
using Json = nlohmann::json;
namespace {
void Check(bool value, const char* label) { if (!value) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); } }
std::string Request(const char* id, const char* method, Json fields = Json::object()) {
    fields["id"] = id; fields["protocolVersion"] = economy_host::ProtocolVersion; fields["method"] = method;
    return fields.dump();
}
Json Call(economy_host::Dispatcher& d, const char* id, const char* method, Json fields = Json::object()) {
    return Json::parse(d.HandleLine(Request(id, method, std::move(fields))));
}
}
int main() {
    economy::Catalog catalog;
    economy::ParseError parse;
    const std::string text = "openrepublic-economy 1\nresource grain\ntrade grain buy 5 sell 3\n"
        "building station 1 1\n store grain 10\n recipe grow every 1 out grain:4\n tradepost\nend\n";
    Check(economy::ParseCatalogText(text, catalog, parse), "parse protocol trade catalog");
    economy::World world(catalog, 8, 8);
    world.PlaceBuilding(0, {2, 2});
    Check(world.Step().accepted, "place station and generate stock");
    auto snapshot = world.Snapshot();
    auto disable = world.SetBuildingEnabled(snapshot.buildings[0].id, false);
    Check(disable && world.Step().accepted, "disable recipe to isolate trade stock");
    snapshot = world.Snapshot();
    economy_host::Dispatcher controller(economy_host::Role::Controller, world);
    economy_host::Dispatcher observer(economy_host::Role::Observer, world);
    auto cat = Call(observer, "cat", "catalog");
    Check(cat["result"]["resources"][0]["tradeDeclared"] == true, "observer catalog declares price");
    auto result = Call(controller, "sell", "trade_export", Json{{"revision", std::to_string(world.Revision())},
        {"buildingId", snapshot.buildings[0].id}, {"resource", "grain"}, {"amount", "4"}, {"rateCap", "2"}});
    Check(result["ok"] && result["result"]["state"] == "queued", "controller queues protocol trade");
    auto denied = Call(observer, "denied", "trade_export", Json{{"revision", std::to_string(world.Revision())},
        {"buildingId", snapshot.buildings[0].id}, {"resource", "grain"}, {"amount", "1"}, {"rateCap", "1"}});
    Check(denied["result"]["errorCode"] == "permission_denied", "observer trade mutation denied");
    Call(controller, "step", "step", Json{{"ticks", 1}});
    auto state = Call(observer, "world", "world");
    Check(state["result"]["world"]["treasury"] == "6", "world JSON exposes treasury");
    return 0;
}
