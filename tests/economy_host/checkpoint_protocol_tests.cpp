#include "economy_protocol.h"

#include "json.hpp"
#include "openrepublic/economy/checkpoint.h"

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

std::string Request(const char *id, const char *method, const Json &fields = Json::object(),
                    const std::string &sessionId = {}) {
  Json request = fields;
  request["id"] = id;
  request["protocolVersion"] = economy_host::ProtocolVersion;
  request["method"] = method;
  if (!sessionId.empty())
    request["sessionId"] = sessionId;
  return request.dump();
}

Json Response(economy_host::Dispatcher &dispatcher, const std::string &line) {
  return Json::parse(dispatcher.HandleLine(line));
}

bool DecodeBase64(const std::string &encoded, std::string &bytes) {
  if (encoded.size() % 4 != 0)
    return false;
  auto value = [](char c) -> int {
    static constexpr char chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; ++i)
      if (chars[i] == c)
        return i;
    return -1;
  };
  bytes.clear();
  for (std::size_t i = 0; i < encoded.size(); i += 4) {
    const int a = value(encoded[i]), b = value(encoded[i + 1]);
    const int c = encoded[i + 2] == '=' ? 0 : value(encoded[i + 2]);
    const int d = encoded[i + 3] == '=' ? 0 : value(encoded[i + 3]);
    if (a < 0 || b < 0 || c < 0 || d < 0 ||
        (encoded[i + 2] == '=' && encoded[i + 3] != '=') ||
        (i + 4 != encoded.size() && (encoded[i + 2] == '=' || encoded[i + 3] == '=')))
      return false;
    const auto block = (static_cast<std::uint32_t>(a) << 18) |
                       (static_cast<std::uint32_t>(b) << 12) |
                       (static_cast<std::uint32_t>(c) << 6) |
                       static_cast<std::uint32_t>(d);
    bytes.push_back(static_cast<char>((block >> 16) & 0xff));
    if (encoded[i + 2] != '=')
      bytes.push_back(static_cast<char>((block >> 8) & 0xff));
    if (encoded[i + 3] != '=')
      bytes.push_back(static_cast<char>(block & 0xff));
  }
  return true;
}

std::string ReadText(economy_host::Dispatcher &dispatcher, const std::string &sessionId,
                     const std::string &checkpointId, std::uint64_t totalBytes, bool &ok,
                     std::uint64_t chunkLimit = economy_host::MaxCheckpointChunkBytes) {
  std::string result;
  std::uint64_t offset = 0;
  while (offset < totalBytes) {
    const auto response = Response(dispatcher, Request(
        "read-chunk", "read_checkpoint",
        Json{{"checkpointId", checkpointId}, {"offset", offset},
             {"limit", chunkLimit}}, sessionId));
    const auto &chunk = response["result"];
    ok &= Check(response["ok"] && chunk["errorCode"] == "none" &&
                    chunk["checkpointId"] == checkpointId && chunk["offset"] == offset &&
                    chunk["totalBytes"] == totalBytes,
                "checkpoint chunk metadata matches requested immutable snapshot");
    if (!response["ok"] || chunk.value("errorCode", std::string()) != "none")
      break;
    std::string text;
    const bool decoded = DecodeBase64(chunk["dataBase64"].get<std::string>(), text);
    ok &= Check(decoded, "checkpoint chunk is valid padded Base64");
    ok &= Check(!text.empty() || offset == totalBytes,
                "nonterminal checkpoint reads return data");
    result += text;
    const auto next = chunk["nextOffset"].get<std::uint64_t>();
    ok &= Check(next > offset && next <= totalBytes,
                "checkpoint chunks advance within the captured byte range");
    offset = next;
  }
  return result;
}

bool HasAction(const Json &hello, const char *action) {
  const auto &actions = hello["result"]["actions"];
  return std::find(actions.begin(), actions.end(), action) != actions.end();
}
} // namespace

