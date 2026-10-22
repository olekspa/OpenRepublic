#include "openrepublic/economy_api/economy_api.h"

#include <cstdlib>
#include <iostream>
#include <limits>

using namespace openrepublic;

namespace openrepublic::economy_api {
struct EconomyApiTestAccess {
    static void SetNextHandle(EconomyApi& api, std::uint64_t value) { api.nextHandle_ = value; }
};
}

namespace {
void Check(bool ok, const char* label) {
    if (!ok) {
        std::cerr << "FAIL: " << label << '\n';
        std::exit(1);
    }
}

economy::Catalog MakeCatalog() {
    economy::CatalogBuilder builder;
    economy::Catalog catalog;
    economy::CatalogErrorInfo error;
    economy::ResourceId water{}, grain{};
    economy::BuildingTypeId plant{}, house{};
    economy::VehicleTypeId truck{};
    Check(builder.AddUtilityResource("water", &water, error), "add water utility");
    Check(builder.AddResource("grain", &grain, error), "add haulable resource");
    Check(builder.AddBuildingType("plant", 1, 1, &plant, error), "add plant");
    Check(builder.AddBuildingType("house", 1, 1, &house, error), "add house");
    Check(builder.AddStorage(plant, grain, 10, error), "add plant grain storage");
    Check(builder.AddStorage(house, grain, 10, error), "add house grain storage");
    Check(builder.AddVehicleType("truck", 4, 1, &truck, error), "add truck");
    Check(builder.Build(catalog, error), "build utility fixture");
    return catalog;
}
}

