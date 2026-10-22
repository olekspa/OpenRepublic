#include "openrepublic/compat/region_roads.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace openrepublic::compat {
namespace {

using Cell = economy::Cell;

bool CellLess(const Cell &a, const Cell &b) {
    return a.x != b.x ? a.x < b.x : a.z < b.z;
}

struct CellOrder {
    bool operator()(const Cell &a, const Cell &b) const { return CellLess(a, b); }
};

struct Segment {
    Cell a;
    Cell b;
};

struct SegmentOrder {
    bool operator()(const Segment &l, const Segment &r) const {
        if (CellLess(l.a, r.a)) return true;
        if (CellLess(r.a, l.a)) return false;
        return CellLess(l.b, r.b);
    }
};

Segment Ordered(Cell a, Cell b) {
    return CellLess(b, a) ? Segment{b, a} : Segment{a, b};
}

bool ToCell(double x, double z, const RegionImportReport &imported, const RegionImportOptions &options,
            Cell &cell) {
    const double cx = std::floor((x - imported.originX) / options.cellSizeMeters);
    const double cz = std::floor((z - imported.originZ) / options.cellSizeMeters);
    if (!std::isfinite(cx) || !std::isfinite(cz) || std::fabs(cx) > 1e6 || std::fabs(cz) > 1e6)
        return false;
    cell = {static_cast<std::int32_t>(cx), static_cast<std::int32_t>(cz)};
    return true;
}

bool InGrid(const Cell &c, const RegionImportReport &imported) {
    return c.x >= 0 && c.z >= 0 && c.x < static_cast<std::int32_t>(imported.gridWidth) &&
           c.z < static_cast<std::int32_t>(imported.gridDepth);
}

// 4-connected grid walk from a to b (each step changes exactly one coordinate).
std::vector<Cell> Walk(Cell a, Cell b) {
    std::vector<Cell> cells{a};
    const int nx = std::abs(b.x - a.x);
    const int nz = std::abs(b.z - a.z);
    const int sx = b.x >= a.x ? 1 : -1;
    const int sz = b.z >= a.z ? 1 : -1;
    Cell current = a;
    int ix = 0;
    int iz = 0;
    while (ix < nx || iz < nz) {
        const bool stepX = iz >= nz || (ix < nx && (0.5 + ix) / nx < (0.5 + iz) / nz);
        if (stepX) {
            current.x += sx;
            ++ix;
        } else {
            current.z += sz;
            ++iz;
        }
        cells.push_back(current);
    }
    return cells;
}

// Shortest 4-connected path (breadth first, fixed neighbor order) from `from` to the
// nearest road cell through cells not in `blocked`, at most `maxSteps` long. Empty if none.
std::vector<Cell> ShortestConnector(Cell from, const std::set<Cell, CellOrder> &roads,
                                    const std::set<Cell, CellOrder> &blocked,
                                    const RegionImportReport &imported, std::uint32_t maxSteps) {
    std::map<Cell, Cell, CellOrder> parent;
    std::vector<Cell> frontier{from};
    parent[from] = from;
    static const int kDx[4] = {1, -1, 0, 0};
    static const int kDz[4] = {0, 0, 1, -1};
    for (std::uint32_t depth = 0; depth <= maxSteps && !frontier.empty(); ++depth) {
        std::vector<Cell> nextFrontier;
        for (const Cell &cell : frontier) {
            if (roads.count(cell) != 0) {
                std::vector<Cell> path;
                for (Cell c = cell; !(c.x == from.x && c.z == from.z); c = parent[c]) path.push_back(c);
                path.push_back(from);
                std::reverse(path.begin(), path.end());
                return path;
            }
            for (int k = 0; k < 4; ++k) {
                const Cell n{cell.x + kDx[k], cell.z + kDz[k]};
                if (!InGrid(n, imported) || blocked.count(n) != 0 || parent.count(n) != 0) continue;
                parent[n] = cell;
                nextFrontier.push_back(n);
            }
        }
        frontier = std::move(nextFrontier);
    }
    return {};
}

struct Queued {
    Segment segment;
    economy::CommandId command{};
};

} // namespace

