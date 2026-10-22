#include "economy_protocol.h"

#include "json.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
using namespace openrepublic;
using Json = nlohmann::json;

void Check(bool ok, const char *label) {
    if (!ok) {
        std::cerr << "FAIL: " << label << '\n';
        std::exit(1);
    }
}

std::string Request(const char *id, const char *method, const Json &fields = Json::object()) {
    Json request = fields;
    request["id"] = id;
    request["protocolVersion"] = economy_host::ProtocolVersion;
    request["method"] = method;
    return request.dump();
}

Json Response(economy_host::Dispatcher &dispatcher, const std::string &line) {
    return Json::parse(dispatcher.HandleLine(line));
}

economy::Catalog MakeCatalog() {
    economy::CatalogBuilder builder;
    economy::Catalog catalog;
    economy::CatalogErrorInfo error;
    economy::ResourceId electricity{}, product{};
    economy::BuildingTypeId generator{}, consumer{};
    const bool ok = builder.AddUtilityResource("electricity", &electricity, error) &&
        builder.AddResource("product", &product, error) &&
        builder.AddBuildingType("generator", 1, 1, &generator, error) &&
        builder.AddBuildingType("consumer", 1, 1, &consumer, error) &&
        builder.AddStorage(consumer, product, 8, error) &&
        builder.AddRecipe(generator, {"generate", 1, {}, {{electricity, 3}}}, error) &&
        builder.AddRecipe(consumer, {"consume", 1, {{electricity, 2}}, {{product, 1}}}, error) &&
        builder.Build(catalog, error);
    if (!ok) {
        std::cerr << "FAIL: build utility diagnostics protocol catalog: " << error.message << '\n';
        std::exit(1);
    }
    return catalog;
}
}

