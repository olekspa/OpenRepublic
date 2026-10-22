#include "economy_protocol.h"

#include "json.hpp"

#include <cstdlib>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <tuple>
#include <map>
#include <charconv>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <unordered_set>
#include <vector>

namespace openrepublic::economy_host {
namespace {
using Json = nlohmann::json;
using namespace openrepublic;

std::string NewSessionId() {
  static std::atomic<std::uint64_t> nextId{1};
  const auto started = std::chrono::steady_clock::now().time_since_epoch().count();
  std::uint64_t randomBits = 0;
  try {
    std::random_device random;
    randomBits = (static_cast<std::uint64_t>(random()) << 32) | random();
  } catch (...) {
    // Keep host startup available if the standard library has no random source.
  }
  return std::to_string(started) + "-" + std::to_string(randomBits) + "-" +
         std::to_string(nextId.fetch_add(1, std::memory_order_relaxed));
}

struct ParseState {
  std::size_t nodes{};
  bool deep{}, many{}, duplicate{};
  std::vector<std::unordered_set<std::string>> keys;
};

bool Parse(std::string_view line, Json &json, std::string &code, std::string &message) {
  if (line.size() > MaxRequestLineBytes) {
    code = "request_too_large";
    message = "request line exceeds the configured byte limit";
    return false;
  }
  bool string = false, escaped = false;
  std::size_t depth = 0;
  for (const char ch : line) {
    if (string) {
      if (escaped)
        escaped = false;
      else if (ch == '\\')
        escaped = true;
      else if (ch == '"')
        string = false;
    } else if (ch == '"')
      string = true;
    else if (ch == '{' || ch == '[') {
      if (++depth > MaxJsonNesting) {
        code = "json_too_deep";
        message = "JSON nesting exceeds the configured depth limit";
        return false;
      }
    } else if ((ch == '}' || ch == ']') && depth != 0)
      --depth;
  }
  ParseState state;
  const auto callback = [&state](int level, Json::parse_event_t event, Json &value) {
    if (level > static_cast<int>(MaxJsonNesting))
      state.deep = true;
    if (++state.nodes > MaxJsonNodes)
      state.many = true;
    if (event == Json::parse_event_t::object_start)
      state.keys.emplace_back();
    else if (event == Json::parse_event_t::key && !state.keys.empty()) {
      if (!state.keys.back().insert(value.get<std::string>()).second)
        state.duplicate = true;
    } else if (event == Json::parse_event_t::object_end && !state.keys.empty())
      state.keys.pop_back();
    return true;
  };
  try {
    json = Json::parse(line.begin(), line.end(), callback, true, false);
  } catch (const std::exception &) {
    code = "malformed_json";
    message = "request line is not valid JSON";
    return false;
  }
  if (state.deep) {
    code = "json_too_deep";
    message = "JSON nesting exceeds the configured depth limit";
    return false;
  }
  if (state.many) {
    code = "json_too_complex";
    message = "JSON structure exceeds the configured node limit";
    return false;
  }
  if (state.duplicate) {
    code = "duplicate_json_key";
    message = "duplicate object keys are not allowed";
    return false;
  }
  return true;
}

bool Fields(const Json &object, std::initializer_list<const char *> names, std::string &error) {
  if (!object.is_object()) {
    error = "request must be a JSON object";
    return false;
  }
  std::set<std::string> allowed{"id", "protocolVersion", "method", "sessionId"};
  for (const auto *name : names)
    allowed.emplace(name);
  for (auto it = object.begin(); it != object.end(); ++it)
    if (!allowed.count(it.key())) {
      error = "unknown request field: " + it.key();
      return false;
    }
  for (const auto *name : names)
    if (!object.contains(name)) {
      error = std::string("missing request field: ") + name;
      return false;
    }
  return true;
}

bool NestedFields(const Json &object, std::initializer_list<const char *> names, std::string &error) {
  if (!object.is_object()) {
    error = "command must be a JSON object";
    return false;
  }
  std::set<std::string> allowed;
  for (const auto *name : names)
    allowed.emplace(name);
  for (auto it = object.begin(); it != object.end(); ++it)
    if (!allowed.count(it.key())) {
      error = "unknown command field: " + it.key();
      return false;
    }
  for (const auto *name : names)
    if (!object.contains(name)) {
      error = std::string("missing command field: ") + name;
      return false;
    }
  return true;
}

bool String(const Json &obj, const char *key, std::string &out, std::string &err) {
  if (!obj.contains(key) || !obj[key].is_string()) {
    err = std::string("field must be a string: ") + key;
    return false;
  }
  out = obj[key].get<std::string>();
  return true;
}
bool U64(const Json &obj, const char *key, std::uint64_t &out, std::string &err) {
  std::string token;
  if (!String(obj, key, token, err))
    return false;
  if (token.empty() || token.size() > 20 || token.find_first_not_of("0123456789") != std::string::npos) {
    err = std::string("field must be an unsigned decimal string: ") + key;
    return false;
  }
  const auto result = std::from_chars(token.data(), token.data() + token.size(), out);
  if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) {
    err = std::string("field is outside uint64 range: ") + key;
    return false;
  }
  return true;
}
bool Unsigned(const Json &obj, const char *key, std::uint64_t max, std::uint64_t &out, std::string &err) {
  if (!obj.contains(key) || !obj[key].is_number_integer()) {
    err = std::string("field must be an integer: ") + key;
    return false;
  }
  if (obj[key].is_number_unsigned())
    out = obj[key].get<std::uint64_t>();
  else {
    const auto n = obj[key].get<std::int64_t>();
    if (n < 0) {
      err = "field must be non-negative: " + std::string(key);
      return false;
    }
    out = static_cast<std::uint64_t>(n);
  }
  if (out > max) {
    err = "field exceeds permitted maximum: " + std::string(key);
    return false;
  }
  return true;
}
bool Int32(const Json &obj, const char *key, std::int32_t &out, std::string &err) {
  if (!obj.contains(key) || !obj[key].is_number_integer()) {
    err = std::string("field must be an integer: ") + key;
    return false;
  }
  if (obj[key].is_number_unsigned()) {
    const auto n = obj[key].get<std::uint64_t>();
    if (n > static_cast<std::uint64_t>(INT32_MAX)) {
      err = "int32 out of range";
      return false;
    }
    out = static_cast<std::int32_t>(n);
  } else {
    const auto n = obj[key].get<std::int64_t>();
    if (n < INT32_MIN || n > INT32_MAX) {
      err = "int32 out of range";
      return false;
    }
    out = static_cast<std::int32_t>(n);
  }
  return true;
}

std::string Error(economy_api::ErrorCode e) {
  using E = economy_api::ErrorCode;
  switch (e) {
  case E::None:
    return "none";
  case E::PermissionDenied:
    return "permission_denied";
  case E::StaleRevision:
    return "stale_revision";
  case E::InvalidArgument:
    return "invalid_argument";
  case E::UnknownCommand:
    return "unknown_command";
  case E::InvalidPageLimit:
    return "invalid_page_limit";
  case E::InvalidEventLimit:
    return "invalid_event_limit";
  case E::FutureEventCursor:
    return "future_event_cursor";
  case E::StepLimitExceeded:
    return "step_limit_exceeded";
  case E::CommandHandleExhausted:
    return "command_handle_exhausted";
  }
  return "unknown_api_error";
}
std::string Failure(economy::CommandFailure e) {
  using F = economy::CommandFailure;
  switch (e) {
  case F::None:
    return "none";
  case F::InvalidCommand:
    return "invalid_command";
  case F::UnknownBuildingType:
    return "unknown_building_type";
  case F::UnknownBuilding:
    return "unknown_building";
  case F::UnknownResource:
    return "unknown_resource";
  case F::UnknownVehicleType:
    return "unknown_vehicle_type";
  case F::UnknownRoad:
    return "unknown_road";
  case F::UnknownOrder:
    return "unknown_order";
  case F::OutOfBounds:
    return "out_of_bounds";
  case F::InvalidRoadGeometry:
    return "invalid_road_geometry";
  case F::DuplicateRoad:
    return "duplicate_road";
  case F::RoadOnBuilding:
    return "road_on_building";
  case F::BuildingCollision:
    return "building_collision";
  case F::AccessCellUnavailable:
    return "access_cell_unavailable";
  case F::ReferencedByOrder:
    return "referenced_by_order";
  case F::DuplicateOrder:
    return "duplicate_order";
  case F::InvalidOrderEndpoint:
    return "invalid_order_endpoint";
  case F::NoRoute:
    return "no_route";
  case F::SourceEmpty:
    return "source_empty";
  case F::DestinationFull:
    return "destination_full";
  case F::RoadDisabled:
    return "road_disabled";
  case F::StepLimitExceeded:
    return "step_limit_exceeded";
  case F::SaveInvalid:
    return "save_invalid";
  case F::ParseInvalid:
    return "parse_invalid";
  }
  return "unknown_failure";
}
std::string State(economy::CommandState s) {
  switch (s) {
  case economy::CommandState::Queued:
    return "queued";
  case economy::CommandState::Accepted:
    return "accepted";
  case economy::CommandState::Rejected:
    return "rejected";
  default:
    return "unknown";
  }
}
std::string Stall(economy::StallReason s) {
  switch (s) {
  case economy::StallReason::Running:
    return "running";
  case economy::StallReason::MissingInput:
    return "missing_input";
  case economy::StallReason::OutputFull:
    return "output_full";
  case economy::StallReason::Disabled:
    return "disabled";
  }
  return "unknown";
}
std::string HaulFailure(economy::HaulFailure failure) {
  switch (failure) {
  case economy::HaulFailure::None:
    return "none";
  case economy::HaulFailure::NoRoute:
    return "no_route";
  case economy::HaulFailure::SourceEmpty:
    return "source_empty";
  case economy::HaulFailure::DestinationFull:
    return "destination_full";
  case economy::HaulFailure::RoadDisabled:
    return "road_disabled";
  }
  return "unknown";
}
std::string Haul(economy::HaulState s) {
  switch (s) {
  case economy::HaulState::Idle:
    return "idle";
  case economy::HaulState::ToDestination:
    return "to_destination";
  case economy::HaulState::ToSource:
    return "to_source";
  case economy::HaulState::Waiting:
    return "waiting";
  case economy::HaulState::NoRoute:
    return "no_route";
  }
  return "unknown";
}
std::string EventType(economy_api::EventType t) {
  using T = economy_api::EventType;
  switch (t) {
  case T::CommandAccepted:
    return "command_accepted";
  case T::CommandRejected:
    return "command_rejected";
  case T::BuildingStalled:
    return "building_stalled";
  case T::BuildingUnstalled:
    return "building_unstalled";
  case T::DeliveryCompleted:
    return "delivery_completed";
  case T::TickAdvanced:
    return "tick_advanced";
  }
  return "unknown";
}
std::string Action(economy_api::Action action) {
  using A = economy_api::Action;
  switch (action) {
  case A::QueryWorld:
    return "query_world";
  case A::QueryCatalog:
    return "query_catalog";
  case A::ExplainBuilding:
    return "explain_building";
  case A::Step:
    return "step";
  case A::PlaceBuilding:
    return "place_building";
  case A::RemoveBuilding:
    return "remove_building";
  case A::BuildRoad:
    return "build_road";
  case A::SetRoadEnabled:
    return "set_road_enabled";
  case A::SetBuildingEnabled:
    return "set_building_enabled";
  case A::CreateHaulOrder:
    return "create_haul_order";
  case A::CancelHaulOrder:
    return "cancel_haul_order";
  case A::PollCommand:
    return "poll_command";
  case A::ReadEvents:
    return "read_events";
  }
  return "unknown";
}
Json Cell(economy::Cell c) {
  return Json{{"x", c.x}, {"z", c.z}};
}
Json Catalog(const economy_api::CatalogView &c) {
  Json resources = Json::array(), buildings = Json::array(), vehicles = Json::array();
  for (const auto &r : c.resources)
    resources.push_back(Json{{"id", r.id}, {"name", r.name}});
  for (const auto &b : c.buildingTypes) {
    Json storage = Json::array(), recipes = Json::array();
    for (const auto &s : b.storage)
      storage.push_back(Json{{"resource", s.resource}, {"capacity", std::to_string(s.capacity)}});
    for (const auto &r : b.recipes) {
      Json inputs = Json::array(), outputs = Json::array();
      for (const auto &a : r.inputs)
        inputs.push_back(Json{{"resource", a.resource}, {"amount", std::to_string(a.amount)}});
      for (const auto &a : r.outputs)
        outputs.push_back(Json{{"resource", a.resource}, {"amount", std::to_string(a.amount)}});
      recipes.push_back(Json{
          {"name", r.name}, {"intervalTicks", r.intervalTicks}, {"inputs", inputs}, {"outputs", outputs}});
    }
    buildings.push_back(Json{{"id", b.id},
                             {"name", b.name},
                             {"width", b.width},
                             {"depth", b.depth},
                             {"storage", storage},
                             {"recipes", recipes}});
  }
  for (const auto &v : c.vehicleTypes)
    vehicles.push_back(Json{{"id", v.id},
                            {"name", v.name},
                            {"capacity", std::to_string(v.capacity)},
                            {"cellsPerTick", v.cellsPerTick}});
  return Json{{"errorCode", Error(c.error)},
              {"apiVersion", c.apiVersion},
              {"resources", resources},
              {"buildingTypes", buildings},
              {"vehicleTypes", vehicles}};
}
Json World(const economy_api::WorldView &w) {
  Json buildings = Json::array(), roads = Json::array(), vehicles = Json::array(), orders = Json::array();
  for (const auto &b : w.buildings) {
    Json inventory = Json::array();
    for (const auto &i : b.inventory)
      inventory.push_back(Json{{"resource", i.resource},
                               {"amount", std::to_string(i.amount)},
                               {"capacity", std::to_string(i.capacity)}});
    buildings.push_back(Json{{"id", b.id},
                             {"type", b.type},
                             {"cell", Cell(b.cell)},
                             {"enabled", b.enabled},
                             {"inventory", inventory},
                             {"stallReason", Stall(b.stallReason)},
                             {"stallExplanation", b.stallExplanation}});
  }
  for (const auto &r : w.roads)
    roads.push_back(Json{{"id", r.id}, {"a", Cell(r.a)}, {"b", Cell(r.b)}, {"enabled", r.enabled}});
  for (const auto &v : w.vehicles)
    vehicles.push_back(Json{{"id", v.id},
                            {"orderId", v.order},
                            {"type", v.type},
                            {"cell", Cell(v.cell)},
                            {"cargo", std::to_string(v.cargo)},
                            {"state", Haul(v.state)}});
  for (const auto &o : w.orders)
    orders.push_back(Json{{"id", o.id},
                          {"source", o.source},
                          {"destination", o.destination},
                          {"resource", o.resource},
                          {"vehicleType", o.vehicleType},
                          {"vehicleId", o.vehicle},
                          {"state", Haul(o.state)},
                          {"failure", HaulFailure(o.failure)},
                          {"cargo", std::to_string(o.cargo)},
                          {"vehicleCell", Cell(o.vehicleCell)}});
  return Json{{"apiVersion", w.apiVersion},     {"revision", std::to_string(w.revision)},
              {"tick", std::to_string(w.tick)}, {"stateHash", std::to_string(w.stateHash)},
              {"buildings", buildings},         {"roads", roads},
              {"vehicles", vehicles},           {"orders", orders}};
}
Json Result(const Json &id, Json value) {
  return Json{{"id", id}, {"protocolVersion", ProtocolVersion}, {"ok", true}, {"result", value}};
}
Json ProtocolError(const Json &id, std::string code, std::string message) {
  return Json{{"id", id},
              {"protocolVersion", ProtocolVersion},
              {"ok", false},
              {"error", Json{{"code", code}, {"message", message}}}};
}
} // namespace

