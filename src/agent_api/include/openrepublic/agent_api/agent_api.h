#pragma once

#include "openrepublic/sim/simulation.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <vector>

namespace openrepublic::agent_api {

// Internal, in-process API version. It describes this OpenRepublic interface,
// not a WRSR protocol or compatibility contract.
// Typed C++ signatures and structs are the schema boundary; no wire format or
// IPC serialization is defined by this API.
constexpr std::uint32_t ApiVersion = 0;
// Limits work performed by one API call. Larger runs should be split across
// calls so controllers can observe state and command results between batches.
constexpr std::uint64_t MaxStepTicks = 10000;
// Completed command records are retained until explicitly acknowledged.
constexpr std::size_t MaxTrackedCommands = 64;
constexpr std::size_t MaxEntityPageSize = 32;
constexpr std::size_t MaxEventJournalEntries = 128;
constexpr std::size_t MaxEventPageSize = 32;

enum class Action : std::uint8_t {
    QueryWorld = 1,
    StepSimulation = 2,
    ConstructFacility = 3,
    ConstructRoad = 4,
    ConstructStorage = 5,
    AssignTransportPlan = 6,
    QueryEntityPage = 7,
    ReadEvents = 8
};

struct Permissions {
    bool queryWorld{true};
    bool stepSimulation{false};
    bool construct{false};
    bool assignTransport{false};
};

struct Capabilities {
    std::uint32_t apiVersion{ApiVersion};
    std::vector<Action> actions;
};

struct Entity {
    sim::EntityId id{};
    sim::FacilityType facilityType{sim::FacilityType::Farm};
    bool isFacility{};
    bool isStorage{};
    std::int32_t gridX{};
    std::int32_t gridZ{};
    sim::InventoryView inventory{};
};

struct Route {
    sim::TruckPlan assignedPlan{0, 0, 0};
    std::vector<sim::EntityId> activeNodes;
    std::size_t cursor{};
    sim::EntityId truckNode{};
    sim::Resource cargo{sim::Resource::None};
    std::uint32_t cargoAmount{};
    sim::TruckStage stage{sim::TruckStage::LoadGrainAtFarm};
    std::vector<sim::RoadView> roads;
};

enum class ErrorCode : std::uint8_t {
    None = 0,
    PermissionDenied,
    StaleRevision,
    InvalidArgument,
    QueueFull,
    UnknownCommand,
    CommandNotCompleted,
    CommandTrackingFull,
    StepLimitExceeded,
    InvalidEntityPageLimit,
    InvalidEventPageLimit,
    FutureEventCursor,
    SimulationRejected
};

struct WorldView {
    std::uint32_t apiVersion{ApiVersion};
    std::uint64_t revision{};
    sim::Tick tick{};
    std::uint64_t deliveredFlourTotal{};
    // Zeroed when no authorized world snapshot is available.
    sim::SimulationRules rules{0, 0, 0};
    std::vector<Entity> entities;
    Route route;
};

struct WorldQuery {
    ErrorCode error{ErrorCode::None};
    WorldView world;
};

struct EntityPage {
    std::uint32_t apiVersion{ApiVersion};
    std::uint64_t revision{};
    std::size_t offset{};
    std::size_t nextOffset{};
    bool hasMore{};
    std::vector<Entity> entities;
};

struct EntityPageQuery {
    ErrorCode error{ErrorCode::None};
    EntityPage page;
};

enum class EventType : std::uint8_t {
    CommandSubmitted = 1,
    CommandApplied,
    CommandRejected,
    SimulationAdvanced
};

struct AgentEvent {
    std::uint64_t sequence{};
    EventType type{EventType::SimulationAdvanced};
    std::uint64_t commandId{};
    sim::ConstructionCommandType commandType{sim::ConstructionCommandType::FacilityPlacement};
    sim::ConstructionFailureReason failureReason{sim::ConstructionFailureReason::None};
    sim::Tick tick{};
    std::uint64_t revision{};
    std::uint64_t stepTicks{};
};

struct EventPage {
    bool cursorFellBehind{};
    bool hasMore{};
    std::uint64_t oldestAvailableSequence{};
    std::uint64_t newestAvailableSequence{};
    std::uint64_t nextCursor{};
    std::vector<AgentEvent> events;
};

struct EventPageQuery {
    ErrorCode error{ErrorCode::None};
    EventPage page;
};

enum class CommandState : std::uint8_t { Unknown = 0, Queued, Applied, Rejected };

struct Submission {
    ErrorCode error{ErrorCode::None};
    std::uint64_t commandId{};
    CommandState state{CommandState::Unknown};
    explicit operator bool() const noexcept { return error == ErrorCode::None; }
};

struct CommandResult {
    ErrorCode error{ErrorCode::None};
    std::uint64_t commandId{};
    CommandState state{CommandState::Unknown};
    sim::ConstructionCommandType type{sim::ConstructionCommandType::FacilityPlacement};
    sim::EntityId entityId{};
    sim::ConstructionFailureReason failureReason{sim::ConstructionFailureReason::None};
};

class AgentApi final {
public:
    explicit AgentApi(Permissions permissions = {});
    AgentApi(sim::Simulation simulation, Permissions permissions = {});

    Capabilities DiscoverCapabilities() const;
    WorldQuery QueryWorld() const;
    EntityPageQuery QueryEntityPage(std::uint64_t expectedRevision, std::size_t offset,
        std::size_t limit) const;
    EventPageQuery ReadEvents(std::uint64_t afterSequence, std::size_t limit) const;
    // For Step(n>1), the first-tick advance and any command outcomes generated
    // on that tick are journaled before the remaining ticks' advancement event.
    ErrorCode Step(std::uint64_t ticks = 1);

    Submission PlaceFacility(std::uint64_t expectedRevision, sim::FacilityType type,
        std::int32_t gridX, std::int32_t gridZ);
    Submission AddRoadSegment(std::uint64_t expectedRevision, std::int32_t ax,
        std::int32_t az, std::int32_t bx, std::int32_t bz);
    Submission PlaceStorage(std::uint64_t expectedRevision, std::int32_t gridX,
        std::int32_t gridZ);
    Submission AssignTransportPlan(std::uint64_t expectedRevision, const sim::TruckPlan& plan);

    CommandResult GetCommandResult(std::uint64_t commandId) const;
    // Releases a completed result and its tracked memory. Queued commands
    // cannot be acknowledged until execution finishes.
    ErrorCode AcknowledgeCommandResult(std::uint64_t commandId);

private:
    struct TrackedCommand {
        CommandResult result;
        bool completed{};
    };

    bool HasPermission(Action action) const noexcept;
    Submission CheckSubmission(Action action, std::uint64_t expectedRevision) const;
    Submission Track(std::uint64_t commandId, sim::ConstructionCommandType type);
    void CollectCompletedCommands();
    void RecordEvent(EventType type, std::uint64_t commandId = 0,
        sim::ConstructionCommandType commandType = sim::ConstructionCommandType::FacilityPlacement,
        sim::ConstructionFailureReason failureReason = sim::ConstructionFailureReason::None,
        std::uint64_t stepTicks = 0);

    sim::Simulation simulation_;
    Permissions permissions_;
    std::map<std::uint64_t, TrackedCommand> commands_;
    std::deque<AgentEvent> eventJournal_;
    std::uint64_t lastEventSequence_{};
};

} // namespace openrepublic::agent_api
