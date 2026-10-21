#include "agent_protocol.h"

#include "json.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace openrepublic::agent_host {
namespace {
using Json = nlohmann::json;
using namespace openrepublic;

struct ParseState {
    std::size_t nodes{};
    bool tooDeep{};
    bool tooManyNodes{};
    bool duplicateKey{};
    std::vector<std::unordered_set<std::string>> objectKeys;
    std::vector<bool> containerIsObject;
};

bool IsJsonObject(const Json& value) { return value.is_object(); }

bool HasOnlyFields(const Json& value, const std::vector<const char*>& fields,
    std::string& error) {
    if (!IsJsonObject(value)) {
        error = "request must be a JSON object";
        return false;
    }
    std::set<std::string> allowed;
    for (const auto* field : fields) allowed.emplace(field);
    for (auto it = value.begin(); it != value.end(); ++it) {
        if (!allowed.count(it.key())) {
            error = "unknown request field: " + it.key();
            return false;
        }
    }
    for (const auto* field : fields) {
        if (!value.contains(field)) {
            error = std::string("missing request field: ") + field;
            return false;
        }
    }
    return true;
}

bool ReadString(const Json& object, const char* key, std::string& out, std::string& error) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) {
        error = std::string("field must be a string: ") + key;
        return false;
    }
    out = it->get<std::string>();
    return true;
}

bool ReadU64String(const Json& object, const char* key, std::uint64_t& out,
    std::string& error) {
    std::string token;
    if (!ReadString(object, key, token, error)) return false;
    if (token.empty() || token.size() > 20) {
        error = std::string("field must be an unsigned decimal string: ") + key;
        return false;
    }
    for (const unsigned char c : token) {
        if (c < '0' || c > '9') {
            error = std::string("field must be an unsigned decimal string: ") + key;
            return false;
        }
    }
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), out);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) {
        error = std::string("field is outside the uint64 range: ") + key;
        return false;
    }
    return true;
}

bool ReadUnsigned(const Json& object, const char* key, std::uint64_t max,
    std::uint64_t& out, std::string& error) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer()) {
        error = std::string("field must be an integer: ") + key;
        return false;
    }
    if (it->is_number_unsigned()) {
        out = it->get<std::uint64_t>();
    } else {
        const auto signedValue = it->get<std::int64_t>();
        if (signedValue < 0) {
            error = std::string("field must be non-negative: ") + key;
            return false;
        }
        out = static_cast<std::uint64_t>(signedValue);
    }
    if (out > max) {
        error = std::string("field exceeds its permitted maximum: ") + key;
        return false;
    }
    return true;
}

bool ReadInt32(const Json& object, const char* key, std::int32_t& out, std::string& error) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer()) {
        error = std::string("field must be an integer: ") + key;
        return false;
    }
    if (it->is_number_unsigned()) {
        const auto value = it->get<std::uint64_t>();
        if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
            error = std::string("field is outside the int32 range: ") + key;
            return false;
        }
        out = static_cast<std::int32_t>(value);
        return true;
    }
    const auto value = it->get<std::int64_t>();
    if (value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max()) {
        error = std::string("field is outside the int32 range: ") + key;
        return false;
    }
    out = static_cast<std::int32_t>(value);
    return true;
}

std::string ApiError(agent_api::ErrorCode error) {
    using agent_api::ErrorCode;
    switch (error) {
    case ErrorCode::None: return "none";
    case ErrorCode::PermissionDenied: return "permission_denied";
    case ErrorCode::StaleRevision: return "stale_revision";
    case ErrorCode::InvalidArgument: return "invalid_argument";
    case ErrorCode::QueueFull: return "queue_full";
    case ErrorCode::UnknownCommand: return "unknown_command";
    case ErrorCode::CommandNotCompleted: return "command_not_completed";
    case ErrorCode::CommandTrackingFull: return "command_tracking_full";
    case ErrorCode::CommandIdExhausted: return "command_id_exhausted";
    case ErrorCode::StepLimitExceeded: return "step_limit_exceeded";
    case ErrorCode::InvalidEntityPageLimit: return "invalid_entity_page_limit";
    case ErrorCode::InvalidEventPageLimit: return "invalid_event_page_limit";
    case ErrorCode::FutureEventCursor: return "future_event_cursor";
    case ErrorCode::SimulationRejected: return "simulation_rejected";
    }
    return "unknown_api_error";
}

