#include "economy_protocol.h"

#include "json.hpp"

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
  const auto denied = Response(observer, Request("step", "step", Json{{"ticks", 1}}));
  ok &=
      Check(denied["ok"] && denied["result"]["errorCode"] == "permission_denied", "observer mutation denied");
  const auto duplicate = Json::parse(
      observer.HandleLine("{\"id\":\"a\",\"id\":\"b\",\"protocolVersion\":0,\"method\":\"world\"}"));
  ok &=
      Check(!duplicate["ok"] && duplicate["error"]["code"] == "duplicate_json_key", "duplicate key rejected");
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

  economy_host::Dispatcher controller(economy_host::Role::Controller, catalog);
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
  return ok ? 0 : 1;
}
