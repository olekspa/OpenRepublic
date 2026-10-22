#include "openrepublic/economy/checkpoint.h"
#include "openrepublic/economy_api/economy_api.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

using namespace openrepublic;
namespace {
void Check(bool ok, const char* text) {
    if (!ok) { std::cerr << "FAIL: " << text << '\n'; std::exit(1); }
}
}
int main() {
    auto catalog = economy::MakeSyntheticCatalog();
    economy::World world(catalog, 32, 16);
    economy_api::EconomyApi api(world, {true, true});
    auto queued = api.PlaceBuilding(world.Revision(), "farm", {2, 2});
    Check(queued.error == economy_api::ErrorCode::None, "placement queued");
    const auto before = economy::SaveWorld(world);
    const auto events = api.ReadEvents(0, 32).page.newestAvailableSequence;
    const auto exported = api.ExportCheckpoint(world.Revision());
    Check(exported.error == economy_api::ErrorCode::None && !exported.text.empty() &&
          exported.revision == world.Revision() && exported.tick == world.Tick(), "export metadata");
    Check(economy::SaveWorld(world) == before &&
          api.ReadEvents(0, 32).page.newestAvailableSequence == events &&
          api.PollCommand(queued.handle).result.state == economy::CommandState::Queued,
          "export does not apply pending commands or change events/handles");
    economy::RestoredCheckpoint restored;
    economy::ParseError error;
    Check(economy::RestoreCheckpoint(exported.text, restored, error), "API export restores");
    Check(economy::SaveWorld(*restored.world) == before, "pending state exactly retained");
    Check(api.Step() == economy_api::ErrorCode::None && restored.world->Step().accepted,
          "original and restored apply pending work");
    Check(world.Snapshot().stateHash == restored.world->Snapshot().stateHash,
          "resumed state equals original");
    const auto stale = api.ExportCheckpoint(exported.revision);
    Check(stale.error == economy_api::ErrorCode::StaleRevision && stale.text.empty(), "stale export rejects");
    economy_api::EconomyApi observer(world, {true, false});
    Check(observer.ExportCheckpoint(world.Revision()).error == economy_api::ErrorCode::None,
          "query-only observer can export");
    auto actions = observer.DiscoverCapabilities().actions;
    Check(std::find(actions.begin(), actions.end(), economy_api::Action::ExportCheckpoint) != actions.end(),
          "query capability advertises export");
    economy_api::EconomyApi denied(world, {false, true});
    const auto noExport = denied.ExportCheckpoint(world.Revision());
    Check(noExport.error == economy_api::ErrorCode::PermissionDenied && noExport.text.empty(),
          "control without query permission cannot export");

    // Export failure must not silently return an empty successful save or change pending state.
    economy::World oversized(catalog, 32, 16);
    for (unsigned i = 0; i < 24000; ++i) oversized.PlaceBuilding(0, {2, 2});
    economy_api::EconomyApi tooLarge(oversized, {true, true});
    const auto hash = oversized.Snapshot().stateHash;
    const auto failed = tooLarge.ExportCheckpoint(oversized.Revision());
    Check(failed.error == economy_api::ErrorCode::PersistenceFailed && failed.text.empty() &&
          !failed.message.empty() && hash == oversized.Snapshot().stateHash,
          "size failure is explicit and preserves world");
    std::cout << "Checkpoint API checks passed\n";
}