std::string CommandStateName(agent_api::CommandState state) {
    using agent_api::CommandState;
    switch (state) {
    case CommandState::Unknown: return "unknown";
    case CommandState::Queued: return "queued";
    case CommandState::Applied: return "applied";
    case CommandState::Rejected: return "rejected";
    }
    return "unknown";
}

std::string FacilityName(sim::FacilityType type) {
    switch (type) {
    case sim::FacilityType::Farm: return "farm";
    case sim::FacilityType::Mill: return "mill";
    }
    return "unknown";
}

std::string ResourceName(sim::Resource resource) {
    switch (resource) {
    case sim::Resource::None: return "none";
    case sim::Resource::Grain: return "grain";
    case sim::Resource::Flour: return "flour";
    }
    return "unknown";
}

std::string StageName(sim::TruckStage stage) {
    switch (stage) {
    case sim::TruckStage::LoadGrainAtFarm: return "load_grain_at_farm";
    case sim::TruckStage::DeliverGrainToMill: return "deliver_grain_to_mill";
    case sim::TruckStage::LoadFlourAtMill: return "load_flour_at_mill";
    // The simulation's legacy enum name says "warehouse" even when the
    // currently assigned destination is a player-created storage.
    case sim::TruckStage::DeliverFlourToWarehouse: return "deliver_flour_to_storage";
    case sim::TruckStage::ReturnToFarm: return "return_to_farm";
    }
    return "unknown";
}

std::string ActionName(agent_api::Action action) {
    using agent_api::Action;
    switch (action) {
    case Action::QueryWorld: return "query_world";
    case Action::StepSimulation: return "step_simulation";
    case Action::ConstructFacility: return "construct_facility";
    case Action::ConstructRoad: return "construct_road";
    case Action::ConstructStorage: return "construct_storage";
    case Action::AssignTransportPlan: return "assign_transport_plan";
    case Action::QueryEntityPage: return "query_entity_page";
    case Action::ReadEvents: return "read_events";
    case Action::ConfigureSimulation: return "configure_simulation";
    }
    return "unknown";
}

std::string EventTypeName(agent_api::EventType type) {
    using agent_api::EventType;
    switch (type) {
    case EventType::CommandSubmitted: return "command_submitted";
    case EventType::CommandApplied: return "command_applied";
    case EventType::CommandRejected: return "command_rejected";
    case EventType::SimulationAdvanced: return "simulation_advanced";
    case EventType::SimulationConfigured: return "simulation_configured";
    case EventType::WorldLoaded: return "world_loaded";
    }
    return "unknown";
}

std::string CommandTypeName(sim::ConstructionCommandType type) {
    switch (type) {
    case sim::ConstructionCommandType::FacilityPlacement: return "facility_placement";
    case sim::ConstructionCommandType::RoadSegment: return "road_segment";
    case sim::ConstructionCommandType::TruckPlan: return "truck_plan";
    case sim::ConstructionCommandType::StoragePlacement: return "storage_placement";
    }
    return "unknown";
}

