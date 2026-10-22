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

std::string CheckpointText(economy_host::Dispatcher &dispatcher, const std::string &session,
                           const std::string &checkpointId, std::uint64_t totalBytes, bool &ok) {
  std::string text;
  std::uint64_t offset = 0;
  while (offset < totalBytes) {
    const auto read = Response(dispatcher, Request(
        "read-history-checkpoint", "read_checkpoint",
        Json{{"checkpointId", checkpointId}, {"offset", offset}, {"limit", 16384}}, session));
    const auto &result = read["result"];
    ok &= Check(read["ok"] && result["errorCode"] == "none" &&
                    result["offset"] == offset && result["checkpointId"] == checkpointId,
                "frozen checkpoint chunk is readable after history compaction");
    if (!read["ok"] || result.value("errorCode", std::string()) != "none")
      break;
    const auto bytes = result["dataBase64"].get<std::string>();
    // The fixture uses ASCII catalog/checkpoint data. Decode padded Base64 without
    // coupling this protocol contract to the checkpoint client's implementation.
    auto value = [](char c) -> int {
      static constexpr char chars[] =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
      const auto *found = std::find(chars, chars + 64, c);
      return found == chars + 64 ? -1 : static_cast<int>(found - chars);
    };
    if (bytes.size() % 4 != 0) {
      ok &= Check(false, "checkpoint chunk is padded Base64");
      break;
    }
    std::string decoded;
    for (std::size_t i = 0; i < bytes.size(); i += 4) {
      const int a = value(bytes[i]), b = value(bytes[i + 1]);
      const int c = bytes[i + 2] == '=' ? 0 : value(bytes[i + 2]);
      const int d = bytes[i + 3] == '=' ? 0 : value(bytes[i + 3]);
      if (a < 0 || b < 0 || c < 0 || d < 0 ||
          (bytes[i + 2] == '=' && bytes[i + 3] != '=')) {
        ok &= Check(false, "checkpoint chunk has valid Base64 alphabet and padding");
        break;
      }
      const auto block = (static_cast<std::uint32_t>(a) << 18) |
                         (static_cast<std::uint32_t>(b) << 12) |
                         (static_cast<std::uint32_t>(c) << 6) |
                         static_cast<std::uint32_t>(d);
      decoded.push_back(static_cast<char>((block >> 16) & 0xff));
      if (bytes[i + 2] != '=')
        decoded.push_back(static_cast<char>((block >> 8) & 0xff));
      if (bytes[i + 3] != '=')
        decoded.push_back(static_cast<char>(block & 0xff));
    }
    text += decoded;
    offset = result["nextOffset"].get<std::uint64_t>();
    ok &= Check(offset <= totalBytes, "checkpoint chunk offset remains within snapshot");
  }
  return text;
}

bool HasAction(const Json &hello, const char *name) {
  const auto &actions = hello["result"]["actions"];
  return std::find(actions.begin(), actions.end(), name) != actions.end();
}
} // namespace

