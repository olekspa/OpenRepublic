#pragma once

#include "openrepublic/economy/economy.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace openrepublic::economy::transit {

// OpenRepublic synthetic transit primitives. They do not describe WRSR rules.
struct RoadEdge {
    Cell a{};
    Cell b{};
    bool enabled{true};
};

struct Route {
    std::vector<Cell> stops;
    std::uint64_t seats{};
    std::uint32_t cellsPerTick{1};
    std::uint32_t loadTicks{1};
    std::uint32_t unloadTicks{1};
};

// The path includes both endpoints. Equal-length choices use neighbor order
// west, east, north, south, making paths stable across platforms.
std::vector<Cell> ShortestRoadPath(const std::vector<RoadEdge>& roads, Cell start, Cell goal);

bool ValidateRoute(const Route& route, const std::vector<RoadEdge>& roads, std::string& error);

struct CommuteResult {
    bool roadReachable{};
    std::uint64_t roadDistance{};
    bool requiresTransit{};
    bool routeConnects{};
    bool eligible{};
};

// A tick-local derived road index used by workforce assignment. Distances are
// cached per queried cell and limited to the commute or transfer radius.
class CommuteIndex final {
public:
    CommuteIndex(const std::vector<RoadEdge>& roads, const std::vector<Cell>& homes);
    bool Eligible(Cell home, Cell workplace, std::uint64_t roadDistanceThreshold,
                  const Route* route, std::uint64_t transferDistance = 0);
    bool WithinDistance(Cell start, Cell goal, std::uint64_t maximumDistance);
    std::vector<std::size_t> CandidateHomes(Cell workplace, std::uint64_t roadDistanceThreshold,
                                             const Route* route,
                                             std::uint64_t transferDistance = 0);

private:
    using Distances = std::unordered_map<std::uint64_t, std::uint64_t>;
    const Distances& DistancesFrom(Cell source, std::uint64_t maximumDistance);
    std::vector<RoadEdge> roads_;
    std::unordered_map<std::uint64_t, std::vector<Cell>> adjacency_;
    std::unordered_map<std::uint64_t, std::uint64_t> component_;
    std::unordered_map<std::uint64_t, Distances> distanceCache_;
    std::unordered_map<std::uint64_t, std::uint64_t> cacheRadius_;
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> homesByCell_;
    const Route* validatedRoute_{};
    bool routeChecked_{};
    bool routeValid_{};
};

// Homes at or below the threshold can commute over the road network directly.
// More distant homes require an ordered route with a stop reachable from both
// endpoints by road within transferDistance. Transfers themselves are not
// modeled as passenger walking trips, only as a binary authored connection.
CommuteResult EvaluateCommute(Cell home, Cell workplace, std::uint64_t roadDistanceThreshold,
                              const Route* route, const std::vector<RoadEdge>& roads,
                              std::uint64_t transferDistance = 0);

enum class BusPhase : std::uint8_t { Unloading = 0, Loading, Traveling };

struct Bus {
    std::size_t stopIndex{};
    Cell cell{};
    std::uint64_t passengers{};
    BusPhase phase{BusPhase::Unloading};
    std::uint32_t dwellRemaining{};
    std::vector<Cell> path;
    std::size_t pathIndex{};
};

struct BusTick {
    std::uint64_t unloaded{};
    std::uint64_t boarded{};
    bool arrived{};
};

// Advances one deterministic simulation tick. On stop arrival all riders
// alight, then waiting passengers board up to seat capacity; dwell counters
// include the current tick. Empty/no-route paths leave the bus at the stop.
BusTick AdvanceBus(const Route& route, const std::vector<RoadEdge>& roads,
                   std::uint64_t waitingPassengers, Bus& bus);

} // namespace openrepublic::economy::transit