Dispatcher::Dispatcher(Role role, const economy::Catalog &catalog)
    : ownedApi_(std::make_unique<economy_api::EconomyApi>(
          catalog, 64, 64, economy_api::Permissions{true, role == Role::Controller})),
      api_(ownedApi_.get()), role_(role), sessionId_(NewSessionId()) {}

Dispatcher::Dispatcher(Role role, economy::World &world)
    : ownedApi_(std::make_unique<economy_api::EconomyApi>(
          world, economy_api::Permissions{true, role == Role::Controller})),
      api_(ownedApi_.get()), role_(role), sessionId_(NewSessionId()) {}

Dispatcher::Dispatcher(Role role, economy_api::EconomyApi &api, bool requireSessionBinding, bool allowShutdown)
    : api_(&api), role_(role), requireSessionBinding_(requireSessionBinding),
      allowShutdown_(allowShutdown), sessionId_(NewSessionId()) {}

void Dispatcher::SetRegionDescription(std::string description) {
  regionDescription_ = std::move(description);
}

void Dispatcher::SetRulesBuilder(RulesBuilder builder) {
  rulesBuilder_ = std::move(builder);
}
void Dispatcher::SetRegionInfoJson(std::string json) {
  regionInfoJson_ = std::move(json);
}

bool Dispatcher::ShutdownRequested() const noexcept {
  return shutdown_;
}

