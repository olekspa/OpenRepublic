#include "economy_protocol.h"

#include "json.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
using namespace openrepublic;
using Json = nlohmann::json;

void Check(bool ok, const char* label) {
    if (!ok) {
        std::cerr << "FAIL: " << label << '\n';
        std::exit(1);
    }
}

std::string Request(const char* id, const char* method, const Json& fields = Json::object()) {
    Json request = fields;
    request["id"] = id;
    request["protocolVersion"] = economy_host::ProtocolVersion;
    request["method"] = method;
    return request.dump();
}

Json Response(economy_host::Dispatcher& dispatcher, const std::string& line) {
    return Json::parse(dispatcher.HandleLine(line));
}

economy::Catalog MakeCatalog() {
    economy::CatalogBuilder builder;
    economy::Catalog catalog;
    economy::CatalogErrorInfo error;
    economy::ResourceId water{}, grain{};
    economy::BuildingTypeId plant{}, house{};
    if (!builder.AddUtilityResource("water", &water, error) ||
        !builder.AddResource("grain", &grain, error) ||
        !builder.AddBuildingType("plant", 1, 1, &plant, error) ||
        !builder.AddBuildingType("house", 1, 1, &house, error) ||
        !builder.AddStorage(plant, grain, 10, error) ||
        !builder.AddStorage(house, grain, 10, error) ||
        !builder.AddRecipe(plant, {"pump", 1, {}, {{water, 5}}}, error) ||
        !builder.AddRecipe(house, {"consume", 1, {{water, 1}}, {{grain, 1}}}, error) ||
        !builder.Build(catalog, error)) {
        std::cerr << "FAIL: build utility protocol catalog: " << error.message << '\n';
        std::exit(1);
    }
    return catalog;
}
}