int main() {
    auto catalog = MakeCatalog();
    economy::World world(catalog, 8, 8);
    economy_api::EconomyApi api(world, {true, true});
    const auto definitions = api.DescribeCatalog();
    Check(definitions.resources.size() == 2, "catalog exposes both resource classes");
    Check(definitions.resources[0].name == "water" && definitions.resources[0].utility &&
          definitions.resources[1].name == "grain" && !definitions.resources[1].utility,
          "catalog identifies utility resources");

    auto observer = economy_api::EconomyApi(world, {true, false});
    Check(observer.ConnectUtility(world.Revision(), "water", 1, 2).error ==
              economy_api::ErrorCode::PermissionDenied,
          "observer cannot connect utilities");
    Check(observer.DisconnectUtility(world.Revision(), "water", 1, 2).error ==
              economy_api::ErrorCode::PermissionDenied,
          "observer cannot disconnect utilities");

    Check(api.ConnectUtility(world.Revision() + 1, "water", 1, 2).error ==
              economy_api::ErrorCode::StaleRevision,
          "stale utility connection rejected");
    Check(api.ConnectUtility(world.Revision(), "grain", 1, 2).error ==
              economy_api::ErrorCode::InvalidArgument,
          "non-utility resource cannot be connected as utility");
    Check(api.ConnectUtility(world.Revision(), "missing", 1, 2).error ==
              economy_api::ErrorCode::InvalidArgument,
          "unknown utility resource rejected");
    Check(api.DisconnectUtility(world.Revision(), "grain", 1, 2).error ==
              economy_api::ErrorCode::InvalidArgument,
          "non-utility resource cannot be disconnected as utility");

    auto view = api.QueryWorld();
    const auto p1 = api.PlaceBuilding(view.world.revision, "plant", {1, 1});
    const auto p2 = api.PlaceBuilding(view.world.revision, "house", {4, 1});
    Check(p1.error == economy_api::ErrorCode::None && p2.error == economy_api::ErrorCode::None,
          "fixture buildings queued");
    Check(api.Step() == economy_api::ErrorCode::None, "fixture buildings placed");
    view = api.QueryWorld();
    Check(view.world.buildings.size() == 2, "fixture buildings visible");
    const auto a = view.world.buildings[0].id;
    const auto b = view.world.buildings[1].id;

    const auto connected = api.ConnectUtility(view.world.revision, "water", b, a);
    Check(connected.error == economy_api::ErrorCode::None && connected.handle != 0 &&
          connected.state == economy::CommandState::Queued,
          "valid utility connection queues");
    Check(api.QueryWorld().world.utilityConnections.empty(),
          "queued connection does not mutate before tick");
    Check(api.Step() == economy_api::ErrorCode::None, "connection command applied");
    auto result = api.PollCommand(connected.handle);
    Check(result.error == economy_api::ErrorCode::None &&
          result.result.state == economy::CommandState::Accepted,
          "connection result is pollable");
    view = api.QueryWorld();
    Check(view.world.utilityConnections.size() == 1 &&
          view.world.utilityConnections[0].resource == "water" &&
          view.world.utilityConnections[0].a == a && view.world.utilityConnections[0].b == b,
          "connection snapshot is named and canonicalized");
    const auto connectionPage = api.QueryWorld(view.world.revision, "utility_connections", 0, 1);
    Check(connectionPage.error == economy_api::ErrorCode::None &&
          connectionPage.page.total == 1 && connectionPage.page.utilityConnections.size() == 1 &&
          connectionPage.page.utilityConnections[0].resource == "water",
          "utility connection collection supports bounded revision-checked paging");

    const auto invalidEndpoint = api.ConnectUtility(view.world.revision, "water", a, 9999);
    Check(invalidEndpoint.error == economy_api::ErrorCode::None &&
          api.Step() == economy_api::ErrorCode::None,
          "invalid endpoint reaches tick-boundary core validation");
    result = api.PollCommand(invalidEndpoint.handle);
    Check(result.error == economy_api::ErrorCode::None &&
          result.result.state == economy::CommandState::Rejected &&
          result.result.failure == economy::CommandFailure::InvalidUtilityConnection,
          "core rejects unknown utility endpoint");
    Check(api.QueryWorld().world.utilityConnections.size() == 1,
          "invalid endpoint leaves connection set unchanged");

    const auto nonHaul = api.CreateHaulOrder(api.QueryWorld().world.revision, a, b, "water", "truck");
    Check(nonHaul.error == economy_api::ErrorCode::None && api.Step() == economy_api::ErrorCode::None,
          "utility haul attempt reaches core validation");
    result = api.PollCommand(nonHaul.handle);
    Check(result.error == economy_api::ErrorCode::None &&
          result.result.state == economy::CommandState::Rejected &&
          result.result.failure == economy::CommandFailure::InvalidOrderEndpoint,
          "utility resources cannot be hauled");

    view = api.QueryWorld();
    const auto duplicate = api.ConnectUtility(view.world.revision, "water", a, b);
    Check(duplicate.error == economy_api::ErrorCode::None && api.Step() == economy_api::ErrorCode::None,
          "duplicate is admitted then checked by simulation");
    result = api.PollCommand(duplicate.handle);
    Check(result.result.state == economy::CommandState::Rejected &&
          result.result.failure == economy::CommandFailure::DuplicateUtilityConnection,
          "duplicate connection is rejected by core");
    Check(api.QueryWorld().world.utilityConnections.size() == 1,
          "duplicate rejection preserves the existing link");

    view = api.QueryWorld();
    const auto removeConnected = api.RemoveBuilding(view.world.revision, a);
    Check(removeConnected.error == economy_api::ErrorCode::None && api.Step() == economy_api::ErrorCode::None,
          "building removal is evaluated while utility-connected");
    result = api.PollCommand(removeConnected.handle);
    Check(result.result.state == economy::CommandState::Rejected &&
          result.result.failure == economy::CommandFailure::ReferencedByUtilityConnection,
          "connected buildings cannot be removed until the link is disconnected");

    view = api.QueryWorld();
    const auto disconnected = api.DisconnectUtility(view.world.revision, "water", b, a);
    Check(disconnected.error == economy_api::ErrorCode::None && api.Step() == economy_api::ErrorCode::None,
          "reversed endpoint disconnect queues and applies");
    Check(api.PollCommand(disconnected.handle).result.state == economy::CommandState::Accepted &&
          api.QueryWorld().world.utilityConnections.empty(),
          "disconnect removes the canonical link");

    const auto missingDisconnect = api.DisconnectUtility(api.QueryWorld().world.revision, "water", a, b);
    Check(missingDisconnect.error == economy_api::ErrorCode::None &&
          api.Step() == economy_api::ErrorCode::None &&
          api.PollCommand(missingDisconnect.handle).result.state == economy::CommandState::Rejected &&
          api.PollCommand(missingDisconnect.handle).result.failure ==
              economy::CommandFailure::MissingUtilityConnection,
          "disconnecting a missing link is rejected at tick boundary");

    auto capabilities = api.DiscoverCapabilities();
    bool connectAction = false, disconnectAction = false;
    for (const auto action : capabilities.actions) {
        connectAction |= action == economy_api::Action::ConnectUtility;
        disconnectAction |= action == economy_api::Action::DisconnectUtility;
    }
    Check(connectAction && disconnectAction, "controller capabilities expose utility commands");

    economy_api::EconomyApiTestAccess::SetNextHandle(api, std::numeric_limits<std::uint64_t>::max());
    const auto before = economy::SaveWorld(world);
    const auto exhausted = api.ConnectUtility(world.Revision(), "water", a, b);
    Check(exhausted.error == economy_api::ErrorCode::CommandHandleExhausted && exhausted.handle == 0 &&
          economy::SaveWorld(world) == before,
          "handle exhaustion refuses command before core enqueue");
    std::cout << "Economy utility API contracts passed\n";
}
