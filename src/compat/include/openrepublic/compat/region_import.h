#pragma once

#include "openrepublic/economy/economy.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace openrepublic::compat {

struct RegionImportOptions {
    double cellSizeMeters = 4.0;
    double radiusMeters = 400.0;
    // Caps diagnostic text retained in RegionImportReport. Counters remain exact.
    std::size_t maxDiagnosticNotes = 32;
};

// Plain map position data; this adapter does not depend on a map file format.
struct PlacedBuildingRef {
    std::string typeName;
    double x{};
    double z{};
    // Optional caller-owned identity for the source placement. Map adapters can
    // use a placement index from their source package; generic callers may omit it.
    std::optional<std::uint64_t> sourcePlacementKey;
    // Exact source building-table slot when supplied by a strict native reader.
    // This is independent from sourcePlacementKey, which may be a render-vector index.
    std::optional<std::uint32_t> sourceBuildingTableIndex;
};

struct ImportedBuildingIdentity {
    std::uint64_t sourcePlacementKey{};
    std::string typeName;
    economy::BuildingId buildingId{};
    std::optional<std::uint32_t> sourceBuildingTableIndex;
};

// Exact source-table identity mapped to an accepted economy building,
// independent of any renderer or caller-owned placement key.
struct ImportedNativeBuildingIdentity {
    std::uint32_t sourceBuildingTableIndex{};
    std::string typeName;
    economy::BuildingId buildingId{};
};

struct RegionImportReport {
    std::size_t considered{};
    std::size_t placed{};
    std::size_t skippedUnknownType{};
    std::size_t skippedOverlap{};
    std::size_t skippedAccessConflict{};
    std::size_t skippedOutOfBounds{};
    double originX{};
    double originZ{};
    std::uint32_t gridWidth{};
    std::uint32_t gridDepth{};
    std::vector<std::string> notes;
    std::vector<std::pair<std::string, economy::BuildingId>> placedBuildings;
    // Exact caller-key to economy identity for accepted placements that supplied a key.
    std::vector<ImportedBuildingIdentity> placedSourceBuildings;
    // Exact native source-table identity for accepted placements that supplied one.
    std::vector<ImportedNativeBuildingIdentity> placedNativeBuildings;
};

// Creates a new world transactionally; on invalid input, outWorld is unchanged.
bool ImportRegionIntoWorld(const std::vector<PlacedBuildingRef> &placements,
                           std::size_t centerIndex, const economy::Catalog &catalog,
                           const RegionImportOptions &options,
                           std::unique_ptr<economy::World> &outWorld,
                           RegionImportReport &report);

// Places the explicitly synthetic fleet depot in a free cell immediately south
// of an imported road cell. The depot access cell is therefore the existing road.
bool PlaceSyntheticRegionFleetDepot(economy::World &world,
                                    economy::BuildingTypeId depotType,
                                    economy::BuildingId &outDepot,
                                    std::string &error);

// Formats a deterministic concise summary of the import result.
std::string DescribeRegionImport(const RegionImportReport &report);

} // namespace openrepublic::compat