int main() {
    auto catalog = MakeCatalog();
    economy::World world(catalog, 8, 8);
    world.PlaceBuilding(0, {1, 1});
    world.PlaceBuilding(1, {4, 1});
    Check(world.Step().accepted, "place protocol fixture buildings");
    const auto before = world.Snapshot();
    Check(before.buildings.size() == 2, "fixture buildings exist");
    const auto a = before.buildings[0].id;
    const auto b = before.buildings[1].id;

    economy_host::Dispatcher controller(economy_host::Role::Controller, world);
    const auto hello = Response(controller, Request("hello", "hello"));
    Check(hello["ok"], "controller hello works");
    const auto& actions = hello["result"]["actions"];
    Check(std::find(actions.begin(), actions.end(), "connect_utility") != actions.end() &&
          std::find(actions.begin(), actions.end(), "disconnect_utility") != actions.end(),
          "hello advertises utility commands");

    const auto catalogReply = Response(controller, Request("catalog", "catalog"));
    const auto resources = catalogReply["result"]["resources"];
    Check(resources.size() == 2 && resources[0]["name"] == "water" &&
          resources[0]["utility"] == true && resources[1]["name"] == "grain" &&
          resources[1]["utility"] == false,
          "catalog JSON identifies utility resource definitions");

    auto revision = std::to_string(world.Revision());
    const Json command{{"kind", "connect_utility"}, {"resource", "water"}, {"a", a}, {"b", b}};
    auto queued = Response(controller, Request("connect", "submit",
        Json{{"revision", revision}, {"command", command}}));
    Check(queued["ok"] && queued["result"]["errorCode"] == "none" &&
          queued["result"]["state"] == "queued",
          "connect_utility command is accepted into the tick queue");
    auto worldReply = Response(controller, Request("before", "world"));
    Check(worldReply["result"]["world"]["utilityConnections"].empty(),
          "queued connection is not exposed before its tick");
    Check(Response(controller, Request("step", "step", Json{{"ticks", 1}}))["result"]["errorCode"] == "none",
          "step applies utility command");
    worldReply = Response(controller, Request("after", "world"));
    const auto connections = worldReply["result"]["world"]["utilityConnections"];
    Check(connections.size() == 1 && connections[0]["resource"] == "water" &&
          connections[0]["a"] == a && connections[0]["b"] == b,
          "world JSON exposes canonical named utility connections");
    const auto advice = Response(controller, Request("advice", "advise"));
    bool utilityNeedsHaul = false, utilitySurplus = false;
    for (const auto& need : advice["result"]["unservedInputs"])
        utilityNeedsHaul |= need["resource"] == "water";
    for (const auto& surplus : advice["result"]["unconsumedSurplus"])
        utilitySurplus |= surplus["resource"] == "water";
    Check(advice["ok"] && !utilityNeedsHaul && !utilitySurplus,
          "logistics adviser does not turn utility demand or output into haul advice");
    const auto page = Response(controller, Request("connection-page", "world", Json{
        {"revision", worldReply["result"]["world"]["revision"]},
        {"kind", "utility_connections"}, {"offset", 0}, {"limit", 1}}));
    Check(page["ok"] && page["result"]["kind"] == "utility_connections" &&
          page["result"]["total"] == 1 && page["result"]["entities"].size() == 1 &&
          page["result"]["entities"][0]["resource"] == "water",
          "utility connections are exposed through bounded JSONL paging");

    const auto duplicateSubmit = Response(controller, Request("duplicate", "submit", Json{
        {"revision", worldReply["result"]["world"]["revision"]},
        {"command", command}}));
    Check(duplicateSubmit["ok"] && duplicateSubmit["result"]["errorCode"] == "none",
          "duplicate utility link reaches core validation over JSONL");
    const auto duplicateHandle = duplicateSubmit["result"]["handle"].get<std::string>();
    Check(Response(controller, Request("step-duplicate", "step", Json{{"ticks", 1}}))["result"]["errorCode"] == "none",
          "step evaluates duplicate utility link");
    const auto duplicatePoll = Response(controller, Request("poll-duplicate", "poll",
        Json{{"handle", duplicateHandle}}));
    Check(duplicatePoll["ok"] && duplicatePoll["result"]["state"] == "rejected" &&
          duplicatePoll["result"]["failure"] == "duplicate_utility_connection",
          "protocol serializes duplicate utility failure");

    worldReply = Response(controller, Request("after-duplicate", "world"));
    revision = worldReply["result"]["world"]["revision"].get<std::string>();
    const auto removeConnected = Response(controller, Request("remove-connected", "submit", Json{
        {"revision", revision},
        {"command", Json{{"kind", "remove_building"}, {"buildingId", a}}}}));
    Check(removeConnected["ok"] && removeConnected["result"]["errorCode"] == "none",
          "connected building removal reaches core validation");
    const auto removalHandle = removeConnected["result"]["handle"].get<std::string>();
    Check(Response(controller, Request("step-remove", "step", Json{{"ticks", 1}}))["result"]["errorCode"] == "none",
          "step evaluates connected building removal");
    const auto removalPoll = Response(controller, Request("poll-remove", "poll",
        Json{{"handle", removalHandle}}));
    Check(removalPoll["ok"] && removalPoll["result"]["state"] == "rejected" &&
          removalPoll["result"]["failure"] == "referenced_by_utility_connection",
          "protocol exposes utility connection removal failure");
    worldReply = Response(controller, Request("after-remove", "world"));
    revision = worldReply["result"]["world"]["revision"].get<std::string>();
    const Json disconnect{{"kind", "disconnect_utility"}, {"resource", "water"}, {"a", b}, {"b", a}};
    queued = Response(controller, Request("disconnect", "submit",
        Json{{"revision", revision}, {"command", disconnect}}));
    Check(queued["ok"] && queued["result"]["errorCode"] == "none",
          "disconnect_utility command accepts reversed canonical endpoints");
    Check(Response(controller, Request("step2", "step", Json{{"ticks", 1}}))["result"]["errorCode"] == "none",
          "step applies disconnect command");
    worldReply = Response(controller, Request("after-disconnect", "world"));
    Check(worldReply["result"]["world"]["utilityConnections"].empty(),
          "world JSON removes disconnected utility");

    const std::string strictExtra = Request("extra", "submit", Json{
        {"revision", std::to_string(world.Revision())},
        {"command", Json{{"kind", "connect_utility"}, {"resource", "water"}, {"a", a}, {"b", b}, {"extra", 1}}}});
    const auto extraReply = Response(controller, strictExtra);
    Check(!extraReply["ok"] && extraReply["error"]["code"] == "invalid_request",
          "utility submit rejects unknown nested fields");
    const std::string duplicate = "{\"id\":\"dup\",\"protocolVersion\":0,\"method\":\"submit\",\"revision\":\"" +
        std::to_string(world.Revision()) + "\",\"command\":{\"kind\":\"connect_utility\",\"resource\":\"water\",\"a\":" +
        std::to_string(a) + ",\"b\":" + std::to_string(b) + ",\"a\":" + std::to_string(a) + "}}";
    const auto duplicateReply = Response(controller, duplicate);
    Check(!duplicateReply["ok"] && duplicateReply["error"]["code"] == "duplicate_json_key",
          "duplicate utility endpoint key is rejected");

    world.ConnectUtility(0, a, b);
    Check(world.Step().accepted && world.Snapshot().utilityConnections.size() == 1,
          "prepare connection for save/restore check");
    const auto saved = economy::SaveWorld(world);
    economy::World restored(catalog, 8, 8);
    economy::ParseError parseError;
    Check(economy::LoadWorld(saved, catalog, restored, parseError), "restore utility world snapshot");
    const auto restoredSnapshot = restored.Snapshot();
    Check(restoredSnapshot.utilityConnections.size() == 1 &&
          restoredSnapshot.utilityConnections[0].resource == 0 &&
          restoredSnapshot.utilityConnections[0].a == a && restoredSnapshot.utilityConnections[0].b == b,
          "restored snapshot preserves canonical utility connection");
    economy_host::Dispatcher restoredController(economy_host::Role::Controller, restored);
    const auto restoredWorld = Response(restoredController, Request("restored", "world"));
    Check(restoredWorld["result"]["world"]["utilityConnections"].size() == 1 &&
          restoredWorld["result"]["world"]["utilityConnections"][0]["resource"] == "water",
          "restored utility is visible through typed JSONL view");

    economy_host::Dispatcher observer(economy_host::Role::Observer, world);
    const auto denied = Response(observer, Request("denied", "submit", Json{
        {"revision", std::to_string(world.Revision())}, {"command", command}}));
    Check(denied["ok"] && denied["result"]["errorCode"] == "permission_denied",
          "observer cannot submit utility commands");
    std::cout << "Economy utility protocol contracts passed\n";
}
