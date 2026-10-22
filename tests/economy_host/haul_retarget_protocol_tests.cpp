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
  request["protocolVersion"] = economy_host::ProtocolVersion;
  request["method"] = method;
  return request.dump();
}

Json Response(economy_host::Dispatcher &dispatcher, const std::string &line) {
  return Json::parse(dispatcher.HandleLine(line));
}

economy::Catalog RetargetCatalog() {
  economy::CatalogBuilder builder;
  economy::Catalog catalog;
  economy::CatalogErrorInfo error;
  economy::ResourceId grain{};
  economy::BuildingTypeId farm{}, depot{};
  economy::VehicleTypeId truck{};
  if (!builder.AddResource("grain", &grain, error) ||
      !builder.AddBuildingType("farm", 1, 1, &farm, error) ||
      !builder.AddBuildingType("depot", 1, 1, &depot, error) ||
      !builder.AddStorage(farm, grain, 20, error) ||
      !builder.AddStorage(depot, grain, 20, error) ||
      !builder.AddRecipe(farm, {"harvest", 1, {}, {{grain, 1}}}, error) ||
      !builder.AddVehicleType("truck", 4, 1, &truck, error) ||
      !builder.Build(catalog, error)) {
    std::cerr << "FAIL: construct retarget fixture catalog: " << error.message << '\n';
    return {};
  }
  return catalog;
}

bool SetUpLoadedOrder(economy::World &world) {
  for (std::int32_t x = 1; x < 9; ++x)
    world.BuildRoad({x, 1}, {x + 1, 1});
  world.PlaceBuilding(0, {1, 2});
  world.PlaceBuilding(1, {4, 2});
  world.PlaceBuilding(1, {7, 2});
  if (!world.Step().accepted)
    return false;
  if (world.Snapshot().buildings.size() != 3 || world.Snapshot().roads.size() != 8)
    return false;
  world.CreateHaulOrder(1, 2, 0, 0);
  if (!world.Step().accepted)
    return false;
  for (int i = 0; i < 8; ++i) {
    if (!world.Step().accepted)
      return false;
    const auto snapshot = world.Snapshot();
    if (!snapshot.orders.empty() && snapshot.orders.front().cargo > 0)
      return true;
  }
  return false;
}
} // namespace