std::string Dispatcher::HandleLine(std::string_view line) {
  const auto tagged = [this](Json response) {
    response["sessionId"] = sessionId_;
    return response.dump();
  };
  Json req;
  std::string code, message;
  if (!Parse(line, req, code, message))
    return tagged(ProtocolError(nullptr, code, message));
  Json id = nullptr;
  if (!req.is_object() || !req.contains("id") || !req["id"].is_string() ||
      req["id"].get<std::string>().empty() || req["id"].get<std::string>().size() > 128)
    return tagged(ProtocolError(nullptr, "invalid_request", "id must be a non-empty string of at most 128 bytes"));
  id = req["id"];
  if (!req.contains("protocolVersion") || !req["protocolVersion"].is_number_integer())
    return tagged(ProtocolError(id, "invalid_request", "protocolVersion must be a non-negative integer"));
  bool versionIsZero = false;
  if (req["protocolVersion"].is_number_unsigned())
    versionIsZero = req["protocolVersion"].get<std::uint64_t>() == ProtocolVersion;
  else {
    const auto version = req["protocolVersion"].get<std::int64_t>();
    if (version < 0)
      return tagged(ProtocolError(id, "invalid_request", "protocolVersion must be non-negative"));
    versionIsZero = static_cast<std::uint64_t>(version) == ProtocolVersion;
  }
  if (!versionIsZero)
    return tagged(ProtocolError(id, "unsupported_protocol_version", "unsupported protocolVersion"));
  if (!req.contains("method") || !req["method"].is_string())
    return tagged(ProtocolError(id, "invalid_request", "method must be a string"));
  const std::string method = req["method"].get<std::string>();
  if (requireSessionBinding_ && method != "hello" && method != "capabilities" && !req.contains("sessionId"))
    return tagged(ProtocolError(id, "missing_session", "request must include sessionId returned by hello"));
  const auto invalid = [&id, &tagged](const std::string &err) {
    return tagged(ProtocolError(id, "invalid_request", err));
  };
  if (req.contains("sessionId")) {
    if (!req["sessionId"].is_string() || req["sessionId"].get<std::string>().empty())
      return invalid("sessionId must be a non-empty string when supplied");
    if (req["sessionId"].get<std::string>() != sessionId_)
      return tagged(ProtocolError(id, "stale_session", "request belongs to a different host session"));
  }
  Json out;
  if (method == "hello" || method == "capabilities") {
    if (!Fields(req, {}, message))
      return invalid(message);
    const Json roles = Json::array({"observer", "controller"}), control = Json::array({"controller"});
    Json methods = Json::array();
    for (const char *name : {"hello", "capabilities", "catalog", "world", "explain", "advise", "poll", "events"})
      methods.push_back(Json{{"name", name}, {"roles", roles}});
    for (const char *name : {"submit", "step"})
      methods.push_back(Json{{"name", name}, {"roles", control}});
    if (allowShutdown_)
      methods.push_back(Json{{"name", "shutdown"}, {"roles", control}});
    Json actions = Json::array();
    for (const auto action : api_->DiscoverCapabilities().actions)
      actions.push_back(Action(action));
    out = Json{{"apiVersion", economy_api::ApiVersion},
               {"sessionId", sessionId_},
               {"role", role_ == Role::Controller ? "controller" : "observer"},
               {"actions", actions},
               {"methods", methods},
               {"limits", Json{{"maxRequestLineBytes", MaxRequestLineBytes},
                               {"maxRequestsPerProcess", MaxRequestsPerProcess},
                               {"maxJsonNesting", MaxJsonNesting},
                               {"maxJsonNodes", MaxJsonNodes},
                               {"maxStepTicks", economy_api::MaxStepTicks},
                               {"maxEntityPageSize", economy_api::MaxEntityPageSize},
                               {"maxEventPageSize", economy_api::MaxEventPageSize}}},
               {"uint64Encoding", "decimal_string"}};
    if (!regionDescription_.empty())
      out["region"] = regionDescription_;
    if (!regionInfoJson_.empty()) {
      Json info = Json::parse(regionInfoJson_, nullptr, false);
      if (!info.is_discarded())
        out["regionInfo"] = info;
    }
  } else if (method == "catalog") {
    if (!Fields(req, {}, message))
      return invalid(message);
    out = Catalog(api_->DescribeCatalog());
  } else if (method == "world") {
    const bool paged =
        req.contains("revision") || req.contains("kind") || req.contains("offset") || req.contains("limit");
    if (!paged) {
      if (!Fields(req, {}, message))
        return invalid(message);
      const auto q = api_->QueryWorld();
      out = Json{{"errorCode", Error(q.error)}, {"world", World(q.world)}};
    } else {
      if (!Fields(req, {"revision", "kind", "offset", "limit"}, message))
        return invalid(message);
      std::uint64_t revision{}, offset{}, limit{};
      std::string kind;
      if (!U64(req, "revision", revision, message) || !String(req, "kind", kind, message) ||
          !Unsigned(req, "offset", UINT32_MAX, offset, message) ||
          !Unsigned(req, "limit", economy_api::MaxEntityPageSize, limit, message))
        return invalid(message);
      const auto q =
          api_->QueryWorld(revision, kind, static_cast<std::size_t>(offset), static_cast<std::size_t>(limit));
      if (q.error != economy_api::ErrorCode::None) {
        out = Json{{"errorCode", Error(q.error)}};
      } else {
        Json entities = Json::array();
        for (const auto &b : q.page.buildings) {
          Json inventory = Json::array();
          for (const auto &item : b.inventory)
            inventory.push_back(Json{{"resource", item.resource},
                                     {"amount", std::to_string(item.amount)},
                                     {"capacity", std::to_string(item.capacity)}});
          entities.push_back(Json{{"id", b.id},
                                  {"type", b.type},
                                  {"cell", Cell(b.cell)},
                                  {"enabled", b.enabled},
                                  {"inventory", inventory},
                                  {"stallReason", Stall(b.stallReason)},
                                  {"stallExplanation", b.stallExplanation}});
        }
        for (const auto &road : q.page.roads)
          entities.push_back(
              Json{{"id", road.id}, {"a", Cell(road.a)}, {"b", Cell(road.b)}, {"enabled", road.enabled}});
        for (const auto &vehicle : q.page.vehicles)
          entities.push_back(Json{{"id", vehicle.id},
                                  {"orderId", vehicle.order},
                                  {"type", vehicle.type},
                                  {"cell", Cell(vehicle.cell)},
                                  {"cargo", std::to_string(vehicle.cargo)},
                                  {"state", Haul(vehicle.state)}});
        for (const auto &order : q.page.orders)
          entities.push_back(Json{{"id", order.id},
                                  {"source", order.source},
                                  {"destination", order.destination},
                                  {"resource", order.resource},
                                  {"vehicleType", order.vehicleType},
                                  {"vehicleId", order.vehicle},
                                  {"state", Haul(order.state)},
                                  {"cargo", std::to_string(order.cargo)},
                                  {"vehicleCell", Cell(order.vehicleCell)}});
        out = Json{{"errorCode", Error(q.error)},
                   {"revision", std::to_string(q.page.revision)},
                   {"kind", q.page.kind},
                   {"offset", q.page.offset},
                   {"nextOffset", q.page.nextOffset},
                   {"total", q.page.total},
                   {"hasMore", q.page.hasMore},
                   {"entities", entities}};
      }
    }
  } else if (method == "explain") {
    if (!Fields(req, {"buildingId"}, message))
      return invalid(message);
    std::uint64_t b{};
    if (!Unsigned(req, "buildingId", UINT32_MAX, b, message))
      return invalid(message);
    const auto e = api_->Explain(static_cast<economy::BuildingId>(b));
    out = Json{{"errorCode", Error(e.error)},
               {"buildingId", b},
               {"reason", Stall(e.stall.reason)},
               {"explanation", e.text}};
  } else if (method == "advise") {
    // Read-only logistics planner: for every building input that no haul order serves, list the
    // producers of that resource (nearest first) with a ready-to-submit create_haul_order command.
    if (!Fields(req, {}, message))
      return invalid(message);
    const auto catalog = api_->DescribeCatalog();
    const auto q = api_->QueryWorld();
    std::map<std::string, const economy_api::BuildingTypeInfo *> types;
    for (const auto &t : catalog.buildingTypes)
      types[t.name] = &t;
    auto cellDistance = [](economy::Cell a, economy::Cell b) {
      return std::abs(static_cast<long long>(a.x) - b.x) + std::abs(static_cast<long long>(a.z) - b.z);
    };
    std::set<std::tuple<economy::BuildingId, economy::BuildingId, std::string>> served;
    for (const auto &o : q.world.orders)
      served.insert({o.source, o.destination, o.resource});
    Json needs = Json::array(), surplus = Json::array();
    for (const auto &consumer : q.world.buildings) {
      const auto found = types.find(consumer.type);
      if (found == types.end() || found->second->recipes.empty())
        continue;
      std::set<std::string> asked;
      for (const auto &input : found->second->recipes.front().inputs) {
        if (!asked.insert(input.resource).second)
          continue;
        std::uint64_t held = 0;
        for (const auto &item : consumer.inventory)
          if (item.resource == input.resource)
            held = item.amount;
        if (held >= input.amount)
          continue;
        std::vector<std::pair<long long, economy::BuildingId>> producers;
        bool alreadyServed = false;
        for (const auto &source : q.world.buildings) {
          if (source.id == consumer.id)
            continue;
          const auto st = types.find(source.type);
          if (st == types.end())
            continue;
          bool makes = false;
          for (const auto &r : st->second->recipes)
            for (const auto &out : r.outputs)
              makes = makes || out.resource == input.resource;
          if (!makes)
            continue;
          if (served.count({source.id, consumer.id, input.resource}))
            alreadyServed = true;
          producers.push_back({cellDistance(source.cell, consumer.cell), source.id});
        }
        if (alreadyServed)
          continue;
        std::sort(producers.begin(), producers.end());
        Json candidates = Json::array();
        for (std::size_t i = 0; i < producers.size() && i < 3; ++i)
          candidates.push_back(Json{{"source", producers[i].second},
                                    {"distanceCells", producers[i].first},
                                    {"command", Json{{"kind", "create_haul_order"},
                                                     {"source", producers[i].second},
                                                     {"destination", consumer.id},
                                                     {"resource", input.resource},
                                                     {"vehicleType", "truck"}}}});
        needs.push_back(Json{{"building", consumer.id},
                             {"type", consumer.type},
                             {"resource", input.resource},
                             {"held", std::to_string(held)},
                             {"neededPerCycle", std::to_string(input.amount)},
                             {"producers", candidates},
                             {"note", candidates.empty() ? "no building in this world produces it; place a producer or accept the stall"
                                                          : "unserved input"}});
      }
    }
    for (const auto &source : q.world.buildings) {
      const auto st = types.find(source.type);
      if (st == types.end())
        continue;
      std::set<std::string> outputs;
      for (const auto &r : st->second->recipes)
        for (const auto &out : r.outputs)
          outputs.insert(out.resource);
      for (const auto &resource : outputs) {
        bool consumed = false;
        for (const auto &other : q.world.buildings) {
          const auto ot = types.find(other.type);
          if (other.id == source.id || ot == types.end())
            continue;
          for (const auto &r : ot->second->recipes)
            for (const auto &in : r.inputs)
              consumed = consumed || in.resource == resource;
        }
        if (!consumed && source.stallReason == economy::StallReason::OutputFull)
          surplus.push_back(Json{{"building", source.id}, {"type", source.type}, {"resource", resource},
                                 {"note", "output_full and nothing here consumes it"}});
      }
    }
    out = Json{{"errorCode", Error(q.error)},
               {"revision", std::to_string(q.world.revision)},
               {"tick", std::to_string(q.world.tick)},
               {"unservedInputs", needs},
               {"unconsumedSurplus", surplus}};
  } else if (method == "poll") {
    if (!Fields(req, {"handle"}, message))
      return invalid(message);
    std::uint64_t h{};
    if (!U64(req, "handle", h, message))
      return invalid(message);
    const auto p = api_->PollCommand(h);
    out = Json{{"errorCode", Error(p.error)},    {"handle", std::to_string(h)},
               {"state", State(p.result.state)}, {"failure", Failure(p.result.failure)},
               {"message", p.result.message},    {"buildingId", p.result.building},
               {"roadId", p.result.road},        {"orderId", p.result.order}};
  } else if (method == "step") {
    if (!Fields(req, {"ticks"}, message))
      return invalid(message);
    std::uint64_t n{};
    if (!Unsigned(req, "ticks", economy_api::MaxStepTicks, n, message))
      return invalid(message);
    out = Json{{"errorCode", Error(api_->Step(static_cast<std::uint32_t>(n)))}};
  } else if (method == "events") {
    if (!Fields(req, {"afterSequence", "limit"}, message))
      return invalid(message);
    std::uint64_t cursor{}, limit{};
    if (!U64(req, "afterSequence", cursor, message) ||
        !Unsigned(req, "limit", economy_api::MaxEventPageSize, limit, message))
      return invalid(message);
    const auto q = api_->ReadEvents(cursor, static_cast<std::size_t>(limit));
    Json events = Json::array();
    for (const auto &e : q.page.events)
      events.push_back(Json{{"sequence", std::to_string(e.sequence)},
                            {"type", EventType(e.type)},
                            {"tick", std::to_string(e.tick)},
                            {"commandHandle", std::to_string(e.commandHandle)},
                            {"buildingId", e.building},
                            {"failure", Failure(e.failure)},
                            {"stallReason", Stall(e.stallReason)}});
    out = Json{{"errorCode", Error(q.error)},
               {"cursorFellBehind", q.page.cursorFellBehind},
               {"hasMore", q.page.hasMore},
               {"oldestAvailableSequence", std::to_string(q.page.oldestAvailableSequence)},
               {"newestAvailableSequence", std::to_string(q.page.newestAvailableSequence)},
               {"nextCursor", std::to_string(q.page.nextCursor)},
               {"events", events}};
  } else if (method == "submit") {
    if (!Fields(req, {"revision", "command"}, message))
      return invalid(message);
    std::uint64_t revision{};
    if (!U64(req, "revision", revision, message))
      return invalid(message);
    if (!req["command"].is_object() || !req["command"].contains("kind") ||
        !req["command"]["kind"].is_string())
      return invalid("command.kind must be a string");
    const auto &c = req["command"];
    const auto kind = c["kind"].get<std::string>();
    economy_api::Submission s;
    std::uint64_t id1{}, id2{};
    std::string name, resource, vehicle;
    std::int32_t x{}, z{}, ax{}, az{}, bx{}, bz{};
    if (kind == "place_building") {
      if (!NestedFields(c, {"kind", "type", "x", "z"}, message))
        return invalid(message);
      if (!String(c, "type", name, message) || !Int32(c, "x", x, message) || !Int32(c, "z", z, message))
        return invalid(message);
      s = api_->PlaceBuilding(revision, name, {x, z});
    } else if (kind == "remove_building" || kind == "set_building_enabled") {
      if (kind == "remove_building") {
        if (!NestedFields(c, {"kind", "buildingId"}, message))
          return invalid(message);
        if (!Unsigned(c, "buildingId", UINT32_MAX, id1, message))
          return invalid(message);
        s = api_->RemoveBuilding(revision, static_cast<economy::BuildingId>(id1));
      } else {
        if (!NestedFields(c, {"kind", "buildingId", "enabled"}, message))
          return invalid(message);
        if (!Unsigned(c, "buildingId", UINT32_MAX, id1, message) || !c["enabled"].is_boolean())
          return invalid("buildingId or enabled has invalid type");
        s = api_->SetBuildingEnabled(revision, static_cast<economy::BuildingId>(id1),
                                    c["enabled"].get<bool>());
      }
    } else if (kind == "build_road") {
      if (!NestedFields(c, {"kind", "ax", "az", "bx", "bz"}, message))
        return invalid(message);
      if (!Int32(c, "ax", ax, message) || !Int32(c, "az", az, message) || !Int32(c, "bx", bx, message) ||
          !Int32(c, "bz", bz, message))
        return invalid(message);
      s = api_->BuildRoad(revision, {ax, az}, {bx, bz});
    } else if (kind == "set_road_enabled") {
      if (!NestedFields(c, {"kind", "roadId", "enabled"}, message))
        return invalid(message);
      if (!Unsigned(c, "roadId", UINT32_MAX, id1, message) || !c["enabled"].is_boolean())
        return invalid("roadId or enabled has invalid type");
      s = api_->SetRoadEnabled(revision, static_cast<economy::RoadId>(id1), c["enabled"].get<bool>());
    } else if (kind == "create_haul_order") {
      if (!NestedFields(c, {"kind", "source", "destination", "resource", "vehicleType"}, message))
        return invalid(message);
      if (!Unsigned(c, "source", UINT32_MAX, id1, message) ||
          !Unsigned(c, "destination", UINT32_MAX, id2, message) ||
          !String(c, "resource", resource, message) || !String(c, "vehicleType", vehicle, message))
        return invalid(message);
      s = api_->CreateHaulOrder(revision, static_cast<economy::BuildingId>(id1),
                               static_cast<economy::BuildingId>(id2), resource, vehicle);
    } else if (kind == "cancel_haul_order") {
      if (!NestedFields(c, {"kind", "orderId"}, message))
        return invalid(message);
      if (!Unsigned(c, "orderId", UINT32_MAX, id1, message))
        return invalid(message);
      s = api_->CancelHaulOrder(revision, static_cast<economy::OrderId>(id1));
    } else
      return invalid("unknown command kind");
    out =
        Json{{"errorCode", Error(s.error)}, {"handle", std::to_string(s.handle)}, {"state", State(s.state)}};
  } else if (method == "reload_rules") {
    // Swaps in a Lua-tuned catalog of identical structure between ticks (controller only).
    if (!Fields(req, {"text"}, message))
      return invalid(message);
    std::string text;
    if (!String(req, "text", text, message))
      return invalid(message);
    if (role_ != Role::Controller)
      out = Json{{"errorCode", "permission_denied"}};
    else if (!rulesBuilder_)
      out = Json{{"errorCode", "unsupported"}, {"message", "this host was built without Lua rules"}};
    else {
      auto tuned = std::make_unique<economy::Catalog>();
      std::string error;
      if (!rulesBuilder_(text, *tuned, error)) {
        out = Json{{"errorCode", "invalid_argument"}, {"message", error}};
      } else {
        const auto code = api_->ReplaceCatalog(*tuned, error);
        if (code == economy_api::ErrorCode::None) {
          retiredCatalogs_.push_back(std::move(tuned));
          out = Json{{"errorCode", "none"}};
        } else {
          out = Json{{"errorCode", Error(code)}, {"message", error}};
        }
      }
    }
  } else if (method == "shutdown") {
    if (!Fields(req, {}, message))
      return invalid(message);
    if (role_ != Role::Controller || !allowShutdown_)
      out = Json{{"errorCode", "permission_denied"}};
    else {
      shutdown_ = true;
      out = Json{{"errorCode", "none"}};
    }
  } else
    return tagged(ProtocolError(id, "method_not_found", "unknown method"));
  return tagged(Result(id, std::move(out)));
}
} // namespace openrepublic::economy_host
