#include "openrepublic/agent_api/agent_api.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace openrepublic::agent_api {
namespace {
bool InBounds(std::int32_t x, std::int32_t z) {
    return x >= sim::Simulation::PrototypeGridMin && x <= sim::Simulation::PrototypeGridMax &&
        z >= sim::Simulation::PrototypeGridMin && z <= sim::Simulation::PrototypeGridMax;
}
}

AgentApi::AgentApi(Permissions permissions)
    : ownedSimulation_(std::make_unique<sim::Simulation>(sim::Simulation::CreateSyntheticDemo())),
      simulation_(ownedSimulation_.get()), permissions_(permissions) {
    CollectCompletedCommands();
}

AgentApi::AgentApi(sim::Simulation& simulation, Permissions permissions)
    : simulation_(&simulation), permissions_(permissions) {
    CollectCompletedCommands();
}

AgentApi::AgentApi(sim::Simulation&& simulation, Permissions permissions)
    : ownedSimulation_(std::make_unique<sim::Simulation>(std::move(simulation))),
      simulation_(ownedSimulation_.get()), permissions_(permissions) {
    CollectCompletedCommands();
}

Capabilities AgentApi::DiscoverCapabilities() const {
    Capabilities out;
    if (permissions_.queryWorld) {
        out.actions.push_back(Action::QueryWorld);
        out.actions.push_back(Action::QueryEntityPage);
        out.actions.push_back(Action::ReadEvents);
    }
    if (permissions_.stepSimulation) out.actions.push_back(Action::StepSimulation);
    if (permissions_.configureSimulation) out.actions.push_back(Action::ConfigureSimulation);
    if (permissions_.construct) {
        out.actions.push_back(Action::ConstructFacility);
        out.actions.push_back(Action::ConstructRoad);
        out.actions.push_back(Action::ConstructStorage);
    }
    if (permissions_.assignTransport) out.actions.push_back(Action::AssignTransportPlan);
    return out;
}

WorldQuery AgentApi::QueryWorld() const {
    WorldQuery query;
    if (!permissions_.queryWorld) {
        query.error = ErrorCode::PermissionDenied;
        return query;
    }

    const auto snapshot = simulation_->GetSnapshot();
    auto& out = query.world;
    out.revision = snapshot.stateRevision;
    out.tick = snapshot.tick;
    out.deliveredFlourTotal = snapshot.deliveredFlourTotal;
    out.rules = snapshot.rules;
    out.entities.reserve(snapshot.facilities.size() + snapshot.storages.size());
    for (const auto& facility : snapshot.facilities) {
        out.entities.push_back(Entity{facility.id, facility.type, true, false,
            facility.gridX, facility.gridZ, facility.inventory});
    }
    for (const auto& storage : snapshot.storages) {
        out.entities.push_back(Entity{storage.id, sim::FacilityType::Farm, false, true,
            storage.gridX, storage.gridZ, storage.inventory});
    }
    out.route.assignedPlan = snapshot.truckPlan;
    out.route.activeNodes = snapshot.activeRoute;
    out.route.cursor = snapshot.routeCursor;
    out.route.truckNode = snapshot.truckNode;
    out.route.cargo = snapshot.truckCargo;
    out.route.cargoAmount = snapshot.truckCargoAmount;
    out.route.stage = snapshot.truckStage;
    out.route.roads = snapshot.roads;
    return query;
}

SnapshotQuery AgentApi::QuerySnapshot() const {
    SnapshotQuery query;
    if (!permissions_.queryWorld) {
        query.error = ErrorCode::PermissionDenied;
        query.snapshot.rules = {0, 0, 0};
        query.snapshot.truckPlan = {0, 0, 0};
        return query;
    }
    query.snapshot = simulation_->GetSnapshot();
    return query;
}

SaveQuery AgentApi::SaveWorld() const {
    SaveQuery query;
    if (!permissions_.queryWorld) {
        query.error = ErrorCode::PermissionDenied;
        return query;
    }
    query.data = simulation_->Save();
    return query;
}

ErrorCode AgentApi::LoadWorld(std::uint64_t expectedRevision, std::string_view data,
    std::string* error) {
    if (!permissions_.configureSimulation) return ErrorCode::PermissionDenied;
    if (expectedRevision != simulation_->GetStateRevision()) return ErrorCode::StaleRevision;
    if (!simulation_->Load(data, error)) return ErrorCode::InvalidArgument;

    // A load can restore command IDs from an earlier timeline. Drop API-side
    // tracking from the replaced timeline, but preserve the API handle counter
    // so an old controller handle can never name a command in the loaded world.
    // Announce the new world before adopting any persisted completed results.
    commands_.clear();
    simulationCommandToApiCommand_.clear();
    eventJournal_.clear();
    RecordEvent(EventType::WorldLoaded);
    CollectCompletedCommands();
    return ErrorCode::None;
}

