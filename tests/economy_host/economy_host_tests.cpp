#include "economy_protocol.h"

#include "json.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>

namespace {
using namespace openrepublic;
using Json = nlohmann::json;

bool Check(bool value, const char *label) {
  if (!value)
    std::cerr << "FAIL: " << label << '\n';
  return value;
}
std::string Request(const char *id, const char *method, const Json &fields = Json::object()) {
  Json request = fields;
  request["id"] = id;
  request["protocolVersion"] = 0;
  request["method"] = method;
  return request.dump();
}
Json Response(economy_host::Dispatcher &dispatcher, const std::string &line) {
  return Json::parse(dispatcher.HandleLine(line));
}
} // namespace

int main() {
  const auto catalog = economy::MakeSyntheticCatalog();
  economy_host::Dispatcher observer(economy_host::Role::Observer, catalog);
  const auto hello = Response(observer, Request("hello", "hello"));
  bool ok = Check(hello["ok"] && hello["result"]["role"] == "observer", "observer hello");
  const std::string observerSession = hello.value("sessionId", std::string());
  ok &= Check(!observerSession.empty() && hello["result"]["sessionId"] == observerSession,
              "hello reports a stable host session in the envelope and result");
  const auto denied = Response(observer, Request("step", "step", Json{{"ticks", 1}}));
  ok &=
      Check(denied["ok"] && denied["result"]["errorCode"] == "permission_denied", "observer mutation denied");
  const auto duplicate = Json::parse(
      observer.HandleLine("{\"id\":\"a\",\"id\":\"b\",\"protocolVersion\":0,\"method\":\"world\"}"));
  ok &=
      Check(!duplicate["ok"] && duplicate["error"]["code"] == "duplicate_json_key", "duplicate key rejected");
  ok &= Check(duplicate.value("sessionId", std::string()) == observerSession,
              "protocol errors carry the current host session");
  const auto unknown = Response(observer, Request("extra", "world", Json{{"extra", true}}));
  ok &= Check(!unknown["ok"] && unknown["error"]["code"] == "invalid_request", "unknown field rejected");
  const auto catalogResult = Response(observer, Request("catalog", "catalog"));
  ok &= Check(catalogResult["ok"] && catalogResult["result"]["errorCode"] == "none" &&
                  catalogResult["result"]["buildingTypes"].size() >= 4,
              "catalog discovery exposes synthetic building types");
  const auto farmType = catalogResult["result"]["buildingTypes"][0];
  ok &= Check(farmType["storage"][0]["capacity"].is_string() &&
                  farmType["recipes"][0]["outputs"][0]["amount"].is_string(),
              "catalog uint64 quantities use decimal strings");
  const auto legacySummary = Response(observer, Request("legacy-summary", "summary"));
  ok &= Check(legacySummary["ok"] && legacySummary["result"]["population"].is_null() &&
                  legacySummary["result"]["workers"].is_null() &&
                  legacySummary["result"]["workersRequired"].is_null(),
              "catalogs without population declarations preserve unavailable summary fields");

  economy_host::Dispatcher controller(economy_host::Role::Controller, catalog);
  const auto controllerHello = Response(controller, Request("controller-hello", "hello"));
  ok &= Check(Response(controller, Request("history", "rules_history"))["result"]["history"].empty(),
              "a fresh host has no rule reloads in its history");
  {
    // Batch haul orders: bounds are enforced, observers are denied, entries are queued independently.
    const auto order = Json{{"source", 1}, {"destination", 2}, {"resource", "grain"}, {"vehicleType", "truck"}};
    const auto batch = Response(controller, Request("batch", "create_haul_orders",
                                                     Json{{"revision", "0"}, {"orders", Json::array({order, order})}}));
    ok &= Check(batch["ok"] && batch["result"]["results"].size() == 2 &&
                    batch["result"]["results"][1]["handle"] != batch["result"]["results"][0]["handle"],
                "batch haul orders return one result per order");
    ok &= Check(!Response(controller, Request("empty", "create_haul_orders",
                                               Json{{"revision", "0"}, {"orders", Json::array()}}))["ok"],
                "an empty batch is rejected");
    ok &= Check(Response(observer, Request("deny", "create_haul_orders",
                                            Json{{"revision", "0"}, {"orders", Json::array({order})}}))["result"]["errorCode"] ==
                    "permission_denied",
                "observers cannot batch haul orders");
  }
  const std::string controllerSession = controllerHello.value("sessionId", std::string());
  ok &= Check(!controllerSession.empty() && controllerSession != observerSession,
              "independent host dispatchers receive different session IDs");
  const auto beforeStaleRequest = Response(controller, Request("before-stale", "world"))["result"]["world"];
  const auto staleSessionStep = Response(
      controller, Request("stale-session", "step", Json{{"ticks", 1}, {"sessionId", "replaced-host"}}));
  const auto afterStaleRequest = Response(controller, Request("after-stale", "world"))["result"]["world"];
  ok &= Check(!controllerSession.empty() && staleSessionStep["error"]["code"] == "stale_session" &&
                  staleSessionStep["sessionId"] == controllerSession,
              "host rejects requests bound to a different session");
  ok &= Check(beforeStaleRequest["tick"] == afterStaleRequest["tick"],
              "stale-session step does not advance the world");
  const auto world = Response(controller, Request("world", "world"))["result"]["world"];
  const auto submitted = Response(
      controller,
      Request("place", "submit",
              Json{{"revision", world["revision"]},
                   {"command", Json{{"kind", "place_building"}, {"type", "farm"}, {"x", 1}, {"z", 1}}}}));
  ok &= Check(submitted["ok"] && submitted["result"]["errorCode"] == "none" &&
                  submitted["result"]["state"] == "queued",
              "typed command accepted");
  const auto nestedExtra = Response(controller, Request("nested", "submit",
                                                        Json{{"revision", world["revision"]},
                                                             {"command", Json{{"kind", "place_building"},
                                                                              {"type", "farm"},
                                                                              {"x", 1},
                                                                              {"z", 1},
                                                                              {"method", "world"}}}}));
  ok &= Check(!nestedExtra["ok"] && nestedExtra["error"]["code"] == "invalid_request",
              "nested command rejects envelope fields");
  const auto stepped = Response(controller, Request("step", "step", Json{{"ticks", 1}}));
  ok &= Check(stepped["ok"] && stepped["result"]["errorCode"] == "none", "controller advances world");
  const auto stalePage = Response(
      controller,
      Request("stale-page", "world",
              Json{{"revision", world["revision"]}, {"kind", "buildings"}, {"offset", 0}, {"limit", 1}}));
  ok &= Check(stalePage["ok"] && stalePage["result"]["errorCode"] == "stale_revision" &&
                  !stalePage["result"].contains("entities"),
              "stale page returns typed error without default page");
  const auto currentWorld = Response(controller, Request("current", "world"))["result"]["world"];
  const auto page = Response(
      controller,
      Request(
          "page", "world",
          Json{{"revision", currentWorld["revision"]}, {"kind", "buildings"}, {"offset", 0}, {"limit", 1}}));
  ok &= Check(page["ok"] && page["result"]["errorCode"] == "none" && page["result"]["entities"].size() == 1 &&
                  page["result"]["total"] == 1 && page["result"]["nextOffset"] == 1,
              "revision-checked entity pagination");
  const auto malformedVersion =
      Response(controller, "{\"id\":\"v\",\"protocolVersion\":7,\"method\":\"world\"}");
  ok &= Check(!malformedVersion["ok"] && malformedVersion["error"]["code"] == "unsupported_protocol_version",
              "unsupported version rejected");

  economy::World sharedWorld(catalog, 64, 64);
  economy_api::EconomyApi sharedApi(sharedWorld, economy_api::Permissions{true, true});
  economy_host::Dispatcher appController(economy_host::Role::Controller, sharedApi, true);
  const auto appHello = Response(appController, Request("app-hello", "hello"));
  const std::string appSession = appHello.value("sessionId", std::string());
  ok &= Check(appHello["ok"] && !appSession.empty(), "borrowed-API dispatcher permits initial hello");
  const auto appCapabilities = Response(appController, Request("app-capabilities", "capabilities"));
  ok &= Check(appCapabilities["ok"], "borrowed-API dispatcher permits initial capabilities");
  const bool advertisesShutdown = std::any_of(appCapabilities["result"]["methods"].begin(),
      appCapabilities["result"]["methods"].end(), [](const Json& method) {
        return method.value("name", std::string()) == "shutdown";
      });
  ok &= Check(!advertisesShutdown, "embedded dispatcher does not advertise app shutdown");
  const auto unboundWorld = Response(appController, Request("unbound-world", "world"));
  const auto unboundStep = Response(appController, Request("unbound-step", "step", Json{{"ticks", 1}}));
  ok &= Check(!unboundWorld["ok"] && unboundWorld["error"]["code"] == "missing_session" &&
                  !unboundStep["ok"] && unboundStep["error"]["code"] == "missing_session",
              "embedded dispatcher requires the handshake session on every later request");
  Json appSessionFields{{"sessionId", appSession}};
  const auto appWorldBefore = Response(appController, Request("app-world-before", "world", appSessionFields));
  const auto appRevision = appWorldBefore["result"]["world"]["revision"];
  const auto appSubmission = Response(
      appController,
      Request("app-place", "submit",
              Json{{"sessionId", appSession}, {"revision", appRevision},
                   {"command", Json{{"kind", "place_building"}, {"type", "farm"}, {"x", 1}, {"z", 1}}}}));
  const std::string sharedHandle = appSubmission["result"].value("handle", std::string());
  ok &= Check(appSubmission["ok"] && appSubmission["result"]["errorCode"] == "none" &&
                  !sharedHandle.empty(),
              "embedded dispatcher submits through the borrowed API");
  ok &= Check(sharedApi.Step(1) == economy_api::ErrorCode::None,
              "direct app API advances the world after an external submission");
  const std::uint64_t appHandleValue = std::stoull(sharedHandle);
  const auto sharedPoll = sharedApi.PollCommand(appHandleValue);
  ok &= Check(sharedPoll.error == economy_api::ErrorCode::None &&
                  sharedPoll.result.state == economy::CommandState::Accepted,
              "external command handle is visible to the app's shared API instance");
  const auto appWorldAfter = Response(appController, Request("app-world-after", "world", appSessionFields));
  ok &= Check(appWorldAfter["ok"] && appWorldAfter["result"]["world"]["tick"] == "1" &&
                  appWorldAfter["result"]["world"]["buildings"].size() == 1,
              "embedded dispatcher and app API observe the same world state");
  const auto appShutdown = Response(appController, Request("app-shutdown", "shutdown", appSessionFields));
  ok &= Check(appShutdown["ok"] && appShutdown["result"]["errorCode"] == "permission_denied" &&
                  !appController.ShutdownRequested(),
              "embedded controller cannot shut down the desktop app");

  economy::CatalogBuilder populationBuilder;
  economy::Catalog populationCatalog;
  economy::CatalogErrorInfo populationError;
  economy::BuildingTypeId housing{}, mine{};
  const bool populationCatalogBuilt =
      populationBuilder.AddBuildingType("housing", 1, 1, &housing, populationError) &&
      populationBuilder.SetHousing(housing, 20, populationError) &&
      populationBuilder.AddBuildingType("mine", 1, 1, &mine, populationError) &&
      populationBuilder.SetWorkers(mine, 10, populationError) &&
      populationBuilder.Build(populationCatalog, populationError);
  ok &= Check(populationCatalogBuilt, "build opt-in population host catalog");
  if (populationCatalogBuilt) {
    economy::World populationWorld(populationCatalog, 8, 8);
    populationWorld.PlaceBuilding(housing, {1, 1});
    populationWorld.PlaceBuilding(mine, {4, 1});
    const auto populationStep = populationWorld.Step();
    ok &= Check(populationStep.accepted, "advance opt-in population host world");
    economy_host::Dispatcher populationHost(economy_host::Role::Observer, populationWorld);
    const auto populationSummary = Response(populationHost, Request("population-summary", "summary"));
    ok &= Check(populationSummary["ok"] &&
                    populationSummary["result"]["population"] == "20" &&
                    populationSummary["result"]["workers"] == "10" &&
                    populationSummary["result"]["workersRequired"] == "10",
                "protocol v0 summary exposes population and worker totals as decimal strings");
  }
  return ok ? 0 : 1;
}