int main() {
  economy::CatalogBuilder builder;
  economy::Catalog catalog;
  economy::CatalogErrorInfo catalogError;
  economy::ResourceId rawResource{};
  economy::BuildingTypeId farmType{};
  std::string rawName = "grain";
  rawName.push_back('\0');
  rawName.push_back(static_cast<char>(0xff));
  rawName.push_back(static_cast<char>(0xc0));
  bool catalogReady = builder.AddResource(rawName, &rawResource, catalogError) &&
      builder.AddBuildingType("farm", 2, 2, &farmType, catalogError) &&
      builder.AddStorage(farmType, rawResource, 100, catalogError) &&
      builder.Build(catalog, catalogError);
  if (!catalogReady) {
    std::cerr << "FAIL: construct raw-byte checkpoint catalog: " << catalogError.message << '\n';
    return 1;
  }

  // Exercise one-byte slices through a catalog containing NUL and invalid UTF-8 bytes.
  economy::World tinyWorld(catalog, 16, 16);
  economy_api::EconomyApi tinyApi(tinyWorld, {true, true});
  economy_host::Dispatcher tinyDispatcher(economy_host::Role::Controller, tinyApi, false);
  std::string tinyExpected;
  economy::ParseError parseError;
  bool ok = Check(economy::SaveCheckpoint(tinyWorld, tinyExpected, parseError),
                  "capture raw-byte expected checkpoint text");
  const auto tinyBegin = Response(tinyDispatcher, Request(
      "tiny-begin", "begin_checkpoint", Json{{"revision", "0"}}));
  const auto tinyId = tinyBegin["result"]["checkpointId"].get<std::string>();
  const auto byteChunks = ReadText(tinyDispatcher, "", tinyId, tinyExpected.size(), ok, 1);
  ok &= Check(byteChunks == tinyExpected,
              "one-byte Base64 checkpoint chunks preserve NUL and invalid UTF-8 exactly");

  economy::World world(catalog, 64, 64);
  economy_api::EconomyApi api(world, {true, true});
  auto query = api.QueryWorld();
  const auto placed = api.PlaceBuilding(query.world.revision, "farm", {4, 4});
  ok &= Check(placed.error == economy_api::ErrorCode::None &&
                  api.Step() == economy_api::ErrorCode::None,
              "checkpoint fixture building accepted");
  query = api.QueryWorld();
  for (std::uint32_t i = 0; i < 1600; ++i) {
    const auto command = api.SetBuildingEnabled(query.world.revision, 1, (i % 2) == 0);
    ok &= Check(command.error == economy_api::ErrorCode::None,
                "checkpoint fixture command history queues");
  }
  ok &= Check(api.Step() == economy_api::ErrorCode::None,
              "checkpoint fixture command history applies");

  std::string expected;
  ok &= Check(economy::SaveCheckpoint(world, expected, parseError),
              "capture canonical expected checkpoint text");
  const auto frozenWorld = api.QueryWorld().world;

  economy_host::Dispatcher controller(economy_host::Role::Controller, api, true);
  const auto hello = Response(controller, Request("hello", "hello"));
  const auto sessionId = hello["sessionId"].get<std::string>();
  ok &= Check(hello["ok"] && HasAction(hello, "export_checkpoint"),
              "controller capabilities expose checkpoint export");
  ok &= Check(hello["result"]["limits"]["maxCheckpointBytes"] == economy_host::MaxCheckpointBytes &&
                  hello["result"]["limits"]["maxCheckpointChunkBytes"] == economy_host::MaxCheckpointChunkBytes,
              "hello advertises checkpoint bounds");
  bool advertisedBegin = false, advertisedRead = false;
  for (const auto &method : hello["result"]["methods"]) {
    advertisedBegin |= method["name"] == "begin_checkpoint" &&
        std::find(method["roles"].begin(), method["roles"].end(), "observer") != method["roles"].end();
    advertisedRead |= method["name"] == "read_checkpoint" &&
        std::find(method["roles"].begin(), method["roles"].end(), "observer") != method["roles"].end();
  }
  ok &= Check(advertisedBegin && advertisedRead,
              "checkpoint methods are advertised for observer and controller roles");

  const auto missingSession = Response(controller, Request(
      "missing-session", "begin_checkpoint", Json{{"revision", std::to_string(frozenWorld.revision)}}));
  ok &= Check(!missingSession["ok"] && missingSession["error"]["code"] == "missing_session",
              "embedded checkpoint begin requires the established session");
  const auto badRevisionType = Response(controller, Request(
      "bad-revision-type", "begin_checkpoint", Json{{"revision", frozenWorld.revision}}, sessionId));
  ok &= Check(!badRevisionType["ok"] && badRevisionType["error"]["code"] == "invalid_request",
              "checkpoint revision requires decimal-string encoding");
  const auto extraBeginField = Response(controller, Request(
      "extra-begin-field", "begin_checkpoint",
      Json{{"revision", std::to_string(frozenWorld.revision)}, {"path", "ignored"}}, sessionId));
  ok &= Check(!extraBeginField["ok"] && extraBeginField["error"]["code"] == "invalid_request",
              "checkpoint begin rejects unknown fields");

  const auto begun = Response(controller, Request(
      "begin", "begin_checkpoint", Json{{"revision", std::to_string(frozenWorld.revision)}}, sessionId));
  ok &= Check(begun["ok"] && begun["result"]["errorCode"] == "none" &&
                  begun["result"]["totalBytes"] == expected.size() &&
                  begun["result"]["revision"] == std::to_string(frozenWorld.revision) &&
                  begun["result"]["tick"] == std::to_string(frozenWorld.tick) &&
                  begun["result"]["maxChunkBytes"] == economy_host::MaxCheckpointChunkBytes,
              "checkpoint begin returns immutable snapshot metadata");
  const auto firstId = begun["result"]["checkpointId"].get<std::string>();
  ok &= Check(expected.size() > economy_host::MaxCheckpointChunkBytes,
              "fixture checkpoint exercises multiple bounded reads");
  auto assembled = ReadText(controller, sessionId, firstId, expected.size(), ok);
  ok &= Check(assembled == expected,
              "assembled checkpoint chunks equal the captured SaveCheckpoint output");

  const auto emptyEnd = Response(controller, Request(
      "end-offset", "read_checkpoint",
      Json{{"checkpointId", firstId}, {"offset", expected.size()}, {"limit", 1}}, sessionId));
  ok &= Check(emptyEnd["ok"] && emptyEnd["result"]["errorCode"] == "none" &&
                  emptyEnd["result"]["dataBase64"] == "" &&
                  emptyEnd["result"]["nextOffset"] == expected.size() &&
                  !emptyEnd["result"]["hasMore"],
              "offset equal to checkpoint size returns an empty terminal chunk");

  const auto zeroLimit = Response(controller, Request(
      "zero-limit", "read_checkpoint",
      Json{{"checkpointId", firstId}, {"offset", 0}, {"limit", 0}}, sessionId));
  const auto oversizedLimit = Response(controller, Request(
      "big-limit", "read_checkpoint",
      Json{{"checkpointId", firstId}, {"offset", 0},
           {"limit", economy_host::MaxCheckpointChunkBytes + 1}}, sessionId));
  const auto negativeOffset = Response(controller, Request(
      "negative-offset", "read_checkpoint",
      Json{{"checkpointId", firstId}, {"offset", -1}, {"limit", 1}}, sessionId));
  const auto beyondEnd = Response(controller, Request(
      "beyond-end", "read_checkpoint",
      Json{{"checkpointId", firstId}, {"offset", expected.size() + 1}, {"limit", 1}}, sessionId));
  const auto unknownId = Response(controller, Request(
      "unknown-checkpoint", "read_checkpoint",
      Json{{"checkpointId", "not-this-checkpoint"}, {"offset", 0}, {"limit", 1}}, sessionId));
  const auto extraReadField = Response(controller, Request(
      "extra-read-field", "read_checkpoint",
      Json{{"checkpointId", firstId}, {"offset", 0}, {"limit", 1}, {"extra", true}}, sessionId));
  ok &= Check(!zeroLimit["ok"] && zeroLimit["error"]["code"] == "invalid_request" &&
                  !oversizedLimit["ok"] && oversizedLimit["error"]["code"] == "invalid_request" &&
                  !negativeOffset["ok"] && negativeOffset["error"]["code"] == "invalid_request" &&
                  !beyondEnd["ok"] && beyondEnd["error"]["code"] == "invalid_request" &&
                  !extraReadField["ok"] && extraReadField["error"]["code"] == "invalid_request",
              "checkpoint read bounds and schema are strict");
  ok &= Check(unknownId["ok"] && unknownId["result"]["errorCode"] == "invalid_argument",
              "unknown checkpoint ID is an invalid argument");

  std::string reloadError;
  ok &= Check(api.ReplaceCatalog(catalog, reloadError) == economy_api::ErrorCode::None,
              "same catalog can be reloaded after checkpoint capture");
  auto afterReload = api.QueryWorld();
  const auto pending = api.PlaceBuilding(afterReload.world.revision, "farm", {10, 10});
  ok &= Check(pending.error == economy_api::ErrorCode::None,
              "a pending world change can be queued after checkpoint capture");
  const auto staleBegin = Response(controller, Request(
      "stale-begin", "begin_checkpoint", Json{{"revision", std::to_string(frozenWorld.revision)}}, sessionId));
  ok &= Check(staleBegin["ok"] && staleBegin["result"]["errorCode"] == "stale_revision",
              "stale checkpoint begin is rejected");
  auto afterStale = ReadText(controller, sessionId, firstId, expected.size(), ok);
  ok &= Check(afterStale == expected,
              "failed stale begin preserves the prior cached checkpoint");
  ok &= Check(api.Step() == economy_api::ErrorCode::None,
              "post-capture world change applies");
  auto afterStep = ReadText(controller, sessionId, firstId, expected.size(), ok);
  ok &= Check(afterStep == expected,
              "cached checkpoint remains immutable across reload, pending command, and world step");

  const auto latest = api.QueryWorld();
  std::string newExpected;
  ok &= Check(economy::SaveCheckpoint(world, newExpected, parseError),
              "capture replacement checkpoint expected text");
  const auto replacement = Response(controller, Request(
      "replacement", "begin_checkpoint", Json{{"revision", std::to_string(latest.world.revision)}}, sessionId));
  ok &= Check(replacement["ok"] && replacement["result"]["errorCode"] == "none" &&
                  replacement["result"]["checkpointId"] != firstId,
              "new successful begin replaces the single cached checkpoint");
  const auto replacementId = replacement["result"]["checkpointId"].get<std::string>();
  const auto invalidatedOld = Response(controller, Request(
      "old-invalidated", "read_checkpoint",
      Json{{"checkpointId", firstId}, {"offset", 0}, {"limit", 1}}, sessionId));
  ok &= Check(invalidatedOld["ok"] && invalidatedOld["result"]["errorCode"] == "invalid_argument",
              "successful replacement invalidates the old checkpoint ID");
  auto replacementText = ReadText(controller, sessionId, replacementId, newExpected.size(), ok);
  ok &= Check(replacementText == newExpected,
              "replacement checkpoint assembles to its captured canonical text");

  economy_api::EconomyApi observerApi(world, {true, false});
  economy_host::Dispatcher observer(economy_host::Role::Observer, observerApi, false);
  const auto observerHello = Response(observer, Request("observer-hello", "hello"));
  const auto observerBegin = Response(observer, Request(
      "observer-begin", "begin_checkpoint",
      Json{{"revision", std::to_string(latest.world.revision)}}));
  ok &= Check(observerHello["ok"] && HasAction(observerHello, "export_checkpoint") &&
                  observerBegin["ok"] && observerBegin["result"]["errorCode"] == "none",
              "query-only observers can export a checkpoint");
  const auto observerId = observerBegin["result"]["checkpointId"].get<std::string>();
  const auto observerRead = Response(observer, Request(
      "observer-read", "read_checkpoint",
      Json{{"checkpointId", observerId}, {"offset", 0}, {"limit", 128}}));
  ok &= Check(observerRead["ok"] && observerRead["result"]["errorCode"] == "none",
              "query-only observers can read their checkpoint");

  economy_api::EconomyApi noQueryApi(world, {false, false});
  economy_host::Dispatcher noQuery(economy_host::Role::Observer, noQueryApi, false);
  const auto noQueryHello = Response(noQuery, Request("no-query-hello", "hello"));
  const auto noQueryBegin = Response(noQuery, Request(
      "no-query-begin", "begin_checkpoint", Json{{"revision", std::to_string(latest.world.revision)}}));
  const auto noQueryRead = Response(noQuery, Request(
      "no-query-read", "read_checkpoint",
      Json{{"checkpointId", observerId}, {"offset", 0}, {"limit", 1}}));
  ok &= Check(!HasAction(noQueryHello, "export_checkpoint") &&
                  noQueryBegin["ok"] && noQueryBegin["result"]["errorCode"] == "permission_denied" &&
                  noQueryRead["ok"] && noQueryRead["result"]["errorCode"] == "permission_denied",
              "checkpoint operations require query permission");

  // A failed export at the native byte bound must preserve a previously usable snapshot.
  for (unsigned i = 0; i < 24000; ++i)
    world.SetBuildingEnabled(1, true);
  const auto oversizedHash = world.Snapshot().stateHash;
  const auto tooLarge = Response(controller, Request(
      "oversized-export", "begin_checkpoint", Json{{"revision", std::to_string(world.Revision())}}, sessionId));
  ok &= Check(tooLarge["ok"] && tooLarge["result"]["errorCode"] == "persistence_failed" &&
                  !tooLarge["result"].value("message", std::string()).empty() &&
                  world.Snapshot().stateHash == oversizedHash,
              "oversized export fails explicitly without changing pending state");
  ok &= Check(ReadText(controller, sessionId, replacementId, newExpected.size(), ok) == newExpected,
              "failed oversized export retains the previous cached snapshot");

  if (ok)
    std::cout << "Checkpoint protocol contracts passed\n";
  return ok ? 0 : 1;
}
