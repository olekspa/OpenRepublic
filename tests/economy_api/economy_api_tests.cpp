#include "openrepublic/economy_api/economy_api.h"

#include <cstdlib>
#include <iostream>

using namespace openrepublic;

namespace openrepublic::economy_api {
struct EconomyApiTestAccess {
    static void SetNextHandle(EconomyApi& api, std::uint64_t value) { api.nextHandle_ = value; }
};
}

namespace {
void Check(bool ok, const char* message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

}

int main() {
    auto catalog = economy::MakeSyntheticCatalog();
    economy_api::EconomyApi api(catalog, 64, 64, {true, true});
    const auto definitions = api.DescribeCatalog();
    Check(!definitions.resources.empty() && !definitions.buildingTypes.empty(), "catalog description");
    Check(api.DiscoverCapabilities().actions.size() > 5, "controller capabilities");
    auto world = api.QueryWorld();
    Check(world.error == economy_api::ErrorCode::None, "initial world query");
    const auto type = definitions.buildingTypes.front().name;
    auto placed = api.PlaceBuilding(world.world.revision, type, {8, 8});
    Check(placed.error == economy_api::ErrorCode::None && placed.handle != 0, "place submission");
    Check(api.Step() == economy_api::ErrorCode::None, "step applies command");
    auto result = api.PollCommand(placed.handle);
    Check(result.error == economy_api::ErrorCode::None &&
        result.result.state == economy::CommandState::Accepted, "poll applied result");
    world = api.QueryWorld();
    Check(world.world.buildings.size() == 1, "building visible");
    const auto page = api.QueryWorld(world.world.revision, "buildings", 0, 32);
    Check(page.error == economy_api::ErrorCode::None && page.page.buildings.size() == 1,
        "revision-checked entity page");
    Check(api.QueryWorld(world.world.revision + 1, "buildings", 0, 1).error ==
        economy_api::ErrorCode::StaleRevision, "stale page rejected");
    Check(api.Explain(world.world.buildings.front().id).error == economy_api::ErrorCode::None,
        "building explanation");
    economy_api::EconomyApi observer(catalog, 8, 8, {true, false});
    Check(observer.Step() == economy_api::ErrorCode::PermissionDenied, "observer cannot step");
    Check(observer.PlaceBuilding(0, type, {3, 3}).error == economy_api::ErrorCode::PermissionDenied,
        "observer cannot submit");
    economy_api::EconomyApi noQuery(catalog, 8, 8, {false, false});
    Check(noQuery.DescribeCatalog().error == economy_api::ErrorCode::PermissionDenied,
        "catalog requires query permission");
    economy::CatalogBuilder builder;
    economy::Catalog otherCatalog;
    economy::CatalogErrorInfo catalogError;
    economy::ResourceId otherResource{};
    economy::BuildingTypeId otherBuilding{};
    Check(builder.AddResource("OtherResource", &otherResource, catalogError),
        "build alternate catalog resource");
    Check(builder.AddBuildingType("OtherBuilding", 1, 1, &otherBuilding, catalogError),
        "build alternate catalog type");
    Check(builder.AddStorage(otherBuilding, otherResource, 10, catalogError),
        "build alternate catalog storage");
    Check(builder.Build(otherCatalog, catalogError), "finish alternate catalog");
    economy::World externalWorld(otherCatalog, 8, 8);
    economy_api::EconomyApi attached(externalWorld, {true, false});
    const auto attachedCatalog = attached.DescribeCatalog();
    Check(attachedCatalog.resources.size() == 1 &&
        attachedCatalog.resources.front().name == "OtherResource",
        "attached API uses its world's catalog");
    const auto events = api.ReadEvents(0, 32);
    Check(events.error == economy_api::ErrorCode::None && !events.page.events.empty(), "event journal");
    auto current = api.QueryWorld();
    auto rejected = api.SetBuildingEnabled(current.world.revision, 999999, false);
    Check(rejected.error == economy_api::ErrorCode::None, "invalid target accepted into queue");
    Check(api.Step() == economy_api::ErrorCode::None, "step processes invalid target");
    const auto terminal = api.PollCommand(rejected.handle);
    Check(terminal.error == economy_api::ErrorCode::None &&
        terminal.result.state == economy::CommandState::Rejected,
        "rejected command remains pollable");
    const auto journal = api.ReadEvents(0, 32);
    bool sawQueuedAcceptance = false;
    bool sawExecutionRejection = false;
    for (const auto& event : journal.page.events) {
        if (event.commandHandle != rejected.handle) {
            continue;
        }
        sawQueuedAcceptance |= event.type == economy_api::EventType::CommandAccepted;
        sawExecutionRejection |= event.type == economy_api::EventType::CommandRejected;
    }
    Check(sawQueuedAcceptance, "accepted event means command entered queue");
    Check(sawExecutionRejection, "rejection event reports tick-boundary failure");
    {
        economy::World exhausted(catalog, 8, 8);
        auto save = economy::SaveWorld(exhausted);
        const std::string from = "REVISION 0\n";
        const auto at = save.find(from);
        Check(at != std::string::npos, "counter-limit fixture has revision");
        save.replace(at, from.size(), "REVISION 18446744073709551614\n");
        economy::ParseError error;
        Check(economy::LoadWorld(save, catalog, exhausted, error), "counter-limit API fixture loads");
        economy_api::EconomyApi exhaustedApi(exhausted, {true, true});
        const auto before = economy::SaveWorld(exhausted);
        const auto refused = exhaustedApi.PlaceBuilding(exhausted.Revision(), "farm", {2, 2});
        Check(refused.error == economy_api::ErrorCode::CommandQueueExhausted && refused.handle == 0 &&
                  economy::SaveWorld(exhausted) == before,
              "world queue exhaustion returns an explicit API error without admission");
        Check(exhaustedApi.ReadEvents(0, 32).page.events.empty(), "refused command emits no queue event");
    }
    {
        const auto defs = economy_api::EconomyApi(catalog, 8, 8, {true, true}).DescribeCatalog();
        Check(!defs.buildingTypes.empty() && !defs.resources.empty() && !defs.vehicleTypes.empty(),
              "handle-limit fixture definitions");
        const auto exerciseAll = [&](std::uint64_t next, const char* label) {
            economy::World world(catalog, 8, 8);
            economy_api::EconomyApi limited(world, {true, true});
            economy_api::EconomyApiTestAccess::SetNextHandle(limited, next);
            const auto beforeSave = economy::SaveWorld(world);
            const auto beforeRevision = world.Revision();
            const auto checkRefused = [&](economy_api::Submission submission, const char* operation) {
                Check(submission.error == economy_api::ErrorCode::CommandHandleExhausted &&
                    submission.handle == 0, operation);
                Check(world.Revision() == beforeRevision && economy::SaveWorld(world) == beforeSave,
                    label);
            };
            auto rev = world.Revision();
            checkRefused(limited.PlaceBuilding(rev, defs.buildingTypes.front().name, {1, 1}), "place exhaustion");
            checkRefused(limited.RemoveBuilding(rev, 999), "remove exhaustion");
            checkRefused(limited.BuildRoad(rev, {0, 0}, {1, 0}), "road exhaustion");
            checkRefused(limited.SetRoadEnabled(rev, 999, false), "road toggle exhaustion");
            checkRefused(limited.SetBuildingEnabled(rev, 999, false), "building toggle exhaustion");
            checkRefused(limited.CreateHaulOrder(rev, 999, 1000, defs.resources.front().name,
                defs.vehicleTypes.front().name), "haul exhaustion");
            checkRefused(limited.CancelHaulOrder(rev, 999), "cancel exhaustion");
            checkRefused(limited.RetargetHaulOrder(rev, 999, 1000), "retarget exhaustion");
            Check(limited.ReadEvents(0, 32).page.events.empty(), "exhausted mutators emit no events");
        };
        exerciseAll(0, "zero handle exhaustion is transactional");
        exerciseAll(UINT64_MAX, "maximum handle exhaustion is transactional");
    }
    {
        economy::World world(catalog, 8, 8);
        economy_api::EconomyApi boundary(world, {true, true});
        economy_api::EconomyApiTestAccess::SetNextHandle(boundary, UINT64_MAX - 1);
        const auto accepted = boundary.BuildRoad(world.Revision(), {0, 0}, {1, 0});
        Check(accepted.error == economy_api::ErrorCode::None && accepted.handle == UINT64_MAX - 1,
              "last usable API handle accepted");
        Check(boundary.Step() == economy_api::ErrorCode::None, "last usable handle command stepped");
        Check(boundary.PollCommand(accepted.handle).error == economy_api::ErrorCode::None,
              "last usable API handle remains pollable");
        const auto before = economy::SaveWorld(world);
        const auto refused = boundary.BuildRoad(world.Revision(), {0, 1}, {1, 1});
        Check(refused.error == economy_api::ErrorCode::CommandHandleExhausted && refused.handle == 0 &&
              economy::SaveWorld(world) == before, "request after last usable handle does not mutate world");
    }
    std::cout << "Economy API contracts passed\n";
}
