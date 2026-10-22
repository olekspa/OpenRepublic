#pragma once

#include "openrepublic/compat/region_import.h"
#include "openrepublic/economy/economy.h"

#include <cstddef>
#include <string>
#include <vector>

namespace openrepublic::compat {

// A straight road link between two map positions in metres (plain data; the
// decoded game road graph is flattened to this by the caller).
struct RoadLinkRef {
    double x1{};
    double z1{};
    double x2{};
    double z2{};
};

struct RegionRoadOptions {
    // Longest connector, in cells, from a building's access cell to the nearest road.
    std::uint32_t maxConnectorCells = 40;
};

struct RegionRoadReport {
    std::size_t linksConsidered{};
    std::size_t linksOutsideGrid{};
    std::size_t segmentsQueued{};
    std::size_t segmentsAccepted{};
    std::size_t segmentsRejected{};
    std::size_t buildingsOnRoad{};
    std::size_t buildingsConnected{};
    std::size_t buildingsUnconnected{};
    std::vector<std::string> notes;
};

// Rasterizes the links onto the world's unit-step road grid (4-connected walk
// from cell to cell, duplicates dropped), then joins each imported building's
// access cell to the nearest road cell by a shortest 4-connected path around other
// buildings (breadth first, at most maxConnectorCells long). Links are straight chords:
// curved game roads are approximated. The world advances by up to two ticks to
// apply the queued commands. Returns false only if the world rejects a step.
bool ImportRoadsIntoWorld(economy::World &world, const RegionImportReport &imported,
                          const RegionImportOptions &options, const std::vector<RoadLinkRef> &links,
                          const RegionRoadOptions &roadOptions, RegionRoadReport &report);

// Deterministic one-paragraph summary.
std::string DescribeRegionRoads(const RegionRoadReport &report);

} // namespace openrepublic::compat
