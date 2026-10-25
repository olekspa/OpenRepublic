#include "openrepublic/economy_api/economy_api.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

using namespace openrepublic;

int main() {
    economy::Catalog catalog;
    economy::ParseError parse;
    const std::string text =
        "openrepublic-economy 1\nresource ore\ntransit threshold 4 transfer 1\n"
        "building home 1 1\n housing 2\n end\n"
        "building stop 1 1\n station\n end\n"
        "building mine 1 1\n workers 2\n store ore 20\n recipe mine every 1 out ore:1\n end\n"
        "vehicle bus capacity 2 speed 1\n";
    if (!economy::ParseCatalogText(text, catalog, parse)) {
        std::cerr << "FAIL: transit API catalog parse: " << parse.message << '\n';
        return EXIT_FAILURE;
    }
    economy::World world(catalog, 16, 6);
    economy_api::EconomyApi api(world, {true, true});
    std::vector<economy_api::Submission> queued;
    auto revision = world.Revision();
    for (std::int32_t x = 1; x < 12; ++x)
        queued.push_back(api.BuildRoad(revision, {x, 1}, {x + 1, 1}));
    queued.push_back(api.PlaceBuilding(revision, "home", {1, 2}));
    queued.push_back(api.PlaceBuilding(revision, "stop", {3, 2}));
    queued.push_back(api.PlaceBuilding(revision, "stop", {10, 2}));
    queued.push_back(api.PlaceBuilding(revision, "mine", {12, 2}));
    bool ok = std::all_of(queued.begin(), queued.end(), [](const auto& s) {
        return s.error == economy_api::ErrorCode::None && s.handle != 0;
    });
    ok = ok && api.Step() == economy_api::ErrorCode::None;
    for (const auto& item : queued)
        ok = ok && api.PollCommand(item.handle).result.state == economy::CommandState::Accepted;

    const auto route = api.CreateTransitRoute(world.Revision(), {2, 3}, "bus");
    ok = ok && route.error == economy_api::ErrorCode::None && route.handle != 0;
    ok = ok && api.Step() == economy_api::ErrorCode::None;
    ok = ok && api.PollCommand(route.handle).result.state == economy::CommandState::Accepted;
    const auto snapshot = api.QueryWorld();
    ok = ok && snapshot.error == economy_api::ErrorCode::None && snapshot.world.transitRoutes.size() == 1;
    if (snapshot.world.transitRoutes.size() == 1) {
        const auto& view = snapshot.world.transitRoutes.front();
        ok = ok && view.orderedStops == std::vector<economy::BuildingId>({2, 3}) &&
            view.busType == "bus" && view.seats == 2;
    }
    const auto capabilities = api.DiscoverCapabilities();
    ok = ok && std::find(capabilities.actions.begin(), capabilities.actions.end(),
                         economy_api::Action::CreateTransitRoute) != capabilities.actions.end();

    economy_api::EconomyApi observer(world, {true, false});
    ok = ok && observer.CreateTransitRoute(world.Revision(), {2, 3}, "bus").error ==
        economy_api::ErrorCode::PermissionDenied;
    if (!ok) {
        std::cerr << "FAIL: typed transit route submission, snapshot, or permission contract\n";
        return EXIT_FAILURE;
    }
    std::cout << "OpenRepublic transit API tests passed\n";
    return EXIT_SUCCESS;
}