EntityPageQuery AgentApi::QueryEntityPage(std::uint64_t expectedRevision,
    std::size_t offset, std::size_t limit) const {
    EntityPageQuery query;
    if (!permissions_.queryWorld) {
        query.error = ErrorCode::PermissionDenied;
        return query;
    }
    const auto snapshot = simulation_->GetSnapshot();
    if (expectedRevision != snapshot.stateRevision) {
        query.error = ErrorCode::StaleRevision;
        return query;
    }
    if (limit == 0 || limit > MaxEntityPageSize) {
        query.error = ErrorCode::InvalidEntityPageLimit;
        return query;
    }

    std::vector<Entity> entities;
    entities.reserve(snapshot.facilities.size() + snapshot.storages.size());
    for (const auto& facility : snapshot.facilities) {
        entities.push_back(Entity{facility.id, facility.type, true, false,
            facility.gridX, facility.gridZ, facility.inventory});
    }
    for (const auto& storage : snapshot.storages) {
        entities.push_back(Entity{storage.id, sim::FacilityType::Farm, false, true,
            storage.gridX, storage.gridZ, storage.inventory});
    }
    std::sort(entities.begin(), entities.end(), [](const Entity& a, const Entity& b) {
        return a.id < b.id;
    });

    auto& page = query.page;
    page.revision = snapshot.stateRevision;
    page.offset = offset;
    page.nextOffset = offset;
    if (offset >= entities.size()) return query;

    const auto count = std::min(limit, entities.size() - offset);
    page.entities.insert(page.entities.end(), entities.begin() + static_cast<std::ptrdiff_t>(offset),
        entities.begin() + static_cast<std::ptrdiff_t>(offset + count));
    page.nextOffset = offset + count;
    page.hasMore = page.nextOffset < entities.size();
    return query;
}

EventPageQuery AgentApi::ReadEvents(std::uint64_t afterSequence, std::size_t limit) const {
    EventPageQuery query;
    if (!permissions_.queryWorld) {
        query.error = ErrorCode::PermissionDenied;
        return query;
    }
    if (limit == 0 || limit > MaxEventPageSize) {
        query.error = ErrorCode::InvalidEventPageLimit;
        return query;
    }
    if (afterSequence > lastEventSequence_) {
        query.error = ErrorCode::FutureEventCursor;
        return query;
    }

    auto& page = query.page;
    page.nextCursor = afterSequence;
    page.newestAvailableSequence = lastEventSequence_;
    if (eventJournal_.empty()) return query;
    page.oldestAvailableSequence = eventJournal_.front().sequence;
    page.cursorFellBehind = afterSequence < page.oldestAvailableSequence &&
        page.oldestAvailableSequence - afterSequence > 1;

    for (const auto& event : eventJournal_) {
        if (event.sequence <= afterSequence) continue;
        if (page.events.size() == limit) {
            page.hasMore = true;
            break;
        }
        page.events.push_back(event);
        page.nextCursor = event.sequence;
    }
    return query;
}

bool AgentApi::HasPermission(Action action) const noexcept {
    switch (action) {
    case Action::QueryWorld: return permissions_.queryWorld;
    case Action::QueryEntityPage:
    case Action::ReadEvents: return permissions_.queryWorld;
    case Action::StepSimulation: return permissions_.stepSimulation;
    case Action::ConfigureSimulation: return permissions_.configureSimulation;
    case Action::ConstructFacility:
    case Action::ConstructRoad:
    case Action::ConstructStorage: return permissions_.construct;
    case Action::AssignTransportPlan: return permissions_.assignTransport;
    }
    return false;
}

Submission AgentApi::CheckSubmission(Action action, std::uint64_t expectedRevision) const {
    if (!HasPermission(action)) return {ErrorCode::PermissionDenied, 0, CommandState::Unknown};
    if (expectedRevision != simulation_->GetStateRevision()) return {ErrorCode::StaleRevision, 0, CommandState::Unknown};
    if (commands_.size() >= MaxTrackedCommands)
        return {ErrorCode::CommandTrackingFull, 0, CommandState::Unknown};
    if (nextCommandHandle_ == std::numeric_limits<std::uint64_t>::max())
        return {ErrorCode::CommandIdExhausted, 0, CommandState::Unknown};
    return {ErrorCode::None, 0, CommandState::Unknown};
}

Submission AgentApi::Track(std::uint64_t commandId, sim::ConstructionCommandType type) {
    if (commandId == 0) return {ErrorCode::QueueFull, 0, CommandState::Unknown};
    const auto apiCommandId = nextCommandHandle_++;
    CommandResult result;
    result.commandId = apiCommandId;
    result.state = CommandState::Queued;
    result.type = type;
    commands_.emplace(apiCommandId, TrackedCommand{result, commandId, false});
    simulationCommandToApiCommand_.emplace(commandId, apiCommandId);
    RecordEvent(EventType::CommandSubmitted, apiCommandId, type);
    return {ErrorCode::None, apiCommandId, CommandState::Queued};
}