int main() {
  auto catalog = RetargetCatalog();
  bool ok = Check(catalog.Resources().size() == 1 && catalog.BuildingTypes().size() == 2 &&
                      catalog.VehicleTypes().size() == 1,
                  "synthetic fixture catalog built");
  if (!ok)
    return 1;

  economy::World world(catalog, 16, 8);
  ok &= Check(SetUpLoadedOrder(world), "fixture reaches a loaded haul order");
  const auto before = world.Snapshot();
  if (before.orders.empty() || before.vehicles.empty()) {
    std::cerr << "FAIL: loaded fixture has order and vehicle\n";
    return 1;
  }
  const auto originalOrder = before.orders.front();

  economy_api::EconomyApi controllerApi(world, {true, true});
  economy_host::Dispatcher controller(economy_host::Role::Controller, controllerApi, false);
  const auto hello = Response(controller, Request("hello", "hello"));
  ok &= Check(hello["ok"] &&
                  std::find(hello["result"]["actions"].begin(), hello["result"]["actions"].end(),
                            "retarget_haul_order") != hello["result"]["actions"].end(),
              "controller capabilities advertise retarget_haul_order");

  const auto current = Response(controller, Request("world", "world"))["result"]["world"];
  const Json retargetCommand{{"kind", "retarget_haul_order"},
                             {"orderId", originalOrder.id},
                             {"destination", 3}};
  const auto queued = Response(controller, Request(
      "retarget", "submit", Json{{"revision", current["revision"]}, {"command", retargetCommand}}));
  ok &= Check(queued["ok"] && queued["result"]["errorCode"] == "none" &&
                  queued["result"]["state"] == "queued",
              "retarget command is queued through JSONL");
  const auto handle = queued["result"]["handle"].get<std::string>();
  const auto beforeStep = Response(controller, Request("world-before-step", "world"))["result"]["world"];
  const auto queuedOrder = std::find_if(beforeStep["orders"].begin(), beforeStep["orders"].end(),
      [&](const Json &order) { return order["id"] == originalOrder.id; });
  ok &= Check(queuedOrder != beforeStep["orders"].end() && (*queuedOrder)["destination"] == 2 &&
                  (*queuedOrder)["cargo"] == std::to_string(originalOrder.cargo),
              "queued retarget does not mutate world state before a tick");
  const auto stepped = Response(controller, Request("step", "step", Json{{"ticks", 1}}));
  ok &= Check(stepped["ok"] && stepped["result"]["errorCode"] == "none",
              "queued retarget applies at the next tick");
  const auto polled = Response(controller, Request("poll-retarget", "poll", Json{{"handle", handle}}));
  ok &= Check(polled["ok"] && polled["result"]["state"] == "accepted",
              "retarget command result is pollable");
  ok &= Check(polled["result"]["orderId"] == originalOrder.id,
              "polled retarget result identifies the original order");
  const auto after = Response(controller, Request("world-after", "world"))["result"]["world"];
  const auto orderAfter = std::find_if(after["orders"].begin(), after["orders"].end(),
      [&](const Json &order) { return order["id"] == originalOrder.id; });
  const auto vehicleAfter = std::find_if(after["vehicles"].begin(), after["vehicles"].end(),
      [&](const Json &vehicle) { return vehicle["id"] == originalOrder.vehicle; });
  ok &= Check(orderAfter != after["orders"].end() && (*orderAfter)["destination"] == 3 &&
                  (*orderAfter)["vehicleId"] == originalOrder.vehicle &&
                  (*orderAfter)["cargo"] == std::to_string(originalOrder.cargo),
              "retarget preserves order ID, truck, and cargo");
  ok &= Check(vehicleAfter != after["vehicles"].end() &&
                  (*vehicleAfter)["orderId"] == originalOrder.id &&
                  (*vehicleAfter)["cargo"] == std::to_string(originalOrder.cargo),
              "retarget preserves vehicle ownership and cargo");

  const auto stale = Response(controller, Request(
      "stale", "submit", Json{{"revision", current["revision"]}, {"command", retargetCommand}}));
  ok &= Check(stale["ok"] && stale["result"]["errorCode"] == "stale_revision",
              "stale revision rejects retarget submission");

  economy::World observerWorld(catalog, 16, 8);
  ok &= Check(SetUpLoadedOrder(observerWorld), "observer fixture reaches a loaded haul order");
  economy_api::EconomyApi observerApi(observerWorld, {true, false});
  economy_host::Dispatcher observer(economy_host::Role::Observer, observerApi, false);
  const auto observerWorldView = Response(observer, Request("observer-world", "world"))["result"]["world"];
  const auto denied = Response(observer, Request(
      "observer-retarget", "submit",
      Json{{"revision", observerWorldView["revision"]}, {"command", retargetCommand}}));
  ok &= Check(denied["ok"] && denied["result"]["errorCode"] == "permission_denied",
              "observer cannot retarget haul orders");

  Json invalidType = retargetCommand;
  invalidType["destination"] = "3";
  const auto badType = Response(controller, Request(
      "bad-type", "submit", Json{{"revision", after["revision"]}, {"command", invalidType}}));
  ok &= Check(!badType["ok"] && badType["error"]["code"] == "invalid_request",
              "retarget destination requires an unsigned integer");
  Json extra = retargetCommand;
  extra["unexpected"] = true;
  const auto extraField = Response(controller, Request(
      "extra-field", "submit", Json{{"revision", after["revision"]}, {"command", extra}}));
  ok &= Check(!extraField["ok"] && extraField["error"]["code"] == "invalid_request",
              "retarget command rejects extra fields");

  // Cancellation remains available for empty vehicles and reports the core's cargo-in-transit rejection
  // when the truck owns cargo. This checks the new failure travels through poll serialization.
  const auto cancel = Response(controller, Request(
      "cancel-loaded", "submit", Json{{"revision", after["revision"]},
          {"command", Json{{"kind", "cancel_haul_order"}, {"orderId", originalOrder.id}}}}));
  bool cancellationFailureReported = false;
  if (cancel["ok"] && cancel["result"]["errorCode"] == "none") {
    const auto cancelHandle = cancel["result"]["handle"].get<std::string>();
    Response(controller, Request("step-cancel", "step", Json{{"ticks", 1}}));
    const auto cancelResult = Response(controller, Request("poll-cancel", "poll", Json{{"handle", cancelHandle}}));
    cancellationFailureReported = cancelResult["result"]["state"] == "rejected" &&
        cancelResult["result"]["failure"] == "cargo_in_transit";
  }
  ok &= Check(cancellationFailureReported, "loaded cancellation reports cargo_in_transit");

  if (ok)
    std::cout << "Haul retarget protocol contracts passed\n";
  return ok ? 0 : 1;
}
