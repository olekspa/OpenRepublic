#include "openrepublic/economy/transit.h"

#include <algorithm>
#include <limits>
#include <unordered_map>

namespace openrepublic::economy::transit {
namespace {
std::uint64_t Key(Cell c) noexcept {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.x)) << 32) |
           static_cast<std::uint32_t>(c.z);
}

bool Equal(Cell a, Cell b) noexcept { return a.x == b.x && a.z == b.z; }

bool Adjacent(Cell a, Cell b) noexcept {
    const auto dx = static_cast<std::int64_t>(a.x) - b.x;
    const auto dz = static_cast<std::int64_t>(a.z) - b.z;
    return (dx == 0 && (dz == 1 || dz == -1)) || (dz == 0 && (dx == 1 || dx == -1));
}

bool HasRoad(const std::vector<RoadEdge>& roads, Cell c) {
    return std::any_of(roads.begin(), roads.end(), [&](const RoadEdge& edge) {
        return edge.enabled && (Equal(edge.a, c) || Equal(edge.b, c));
    });
}

std::uint64_t Distance(const std::vector<Cell>& path) {
    return path.empty() ? std::numeric_limits<std::uint64_t>::max()
                        : static_cast<std::uint64_t>(path.size() - 1);
}
} // namespace

std::vector<Cell> ShortestRoadPath(const std::vector<RoadEdge>& roads, Cell start, Cell goal) {
    if (Equal(start, goal))
        return {start};
    if (!HasRoad(roads, start) || !HasRoad(roads, goal))
        return {};

    std::unordered_map<std::uint64_t, Cell> previous;
    previous.reserve(roads.size() * 2);
    previous.emplace(Key(start), start);
    std::vector<Cell> queue{start};
    std::size_t head = 0;
    constexpr std::int32_t limit = std::numeric_limits<std::int32_t>::max();
    constexpr std::int32_t low = std::numeric_limits<std::int32_t>::min();
    const Cell directions[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    while (head < queue.size()) {
        const Cell at = queue[head++];
        for (const Cell d : directions) {
            if ((d.x < 0 && at.x == low) || (d.x > 0 && at.x == limit) ||
                (d.z < 0 && at.z == low) || (d.z > 0 && at.z == limit))
                continue;
            const Cell next{static_cast<std::int32_t>(at.x + d.x),
                            static_cast<std::int32_t>(at.z + d.z)};
            const bool linked = std::any_of(roads.begin(), roads.end(), [&](const RoadEdge& edge) {
                if (!edge.enabled)
                    return false;
                return (Equal(edge.a, at) && Equal(edge.b, next)) ||
                       (Equal(edge.b, at) && Equal(edge.a, next));
            });
            if (!linked || !previous.emplace(Key(next), at).second)
                continue;
            queue.push_back(next);
            if (Equal(next, goal)) {
                std::vector<Cell> path{goal};
                Cell current = goal;
                while (!Equal(current, start)) {
                    const auto found = previous.find(Key(current));
                    if (found == previous.end())
                        return {};
                    current = found->second;
                    path.push_back(current);
                }
                std::reverse(path.begin(), path.end());
                return path;
            }
        }
    }
    return {};
}

bool ValidateRoute(const Route& route, const std::vector<RoadEdge>& roads, std::string& error) {
    error.clear();
    if (route.stops.size() < 2) {
        error = "route requires at least two ordered stops";
        return false;
    }
    if (!route.seats || !route.cellsPerTick || !route.loadTicks || !route.unloadTicks) {
        error = "route seats, speed, and handling ticks must be positive";
        return false;
    }
    for (std::size_t i = 0; i < route.stops.size(); ++i) {
        if (!HasRoad(roads, route.stops[i])) {
            error = "every route stop must lie on an enabled road";
            return false;
        }
        if (Equal(route.stops[i], route.stops[(i + 1) % route.stops.size()])) {
            error = "consecutive route stops must be distinct";
            return false;
        }
    }
    for (std::size_t i = 0; i < route.stops.size(); ++i) {
        const auto next = (i + 1) % route.stops.size();
        if (ShortestRoadPath(roads, route.stops[i], route.stops[next]).empty()) {
            error = "each consecutive route stop pair, including the loop, must be road-connected";
            return false;
        }
    }
    return true;
}

CommuteResult EvaluateCommute(Cell home, Cell workplace, std::uint64_t threshold,
                              const Route* route, const std::vector<RoadEdge>& roads,
                              std::uint64_t transferDistance) {
    CommuteResult result;
    const auto direct = ShortestRoadPath(roads, home, workplace);
    result.roadReachable = !direct.empty();
    result.roadDistance = Distance(direct);
    result.requiresTransit = result.roadReachable && result.roadDistance > threshold;
    if (!result.roadReachable)
        return result;
    if (!result.requiresTransit) {
        result.eligible = true;
        return result;
    }
    if (!route || route->stops.size() < 2)
        return result;

    std::string ignored;
    if (!ValidateRoute(*route, roads, ignored))
        return result;
    for (std::size_t homeIndex = 0; homeIndex < route->stops.size(); ++homeIndex) {
        const Cell homeStop = route->stops[homeIndex];
        const auto homePath = ShortestRoadPath(roads, home, homeStop);
        if (homePath.empty() || Distance(homePath) > transferDistance)
            continue;
        for (std::size_t workIndex = 0; workIndex < route->stops.size(); ++workIndex) {
            const Cell workStop = route->stops[workIndex];
            const auto workPath = ShortestRoadPath(roads, workplace, workStop);
            if (workPath.empty() || Distance(workPath) > transferDistance)
                continue;
            if (homeIndex != workIndex) {
                result.routeConnects = true;
                result.eligible = true;
                return result;
            }
        }
    }
    return result;
}

BusTick AdvanceBus(const Route& route, const std::vector<RoadEdge>& roads,
                   std::uint64_t waiting, Bus& bus) {
    BusTick result;
    std::string ignored;
    if (!ValidateRoute(route, roads, ignored))
        return result;
    if (bus.stopIndex >= route.stops.size())
        bus.stopIndex = 0;
    if (!Equal(bus.cell, route.stops[bus.stopIndex]) && bus.path.empty()) {
        bus.phase = BusPhase::Traveling;
    }

    if (bus.phase == BusPhase::Unloading) {
        result.unloaded = bus.passengers;
        bus.passengers = 0;
        bus.phase = BusPhase::Loading;
        bus.dwellRemaining = route.unloadTicks;
        return result;
    }
    if (bus.phase == BusPhase::Loading) {
        if (bus.dwellRemaining) {
            --bus.dwellRemaining;
            return result;
        }
        const auto seats = route.seats - std::min(route.seats, bus.passengers);
        result.boarded = std::min(waiting, seats);
        bus.passengers += result.boarded;
        bus.dwellRemaining = route.loadTicks;
        bus.phase = BusPhase::Traveling;
        bus.path = ShortestRoadPath(roads, bus.cell, route.stops[(bus.stopIndex + 1) % route.stops.size()]);
        bus.pathIndex = 0;
        return result;
    }

    if (bus.path.empty())
        bus.path = ShortestRoadPath(roads, bus.cell, route.stops[(bus.stopIndex + 1) % route.stops.size()]);
    if (bus.path.empty())
        return result;
    if (bus.dwellRemaining) {
        --bus.dwellRemaining;
        return result;
    }
    const auto maxMoves = static_cast<std::size_t>(route.cellsPerTick);
    std::size_t moved = 0;
    while (moved < maxMoves && bus.pathIndex + 1 < bus.path.size()) {
        bus.cell = bus.path[++bus.pathIndex];
        ++moved;
    }
    if (bus.pathIndex + 1 == bus.path.size()) {
        bus.stopIndex = (bus.stopIndex + 1) % route.stops.size();
        bus.path.clear();
        bus.pathIndex = 0;
        bus.phase = BusPhase::Unloading;
        bus.dwellRemaining = 0;
        result.arrived = true;
    }
    return result;
}

} // namespace openrepublic::economy::transit