Submission AgentApi::PlaceFacility(std::uint64_t expectedRevision, sim::FacilityType type,
    std::int32_t gridX, std::int32_t gridZ) {
    auto check = CheckSubmission(Action::ConstructFacility, expectedRevision);
    if (check.error != ErrorCode::None) return check;
    if (!InBounds(gridX, gridZ) || (type != sim::FacilityType::Farm && type != sim::FacilityType::Mill))
        return {ErrorCode::InvalidArgument, 0, CommandState::Unknown};
    return Track(simulation_->QueueFacilityPlacement(type, gridX, gridZ),
        sim::ConstructionCommandType::FacilityPlacement);
}

Submission AgentApi::AddRoadSegment(std::uint64_t expectedRevision, std::int32_t ax,
    std::int32_t az, std::int32_t bx, std::int32_t bz) {
    auto check = CheckSubmission(Action::ConstructRoad, expectedRevision);
    if (check.error != ErrorCode::None) return check;
    if (!InBounds(ax, az) || !InBounds(bx, bz))
        return {ErrorCode::InvalidArgument, 0, CommandState::Unknown};
    const auto dx = static_cast<std::int64_t>(ax) - static_cast<std::int64_t>(bx);
    const auto dz = static_cast<std::int64_t>(az) - static_cast<std::int64_t>(bz);
    const auto absDx = dx < 0 ? -dx : dx;
    const auto absDz = dz < 0 ? -dz : dz;
    if (absDx + absDz != 1)
        return {ErrorCode::InvalidArgument, 0, CommandState::Unknown};
    return Track(simulation_->QueueRoadSegment(ax, az, bx, bz),
        sim::ConstructionCommandType::RoadSegment);
}

Submission AgentApi::PlaceStorage(std::uint64_t expectedRevision, std::int32_t gridX,
    std::int32_t gridZ) {
    auto check = CheckSubmission(Action::ConstructStorage, expectedRevision);
    if (check.error != ErrorCode::None) return check;
    if (!InBounds(gridX, gridZ)) return {ErrorCode::InvalidArgument, 0, CommandState::Unknown};
    return Track(simulation_->QueueStoragePlacement(gridX, gridZ),
        sim::ConstructionCommandType::StoragePlacement);
}

Submission AgentApi::AssignTransportPlan(std::uint64_t expectedRevision, const sim::TruckPlan& plan) {
    auto check = CheckSubmission(Action::AssignTransportPlan, expectedRevision);
    if (check.error != ErrorCode::None) return check;
    if (plan.farmId == 0 || plan.millId == 0 || plan.storageId == 0)
        return {ErrorCode::InvalidArgument, 0, CommandState::Unknown};
    return Track(simulation_->QueueTruckPlan(plan), sim::ConstructionCommandType::TruckPlan);
}

ErrorCode AgentApi::SetRoadEnabled(std::uint64_t expectedRevision, sim::EntityId edgeId, bool enabled) {
    if (!permissions_.configureSimulation) return ErrorCode::PermissionDenied;
    if (expectedRevision != simulation_->GetStateRevision()) return ErrorCode::StaleRevision;
    if (!simulation_->SetRoadEnabled(edgeId, enabled)) return ErrorCode::SimulationRejected;
    RecordEvent(EventType::SimulationConfigured);
    return ErrorCode::None;
}

ErrorCode AgentApi::SetSimulationRules(std::uint64_t expectedRevision,
    const sim::SimulationRules& rules, std::string* error) {
    if (!permissions_.configureSimulation) return ErrorCode::PermissionDenied;
    if (expectedRevision != simulation_->GetStateRevision()) return ErrorCode::StaleRevision;
    if (!simulation_->SetRules(rules, error)) return ErrorCode::InvalidArgument;
    RecordEvent(EventType::SimulationConfigured);
    return ErrorCode::None;
}

ErrorCode AgentApi::Step(std::uint64_t ticks) {
    if (!permissions_.stepSimulation) return ErrorCode::PermissionDenied;
    if (ticks > MaxStepTicks) return ErrorCode::StepLimitExceeded;
    if (ticks == 0) return ErrorCode::None;

    auto beforeTick = simulation_->GetSnapshot().tick;
    simulation_->Step(1);
    auto afterTick = simulation_->GetSnapshot().tick;
    if (afterTick > beforeTick) {
        RecordEvent(EventType::SimulationAdvanced, 0,
            sim::ConstructionCommandType::FacilityPlacement, sim::ConstructionFailureReason::None,
            afterTick - beforeTick);
    }
    CollectCompletedCommands();

    if (ticks > 1) {
        beforeTick = simulation_->GetSnapshot().tick;
        simulation_->Step(ticks - 1);
        afterTick = simulation_->GetSnapshot().tick;
        if (afterTick > beforeTick) {
            RecordEvent(EventType::SimulationAdvanced, 0,
                sim::ConstructionCommandType::FacilityPlacement, sim::ConstructionFailureReason::None,
                afterTick - beforeTick);
        }
        CollectCompletedCommands();
    }
    return ErrorCode::None;
}

