#include "openrepublic/compat/region_import.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>

namespace openrepublic::compat {
namespace {

constexpr std::uint32_t kMaximumGridDimension = 512;
constexpr std::uint32_t kMarginCells = 4;
constexpr const char *kMirrorPrefix = "MIRRORZ_";

struct SelectedPlacement {
    std::size_t index{};
    economy::BuildingTypeId type{};
    const economy::BuildingTypeDef *definition{};
    economy::Cell cell{};
};

void AddNote(RegionImportReport &report, std::size_t maximum, std::string note) {
    if (report.notes.size() < maximum)
        report.notes.push_back(std::move(note));
}

std::string BaseTypeName(const std::string &name) {
    if (name.compare(0, 8, kMirrorPrefix) == 0)
        return name.substr(8);
    return name;
}

bool CellCoordinate(double coordinate, double origin, double cellSize, std::int32_t &cell) {
    const double value = std::floor((coordinate - origin) / cellSize);
    if (!std::isfinite(value) || value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max())
        return false;
    cell = static_cast<std::int32_t>(value);
    return true;
}

} // namespace

bool ImportRegionIntoWorld(const std::vector<PlacedBuildingRef> &placements,
                           std::size_t centerIndex, const economy::Catalog &catalog,
                           const RegionImportOptions &options,
                           std::unique_ptr<economy::World> &outWorld,
                           RegionImportReport &report) {
    RegionImportReport next;
    if (placements.empty() || centerIndex >= placements.size() ||
        !std::isfinite(options.cellSizeMeters) || options.cellSizeMeters <= 0.0 ||
        !std::isfinite(options.radiusMeters) || options.radiusMeters < 0.0 ||
        !std::isfinite(placements[centerIndex].x) || !std::isfinite(placements[centerIndex].z)) {
        AddNote(next, options.maxDiagnosticNotes,
                "Import failed: invalid center placement or region options.");
        report = std::move(next);
        return false;
    }

    std::set<std::uint64_t> sourcePlacementKeys;
    std::set<std::uint32_t> sourceBuildingTableIndices;
    for (const auto &placement : placements) {
        if (placement.sourcePlacementKey &&
            !sourcePlacementKeys.insert(*placement.sourcePlacementKey).second) {
            AddNote(next, options.maxDiagnosticNotes,
                    "Import failed: duplicate source placement key.");
            report = std::move(next);
            return false;
        }
        if (placement.sourceBuildingTableIndex &&
            !sourceBuildingTableIndices.insert(*placement.sourceBuildingTableIndex).second) {
            AddNote(next, options.maxDiagnosticNotes,
                    "Import failed: duplicate source building table index.");
            report = std::move(next);
            return false;
        }
    }

    const auto &center = placements[centerIndex];
    double effectiveRadius = options.radiusMeters;
    auto collect = [&](double radius, std::vector<std::size_t> &indices) {
        indices.clear();
        for (std::size_t i = 0; i < placements.size(); ++i) {
            const auto &placement = placements[i];
            if (!std::isfinite(placement.x) || !std::isfinite(placement.z))
                continue;
            const double distance = std::hypot(placement.x - center.x, placement.z - center.z);
            if (distance <= radius)
                indices.push_back(i);
        }
    };
    std::vector<std::size_t> selected;
    collect(effectiveRadius, selected);
    if (selected.empty()) {
        AddNote(next, options.maxDiagnosticNotes, "Import failed: center is not a finite placement.");
        report = std::move(next);
        return false;
    }

    std::uint32_t maxWidth = 1;
    std::uint32_t maxDepth = 1;
    for (const std::size_t index : selected) {
        economy::BuildingTypeId typeId{};
        if (!catalog.FindBuildingType(BaseTypeName(placements[index].typeName), typeId))
            continue;
        const auto *type = catalog.BuildingType(typeId);
        if (type) {
            maxWidth = std::max(maxWidth, type->width);
            maxDepth = std::max(maxDepth, type->depth);
        }
    }
    if (maxWidth + kMarginCells > kMaximumGridDimension ||
        maxDepth + kMarginCells > kMaximumGridDimension) {
        AddNote(next, options.maxDiagnosticNotes,
                "Import failed: catalog footprint exceeds the supported grid.");
        report = std::move(next);
        return false;
    }

    double minX = 0.0;
    double maxX = 0.0;
    double minZ = 0.0;
    double maxZ = 0.0;
    auto bounds = [&]() {
        minX = maxX = placements[selected.front()].x;
        minZ = maxZ = placements[selected.front()].z;
        for (const std::size_t index : selected) {
            minX = std::min(minX, placements[index].x);
            maxX = std::max(maxX, placements[index].x);
            minZ = std::min(minZ, placements[index].z);
            maxZ = std::max(maxZ, placements[index].z);
        }
    };
    auto dimensions = [&](std::uint32_t &width, std::uint32_t &depth) {
        const double spanX = std::ceil((maxX - minX) / options.cellSizeMeters);
        const double spanZ = std::ceil((maxZ - minZ) / options.cellSizeMeters);
        const double side = std::max(spanX, spanZ) + kMarginCells +
                            std::max(maxWidth, maxDepth);
        if (!std::isfinite(side) || side > kMaximumGridDimension)
            return false;
        width = static_cast<std::uint32_t>(std::max(1.0, side));
        depth = width;
        return true;
    };
    bounds();
    std::uint32_t gridWidth = 0;
    std::uint32_t gridDepth = 0;
    if (!dimensions(gridWidth, gridDepth)) {
        const double maxSpanCells = static_cast<double>(kMaximumGridDimension - kMarginCells -
                                                        std::max(maxWidth, maxDepth));
        const double maxRadius = maxSpanCells * options.cellSizeMeters / 2.0;
        effectiveRadius = std::min(effectiveRadius, maxRadius);
        collect(effectiveRadius, selected);
        bounds();
        if (!dimensions(gridWidth, gridDepth)) {
            AddNote(next, options.maxDiagnosticNotes,
                    "Import failed: selected region cannot fit the supported grid.");
            report = std::move(next);
            return false;
        }
        std::ostringstream note;
        note.imbue(std::locale::classic());
        note << "Selection radius reduced from " << std::setprecision(17) << options.radiusMeters
             << " m to " << effectiveRadius << " m to fit the 512 by 512 grid.";
        AddNote(next, options.maxDiagnosticNotes, note.str());
    }

    next.originX = minX - kMarginCells * options.cellSizeMeters;
    next.originZ = minZ - kMarginCells * options.cellSizeMeters;
    next.cellSizeMeters = options.cellSizeMeters;
    next.gridWidth = gridWidth;
    next.gridDepth = gridDepth;
    next.considered = placements.size();
    AddNote(next, options.maxDiagnosticNotes,
            "Roads are not part of this step; import them with ImportRoadsIntoWorld.");

    std::vector<SelectedPlacement> known;
    known.reserve(selected.size());
    std::vector<bool> included(placements.size(), false);
    for (const auto index : selected)
        included[index] = true;
    for (std::size_t i = 0; i < placements.size(); ++i) {
        const auto &placement = placements[i];
        if (!included[i]) {
            ++next.skippedOutOfBounds;
            continue;
        }
        economy::BuildingTypeId typeId{};
        const std::string name = BaseTypeName(placement.typeName);
        if (!catalog.FindBuildingType(name, typeId)) {
            ++next.skippedUnknownType;
            continue;
        }
        const auto *definition = catalog.BuildingType(typeId);
        std::int32_t x = 0;
        std::int32_t z = 0;
        if (!definition || !CellCoordinate(placement.x, next.originX, options.cellSizeMeters, x) ||
            !CellCoordinate(placement.z, next.originZ, options.cellSizeMeters, z)) {
            ++next.skippedOutOfBounds;
            continue;
        }
        known.push_back({i, typeId, definition, {x, z}});
    }

    auto candidate = std::make_unique<economy::World>(catalog, gridWidth, gridDepth);
    std::vector<std::pair<std::size_t, economy::CommandId>> commands;
    commands.reserve(known.size());
    for (const auto &placement : known)
        commands.emplace_back(placement.index,
                              candidate->PlaceBuilding(placement.type, placement.cell));
    if (!commands.empty()) {
        const auto step = candidate->Step(1);
        if (!step.accepted) {
            AddNote(next, options.maxDiagnosticNotes,
                    "Import failed: economy world rejected the placement batch step.");
            report = std::move(next);
            return false;
        }
    }
    for (const auto &command : commands) {
        const auto result = candidate->GetCommandResult(command.second);
        if (result.state == economy::CommandState::Accepted) {
            const auto &placement = placements[command.first];
            const std::string baseType = BaseTypeName(placement.typeName);
            next.placedBuildings.emplace_back(baseType, result.building);
            if (placement.sourceBuildingTableIndex) {
                next.placedNativeBuildings.push_back(
                    {*placement.sourceBuildingTableIndex, baseType, result.building});
            }
            if (placement.sourcePlacementKey) {
                next.placedSourceBuildings.push_back(
                    {*placement.sourcePlacementKey, baseType, result.building,
                     placement.sourceBuildingTableIndex});
            }
            ++next.placed;
        } else if (result.failure == economy::CommandFailure::OutOfBounds) {
            ++next.skippedOutOfBounds;
        } else if (result.failure == economy::CommandFailure::BuildingCollision) {
            ++next.skippedOverlap;
        } else if (result.failure == economy::CommandFailure::AccessCellUnavailable) {
            ++next.skippedAccessConflict;
            AddNote(next, options.maxDiagnosticNotes,
                    "Placement at source row " + std::to_string(command.first) +
                        " skipped: access cell is unavailable.");
        } else {
            AddNote(next, options.maxDiagnosticNotes,
                    "Import failed: an unexpected economy placement command failed.");
            report = std::move(next);
            return false;
        }
    }
    outWorld = std::move(candidate);
    report = std::move(next);
    return true;
}

bool PlaceSyntheticRegionFleetDepot(economy::World &world,
                                    economy::BuildingTypeId depotType,
                                    economy::BuildingId &outDepot,
                                    std::string &error) {
    error.clear();
    outDepot = 0;
    const auto &catalog = world.GetCatalog();
    const auto *depotDefinition = catalog.BuildingType(depotType);
    if (!depotDefinition || !depotDefinition->depot || depotDefinition->fleet.empty() ||
        depotDefinition->width != 1 || depotDefinition->depth != 1) {
        error = "synthetic region depot type is missing its 1x1 depot fleet declaration";
        return false;
    }

    const economy::WorldSnapshot snapshot = world.Snapshot();
    std::set<std::pair<std::int32_t, std::int32_t>> roadCells;
    for (const auto &road : snapshot.roads) {
        roadCells.emplace(road.a.x, road.a.z);
        roadCells.emplace(road.b.x, road.b.z);
    }
    if (roadCells.empty()) {
        error = "synthetic region fleet needs at least one imported road cell";
        return false;
    }

    std::set<std::pair<std::int32_t, std::int32_t>> blockedCells;
    for (const auto &building : snapshot.buildings) {
        const auto *type = catalog.BuildingType(building.type);
        if (!type) continue;
        for (std::uint32_t dz = 0; dz < type->depth; ++dz)
            for (std::uint32_t dx = 0; dx < type->width; ++dx)
                blockedCells.emplace(building.origin.x + static_cast<std::int32_t>(dx),
                                     building.origin.z + static_cast<std::int32_t>(dz));
        blockedCells.emplace(building.access.x, building.access.z);
    }
    std::vector<std::pair<std::int32_t, std::int32_t>> orderedRoadCells(roadCells.begin(), roadCells.end());
    std::sort(orderedRoadCells.begin(), orderedRoadCells.end(), [&](const auto &lhs, const auto &rhs) {
        const auto distance = [&](const auto &cell) {
            const std::int64_t dx = static_cast<std::int64_t>(cell.first) - snapshot.gridWidth / 2;
            const std::int64_t dz = static_cast<std::int64_t>(cell.second) - snapshot.gridDepth / 2;
            return dx * dx + dz * dz;
        };
        const auto lhsDistance = distance(lhs);
        const auto rhsDistance = distance(rhs);
        return lhsDistance != rhsDistance ? lhsDistance < rhsDistance : lhs < rhs;
    });

    for (const auto &road : orderedRoadCells) {
        const economy::Cell access{road.first, road.second};
        const economy::Cell origin{road.first, road.second + 1};
        if (origin.x < 0 || origin.z < 0 ||
            static_cast<std::uint32_t>(origin.x) >= snapshot.gridWidth ||
            static_cast<std::uint32_t>(origin.z) >= snapshot.gridDepth ||
            roadCells.count({origin.x, origin.z}) != 0)
            continue;
        if (blockedCells.count({origin.x, origin.z}) != 0 ||
            blockedCells.count({access.x, access.z}) != 0)
            continue;

        const economy::CommandId command = world.PlaceBuilding(depotType, origin);
        if (!command) continue;
        const auto step = world.Step(1);
        if (!step.accepted) {
            error = "economy rejected the synthetic depot placement step";
            return false;
        }
        economy::CommandResult result;
        result = world.GetCommandResult(command);
        if (result.state != economy::CommandState::Accepted || !result.building) {
            error = "no road-adjacent cell accepted the synthetic depot placement";
            return false;
        }
        outDepot = result.building;
        return true;
    }
    error = "no free cell beside an imported road can host the synthetic fleet depot";
    return false;
}

std::string DescribeRegionImport(const RegionImportReport &report) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "Region import: " << report.placed << " placed from " << report.considered
           << " placements; " << report.skippedUnknownType << " unknown type, "
           << report.skippedOverlap << " footprint overlap, " << report.skippedAccessConflict
           << " access conflict, " << report.skippedOutOfBounds
           << " outside region or bounds. Grid: " << report.gridWidth << " x " << report.gridDepth
           << " cells at origin (" << std::setprecision(std::numeric_limits<double>::max_digits10)
           << report.originX << ", " << report.originZ << ").";
    for (const auto &note : report.notes)
        output << "\n" << note;
    return output.str();
}

} // namespace openrepublic::compat
