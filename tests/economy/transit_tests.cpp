#include "openrepublic/economy/transit.h"

#include <cstdlib>
#include <iostream>

using namespace openrepublic::economy;
using namespace openrepublic::economy::transit;

int main() {
    int failures = 0;
    auto check = [&](bool condition, const char* message) {
        if (!condition) {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    };
    std::vector<RoadEdge> roads;
    for (std::int32_t x = 0; x < 10; ++x)
        roads.push_back({{x, 0}, {x + 1, 0}, true});
    roads.push_back({{10, 0}, {10, 1}, true});
    for (std::int32_t x = 0; x < 10; ++x)
        roads.push_back({{x, 1}, {x + 1, 1}, true});
    roads.push_back({{0, 1}, {0, 0}, true});

    const auto path = ShortestRoadPath(roads, {0, 0}, {10, 0});
    check(path.size() == 11 && path.front() == Cell{0, 0} && path.back() == Cell{10, 0},
          "shortest road path includes both endpoints");
    const std::vector<RoadEdge> tiedRoads{{{0, 0}, {0, 1}, true}, {{0, 1}, {1, 1}, true},
                                           {{0, 0}, {1, 0}, true}, {{1, 0}, {1, 1}, true}};
    check(ShortestRoadPath(tiedRoads, {0, 0}, {1, 1}) ==
              std::vector<Cell>{{0, 0}, {1, 0}, {1, 1}},
          "road index retains west/east/north/south tie-breaking independent of edge insertion order");

    Route route{{{0, 0}, {10, 0}}, 4, 2, 1, 1};
    std::string error;
    check(ValidateRoute(route, roads, error) && error.empty(), "connected ordered loop route validates");
    auto invalid = route;
    invalid.seats = 0;
    check(!ValidateRoute(invalid, roads, error) && !error.empty(), "zero capacity route rejects with a reason");
    invalid = route;
    invalid.stops[1] = {12, 0};
    check(!ValidateRoute(invalid, roads, error), "route stop without an enabled road rejects");
    auto closedRoads = roads;
    closedRoads[4].enabled = false;
    check(ShortestRoadPath(closedRoads, {0, 0}, {10, 0}).size() == 13,
          "disabled segment is bypassed by an alternate enabled route");

    const auto near = EvaluateCommute({1, 0}, {3, 0}, 3, nullptr, roads);
    check(near.roadReachable && !near.requiresTransit && near.eligible,
          "commute within distance threshold remains eligible without transit");
    const auto distantNoRoute = EvaluateCommute({0, 0}, {10, 0}, 4, nullptr, roads);
    check(distantNoRoute.roadReachable && distantNoRoute.requiresTransit && !distantNoRoute.eligible,
          "distant commute is ineligible without a route");
    const auto distantWithRoute = EvaluateCommute({0, 0}, {10, 0}, 4, &route, roads);
    check(distantWithRoute.routeConnects && distantWithRoute.eligible,
          "distant commute is eligible when route connects home and workplace stops");
    const auto outsideTransfer = EvaluateCommute({1, 0}, {10, 0}, 4, &route, roads, 0);
    check(!outsideTransfer.eligible, "home away from route stop transfer radius is ineligible");

    CommuteIndex indexedRoads(roads, {{1, 0}, {0, 0}});
    check(indexedRoads.Eligible({1, 0}, {3, 0}, 3, nullptr),
          "indexed commute keeps direct short commute eligibility");
    check(!indexedRoads.Eligible({0, 0}, {10, 0}, 4, nullptr),
          "indexed commute still requires transit for long trips");
    check(indexedRoads.Eligible({0, 0}, {10, 0}, 4, &route),
          "indexed commute accepts a connected transit route");
    check(!indexedRoads.Eligible({1, 0}, {10, 0}, 4, &route, 0),
          "indexed commute respects transfer distance");
    check(indexedRoads.CandidateHomes({10, 0}, 4, &route, 0) == std::vector<std::size_t>{1},
          "indexed candidates preserve only homes eligible through distinct route stops");
    check(indexedRoads.WithinDistance({1, 0}, {3, 0}, 2) &&
          !indexedRoads.WithinDistance({1, 0}, {10, 0}, 2),
          "bounded road distances answer transfer-radius reachability");

    Route busRoute{{{0, 0}, {3, 0}}, 2, 1, 1, 1};
    std::vector<RoadEdge> shortRoads{{{0, 0}, {1, 0}, true}, {{1, 0}, {2, 0}, true},
                                      {{2, 0}, {3, 0}, true}, {{3, 0}, {3, 1}, true},
                                      {{0, 1}, {1, 1}, true}, {{1, 1}, {2, 1}, true},
                                      {{2, 1}, {3, 1}, true}, {{0, 1}, {0, 0}, true}};
    Bus bus;
    bus.cell = {0, 0};
    bus.passengers = 3;
    auto tick = AdvanceBus(busRoute, shortRoads, 8, bus);
    check(tick.unloaded == 3 && bus.passengers == 0, "bus unloads its current passengers at a stop");
    tick = AdvanceBus(busRoute, shortRoads, 8, bus); // unload dwell
    check(tick.boarded == 0, "unload handling consumes a tick before loading");
    tick = AdvanceBus(busRoute, shortRoads, 8, bus);
    check(tick.boarded == 2 && bus.passengers == 2, "boarding is capped by bus seats");
    tick = AdvanceBus(busRoute, shortRoads, 8, bus); // load dwell
    check(bus.cell == Cell{0, 0}, "load handling consumes a tick before travel");
    tick = AdvanceBus(busRoute, shortRoads, 8, bus);
    check(bus.cell == Cell{1, 0}, "bus advances along its road route at configured speed");
    AdvanceBus(busRoute, shortRoads, 8, bus);
    tick = AdvanceBus(busRoute, shortRoads, 8, bus);
    check(tick.arrived && bus.cell == Cell{3, 0} && bus.stopIndex == 1,
          "bus arrives at next ordered stop and advances stop index");

    Catalog transitCatalog;
    ParseError parseError;
    const std::string catalogText =
        "openrepublic-economy 1\nresource ore\ntransit threshold 4 transfer 2\n"
        "building home 1 1\n housing 2\n end\n"
        "building bus_stop 1 1\n station\n end\n"
        "building mine 1 1\n workers 2\n store ore 20\n recipe mine every 1 out ore:1\n end\n"
        "vehicle bus capacity 2 speed 1\n";
    check(ParseCatalogText(catalogText, transitCatalog, parseError), "transit catalog declaration parses");
    check(WriteCatalogText(transitCatalog).find("transit threshold 4 transfer 2") != std::string::npos,
          "transit declaration writes canonically");
    World world(transitCatalog, 16, 6);
    world.PlaceBuilding(0, {1, 2});
    world.PlaceBuilding(1, {3, 2});
    world.PlaceBuilding(1, {10, 2});
    world.PlaceBuilding(2, {12, 2});
    for (std::int32_t x = 1; x < 12; ++x) world.BuildRoad({x, 1}, {x + 1, 1});
    check(world.Step(1).accepted, "transit fixture buildings and roads place");
    check(world.Snapshot().workersAssigned == 0, "distant residents are unstaffed until a route is defined");
    const auto routeCommand = world.CreateTransitRoute({2, 3}, 0);
    check(routeCommand != 0 && world.Step(1).accepted &&
          world.GetCommandResult(routeCommand).state == CommandState::Accepted,
          "ordered bus-stop route command is accepted on connected roads");
    check(world.Snapshot().workersAssigned == 2, "route connecting home and workplace enables distant staffing");
    check(world.Snapshot().transitRoutes.size() == 1 && world.Snapshot().transitRoutes[0].seats == 2,
          "snapshot reports the transit bus route and seat capacity");
    ParseError saveError;
    const auto savedRouteWorld = SaveWorld(world);
    check(savedRouteWorld.find("WORLD 10") != std::string::npos, "transit opt-in selects world save version 10");
    World restoredRouteWorld(transitCatalog, 1, 1);
    check(LoadWorld(savedRouteWorld, transitCatalog, restoredRouteWorld, saveError),
          "transit route state restores from version 10");
    check(SaveWorld(restoredRouteWorld) == savedRouteWorld &&
          restoredRouteWorld.Snapshot().stateHash == world.Snapshot().stateHash,
          "transit route save restores canonical bytes and state hash");
    const auto pendingRoute = world.CreateTransitRoute({2, 3}, 0);
    const auto savedPendingRoute = SaveWorld(world);
    World restoredPendingRoute(transitCatalog, 1, 1);
    check(pendingRoute != 0 && LoadWorld(savedPendingRoute, transitCatalog, restoredPendingRoute, saveError),
          "queued transit route and ordered stop IDs restore");
    world.Step(1);
    restoredPendingRoute.Step(1);
    check(SaveWorld(world) == SaveWorld(restoredPendingRoute),
          "pending route command executes identically after restore");

    if (failures)
        return EXIT_FAILURE;
    std::cout << "OpenRepublic synthetic transit tests passed\n";
    return EXIT_SUCCESS;
}
