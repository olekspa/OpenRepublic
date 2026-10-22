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
};

// Plain map position data; this adapter does not depend on a map file format.
struct PlacedBuildingRef {
    std::string typeName;
    double x{};
    double z{};
    // Optional caller-owned identity for the source placement. Map adapters can
    // use a placement index from their source package; generic callers may omit it.
    std::optional<std::uint64_t> sourcePlacementKey;
};

struct ImportedBuildingIdentity {
    std::uint64_t sourcePlacementKey{};
    std::string typeName;
    economy::BuildingId buildingId{};
};

struct RegionImportReport {
    std::size_t considered{};
    std::size_t placed{};
    std::size_t skippedUnknownType{};
    std::size_t skippedOverlap{};
    std::size_t skippedOutOfBounds{};
    double originX{};
    double originZ{};
    std::uint32_t gridWidth{};
    std::uint32_t gridDepth{};
    std::vector<std::string> notes;
    std::vector<std::pair<std::string, economy::BuildingId>> placedBuildings;
    // Exact caller-key to economy identity for accepted placements that supplied a key.
    std::vector<ImportedBuildingIdentity> placedSourceBuildings;
};

// Creates a new world transactionally; on invalid input, outWorld is unchanged.
bool ImportRegionIntoWorld(const std::vector<PlacedBuildingRef> &placements,
                           std::size_t centerIndex, const economy::Catalog &catalog,
                           const RegionImportOptions &options,
                           std::unique_ptr<economy::World> &outWorld,
                           RegionImportReport &report);

// Formats a deterministic concise summary of the import result.
std::string DescribeRegionImport(const RegionImportReport &report);

} // namespace openrepublic::compat