std::string FailureName(sim::ConstructionFailureReason reason) {
    using sim::ConstructionFailureReason;
    switch (reason) {
    case ConstructionFailureReason::None: return "none";
    case ConstructionFailureReason::InvalidFacilityType: return "invalid_facility_type";
    case ConstructionFailureReason::OutOfBounds: return "out_of_bounds";
    case ConstructionFailureReason::OccupiedCell: return "occupied_cell";
    case ConstructionFailureReason::CapacityExceeded: return "capacity_exceeded";
    case ConstructionFailureReason::InvalidRoadGeometry: return "invalid_road_geometry";
    case ConstructionFailureReason::DuplicateRoadSegment: return "duplicate_road_segment";
    case ConstructionFailureReason::InvalidTransportPlan: return "invalid_transport_plan";
    case ConstructionFailureReason::TruckNotAtCommandBoundary: return "truck_not_at_command_boundary";
    case ConstructionFailureReason::DisconnectedRoute: return "disconnected_route";
    case ConstructionFailureReason::StorageRetargetUnavailable: return "storage_retarget_unavailable";
    case ConstructionFailureReason::DiagnosticUnavailableFromSave: return "diagnostic_unavailable_from_save";
    }
    return "unknown";
}

Json InventoryJson(const sim::InventoryView& inventory) {
    return Json{{"grain", inventory.grain}, {"flour", inventory.flour}};
}

Json EntityJson(const agent_api::Entity& entity) {
    return Json{{"id", entity.id}, {"kind", entity.isFacility ? "facility" : "storage"},
        {"facilityType", entity.isFacility ? FacilityName(entity.facilityType) : ""},
        {"x", entity.gridX}, {"z", entity.gridZ}, {"inventory", InventoryJson(entity.inventory)}};
}

Json WorldJson(const agent_api::WorldView& world) {
    Json entities = Json::array();
    for (const auto& entity : world.entities) entities.push_back(EntityJson(entity));
    Json roads = Json::array();
    for (const auto& road : world.route.roads) {
        roads.push_back(Json{{"id", road.id}, {"a", road.a}, {"b", road.b},
            {"enabled", road.enabled}, {"ax", road.aGridX}, {"az", road.aGridZ},
            {"bx", road.bGridX}, {"bz", road.bGridZ}});
    }
    Json activeNodes = Json::array();
    for (const auto id : world.route.activeNodes) activeNodes.push_back(id);
    return Json{{"apiVersion", world.apiVersion}, {"revision", std::to_string(world.revision)},
        {"tick", std::to_string(world.tick)}, {"entities", std::move(entities)},
        {"production", Json{{"deliveredFlourTotal", std::to_string(world.deliveredFlourTotal)},
            {"rules", Json{{"farmGrainPerHarvest", world.rules.farmGrainPerHarvest},
                {"farmHarvestIntervalTicks", world.rules.farmHarvestIntervalTicks},
                {"millGrainPerTick", world.rules.millGrainPerTick}}}}},
        {"route", Json{{"farmId", world.route.assignedPlan.farmId},
            {"millId", world.route.assignedPlan.millId}, {"storageId", world.route.assignedPlan.storageId},
            {"activeNodes", std::move(activeNodes)}, {"cursor", world.route.cursor},
            {"truckNode", world.route.truckNode}, {"cargo", ResourceName(world.route.cargo)},
            {"cargoAmount", world.route.cargoAmount}, {"stage", StageName(world.route.stage)},
            {"roads", std::move(roads)}}}};
}

Json EventJson(const agent_api::AgentEvent& event) {
    return Json{{"sequence", std::to_string(event.sequence)}, {"type", EventTypeName(event.type)},
        {"commandId", std::to_string(event.commandId)}, {"commandType", CommandTypeName(event.commandType)},
        {"failureReason", FailureName(event.failureReason)}, {"tick", std::to_string(event.tick)},
        {"revision", std::to_string(event.revision)}, {"stepTicks", std::to_string(event.stepTicks)}};
}

Json ProtocolError(const Json& id, std::string code, std::string message) {
    return Json{{"id", id}, {"protocolVersion", ProtocolVersion}, {"ok", false},
        {"error", Json{{"code", std::move(code)}, {"message", std::move(message)}}}};
}

