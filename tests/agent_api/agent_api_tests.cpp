#include "openrepublic/agent_api/agent_api.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace openrepublic;

int main() {
    int failures = 0;
    const auto check = [&failures](bool condition, const char* message) {
        if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };

    agent_api::Permissions permissions;
    permissions.stepSimulation = true;
    permissions.assignTransport = true;
    agent_api::AgentApi api(permissions);
    const auto capabilities = api.DiscoverCapabilities();
    const auto hasAction = [&capabilities](agent_api::Action action) {
        return std::find(capabilities.actions.begin(), capabilities.actions.end(), action) != capabilities.actions.end();
    };
    check(capabilities.apiVersion == agent_api::ApiVersion && capabilities.actions.size() == 5 &&
        hasAction(agent_api::Action::QueryEntityPage) && hasAction(agent_api::Action::ReadEvents),
        "capability discovery explicitly lists paged entity and event queries under query permission");

    const auto worldQuery = api.QueryWorld();
    check(worldQuery.error == agent_api::ErrorCode::None, "permitted world query reports success explicitly");
    auto world = worldQuery.world;
    check(world.entities.size() >= 3 && world.route.assignedPlan.farmId == sim::Simulation::FarmId,
        "structured initial world and transport assignment are queryable");
    check(world.deliveredFlourTotal == 0 && world.rules.farmGrainPerHarvest == 2 &&
        world.rules.farmHarvestIntervalTicks == 2 && world.rules.millGrainPerTick == 1,
        "initial world query exposes cumulative production telemetry and active synthetic rules");
    bool foundFarm = false, foundMill = false, foundStorage = false;
    for (const auto& entity : world.entities) {
        foundFarm |= entity.id == sim::Simulation::FarmId && entity.isFacility && entity.facilityType == sim::FacilityType::Farm;
        foundMill |= entity.id == sim::Simulation::MillId && entity.isFacility && entity.facilityType == sim::FacilityType::Mill;
        foundStorage |= entity.id == sim::Simulation::WarehouseId && entity.isStorage;
    }
    check(foundFarm && foundMill && foundStorage, "entity inventory query uses stable simulation IDs");

    sim::Simulation configuredSimulation;
    const sim::SimulationRules configuredRules{3, 4, 2};
    check(configuredSimulation.SetRules(configuredRules), "telemetry fixture accepts a valid custom ruleset");
    const auto configuredSnapshot = configuredSimulation.GetSnapshot();
    agent_api::AgentApi configuredApi(std::move(configuredSimulation), permissions);
    const auto configuredWorld = configuredApi.QueryWorld().world;
    check(configuredWorld.deliveredFlourTotal == configuredSnapshot.deliveredFlourTotal &&
        configuredWorld.rules.farmGrainPerHarvest == configuredRules.farmGrainPerHarvest &&
        configuredWorld.rules.farmHarvestIntervalTicks == configuredRules.farmHarvestIntervalTicks &&
        configuredWorld.rules.millGrainPerTick == configuredRules.millGrainPerTick,
        "world query returns production counters and rules from the authoritative snapshot");

    agent_api::Permissions pendingPlanPermissions;
    pendingPlanPermissions.queryWorld = true;
    pendingPlanPermissions.assignTransport = true;
    pendingPlanPermissions.configureSimulation = true;
    agent_api::AgentApi pendingPlanApi(pendingPlanPermissions);
    const auto pendingPlanStart = pendingPlanApi.QuerySnapshot().snapshot;
    const auto emptyTimelineSave = pendingPlanApi.SaveWorld();
    const auto discardedTimelineSubmission = pendingPlanApi.AssignTransportPlan(pendingPlanStart.stateRevision,
        sim::TruckPlan{sim::Simulation::FarmId, sim::Simulation::MillId, sim::Simulation::WarehouseId});
    const auto beforeTimelineRestore = pendingPlanApi.QuerySnapshot().snapshot;
    std::string timelineRestoreError;
    const auto timelineRestoreErrorCode = pendingPlanApi.LoadWorld(beforeTimelineRestore.stateRevision,
        emptyTimelineSave.data, &timelineRestoreError);
    const auto restoredEmptyTimeline = pendingPlanApi.QuerySnapshot().snapshot;
    const auto pendingPlanSubmission = pendingPlanApi.AssignTransportPlan(restoredEmptyTimeline.stateRevision,
        sim::TruckPlan{sim::Simulation::FarmId, sim::Simulation::MillId, sim::Simulation::WarehouseId});
    const auto queuedPlanSnapshot = pendingPlanApi.QuerySnapshot().snapshot;
    const auto queuedSimulationCommandId = queuedPlanSnapshot.pendingTruckPlans.empty()
        ? 0 : queuedPlanSnapshot.pendingTruckPlans.front().commandId;
    check(emptyTimelineSave.error == agent_api::ErrorCode::None &&
        discardedTimelineSubmission.error == agent_api::ErrorCode::None &&
        timelineRestoreErrorCode == agent_api::ErrorCode::None &&
        restoredEmptyTimeline.stateRevision != 0 && restoredEmptyTimeline.pendingTruckPlans.empty() &&
        pendingPlanSubmission.error == agent_api::ErrorCode::None && queuedSimulationCommandId != 0 &&
        queuedSimulationCommandId != pendingPlanSubmission.commandId &&
        pendingPlanApi.LookupApiCommandHandleForSimulationCommand(queuedSimulationCommandId) ==
            pendingPlanSubmission.commandId,
        "pending plan lookup resolves differing simulation IDs and opaque API handles after loading an older timeline");
    const auto pendingPlanSave = pendingPlanApi.SaveWorld();
    const auto pendingPlanLoadRevision = pendingPlanApi.QuerySnapshot().snapshot.stateRevision;
    std::string pendingPlanLoadError;
    const auto pendingPlanLoadErrorCode = pendingPlanApi.LoadWorld(pendingPlanLoadRevision,
        pendingPlanSave.data, &pendingPlanLoadError);
    const auto restoredPlanSnapshot = pendingPlanApi.QuerySnapshot().snapshot;
    const auto restoredSimulationCommandId = restoredPlanSnapshot.pendingTruckPlans.empty()
        ? 0 : restoredPlanSnapshot.pendingTruckPlans.front().commandId;
    check(pendingPlanSave.error == agent_api::ErrorCode::None && pendingPlanLoadErrorCode == agent_api::ErrorCode::None &&
        restoredSimulationCommandId != 0 &&
        !pendingPlanApi.LookupApiCommandHandleForSimulationCommand(restoredSimulationCommandId).has_value(),
        "restored pending plan is not mislabeled with a handle from the replaced API timeline");

    sim::Simulation sharedSimulation;
    agent_api::Permissions sharedPermissions;
    sharedPermissions.queryWorld = true;
    sharedPermissions.stepSimulation = true;
    sharedPermissions.construct = true;
    sharedPermissions.assignTransport = true;
    sharedPermissions.configureSimulation = true;
    agent_api::AgentApi sharedApi(sharedSimulation, sharedPermissions);
    const auto sharedInitial = sharedApi.QuerySnapshot();
    const auto sharedCapabilities = sharedApi.DiscoverCapabilities();
    check(sharedInitial.error == agent_api::ErrorCode::None &&
        sharedInitial.snapshot.stateRevision == sharedSimulation.GetStateRevision() &&
        sharedInitial.snapshot.tick == sharedSimulation.GetSnapshot().tick &&
        std::find(sharedCapabilities.actions.begin(), sharedCapabilities.actions.end(),
            agent_api::Action::ConfigureSimulation) != sharedCapabilities.actions.end(),
        "injected API snapshot is the same authoritative simulation supplied by the app");
    const auto sharedStorage = sharedApi.PlaceStorage(sharedInitial.snapshot.stateRevision, 7, 7);
    check(sharedStorage.error == agent_api::ErrorCode::None &&
        sharedApi.SetRoadEnabled(sharedInitial.snapshot.stateRevision, 202, false) ==
            agent_api::ErrorCode::StaleRevision &&
        sharedApi.Step() == agent_api::ErrorCode::None,
        "injected API accepts and advances a player-storage command");
    const auto sharedAfterPlacement = sharedSimulation.GetSnapshot();
    const auto sharedCommandResult = sharedApi.GetCommandResult(sharedStorage.commandId);
    check(sharedAfterPlacement.tick == 1 &&
        std::any_of(sharedAfterPlacement.storages.begin(), sharedAfterPlacement.storages.end(),
            [](const sim::PlacedStorageView& storage) { return storage.gridX == 7 && storage.gridZ == 7; }) &&
        sharedCommandResult.state == agent_api::CommandState::Applied,
        "API command outcome and external app-owned simulation state agree");
    const auto sharedSave = sharedApi.SaveWorld();
    const auto secondStorage = sharedApi.PlaceStorage(sharedApi.QuerySnapshot().snapshot.stateRevision, 8, 8);
    check(sharedSave.error == agent_api::ErrorCode::None && secondStorage.error == agent_api::ErrorCode::None &&
        sharedApi.Step() == agent_api::ErrorCode::None,
        "shared-world save and later mutation both use the typed API");
    const auto beforeLoadRevision = sharedApi.QuerySnapshot().snapshot.stateRevision;
    std::string sharedLoadError;
    check(sharedApi.LoadWorld(beforeLoadRevision, sharedSave.data, &sharedLoadError) ==
        agent_api::ErrorCode::None && sharedApi.QuerySnapshot().snapshot.storages.size() == 2 &&
        sharedApi.GetCommandResult(secondStorage.commandId).error == agent_api::ErrorCode::UnknownCommand,
        "typed API load restores the earlier shared simulation state and clears replaced command tracking");
    const auto reusedTimelineStorage = sharedApi.PlaceStorage(
        sharedApi.QuerySnapshot().snapshot.stateRevision, 8, 8);
    check(reusedTimelineStorage.error == agent_api::ErrorCode::None &&
        reusedTimelineStorage.commandId != secondStorage.commandId &&
        sharedApi.Step() == agent_api::ErrorCode::None &&
        sharedApi.GetCommandResult(secondStorage.commandId).error == agent_api::ErrorCode::UnknownCommand &&
        sharedApi.GetCommandResult(reusedTimelineStorage.commandId).state == agent_api::CommandState::Applied,
        "a loaded timeline may reuse simulation IDs without reusing API handles or reviving old results");
    const auto configuredThroughApi = sharedApi.QuerySnapshot().snapshot;
    auto apiRules = configuredThroughApi.rules;
    ++apiRules.farmGrainPerHarvest;
    check(sharedApi.SetSimulationRules(configuredThroughApi.stateRevision, apiRules) == agent_api::ErrorCode::None &&
        sharedSimulation.GetRules().farmGrainPerHarvest == apiRules.farmGrainPerHarvest,
        "typed API configuration updates the same simulation instance");

    const auto denied = api.PlaceStorage(world.revision, 0, 0);
    check(denied.error == agent_api::ErrorCode::PermissionDenied, "construction permission is enforced");
    check(api.SetSimulationRules(world.revision, world.rules) == agent_api::ErrorCode::PermissionDenied,
        "simulation configuration permission is enforced independently of read permission");
    check(api.Step() == agent_api::ErrorCode::None, "permitted controller can advance fixed simulation ticks");
    const auto stale = api.AssignTransportPlan(world.revision, sim::TruckPlan{});
    check(stale.error == agent_api::ErrorCode::StaleRevision, "stale world revision is rejected");

    world = api.QueryWorld().world;
    agent_api::Permissions constructionPermissions;
    constructionPermissions.queryWorld = true;
    constructionPermissions.stepSimulation = true;
    constructionPermissions.construct = true;
    constructionPermissions.assignTransport = true;
    agent_api::AgentApi constructionApi(constructionPermissions);
    world = constructionApi.QueryWorld().world;
    const auto invalid = constructionApi.AddRoadSegment(world.revision, 0, 0, 2, 2);
    check(invalid.error == agent_api::ErrorCode::InvalidArgument, "invalid road geometry gets a structured validation code");
    const auto extremeX = constructionApi.AddRoadSegment(world.revision,
        std::numeric_limits<std::int32_t>::min(), 0,
        std::numeric_limits<std::int32_t>::max(), 0);
    const auto extremeZ = constructionApi.AddRoadSegment(world.revision, 0,
        std::numeric_limits<std::int32_t>::min(), 0,
        std::numeric_limits<std::int32_t>::max());
    check(extremeX.error == agent_api::ErrorCode::InvalidArgument &&
        extremeZ.error == agent_api::ErrorCode::InvalidArgument,
        "road validation rejects int32 extremes without signed-overflow arithmetic");
    const auto plan = constructionApi.AssignTransportPlan(world.revision,
        sim::TruckPlan{sim::Simulation::FarmId, sim::Simulation::MillId, sim::Simulation::WarehouseId});
    check(plan.error == agent_api::ErrorCode::None && plan.state == agent_api::CommandState::Queued,
        "accepted command is explicitly queued before application");
    check(constructionApi.GetCommandResult(plan.commandId).state == agent_api::CommandState::Queued,
        "queued command is not reported as already applied");
    const auto queuedRevision = constructionApi.QueryWorld().world.revision;
    check(queuedRevision > world.revision, "same-tick queue submission advances the world revision immediately");
    constructionApi.Step(1);
    check(constructionApi.GetCommandResult(plan.commandId).state == agent_api::CommandState::Applied,
        "command result transitions to applied at a simulation tick boundary");

    constructionApi.Step(400);
    world = constructionApi.QueryWorld().world;
    bool delivered = false;
    std::uint32_t warehouseFlour = 0;
    for (const auto& entity : world.entities) {
        if (entity.id == sim::Simulation::WarehouseId) {
            warehouseFlour = entity.inventory.flour;
            delivered = warehouseFlour > 0;
        }
    }
    check(delivered, "headless Farm to Mill to Storage controller loop completes without UI");
    check(world.deliveredFlourTotal > 0 && world.deliveredFlourTotal >= warehouseFlour,
        "world query exposes cumulative flour deliveries consistent with warehouse stock");

    sim::Simulation rejectionSimulation;
    rejectionSimulation.QueueStoragePlacement(0, 0);
    agent_api::Permissions all;
    all.stepSimulation = true; all.construct = true; all.assignTransport = true;
    agent_api::AgentApi rejectionApi(std::move(rejectionSimulation), all);
    const auto rejectedSubmission = rejectionApi.PlaceStorage(rejectionApi.QueryWorld().world.revision, 0, 0);
    check(rejectedSubmission.error == agent_api::ErrorCode::None, "simulation validation remains asynchronous after admission");
    rejectionApi.Step();
    const auto rejected = rejectionApi.GetCommandResult(rejectedSubmission.commandId);
    check(rejected.state == agent_api::CommandState::Rejected && rejected.error == agent_api::ErrorCode::SimulationRejected,
        "simulation rejection is distinct from queue acceptance");
    check(rejected.failureReason == sim::ConstructionFailureReason::OccupiedCell,
        "execution rejection carries the simulation's concrete occupied-cell diagnostic");

    sim::Simulation revisions;
    const auto initialRevision = revisions.GetStateRevision();
    auto changedRules = revisions.GetRules();
    ++changedRules.farmGrainPerHarvest;
    revisions.SetRules(changedRules);
    const auto rulesRevision = revisions.GetStateRevision();
    revisions.QueueStoragePlacement(8, 8);
    const auto queuedStateRevision = revisions.GetStateRevision();
    revisions.Step();
    const auto steppedRevision = revisions.GetStateRevision();
    std::string save = revisions.Save();
    revisions.Load(save);
    check(rulesRevision > initialRevision && queuedStateRevision > rulesRevision && steppedRevision > queuedStateRevision &&
        revisions.GetStateRevision() > steppedRevision,
        "simulation revisions advance on queue, tick, and successful load without save-format data");

    sim::Simulation legacyDiagnosticSource;
    const auto rejectedStorageId = legacyDiagnosticSource.QueueStoragePlacement(0, 0);
    legacyDiagnosticSource.Step();
    const auto rejectedSave = legacyDiagnosticSource.Save();
    sim::Simulation legacyDiagnosticRestore;
    legacyDiagnosticRestore.Load(rejectedSave);
    const auto restoredResult = legacyDiagnosticRestore.GetSnapshot().constructionResults.front();
    check(restoredResult.commandId == rejectedStorageId &&
        restoredResult.failureReason == sim::ConstructionFailureReason::DiagnosticUnavailableFromSave,
        "legacy-compatible save retains rejection state while identifying unavailable historical diagnostics");

    agent_api::Permissions queryDeniedPermissions;
    queryDeniedPermissions.queryWorld = false;
    agent_api::AgentApi queryDeniedApi(queryDeniedPermissions);
    const auto deniedQuery = queryDeniedApi.QueryWorld();
    check(deniedQuery.error == agent_api::ErrorCode::PermissionDenied && deniedQuery.world.revision == 0 &&
        deniedQuery.world.tick == 0 && deniedQuery.world.entities.empty() && deniedQuery.world.route.roads.empty() &&
        deniedQuery.world.deliveredFlourTotal == 0 && deniedQuery.world.rules.farmGrainPerHarvest == 0 &&
        deniedQuery.world.rules.farmHarvestIntervalTicks == 0 && deniedQuery.world.rules.millGrainPerTick == 0 &&
        deniedQuery.world.route.assignedPlan.farmId == 0 && deniedQuery.world.route.assignedPlan.millId == 0 &&
        deniedQuery.world.route.assignedPlan.storageId == 0 && deniedQuery.world.route.truckNode == 0,
        "denied world query returns explicit permission status without exposing state or revision");
    const auto deniedSnapshot = queryDeniedApi.QuerySnapshot();
    check(deniedSnapshot.error == agent_api::ErrorCode::PermissionDenied &&
        deniedSnapshot.snapshot.tick == 0 && deniedSnapshot.snapshot.facilities.empty() &&
        deniedSnapshot.snapshot.storages.empty() && deniedSnapshot.snapshot.rules.farmGrainPerHarvest == 0 &&
        deniedSnapshot.snapshot.truckPlan.farmId == 0 && deniedSnapshot.snapshot.truckPlan.millId == 0 &&
        deniedSnapshot.snapshot.truckPlan.storageId == 0,
        "full in-process snapshot query also returns no state when query permission is denied");
    const auto deniedCapabilities = queryDeniedApi.DiscoverCapabilities();
    check(std::find(deniedCapabilities.actions.begin(), deniedCapabilities.actions.end(), agent_api::Action::QueryWorld) == deniedCapabilities.actions.end() &&
        std::find(deniedCapabilities.actions.begin(), deniedCapabilities.actions.end(), agent_api::Action::QueryEntityPage) == deniedCapabilities.actions.end() &&
        std::find(deniedCapabilities.actions.begin(), deniedCapabilities.actions.end(), agent_api::Action::ReadEvents) == deniedCapabilities.actions.end(),
        "capability discovery omits all query actions when world-query permission is denied");

    agent_api::Permissions boundedStepPermissions;
    boundedStepPermissions.queryWorld = true;
    boundedStepPermissions.stepSimulation = true;
    agent_api::AgentApi boundedStepApi(boundedStepPermissions);
    check(boundedStepApi.Step(0) == agent_api::ErrorCode::None && boundedStepApi.QueryWorld().world.tick == 0,
        "zero-tick step is a successful no-op");
    check(boundedStepApi.Step(agent_api::MaxStepTicks) == agent_api::ErrorCode::None &&
        boundedStepApi.QueryWorld().world.tick == agent_api::MaxStepTicks,
        "step accepts the documented maximum tick batch");
    const auto maxStepWorld = boundedStepApi.QueryWorld().world;
    check(boundedStepApi.Step(agent_api::MaxStepTicks + 1) == agent_api::ErrorCode::StepLimitExceeded &&
        boundedStepApi.QueryWorld().world.tick == maxStepWorld.tick &&
        boundedStepApi.QueryWorld().world.revision == maxStepWorld.revision,
        "step rejects max plus one with a distinct code and no state mutation");

    agent_api::Permissions trackedPermissions;
    trackedPermissions.queryWorld = true;
    trackedPermissions.stepSimulation = true;
    trackedPermissions.assignTransport = true;
    agent_api::AgentApi trackedApi(trackedPermissions);
    std::uint64_t firstTrackedCommand = 0;
    for (std::size_t i = 0; i < agent_api::MaxTrackedCommands; ++i) {
        const auto current = trackedApi.QueryWorld();
        const auto submission = trackedApi.AssignTransportPlan(current.world.revision,
            sim::TruckPlan{sim::Simulation::FarmId, sim::Simulation::MillId, sim::Simulation::WarehouseId});
        check(submission.error == agent_api::ErrorCode::None, "command admitted while tracked-result capacity remains");
        if (i == 0) firstTrackedCommand = submission.commandId;
        trackedApi.Step();
    }
    const auto fullTrackingWorld = trackedApi.QueryWorld().world;
    const auto trackingFull = trackedApi.AssignTransportPlan(fullTrackingWorld.revision,
        sim::TruckPlan{sim::Simulation::FarmId, sim::Simulation::MillId, sim::Simulation::WarehouseId});
    check(trackingFull.error == agent_api::ErrorCode::CommandTrackingFull &&
        trackedApi.QueryWorld().world.revision == fullTrackingWorld.revision,
        "tracked command memory has a hard bound and rejects before simulation submission");
    check(trackedApi.AcknowledgeCommandResult(firstTrackedCommand) == agent_api::ErrorCode::None &&
        trackedApi.GetCommandResult(firstTrackedCommand).error == agent_api::ErrorCode::UnknownCommand,
        "acknowledging a completed result releases it and subsequent lookup is unknown");
    const auto afterReleaseWorld = trackedApi.QueryWorld().world;
    const auto afterRelease = trackedApi.AssignTransportPlan(afterReleaseWorld.revision,
        sim::TruckPlan{sim::Simulation::FarmId, sim::Simulation::MillId, sim::Simulation::WarehouseId});
    check(afterRelease.error == agent_api::ErrorCode::None &&
        trackedApi.AcknowledgeCommandResult(afterRelease.commandId) == agent_api::ErrorCode::CommandNotCompleted,
        "acknowledgement releases capacity but is rejected while a command is still queued");

    agent_api::Permissions pagePermissions;
    pagePermissions.queryWorld = true;
    pagePermissions.stepSimulation = true;
    pagePermissions.construct = true;
    agent_api::AgentApi pageApi(pagePermissions);
    auto pageWorld = pageApi.QueryWorld().world;
    const auto facilityPlacement = pageApi.PlaceFacility(pageWorld.revision, sim::FacilityType::Farm, -4, 2);
    pageWorld = pageApi.QueryWorld().world;
    const auto storagePlacement = pageApi.PlaceStorage(pageWorld.revision, 5, 5);
    check(facilityPlacement.error == agent_api::ErrorCode::None && storagePlacement.error == agent_api::ErrorCode::None,
        "entity-page fixture queues a facility and storage at one revision");
    pageApi.Step();
    pageWorld = pageApi.QueryWorld().world;
    const auto firstPage = pageApi.QueryEntityPage(pageWorld.revision, 0, 2);
    const auto secondPage = pageApi.QueryEntityPage(pageWorld.revision, 2, 2);
    const auto thirdPage = pageApi.QueryEntityPage(pageWorld.revision, 4, 2);
    check(firstPage.error == agent_api::ErrorCode::None && firstPage.page.entities.size() == 2 &&
        firstPage.page.entities[0].id == sim::Simulation::FarmId && firstPage.page.entities[1].id == sim::Simulation::MillId &&
        firstPage.page.hasMore && firstPage.page.nextOffset == 2,
        "entity page starts at offset zero and reports continuation");
    check(secondPage.error == agent_api::ErrorCode::None && secondPage.page.entities.size() == 2 &&
        secondPage.page.entities[0].id == sim::Simulation::WarehouseId &&
        secondPage.page.entities[1].id == sim::Simulation::FirstPrototypeStorageId &&
        secondPage.page.hasMore && secondPage.page.nextOffset == 4,
        "entity pages use stable ascending ID order across entity types");
    check(thirdPage.error == agent_api::ErrorCode::None && thirdPage.page.entities.size() == 1 &&
        thirdPage.page.entities[0].id == sim::Simulation::FirstFacilityId &&
        !thirdPage.page.hasMore && thirdPage.page.nextOffset == 5,
        "final entity page reports the exact next offset and no continuation");
    const auto pastEndPage = pageApi.QueryEntityPage(pageWorld.revision, 5, 2);
    check(pastEndPage.error == agent_api::ErrorCode::None && pastEndPage.page.entities.empty() &&
        pastEndPage.page.nextOffset == 5 && !pastEndPage.page.hasMore,
        "entity page at collection end is an empty terminal page");
    check(pageApi.QueryEntityPage(pageWorld.revision, 0, 0).error == agent_api::ErrorCode::InvalidEntityPageLimit &&
        pageApi.QueryEntityPage(pageWorld.revision, 0, agent_api::MaxEntityPageSize + 1).error ==
            agent_api::ErrorCode::InvalidEntityPageLimit,
        "entity page rejects zero and oversized page limits explicitly");
    check(pageApi.QueryEntityPage(pageWorld.revision - 1, 0, 1).error == agent_api::ErrorCode::StaleRevision,
        "entity page rejects stale state revisions");

    agent_api::Permissions eventPermissions;
    eventPermissions.queryWorld = true;
    eventPermissions.stepSimulation = true;
    eventPermissions.construct = true;
    agent_api::AgentApi eventApi(eventPermissions);
    auto eventWorld = eventApi.QueryWorld().world;
    const auto eventStorage = eventApi.PlaceStorage(eventWorld.revision, 5, 5);
    eventApi.Step();
    eventWorld = eventApi.QueryWorld().world;
    const auto rejectedStorageEvent = eventApi.PlaceStorage(eventWorld.revision, 5, 5);
    eventApi.Step();
    const auto allEvents = eventApi.ReadEvents(0, agent_api::MaxEventPageSize);
    check(allEvents.error == agent_api::ErrorCode::None && allEvents.page.events.size() == 6,
        "event journal records submissions, tick advancement, and both command outcomes");
    if (allEvents.page.events.size() == 6) {
        check(allEvents.page.events[0].sequence == 1 &&
            allEvents.page.events[0].type == agent_api::EventType::CommandSubmitted &&
            allEvents.page.events[0].commandId == eventStorage.commandId,
            "successful command submission receives the first event sequence");
        check(allEvents.page.events[1].type == agent_api::EventType::SimulationAdvanced &&
            allEvents.page.events[1].stepTicks == 1 &&
            allEvents.page.events[2].type == agent_api::EventType::CommandApplied &&
            allEvents.page.events[2].commandId == eventStorage.commandId,
            "simulation advancement precedes its applied command outcome in the journal");
        check(allEvents.page.events[3].type == agent_api::EventType::CommandSubmitted &&
            allEvents.page.events[3].commandId == rejectedStorageEvent.commandId &&
            allEvents.page.events[4].type == agent_api::EventType::SimulationAdvanced &&
            allEvents.page.events[5].type == agent_api::EventType::CommandRejected,
            "rejected command also produces submission, advancement, and rejection events");
    }
    const auto eventFirstPage = eventApi.ReadEvents(0, 2);
    const auto eventSecondPage = eventApi.ReadEvents(eventFirstPage.page.nextCursor, 2);
    check(eventFirstPage.error == agent_api::ErrorCode::None && eventFirstPage.page.events.size() == 2 &&
        eventFirstPage.page.hasMore && eventFirstPage.page.nextCursor == 2 &&
        eventSecondPage.page.events.size() == 2 && eventSecondPage.page.events.front().sequence == 3,
        "event cursor pages continue strictly after the returned sequence");
    check(eventApi.ReadEvents(0, 0).error == agent_api::ErrorCode::InvalidEventPageLimit &&
        eventApi.ReadEvents(0, agent_api::MaxEventPageSize + 1).error == agent_api::ErrorCode::InvalidEventPageLimit,
        "event reads reject zero and oversized page limits explicitly");
    const auto futureCursor = eventApi.ReadEvents(allEvents.page.newestAvailableSequence + 1, 1);
    check(futureCursor.error == agent_api::ErrorCode::FutureEventCursor,
        "event reads reject a cursor beyond the newest sequence");

    agent_api::Permissions deniedPagedPermissions;
    deniedPagedPermissions.queryWorld = false;
    agent_api::AgentApi deniedPagedApi(deniedPagedPermissions);
    const auto deniedEntityPage = deniedPagedApi.QueryEntityPage(0, 0, 1);
    const auto deniedEventPage = deniedPagedApi.ReadEvents(0, 1);
    check(deniedEntityPage.error == agent_api::ErrorCode::PermissionDenied &&
        deniedEntityPage.page.revision == 0 && deniedEntityPage.page.entities.empty() &&
        deniedEventPage.error == agent_api::ErrorCode::PermissionDenied && deniedEventPage.page.events.empty(),
        "paged entity and event queries obey query permission without state disclosure");

    agent_api::Permissions journalPermissions;
    journalPermissions.queryWorld = true;
    journalPermissions.stepSimulation = true;
    journalPermissions.assignTransport = true;
    agent_api::AgentApi journalApi(journalPermissions);
    for (std::size_t i = 0; i < 44; ++i) {
        const auto current = journalApi.QueryWorld().world;
        const auto submitted = journalApi.AssignTransportPlan(current.revision,
            sim::TruckPlan{sim::Simulation::FarmId, sim::Simulation::MillId, sim::Simulation::WarehouseId});
        journalApi.Step();
        check(submitted.error == agent_api::ErrorCode::None &&
            journalApi.AcknowledgeCommandResult(submitted.commandId) == agent_api::ErrorCode::None,
            "bounded journal fixture submits, completes, and acknowledges each controller command");
    }
    const auto expiredCursor = journalApi.ReadEvents(0, agent_api::MaxEventPageSize);
    check(expiredCursor.error == agent_api::ErrorCode::None && expiredCursor.page.cursorFellBehind &&
        expiredCursor.page.events.size() == agent_api::MaxEventPageSize &&
        expiredCursor.page.oldestAvailableSequence > 1 &&
        expiredCursor.page.newestAvailableSequence - expiredCursor.page.oldestAvailableSequence + 1 ==
            agent_api::MaxEventJournalEntries,
        "bounded event journal reports an expired cursor and retained sequence window");

    sim::Simulation surfacedResultSimulation;
    for (std::size_t i = 0; i < 3; ++i) {
        const auto retiredCommand = surfacedResultSimulation.QueueStoragePlacement(0, 0);
        surfacedResultSimulation.Step();
        surfacedResultSimulation.AcknowledgeConstructionResult(retiredCommand);
    }
    const auto surfacedCommand = surfacedResultSimulation.QueueStoragePlacement(0, 0);
    surfacedResultSimulation.Step();
    agent_api::AgentApi adoptedApi(std::move(surfacedResultSimulation), all);
    const auto adoptedEvents = adoptedApi.ReadEvents(0, 2);
    const auto adoptedApiCommand = adoptedEvents.page.events.empty() ? 0 : adoptedEvents.page.events[0].commandId;
    const auto adoptedResult = adoptedApi.GetCommandResult(adoptedApiCommand);
    check(adoptedResult.state == agent_api::CommandState::Rejected &&
        adoptedResult.failureReason == sim::ConstructionFailureReason::OccupiedCell &&
        adoptedApiCommand != surfacedCommand,
        "API adoption assigns an opaque handle to a pre-existing simulation result before acknowledging it");
    check(adoptedEvents.page.events.size() == 1 &&
        adoptedEvents.page.events[0].type == agent_api::EventType::CommandRejected &&
        adoptedEvents.page.events[0].commandId == adoptedApiCommand,
        "adopted result event carries the same API handle as its queryable result");

    agent_api::Permissions batchedPermissions;
    batchedPermissions.queryWorld = true;
    batchedPermissions.stepSimulation = true;
    batchedPermissions.construct = true;
    agent_api::AgentApi batchedApi(batchedPermissions);
    const auto batchStart = batchedApi.QueryWorld().world;
    const auto batchedCommand = batchedApi.PlaceStorage(batchStart.revision, 5, 5);
    const auto batchStepError = batchedApi.Step(4);
    const auto batchEvents = batchedApi.ReadEvents(0, agent_api::MaxEventPageSize);
    check(batchStepError == agent_api::ErrorCode::None && batchEvents.page.events.size() == 4,
        "batched step journals first tick, command outcome, and remaining advancement separately");
    if (batchEvents.page.events.size() == 4) {
        check(batchEvents.page.events[1].type == agent_api::EventType::SimulationAdvanced &&
            batchEvents.page.events[1].tick == batchStart.tick + 1 &&
            batchEvents.page.events[1].stepTicks == 1,
            "first batched advance event is timestamped at the first executed tick");
        check(batchEvents.page.events[2].type == agent_api::EventType::CommandApplied &&
            batchEvents.page.events[2].commandId == batchedCommand.commandId &&
            batchEvents.page.events[2].tick == batchStart.tick + 1,
            "batched command outcome is journaled at its actual first-tick execution");
        check(batchEvents.page.events[3].type == agent_api::EventType::SimulationAdvanced &&
            batchEvents.page.events[3].tick == batchStart.tick + 4 &&
            batchEvents.page.events[3].stepTicks == 3,
            "remaining batched advancement is journaled at the final tick");
    }

    sim::Simulation hiddenResultSimulation;
    const auto hiddenResultCommand = hiddenResultSimulation.QueueStoragePlacement(0, 0);
    hiddenResultSimulation.Step();
    const auto preAdoptionRevision = hiddenResultSimulation.GetStateRevision();
    agent_api::Permissions hiddenQueryPermissions;
    hiddenQueryPermissions.queryWorld = false;
    hiddenQueryPermissions.stepSimulation = true;
    hiddenQueryPermissions.assignTransport = true;
    agent_api::AgentApi hiddenResultApi(std::move(hiddenResultSimulation), hiddenQueryPermissions);
    check(hiddenResultApi.GetCommandResult(hiddenResultCommand).error == agent_api::ErrorCode::UnknownCommand &&
        hiddenResultApi.ReadEvents(0, 1).error == agent_api::ErrorCode::PermissionDenied,
        "query-denied API does not adopt or expose a transferred pre-existing result");
    auto hiddenRevision = preAdoptionRevision;
    for (std::size_t i = 0; i < sim::Simulation::MaxUnacknowledgedFacilityResults - 1; ++i) {
        const auto hiddenSubmission = hiddenResultApi.AssignTransportPlan(hiddenRevision,
            sim::TruckPlan{sim::Simulation::FarmId, sim::Simulation::MillId, sim::Simulation::WarehouseId});
        check(hiddenSubmission.error == agent_api::ErrorCode::None,
            "query-denied caller can submit its own command using its current revision token");
        ++hiddenRevision;
    }
    hiddenResultApi.Step();
    hiddenRevision += 1 + (sim::Simulation::MaxUnacknowledgedFacilityResults - 1);
    for (std::size_t i = 0; i < sim::Simulation::MaxUnacknowledgedFacilityResults - 1; ++i) {
        const auto hiddenSubmission = hiddenResultApi.AssignTransportPlan(hiddenRevision,
            sim::TruckPlan{sim::Simulation::FarmId, sim::Simulation::MillId, sim::Simulation::WarehouseId});
        check(hiddenSubmission.error == agent_api::ErrorCode::None,
            "unqueryable pre-existing result remains alongside API-tracked submissions");
        ++hiddenRevision;
    }
    const auto hiddenCapacity = hiddenResultApi.AssignTransportPlan(hiddenRevision,
        sim::TruckPlan{sim::Simulation::FarmId, sim::Simulation::MillId, sim::Simulation::WarehouseId});
    check(hiddenCapacity.error == agent_api::ErrorCode::QueueFull,
        "CollectCompletedCommands preserves untracked transferred results without query permission");

    agent_api::Permissions privateCommandPermissions;
    privateCommandPermissions.queryWorld = false;
    privateCommandPermissions.stepSimulation = true;
    privateCommandPermissions.construct = true;
    agent_api::AgentApi privateCommandApi(privateCommandPermissions);
    const auto privateSubmission = privateCommandApi.PlaceStorage(1, 5, 5);
    privateCommandApi.Step();
    check(privateSubmission.error == agent_api::ErrorCode::None &&
        privateCommandApi.GetCommandResult(privateSubmission.commandId).state == agent_api::CommandState::Applied,
        "a submitter can query results for commands it submitted even without world-query permission");

    if (failures) return EXIT_FAILURE;
    std::cout << "All OpenRepublic Agent API checks passed.\n";
    return EXIT_SUCCESS;
}