void AgentApi::CollectCompletedCommands() {
    const auto snapshot = simulation_->GetSnapshot();
    for (const auto& result : snapshot.constructionResults) {
        auto apiIdIt = simulationCommandToApiCommand_.find(result.commandId);
        if (apiIdIt == simulationCommandToApiCommand_.end()) {
            if (!permissions_.queryWorld || commands_.size() >= MaxTrackedCommands ||
                nextCommandHandle_ == std::numeric_limits<std::uint64_t>::max()) {
                // Leave the result untouched. A later privileged API instance
                // can adopt it if the simulation owner retains this world.
                continue;
            }
            const auto apiCommandId = nextCommandHandle_++;
            CommandResult adopted;
            adopted.commandId = apiCommandId;
            adopted.type = result.type;
            adopted.state = result.accepted ? CommandState::Applied : CommandState::Rejected;
            adopted.error = result.accepted ? ErrorCode::None : ErrorCode::SimulationRejected;
            adopted.entityId = result.entityId;
            adopted.failureReason = result.failureReason;
            commands_.emplace(apiCommandId, TrackedCommand{adopted, result.commandId, true});
            simulationCommandToApiCommand_.emplace(result.commandId, apiCommandId);
            RecordEvent(result.accepted ? EventType::CommandApplied : EventType::CommandRejected,
                apiCommandId, result.type, result.failureReason);
        } else {
            auto it = commands_.find(apiIdIt->second);
            // Do not consume a simulation result if API tracking is inconsistent.
            if (it == commands_.end()) continue;
            if (!it->second.completed) {
                auto& tracked = it->second;
                tracked.completed = true;
                tracked.result.state = result.accepted ? CommandState::Applied : CommandState::Rejected;
                tracked.result.error = result.accepted ? ErrorCode::None : ErrorCode::SimulationRejected;
                tracked.result.entityId = result.entityId;
                tracked.result.failureReason = result.failureReason;
                RecordEvent(result.accepted ? EventType::CommandApplied : EventType::CommandRejected,
                    tracked.result.commandId, result.type, result.failureReason);
            }
        }
        // Never consume a result until it is queryable through the API.
        simulation_->AcknowledgeConstructionResult(result.commandId);
    }
}

void AgentApi::RecordEvent(EventType type, std::uint64_t commandId,
    sim::ConstructionCommandType commandType, sim::ConstructionFailureReason failureReason,
    std::uint64_t stepTicks) {
    if (lastEventSequence_ == std::numeric_limits<std::uint64_t>::max()) return;
    const auto snapshot = simulation_->GetSnapshot();
    AgentEvent event;
    event.sequence = ++lastEventSequence_;
    event.type = type;
    event.commandId = commandId;
    event.commandType = commandType;
    event.failureReason = failureReason;
    event.tick = snapshot.tick;
    event.revision = snapshot.stateRevision;
    event.stepTicks = stepTicks;
    if (eventJournal_.size() == MaxEventJournalEntries) eventJournal_.pop_front();
    eventJournal_.push_back(event);
}

CommandResult AgentApi::GetCommandResult(std::uint64_t commandId) const {
    const auto it = commands_.find(commandId);
    if (it == commands_.end()) {
        CommandResult result;
        result.commandId = commandId;
        result.error = ErrorCode::UnknownCommand;
        return result;
    }
    return it->second.result;
}

std::optional<std::uint64_t> AgentApi::LookupApiCommandHandleForSimulationCommand(
    std::uint64_t simulationCommandId) const {
    if (!permissions_.queryWorld) return std::nullopt;
    const auto it = simulationCommandToApiCommand_.find(simulationCommandId);
    if (it == simulationCommandToApiCommand_.end()) return std::nullopt;
    return it->second;
}

ErrorCode AgentApi::AcknowledgeCommandResult(std::uint64_t commandId) {
    const auto it = commands_.find(commandId);
    if (it == commands_.end()) return ErrorCode::UnknownCommand;
    if (!it->second.completed) return ErrorCode::CommandNotCompleted;
    simulationCommandToApiCommand_.erase(it->second.simulationCommandId);
    commands_.erase(it);
    return ErrorCode::None;
}

} // namespace openrepublic::agent_api
