#include "openrepublic/economy/checkpoint.h"
#include "openrepublic/economy_api/economy_api.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace openrepublic;
namespace {
void Check(bool ok, const char* text) {
    if (!ok) {
        std::cerr << "FAIL: " << text << '\n';
        std::exit(1);
    }
}
std::uint64_t NewestEvent(const economy_api::EconomyApi& api) {
    const auto events = api.ReadEvents(0, economy_api::MaxEventPageSize);
    Check(events.error == economy_api::ErrorCode::None, "read event cursor");
    return events.page.newestAvailableSequence;
}
}

int main() {
    auto catalog = economy::MakeSyntheticCatalog();
    economy::World world(catalog, 64, 64);
    economy_api::EconomyApi api(world, {true, true});

    const auto initial = api.QueryWorld();
    Check(initial.error == economy_api::ErrorCode::None &&
        initial.world.commandHistory.nextCommandId == 1 &&
        initial.world.commandHistory.retainedTerminalResults == 0 &&
        initial.world.commandHistory.pendingCommands == 0,
        "world view exposes initial command history metadata");
    const auto capabilities = api.DiscoverCapabilities();
    bool advertisesCompaction = false;
    for (const auto action : capabilities.actions)
        advertisesCompaction |= action == economy_api::Action::CompactCommandHistory;
    Check(advertisesCompaction, "controller advertises history compaction");

    economy_api::EconomyApi observer(world, {true, false});
    const auto denied = observer.CompactCommandHistory(world.Revision(), 1);
    Check(denied.error == economy_api::ErrorCode::PermissionDenied,
        "query-only observer cannot compact");
    economy_api::EconomyApi noQuery(world, {false, true});
    Check(noQuery.CompactCommandHistory(world.Revision(), 1).error ==
        economy_api::ErrorCode::PermissionDenied,
        "control without query permission cannot compact");
    Check(api.CompactCommandHistory(world.Revision() + 1,
        economy::MaxCommandHistoryRetention + 1).error ==
        economy_api::ErrorCode::StaleRevision,
        "stale revision takes precedence over invalid retention");
    const auto invalid = api.CompactCommandHistory(world.Revision(),
        economy::MaxCommandHistoryRetention + 1);
    Check(invalid.error == economy_api::ErrorCode::InvalidArgument &&
        invalid.removedResults == 0 && invalid.expiredHandles == 0,
        "retention above the core bound is rejected without compaction");

    auto accepted = api.PlaceBuilding(world.Revision(), "farm", {4, 4});
    Check(accepted.error == economy_api::ErrorCode::None && api.Step() ==
        economy_api::ErrorCode::None, "accepted command reaches terminal state");
    Check(api.PollCommand(accepted.handle).result.state == economy::CommandState::Accepted,
        "accepted result is pollable before compaction");

    auto rejected = api.SetBuildingEnabled(world.Revision(), 999999, false);
    Check(rejected.error == economy_api::ErrorCode::None && api.Step() ==
        economy_api::ErrorCode::None, "invalid target is accepted then rejected by the world");
    Check(api.PollCommand(rejected.handle).result.state == economy::CommandState::Rejected,
        "rejected result is pollable before compaction");

    auto pending = api.PlaceBuilding(world.Revision(), "mill", {12, 12});
    Check(pending.error == economy_api::ErrorCode::None &&
        api.PollCommand(pending.handle).result.state == economy::CommandState::Queued,
        "third command stays queued before compaction");
    const auto eventsBefore = NewestEvent(api);
    const auto revisionBefore = world.Revision();
    const auto compacted = api.CompactCommandHistory(revisionBefore, 1);
    Check(compacted.error == economy_api::ErrorCode::None && compacted.removedResults == 1 &&
        compacted.expiredHandles == 1 && compacted.revision == revisionBefore + 1,
        "compaction removes the old terminal prefix and advances revision");
    Check(compacted.history.retainedTerminalResults == 1 &&
        compacted.history.pendingCommands == 1 &&
        compacted.history.nextCommandId == pending.handle + 1,
        "compaction preserves the recent terminal result and every queued command");
    Check(NewestEvent(api) == eventsBefore,
        "compaction does not fabricate API events");
    const auto expired = api.PollCommand(accepted.handle);
    Check(expired.error == economy_api::ErrorCode::CommandExpired &&
        expired.result.commandId == 0 &&
        expired.result.state == economy::CommandState::Expired &&
        expired.result.failure == economy::CommandFailure::None &&
        expired.result.message == "command handle expired",
        "expired issued handle returns synthesized expiration without a retired native ID");
    Check(api.PollCommand(rejected.handle).error == economy_api::ErrorCode::None &&
        api.PollCommand(rejected.handle).result.state == economy::CommandState::Rejected,
        "recent rejected result remains pollable");
    Check(api.PollCommand(pending.handle).error == economy_api::ErrorCode::None &&
        api.PollCommand(pending.handle).result.state == economy::CommandState::Queued,
        "queued command remains pollable");
    const auto unknown = api.PollCommand(pending.handle + 100);
    Check(unknown.error == economy_api::ErrorCode::UnknownCommand,
        "never-issued handle remains distinguishable from expired handle");

    std::string checkpoint;
    economy::ParseError parseError;
    Check(economy::SaveCheckpoint(world, checkpoint, parseError),
        "compacted world can be checkpointed");
    economy::RestoredCheckpoint restored;
    Check(economy::RestoreCheckpoint(checkpoint, restored, parseError),
        "compacted checkpoint restores");
    const auto restoredHistory = restored.world->CommandHistory();
    Check(restoredHistory.firstRetainedCommandId == compacted.history.firstRetainedCommandId &&
        restoredHistory.nextCommandId == compacted.history.nextCommandId &&
        restoredHistory.retainedTerminalResults == compacted.history.retainedTerminalResults &&
        restoredHistory.pendingCommands == compacted.history.pendingCommands,
        "checkpoint roundtrip preserves command history metadata");

    const auto noOpRevision = world.Revision();
    const auto noOpEvents = NewestEvent(api);
    const auto noOp = api.CompactCommandHistory(noOpRevision, 1);
    Check(noOp.error == economy_api::ErrorCode::None && noOp.removedResults == 0 &&
        noOp.expiredHandles == 0 && noOp.revision == noOpRevision &&
        world.Revision() == noOpRevision && NewestEvent(api) == noOpEvents,
        "repeated compaction is a true no-op");

    Check(api.Step() == economy_api::ErrorCode::None,
        "queued command executes after compaction");
    Check(api.PollCommand(pending.handle).result.state == economy::CommandState::Accepted,
        "queued command terminal result remains pollable");
    const auto afterStep = api.ReadEvents(0, economy_api::MaxEventPageSize);
    unsigned rejectedEvents = 0;
    for (const auto& event : afterStep.page.events)
        if (event.type == economy_api::EventType::CommandRejected &&
            event.commandHandle == rejected.handle)
            ++rejectedEvents;
    Check(rejectedEvents == 1, "preexisting rejected command emits its rejection event once");

    // Core callers can compact while an API is attached. Existing mapped handles must
    // still observe core expiration, and a new API must not inherit local API handles.
    economy::World directWorld(catalog, 64, 64);
    economy_api::EconomyApi directApi(directWorld, {true, true});
    auto directAccepted = directApi.PlaceBuilding(directWorld.Revision(), "farm", {2, 2});
    Check(directAccepted.error == economy_api::ErrorCode::None &&
        directApi.Step() == economy_api::ErrorCode::None,
        "direct compaction fixture has a terminal command");
    Check(directApi.PlaceBuilding(directWorld.Revision(), "mill", {12, 12}).error ==
        economy_api::ErrorCode::None && directApi.Step() == economy_api::ErrorCode::None,
        "direct compaction fixture has a second terminal command");
    const auto directCompaction = directWorld.CompactCommandHistory(1);
    Check(directCompaction.error == economy::CommandHistoryError::None &&
        directCompaction.removedResults == 1,
        "direct core compaction expires its terminal prefix");
    Check(directApi.PollCommand(directAccepted.handle).error ==
        economy_api::ErrorCode::CommandExpired,
        "attached API recognizes a result compacted directly by core");
    const auto directRevision = directWorld.Revision();
    const auto directEventSequence = NewestEvent(directApi);
    const auto directCleanup = directApi.CompactCommandHistory(directRevision, 1);
    Check(directCleanup.error == economy_api::ErrorCode::None &&
        directCleanup.removedResults == 0 && directCleanup.expiredHandles == 1 &&
        directCleanup.revision == directRevision && directWorld.Revision() == directRevision &&
        NewestEvent(directApi) == directEventSequence,
        "API no-op compaction cleans direct-core-expired mappings without world or event mutation");
    const auto directExpiredHandle = directApi.PollCommand(directAccepted.handle);
    Check(directExpiredHandle.error == economy_api::ErrorCode::CommandExpired &&
        directExpiredHandle.result.commandId == 0 &&
        directExpiredHandle.result.failure == economy::CommandFailure::None,
        "retired direct-core handle reports API expiration without leaking its retired native ID");
    economy_api::EconomyApi replacement(directWorld, {true, true});
    Check(replacement.PollCommand(directAccepted.handle).error ==
        economy_api::ErrorCode::UnknownCommand,
        "replacement API starts fresh handle namespace on a compacted world");

    economy::World offsetWorld(catalog, 64, 64);
    Check(offsetWorld.PlaceBuilding(0, {2, 2}) != 0 && offsetWorld.Step().accepted &&
        offsetWorld.PlaceBuilding(0, {12, 12}) != 0 && offsetWorld.Step().accepted,
        "offset fixture advances native command IDs before API attachment");
    economy_api::EconomyApi offsetApi(offsetWorld, {true, true});
    auto offsetCommand = offsetApi.PlaceBuilding(offsetWorld.Revision(), "farm", {22, 22});
    Check(offsetCommand.error == economy_api::ErrorCode::None && offsetCommand.handle == 1 &&
        offsetApi.Step() == economy_api::ErrorCode::None &&
        offsetApi.PollCommand(offsetCommand.handle).result.state == economy::CommandState::Accepted,
        "fresh API handle maps to an offset native command ID");
    const auto offsetCompaction = offsetApi.CompactCommandHistory(offsetWorld.Revision(), 0);
    Check(offsetCompaction.error == economy_api::ErrorCode::None &&
        offsetCompaction.expiredHandles == 1,
        "offset native result expires through API compaction");
    const auto offsetExpiredHandle = offsetApi.PollCommand(offsetCommand.handle);
    Check(offsetExpiredHandle.error == economy_api::ErrorCode::CommandExpired &&
        offsetExpiredHandle.result.commandId == 0 &&
        offsetExpiredHandle.result.failure == economy::CommandFailure::None,
        "expired fresh API handle does not confuse its number with the larger native ID");

    // The API must preserve the core's revision-exhaustion refusal without side effects.
    economy::World exhausted(catalog, 64, 64);
    const auto exhaustedCommand = exhausted.PlaceBuilding(0, {2, 2});
    Check(exhaustedCommand != 0 && exhausted.Step().accepted,
        "revision exhaustion fixture has a first terminal result");
    Check(exhausted.PlaceBuilding(0, {12, 12}) != 0 && exhausted.Step().accepted,
        "revision exhaustion fixture has a second terminal result");
    auto saved = economy::SaveWorld(exhausted);
    const std::string revisionLine = "REVISION " + std::to_string(exhausted.Revision()) + "\n";
    const auto revisionPosition = saved.find(revisionLine);
    Check(revisionPosition != std::string::npos, "find revision field for exhaustion fixture");
    saved.replace(revisionPosition, revisionLine.size(),
        "REVISION 18446744073709551615\n");
    economy::ParseError loadError;
    Check(economy::LoadWorld(saved, catalog, exhausted, loadError),
        "load max-revision history fixture");
    economy_api::EconomyApi exhaustedApi(exhausted, {true, true});
    const auto beforeExhaustion = economy::SaveWorld(exhausted);
    const auto refused = exhaustedApi.CompactCommandHistory(exhausted.Revision(), 1);
    Check(refused.error == economy_api::ErrorCode::RevisionExhausted &&
        economy::SaveWorld(exhausted) == beforeExhaustion && refused.removedResults == 0,
        "revision exhaustion rejects pruning without mutation");

    std::cout << "Command history API checks passed\n";
}