Json ApiResult(const Json& id, Json result) {
    return Json{{"id", id}, {"protocolVersion", ProtocolVersion}, {"ok", true},
        {"result", std::move(result)}};
}

std::string RoleName(Role role) { return role == Role::Controller ? "controller" : "observer"; }

Json MethodCapabilities() {
    const Json allRoles = Json::array({"observer", "controller"});
    const Json controllerRoles = Json::array({"controller"});
    Json methods = Json::array();
    const auto add = [&methods, &allRoles, &controllerRoles](const char* name, bool controllerOnly = false) {
        methods.push_back(Json{{"name", name}, {"roles", controllerOnly ? controllerRoles : allRoles}});
    };

    add("hello");
    add("capabilities");
    add("world");
    add("entity_page");
    add("events");
    add("step", true);
    add("place_facility", true);
    add("place_storage", true);
    add("add_road_segment", true);
    add("assign_transport_plan", true);
    add("command_result");
    add("acknowledge", true);
    return methods;
}

bool ValidProtocolEnvelope(const Json& request, Json& id, std::string& method,
    std::string& code, std::string& message) {
    if (!request.is_object()) {
        code = "invalid_request";
        message = "request must be a JSON object";
        return false;
    }
    const auto idIt = request.find("id");
    if (idIt == request.end() || !idIt->is_string() || idIt->get_ref<const std::string&>().empty() ||
        idIt->get_ref<const std::string&>().size() > 128) {
        code = "invalid_request";
        message = "id must be a non-empty string of at most 128 bytes";
        return false;
    }
    id = *idIt;
    const auto versionIt = request.find("protocolVersion");
    if (versionIt == request.end() || !versionIt->is_number_unsigned() &&
        (!versionIt->is_number_integer() || versionIt->get<std::int64_t>() < 0)) {
        code = "invalid_request";
        message = "protocolVersion must be a non-negative integer";
        return false;
    }
    std::uint64_t version{};
    if (!ReadUnsigned(request, "protocolVersion", std::numeric_limits<std::uint32_t>::max(), version, message)) {
        code = "invalid_request";
        return false;
    }
    if (version != ProtocolVersion) {
        code = "unsupported_protocol_version";
        message = "unsupported protocolVersion";
        return false;
    }
    const auto methodIt = request.find("method");
    if (methodIt == request.end() || !methodIt->is_string()) {
        code = "invalid_request";
        message = "method must be a string";
        return false;
    }
    method = methodIt->get<std::string>();
    return true;
}

bool ParseBoundedJson(std::string_view line, Json& out, std::string& code,
    std::string& message) {
    if (line.size() > MaxRequestLineBytes) {
        code = "request_too_large";
        message = "request line exceeds the configured byte limit";
        return false;
    }
    // Reject deeply nested data before the recursive JSON parser sees it.
    // Brackets inside escaped JSON strings are data and do not affect depth.
    std::size_t lexicalDepth = 0;
    bool inString = false;
    bool escaped = false;
    for (const char ch : line) {
        if (inString) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') inString = false;
            continue;
        }
        if (ch == '"') inString = true;
        else if (ch == '{' || ch == '[') {
            if (++lexicalDepth > MaxJsonNesting) {
                code = "json_too_deep";
                message = "JSON nesting exceeds the configured depth limit";
                return false;
            }
        } else if ((ch == '}' || ch == ']') && lexicalDepth > 0) {
            --lexicalDepth;
        }
    }
    ParseState state;
    const auto callback = [&state](int depth, Json::parse_event_t event, Json& parsed) {
        if (depth > static_cast<int>(MaxJsonNesting)) state.tooDeep = true;
        if (++state.nodes > MaxJsonNodes) state.tooManyNodes = true;
        if (event == Json::parse_event_t::object_start) {
            state.containerIsObject.push_back(true);
            state.objectKeys.emplace_back();
        } else if (event == Json::parse_event_t::array_start) {
            state.containerIsObject.push_back(false);
        } else if (event == Json::parse_event_t::key) {
            if (!state.objectKeys.empty() && !state.objectKeys.back().insert(parsed.get<std::string>()).second)
                state.duplicateKey = true;
        } else if (event == Json::parse_event_t::object_end) {
            if (!state.objectKeys.empty()) state.objectKeys.pop_back();
            if (!state.containerIsObject.empty()) state.containerIsObject.pop_back();
        } else if (event == Json::parse_event_t::array_end) {
            if (!state.containerIsObject.empty()) state.containerIsObject.pop_back();
        }
        return true;
    };
    try {
        out = Json::parse(line.begin(), line.end(), callback, true, false);
    } catch (const Json::parse_error&) {
        code = "malformed_json";
        message = "request line is not valid JSON";
        return false;
    } catch (const std::exception&) {
        code = "malformed_json";
        message = "request line could not be parsed";
        return false;
    }
    if (state.tooDeep) {
        code = "json_too_deep";
        message = "JSON nesting exceeds the configured depth limit";
        return false;
    }
    if (state.tooManyNodes) {
        code = "json_too_complex";
        message = "JSON structure exceeds the configured node limit";
        return false;
    }
    if (state.duplicateKey) {
        code = "duplicate_json_key";
        message = "duplicate object keys are not allowed";
        return false;
    }
    return true;
}