bool ImportRoadsIntoWorld(economy::World &world, const RegionImportReport &imported,
                          const RegionImportOptions &options, const std::vector<RoadLinkRef> &links,
                          const RegionRoadOptions &roadOptions, RegionRoadReport &report) {
    RegionRoadReport next;
    std::set<Segment, SegmentOrder> wanted;
    for (const RoadLinkRef &link : links) {
        ++next.linksConsidered;
        Cell a;
        Cell b;
        if (!ToCell(link.x1, link.z1, imported, options, a) || !ToCell(link.x2, link.z2, imported, options, b)) {
            ++next.linksOutsideGrid;
            continue;
        }
        const std::vector<Cell> walk = Walk(a, b);
        bool any = false;
        for (std::size_t i = 0; i + 1 < walk.size(); ++i) {
            if (InGrid(walk[i], imported) && InGrid(walk[i + 1], imported)) {
                wanted.insert(Ordered(walk[i], walk[i + 1]));
                any = true;
            }
        }
        if (!any) ++next.linksOutsideGrid;
    }

    std::set<Cell, CellOrder> roadCells;
    const auto submit = [&](const std::set<Segment, SegmentOrder> &segments, std::size_t &accepted,
                            std::size_t &rejected, std::vector<Segment> *rejectedOut) {
        std::vector<Queued> queued;
        queued.reserve(segments.size());
        for (const Segment &s : segments) {
            queued.push_back({s, world.BuildRoad(s.a, s.b)});
            ++next.segmentsQueued;
        }
        if (!queued.empty() && !world.Step(1).accepted) return false;
        for (const Queued &q : queued) {
            const auto result = world.GetCommandResult(q.command);
            if (result.state == economy::CommandState::Accepted) {
                ++accepted;
                roadCells.insert(q.segment.a);
                roadCells.insert(q.segment.b);
            } else {
                ++rejected;
                if (rejectedOut) rejectedOut->push_back(q.segment);
            }
        }
        return true;
    };
    if (!submit(wanted, next.segmentsAccepted, next.segmentsRejected, nullptr)) {
        next.notes.push_back("Road import failed: the economy world rejected a step.");
        report = std::move(next);
        return false;
    }

    // Connectors: access cell -> nearest road cell by breadth-first search around buildings.
    const economy::WorldSnapshot snapshot = world.Snapshot();
    std::map<economy::BuildingId, Cell> accessById;
    for (const auto &building : snapshot.buildings) accessById[building.id] = building.access;
    std::set<Cell, CellOrder> blocked;
    for (const auto &building : snapshot.buildings) {
        const auto *type = world.GetCatalog().BuildingType(building.type);
        if (type == nullptr) continue;
        for (std::uint32_t dz = 0; dz < type->depth; ++dz) {
            for (std::uint32_t dx = 0; dx < type->width; ++dx) {
                blocked.insert({building.origin.x + static_cast<std::int32_t>(dx),
                                building.origin.z + static_cast<std::int32_t>(dz)});
            }
        }
    }
    for (const auto &placed : imported.placedBuildings) {
        const auto found = accessById.find(placed.second);
        if (found == accessById.end()) continue;
        const Cell access = found->second;
        if (roadCells.count(access) != 0) {
            ++next.buildingsOnRoad;
            continue;
        }
        const std::vector<Cell> path = ShortestConnector(access, roadCells, blocked, imported,
                                                         roadOptions.maxConnectorCells);
        bool connected = false;
        if (!path.empty()) {
            std::set<Segment, SegmentOrder> segments;
            for (std::size_t i = 0; i + 1 < path.size(); ++i) segments.insert(Ordered(path[i], path[i + 1]));
            std::size_t accepted = 0;
            std::size_t rejected = 0;
            if (!submit(segments, accepted, rejected, nullptr)) {
                next.notes.push_back("Road import failed: the economy world rejected a step.");
                report = std::move(next);
                return false;
            }
            connected = true;
            for (const Segment &s : segments) {
                connected = connected && roadCells.count(s.a) != 0 && roadCells.count(s.b) != 0;
            }
        }
        if (connected) {
            ++next.buildingsConnected;
        } else {
            ++next.buildingsUnconnected;
        }
    }
    next.notes.push_back("Links are straight chords between junctions; curved roads are approximated.");
    report = std::move(next);
    return true;
}

std::string DescribeRegionRoads(const RegionRoadReport &report) {
    std::ostringstream out;
    out << "Region roads: " << report.linksConsidered << " links (" << report.linksOutsideGrid
        << " outside the grid); " << report.segmentsAccepted << "/" << report.segmentsQueued
        << " unit segments accepted, " << report.segmentsRejected << " rejected; buildings: "
        << report.buildingsOnRoad << " already on a road, " << report.buildingsConnected
        << " connected, " << report.buildingsUnconnected << " unconnected.";
    for (const auto &note : report.notes) out << "\n" << note;
    return out.str();
}

} // namespace openrepublic::compat