int main() {
    auto catalog = MakeCatalog();
    economy::World world(catalog, 12, 8);
    world.PlaceBuilding(0, {1, 2});
    world.PlaceBuilding(1, {4, 2});
    world.PlaceBuilding(1, {8, 2});
    Check(world.Step().accepted, "place utility diagnostic protocol fixtures");
    const auto placed = world.Snapshot();
    Check(placed.buildings.size() == 3, "all protocol diagnostic fixtures exist");
    const auto generator = placed.buildings[0].id;
    const auto first = placed.buildings[1].id;
    const auto second = placed.buildings[2].id;
    economy::ResourceId electricity{};
    Check(catalog.FindResource("electricity", electricity), "resolve utility resource ID");
    world.ConnectUtility(electricity, generator, first);
    Check(world.Step().accepted && world.Step().accepted, "measure connected and disconnected components");

    economy_host::Dispatcher controller(economy_host::Role::Controller, world);
    const auto hello = Response(controller, Request("hello", "hello"));
    Check(hello["ok"], "diagnostics host hello works");
    bool advertised = false;
    for (const auto &method : hello["result"]["methods"])
        advertised |= method["name"] == "utility_diagnostics" &&
            std::find(method["roles"].begin(), method["roles"].end(), "observer") != method["roles"].end();
    Check(advertised, "hello advertises utility diagnostics to query roles");

    const auto before = world.Snapshot();
    const auto revision = std::to_string(before.revision);
    auto firstPage = Response(controller, Request("diagnostics-1", "utility_diagnostics", Json{
        {"revision", revision}, {"offset", 0}, {"limit", 1}}));
    Check(firstPage["ok"] && firstPage["result"]["errorCode"] == "none" &&
          firstPage["result"]["available"] == true && firstPage["result"]["tick"] == "3" &&
          firstPage["result"]["revision"] == revision && firstPage["result"]["offset"] == 0 &&
          firstPage["result"]["total"] == 2 && firstPage["result"]["hasMore"] == true &&
          firstPage["result"]["networks"].size() == 1,
          "protocol returns revision-checked bounded diagnostic page");
    const auto &connected = firstPage["result"]["networks"][0];
    Check(connected["resource"] == "electricity" && connected["component"] == generator &&
          connected["members"].size() == 2 && connected["members"][0] == generator &&
          connected["members"][1] == first && connected["memberCount"] == 2 &&
          connected["membersComplete"] == true && connected["generated"] == "3" &&
          connected["requested"] == "2" && connected["consumed"] == "2" &&
          connected["unused"] == "1" && connected["starvedRecipes"] == "0",
          "protocol preserves numeric identities and exact decimal utility amounts");

    const auto secondPage = Response(controller, Request("diagnostics-2", "utility_diagnostics", Json{
        {"revision", revision}, {"offset", 1}, {"limit", 1}}));
    Check(secondPage["ok"] && secondPage["result"]["offset"] == 1 &&
          secondPage["result"]["hasMore"] == false && secondPage["result"]["networks"].size() == 1 &&
          secondPage["result"]["networks"][0]["component"] == second &&
          secondPage["result"]["networks"][0]["memberCount"] == 1 &&
          secondPage["result"]["networks"][0]["membersComplete"] == true &&
          secondPage["result"]["networks"][0]["generated"] == "0" &&
          secondPage["result"]["networks"][0]["requested"] == "2" &&
          secondPage["result"]["networks"][0]["consumed"] == "0" &&
          secondPage["result"]["networks"][0]["unused"] == "0" &&
          secondPage["result"]["networks"][0]["starvedRecipes"] == "1",
          "second protocol page reports disconnected consumer shortage");
    const auto filtered = Response(controller, Request("diagnostics-filtered", "utility_diagnostics", Json{
        {"revision", revision}, {"offset", 0}, {"limit", 1}, {"buildingId", second}}));
    Check(filtered["ok"] && filtered["result"]["buildingId"] == second &&
          filtered["result"]["total"] == 1 && filtered["result"]["networks"].size() == 1 &&
          filtered["result"]["networks"][0]["component"] == second,
          "protocol filters diagnostics by exact selected building ID");
    const auto invalidBuilding = Response(controller, Request("diagnostics-building", "utility_diagnostics", Json{
        {"revision", revision}, {"offset", 0}, {"limit", 1}, {"buildingId", 0}}));
    Check(invalidBuilding["ok"] && invalidBuilding["result"]["errorCode"] == "invalid_argument",
          "protocol distinguishes an unknown building ID from an unfiltered query");

    const auto stale = Response(controller, Request("diagnostics-stale", "utility_diagnostics", Json{
        {"revision", std::to_string(before.revision + 1)}, {"offset", 0}, {"limit", 1}}));
    Check(stale["ok"] && stale["result"]["errorCode"] == "stale_revision",
          "protocol exposes stale revision query failure");
    const auto badLimit = Response(controller, Request("diagnostics-limit", "utility_diagnostics", Json{
        {"revision", revision}, {"offset", 0}, {"limit", 0}}));
    Check(badLimit["ok"] && badLimit["result"]["errorCode"] == "invalid_page_limit",
          "protocol exposes invalid diagnostic page limit");
    const auto badOffset = Response(controller, Request("diagnostics-offset", "utility_diagnostics", Json{
        {"revision", revision}, {"offset", 3}, {"limit", 1}}));
    Check(badOffset["ok"] && badOffset["result"]["errorCode"] == "invalid_argument",
          "protocol rejects an offset beyond diagnostic collection");
    const auto unknown = Response(controller, Request("diagnostics-extra", "utility_diagnostics", Json{
        {"revision", revision}, {"offset", 0}, {"limit", 1}, {"extra", true}}));
    Check(!unknown["ok"] && unknown["error"]["code"] == "invalid_request",
          "protocol rejects unknown diagnostic request fields");
    const auto wrongBuildingType = Response(controller, Request("diagnostics-building-type", "utility_diagnostics", Json{
        {"revision", revision}, {"offset", 0}, {"limit", 1}, {"buildingId", "2"}}));
    Check(!wrongBuildingType["ok"] && wrongBuildingType["error"]["code"] == "invalid_request",
          "protocol requires a numeric buildingId filter");
    Check(world.Snapshot().stateHash == before.stateHash && world.Revision() == before.revision &&
          world.Tick() == before.tick,
          "diagnostic protocol success and failure paths leave world unchanged");

    economy_host::Dispatcher observer(economy_host::Role::Observer, world);
    const auto observerQuery = Response(observer, Request("observer-diagnostics", "utility_diagnostics", Json{
        {"revision", revision}, {"offset", 0}, {"limit", 1}}));
    Check(observerQuery["ok"] && observerQuery["result"]["errorCode"] == "none" &&
          observerQuery["result"]["available"] == true,
          "observer role can use the read-only utility diagnostics method");

    std::cout << "Economy utility diagnostics protocol contracts passed\n";
}