int main() {
  economy::CatalogBuilder builder;
  economy::Catalog catalog;
  economy::CatalogErrorInfo catalogError;
  economy::ResourceId grain{};
  economy::BuildingTypeId farm{};
  if (!builder.AddResource("grain", &grain, catalogError) ||
      !builder.AddBuildingType("farm", 1, 1, &farm, catalogError) ||
      !builder.AddStorage(farm, grain, 20, catalogError) ||
      !builder.AddRecipe(farm, {"harvest", 1, {}, {{grain, 1}}}, catalogError) ||
      !builder.Build(catalog, catalogError)) {
    std::cerr << "FAIL: construct command history fixture catalog: " << catalogError.message << '\n';
    return 1;
  }

  economy::World world(catalog, 16, 16);
  economy_api::EconomyApi api(world, {true, true});
  economy_host::Dispatcher controller(economy_host::Role::Controller, api, true);
  bool ok = true;
  const auto hello = Response(controller, Request("hello", "hello"));
  const auto session = hello["sessionId"].get<std::string>();
  bool advertisesCompaction = false;
  for (const auto &method : hello["result"]["methods"])
    advertisesCompaction |= method["name"] == "compact_history" &&
        std::find(method["roles"].begin(), method["roles"].end(), "controller") != method["roles"].end() &&
        std::find(method["roles"].begin(), method["roles"].end(), "observer") == method["roles"].end();
  ok &= Check(hello["ok"] && HasAction(hello, "compact_command_history") && advertisesCompaction,
              "hello advertises compaction as controller-only action and method");
  ok &= Check(hello["result"]["commandHistoryVersion"] == 1 &&
                  hello["result"]["commandHistoryPolicy"] == "explicit" &&
                  hello["result"]["limits"]["maxCommandHistoryRetention"] ==
                      economy::MaxCommandHistoryRetention,
              "hello advertises versioned explicit policy and retention bound");

  const auto initial = Response(controller, Request("initial", "world", {}, session))["result"]["world"];
  ok &= Check(initial["commandHistory"] == Json{{"version", 1},
                  {"firstRetainedCommandId", "1"}, {"nextCommandId", "1"},
                  {"retainedTerminalResults", "0"}, {"pendingCommands", "0"}},
              "world exposes versioned command history counters as decimal strings");

  const auto missingSession = Response(controller, Request(
      "no-session", "compact_history", Json{{"revision", "0"}, {"keepRecent", 0}}));
  ok &= Check(!missingSession["ok"] && missingSession["error"]["code"] == "missing_session",
              "session-bound compaction requires hello session");
  const auto wrongSession = Response(controller, Request(
      "wrong-session", "compact_history", Json{{"revision", "0"}, {"keepRecent", 0}}, "other-session"));
  ok &= Check(!wrongSession["ok"] && wrongSession["error"]["code"] == "stale_session",
              "compaction rejects another process session");

  auto invalid = [&](const char *id, Json fields) {
    return Response(controller, Request(id, "compact_history", fields, session));
  };
  const auto missing = invalid("missing-keep", Json{{"revision", "0"}});
  const auto stringKeep = invalid("string-keep", Json{{"revision", "0"}, {"keepRecent", "0"}});
  const auto fractional = invalid("fractional-keep", Json{{"revision", "0"}, {"keepRecent", 0.5}});
  const auto negative = invalid("negative-keep", Json{{"revision", "0"}, {"keepRecent", -1}});
  const auto tooMany = invalid("too-many", Json{{"revision", "0"},
      {"keepRecent", economy::MaxCommandHistoryRetention + 1}});
  const auto extra = invalid("extra", Json{{"revision", "0"}, {"keepRecent", 0}, {"force", true}});
  const auto numericRevision = invalid("numeric-revision", Json{{"revision", 0}, {"keepRecent", 0}});
  ok &= Check(!missing["ok"] && missing["error"]["code"] == "invalid_request" &&
                  !stringKeep["ok"] && stringKeep["error"]["code"] == "invalid_request" &&
                  !fractional["ok"] && fractional["error"]["code"] == "invalid_request" &&
                  !negative["ok"] && negative["error"]["code"] == "invalid_request" &&
                  !tooMany["ok"] && tooMany["error"]["code"] == "invalid_request" &&
                  !extra["ok"] && extra["error"]["code"] == "invalid_request" &&
                  !numericRevision["ok"] && numericRevision["error"]["code"] == "invalid_request",
              "compaction rejects missing, extra, malformed, negative, and out-of-bound fields");

  const auto placement = api.PlaceBuilding(0, "farm", {4, 4});
  ok &= Check(placement.error == economy_api::ErrorCode::None &&
                  api.Step() == economy_api::ErrorCode::None,
              "fixture creates one terminal command result");
  const auto afterPlacement = api.QueryWorld().world;
  for (std::uint32_t i = 0; i < 2; ++i)
    ok &= Check(api.SetBuildingEnabled(afterPlacement.revision, 1, i != 0).error ==
                    economy_api::ErrorCode::None,
                "fixture queues pending commands to retain");

  std::string checkpointBefore;
  economy::ParseError parseError;
  ok &= Check(economy::SaveCheckpoint(world, checkpointBefore, parseError),
              "fixture captures pre-compaction v2 checkpoint");
  const auto preCompactionWorld = api.QueryWorld().world;
  ok &= Check(preCompactionWorld.commandHistory.retainedTerminalResults == 1 &&
                  preCompactionWorld.commandHistory.pendingCommands == 2,
              "fixture has both a completed result and pending commands");
  const auto begun = Response(controller, Request("begin-before", "begin_checkpoint",
      Json{{"revision", std::to_string(preCompactionWorld.revision)}}, session));
  ok &= Check(begun["ok"] && begun["result"]["errorCode"] == "none",
              "checkpoint snapshot starts before compaction");
  const auto frozenId = begun["result"]["checkpointId"].get<std::string>();

  const auto stale = invalid("stale", Json{{"revision", std::to_string(preCompactionWorld.revision - 1)},
                                             {"keepRecent", 0}});
  ok &= Check(stale["ok"] && stale["result"]["errorCode"] == "stale_revision",
              "compaction checks the exact expected revision");
  const auto compacted = invalid("compact", Json{{"revision", std::to_string(preCompactionWorld.revision)},
                                                  {"keepRecent", 0}});
  const auto &compaction = compacted["result"];
  ok &= Check(compacted["ok"] && compaction["errorCode"] == "none" &&
                  compaction["removedResults"] == "1" && compaction["expiredHandles"] == "1" &&
                  compaction["revision"] == std::to_string(preCompactionWorld.revision + 1),
              "compaction reports pruned results, expired API handles, and new revision");
  ok &= Check(compaction["commandHistory"] == Json{{"version", 1},
                  {"firstRetainedCommandId", "2"}, {"nextCommandId", "4"},
                  {"retainedTerminalResults", "0"}, {"pendingCommands", "2"}},
              "compaction keeps pending commands and publishes updated metadata");

  const auto expired = Response(controller, Request("expired", "poll", Json{{"handle", "1"}}, session));
  const auto pending = Response(controller, Request("pending", "poll", Json{{"handle", "2"}}, session));
  const auto unknown = Response(controller, Request("unknown", "poll", Json{{"handle", "999"}}, session));
  ok &= Check(expired["ok"] && expired["result"]["errorCode"] == "command_expired" &&
                  expired["result"]["state"] == "expired",
              "poll distinguishes expired handles from other API errors");
  ok &= Check(pending["ok"] && pending["result"]["errorCode"] == "none" &&
                  pending["result"]["state"] == "queued",
              "pending handle remains pollable after result compaction");
  ok &= Check(unknown["ok"] && unknown["result"]["errorCode"] == "unknown_command" &&
                  unknown["result"]["state"] == "unknown",
              "poll distinguishes an unknown handle from an expired handle");

  const auto frozen = CheckpointText(controller, session, frozenId,
      begun["result"]["totalBytes"].get<std::uint64_t>(), ok);
  ok &= Check(frozen == checkpointBefore,
              "compaction preserves the previously begun checkpoint byte-for-byte");

  const auto newWorld = api.QueryWorld().world;
  std::string compactedCheckpoint;
  ok &= Check(economy::SaveCheckpoint(world, compactedCheckpoint, parseError),
              "compacted world exports a new native checkpoint");
  ok &= Check(compactedCheckpoint.find("OPENREPUBLIC_ECONOMY_WORLD 3\n") != std::string::npos,
              "compacted native world uses checkpoint-compatible world format version 3");
  const auto newBegin = Response(controller, Request("begin-after", "begin_checkpoint",
      Json{{"revision", std::to_string(newWorld.revision)}}, session));
  ok &= Check(newBegin["ok"] && newBegin["result"]["errorCode"] == "none",
              "new checkpoint begins from compacted revision");
  const auto compactedId = newBegin["result"]["checkpointId"].get<std::string>();
  const auto invalidatedCache = Response(controller, Request("old-cache-after-new-begin", "read_checkpoint",
      Json{{"checkpointId", frozenId}, {"offset", 0}, {"limit", 1}}, session));
  ok &= Check(invalidatedCache["ok"] && invalidatedCache["result"]["errorCode"] == "invalid_argument",
              "new checkpoint begin replaces the one-entry immutable cache");
  const auto newText = CheckpointText(controller, session, compactedId,
      newBegin["result"]["totalBytes"].get<std::uint64_t>(), ok);
  ok &= Check(newText == compactedCheckpoint,
              "new checkpoint cache captures compacted world bytes");
  economy::RestoredCheckpoint restored;
  ok &= Check(economy::RestoreCheckpoint(newText, restored, parseError),
              "new checkpoint restores after compaction");
  if (restored.world) {
    const auto restoredHistory = restored.world->CommandHistory();
    ok &= Check(restoredHistory.firstRetainedCommandId == 2 &&
                    restoredHistory.nextCommandId == 4 && restoredHistory.pendingCommands == 2,
                "restored v3 world retains compacted command floor and pending commands");
  } else {
    ok &= Check(false, "restored checkpoint owns a world");
  }
  const auto maxRetention = invalid("max-retention", Json{{"revision", std::to_string(newWorld.revision)},
      {"keepRecent", economy::MaxCommandHistoryRetention}});
  ok &= Check(maxRetention["ok"] && maxRetention["result"]["errorCode"] == "none" &&
                  maxRetention["result"]["revision"] == std::to_string(newWorld.revision),
              "maximum advertised retention is accepted when no results need removal");

  economy::World observerWorld(catalog, 16, 16);
  economy_api::EconomyApi observerApi(observerWorld, {true, false});
  economy_host::Dispatcher observer(economy_host::Role::Observer, observerApi, true);
  const auto observerHello = Response(observer, Request("observer-hello", "hello"));
  const auto observerSession = observerHello["sessionId"].get<std::string>();
  bool observerListedAsAllowed = false;
  for (const auto &method : observerHello["result"]["methods"])
    observerListedAsAllowed |= method["name"] == "compact_history" &&
        std::find(method["roles"].begin(), method["roles"].end(), "observer") != method["roles"].end();
  const auto denied = Response(observer, Request("observer-compact", "compact_history",
      Json{{"revision", "0"}, {"keepRecent", 0}}, observerSession));
  ok &= Check(!observerListedAsAllowed && !HasAction(observerHello, "compact_command_history") && denied["ok"] &&
                  denied["result"]["errorCode"] == "permission_denied",
              "observer role is excluded from compaction and execution is denied");

  if (ok)
    std::cout << "Command history protocol contracts passed\n";
  return ok ? 0 : 1;
}