bool CheckFields(const Json& request, std::initializer_list<const char*> operationFields,
    std::string& message) {
    std::vector<const char*> all{"id", "protocolVersion", "method"};
    all.insert(all.end(), operationFields.begin(), operationFields.end());
    return HasOnlyFields(request, all, message);
}

Json SubmissionJson(const agent_api::Submission& submission) {
    return Json{{"errorCode", ApiError(submission.error)},
        {"commandId", std::to_string(submission.commandId)}, {"state", CommandStateName(submission.state)}};
}

} // namespace

Dispatcher::Dispatcher(Role role)
    : api_([role] {
        agent_api::Permissions permissions;
        permissions.queryWorld = true;
        const bool controller = role == Role::Controller;
        permissions.stepSimulation = controller;
        permissions.construct = controller;
        permissions.assignTransport = controller;
        return permissions;
    }()), role_(role) {}

std::string Dispatcher::HandleLine(std::string_view line) {
    Json request;
    std::string code, message;
    if (!ParseBoundedJson(line, request, code, message))
        return ProtocolError(nullptr, std::move(code), std::move(message)).dump();

    Json id = nullptr;
    std::string method;
    if (!ValidProtocolEnvelope(request, id, method, code, message))
        return ProtocolError(id, std::move(code), std::move(message)).dump();

    auto invalid = [&id](std::string error) {
        return ProtocolError(id, "invalid_request", std::move(error)).dump();
    };
    std::string fieldError;
    Json result;
    if (method == "hello") {
        if (!CheckFields(request, {}, fieldError)) return invalid(std::move(fieldError));
        Json actions = Json::array();
        for (const auto action : api_.DiscoverCapabilities().actions) actions.push_back(ActionName(action));
        result = Json{{"protocolVersion", ProtocolVersion}, {"agentApiVersion", agent_api::ApiVersion},
            {"role", RoleName(role_)}, {"actions", std::move(actions)}, {"methods", MethodCapabilities()},
            {"limits", Json{{"maxRequestLineBytes", MaxRequestLineBytes},
                {"maxRequestsPerProcess", MaxRequestsPerProcess}, {"maxJsonNesting", MaxJsonNesting},
                {"maxJsonNodes", MaxJsonNodes},
                {"maxStepTicks", agent_api::MaxStepTicks}, {"maxEntityPageSize", agent_api::MaxEntityPageSize},
                {"maxEntityOffset", sim::Simulation::MaxFacilities + sim::Simulation::MaxStorages},
                {"maxEventPageSize", agent_api::MaxEventPageSize}}},
            {"uint64Encoding", "decimal_string"}};
    } else if (method == "capabilities") {
        if (!CheckFields(request, {}, fieldError)) return invalid(std::move(fieldError));
        Json actions = Json::array();
        for (const auto action : api_.DiscoverCapabilities().actions) actions.push_back(ActionName(action));
        result = Json{{"apiVersion", agent_api::ApiVersion}, {"actions", std::move(actions)},
            {"methods", MethodCapabilities()}};
    } else if (method == "world") {
        if (!CheckFields(request, {}, fieldError)) return invalid(std::move(fieldError));
        const auto query = api_.QueryWorld();
        result = Json{{"errorCode", ApiError(query.error)}, {"world", WorldJson(query.world)}};
    } else if (method == "entity_page") {
        if (!CheckFields(request, {"revision", "offset", "limit"}, fieldError)) return invalid(std::move(fieldError));
        std::uint64_t revision{}, offset{}, limit{};
        if (!ReadU64String(request, "revision", revision, fieldError) ||
            !ReadUnsigned(request, "offset",
                sim::Simulation::MaxFacilities + sim::Simulation::MaxStorages, offset, fieldError) ||
            !ReadUnsigned(request, "limit", agent_api::MaxEntityPageSize, limit, fieldError))
            return invalid(std::move(fieldError));
        const auto query = api_.QueryEntityPage(revision, static_cast<std::size_t>(offset), static_cast<std::size_t>(limit));
        Json entities = Json::array();
        for (const auto& entity : query.page.entities) entities.push_back(EntityJson(entity));
        result = Json{{"errorCode", ApiError(query.error)}, {"apiVersion", query.page.apiVersion},
            {"revision", std::to_string(query.page.revision)}, {"offset", query.page.offset},
            {"nextOffset", query.page.nextOffset}, {"hasMore", query.page.hasMore},
            {"entities", std::move(entities)}};
    } else if (method == "events") {
        if (!CheckFields(request, {"afterSequence", "limit"}, fieldError)) return invalid(std::move(fieldError));
        std::uint64_t cursor{}, limit{};
        if (!ReadU64String(request, "afterSequence", cursor, fieldError) ||
            !ReadUnsigned(request, "limit", agent_api::MaxEventPageSize, limit, fieldError))
            return invalid(std::move(fieldError));
        const auto query = api_.ReadEvents(cursor, static_cast<std::size_t>(limit));
        Json events = Json::array();
        for (const auto& event : query.page.events) events.push_back(EventJson(event));
        result = Json{{"errorCode", ApiError(query.error)}, {"cursorFellBehind", query.page.cursorFellBehind},
            {"hasMore", query.page.hasMore}, {"oldestAvailableSequence", std::to_string(query.page.oldestAvailableSequence)},
            {"newestAvailableSequence", std::to_string(query.page.newestAvailableSequence)},
            {"nextCursor", std::to_string(query.page.nextCursor)}, {"events", std::move(events)}};
    } else if (method == "step") {
        if (!CheckFields(request, {"ticks"}, fieldError)) return invalid(std::move(fieldError));
        std::uint64_t ticks{};
        if (!ReadUnsigned(request, "ticks", agent_api::MaxStepTicks, ticks, fieldError))
            return invalid(std::move(fieldError));
        const auto error = api_.Step(ticks);
        result = Json{{"errorCode", ApiError(error)}};
    } else if (method == "place_facility") {
        if (!CheckFields(request, {"revision", "facilityType", "x", "z"}, fieldError)) return invalid(std::move(fieldError));
        std::uint64_t revision{};
        std::string facilityType;
        std::int32_t x{}, z{};
        if (!ReadU64String(request, "revision", revision, fieldError) ||
            !ReadString(request, "facilityType", facilityType, fieldError) ||
            !ReadInt32(request, "x", x, fieldError) || !ReadInt32(request, "z", z, fieldError))
            return invalid(std::move(fieldError));
        if (facilityType != "farm" && facilityType != "mill") return invalid("facilityType must be 'farm' or 'mill'");
        const auto submission = api_.PlaceFacility(revision,
            facilityType == "farm" ? sim::FacilityType::Farm : sim::FacilityType::Mill, x, z);
        result = SubmissionJson(submission);
    } else if (method == "place_storage") {
        if (!CheckFields(request, {"revision", "x", "z"}, fieldError)) return invalid(std::move(fieldError));
        std::uint64_t revision{};
        std::int32_t x{}, z{};
        if (!ReadU64String(request, "revision", revision, fieldError) ||
            !ReadInt32(request, "x", x, fieldError) || !ReadInt32(request, "z", z, fieldError))
            return invalid(std::move(fieldError));
        result = SubmissionJson(api_.PlaceStorage(revision, x, z));
    } else if (method == "add_road_segment") {
        if (!CheckFields(request, {"revision", "ax", "az", "bx", "bz"}, fieldError)) return invalid(std::move(fieldError));
        std::uint64_t revision{};
        std::int32_t ax{}, az{}, bx{}, bz{};
        if (!ReadU64String(request, "revision", revision, fieldError) ||
            !ReadInt32(request, "ax", ax, fieldError) || !ReadInt32(request, "az", az, fieldError) ||
            !ReadInt32(request, "bx", bx, fieldError) || !ReadInt32(request, "bz", bz, fieldError))
            return invalid(std::move(fieldError));
        result = SubmissionJson(api_.AddRoadSegment(revision, ax, az, bx, bz));
    } else if (method == "assign_transport_plan") {
        if (!CheckFields(request, {"revision", "farmId", "millId", "storageId"}, fieldError)) return invalid(std::move(fieldError));
        std::uint64_t revision{}, farmId{}, millId{}, storageId{};
        if (!ReadU64String(request, "revision", revision, fieldError) ||
            !ReadUnsigned(request, "farmId", std::numeric_limits<sim::EntityId>::max(), farmId, fieldError) ||
            !ReadUnsigned(request, "millId", std::numeric_limits<sim::EntityId>::max(), millId, fieldError) ||
            !ReadUnsigned(request, "storageId", std::numeric_limits<sim::EntityId>::max(), storageId, fieldError))
            return invalid(std::move(fieldError));
        result = SubmissionJson(api_.AssignTransportPlan(revision,
            sim::TruckPlan{static_cast<sim::EntityId>(farmId), static_cast<sim::EntityId>(millId),
                static_cast<sim::EntityId>(storageId)}));
    } else if (method == "command_result") {
        if (!CheckFields(request, {"commandId"}, fieldError)) return invalid(std::move(fieldError));
        std::uint64_t commandId{};
        if (!ReadU64String(request, "commandId", commandId, fieldError)) return invalid(std::move(fieldError));
        const auto command = api_.GetCommandResult(commandId);
        result = Json{{"errorCode", ApiError(command.error)}, {"commandId", std::to_string(command.commandId)},
            {"state", CommandStateName(command.state)}, {"commandType", CommandTypeName(command.type)},
            {"entityId", command.entityId}, {"failureReason", FailureName(command.failureReason)}};
    } else if (method == "acknowledge") {
        if (!CheckFields(request, {"commandId"}, fieldError)) return invalid(std::move(fieldError));
        std::uint64_t commandId{};
        if (!ReadU64String(request, "commandId", commandId, fieldError)) return invalid(std::move(fieldError));
        const auto error = role_ == Role::Controller ? api_.AcknowledgeCommandResult(commandId) :
            agent_api::ErrorCode::PermissionDenied;
        result = Json{{"errorCode", ApiError(error)}};
    } else {
        return ProtocolError(id, "method_not_found", "unknown method").dump();
    }
    return ApiResult(id, std::move(result)).dump();
}

} // namespace openrepublic::agent_host
