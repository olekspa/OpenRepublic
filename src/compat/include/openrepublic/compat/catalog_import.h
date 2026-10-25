#pragma once

#include "openrepublic/content/building_definition.h"
#include "openrepublic/economy/economy.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace openrepublic::compat {

struct CatalogImportOptions {
    std::uint32_t ticksPerDay = 24;
    std::uint32_t defaultFootprint = 4;
    double storageDaysFallback = 10.0;
    // Opt-in reviewed mappings from (transport class, resource name) to
    // micro-units per raw explicit-storage amount. The product is rounded to
    // binary64 before capacity is rounded upward to a whole micro-unit.
    // Empty preserves fallback.
    std::map<std::pair<std::string, std::string>, double> storageCapacityFactors;
    // Resources supplied by utility connections. Each name must also occur in
    // the parsed building definitions.
    std::set<std::string> utilityResources;
    // Diagnostic opt-in for retained $COST_* declarations. The current native
    // economy has no construction-site state, so this never changes the catalog.
    bool constructionCosts = false;
    // OpenRepublic-only test fleet support for imported regions. When nonzero,
    // the catalog receives a synthetic depot type that owns this many default
    // trucks. This does not classify any imported building as a depot.
    std::uint32_t syntheticRegionFleetCount = 0;
};

struct SkippedBuilding {
    std::string name;
    std::string reason;
};

struct DirectiveCoverage {
    std::size_t parsedTyped{};
    std::size_t retainedRaw{};
    std::size_t ignored{};
};

struct ImportReport {
    CatalogImportOptions options;
    bool success{};
    std::string error;
    std::size_t definitionsSeen{};
    // Counts of supplied definitions containing each directive. "ignored" means the
    // catalog importer does not interpret the directive; parser preservation
    // in BuildingDefinition::other is independent of this classification.
    std::map<std::string, DirectiveCoverage> directiveCoverage;
    std::size_t resourcesImported{};
    std::size_t buildingsImported{};
    std::size_t buildingsSkipped{};
    std::size_t resourceSpecificStorageEntriesSeen{};
    std::size_t storageEntriesMapped{};
    std::size_t storageEntriesUnmapped{};
    std::size_t ambiguousStorageResources{};
    std::size_t storageOnlyBuildingsImported{};
    std::size_t utilityResourcesImported{};
    std::size_t utilityStorageEntriesIgnored{};
    std::size_t constructionCostDirectivesObserved{};
    std::vector<SkippedBuilding> skippedBuildings;
    std::vector<std::string> warnings;
    std::vector<std::string> storageDiagnostics;
};

// Economy amounts are integer micro-units: 1 unit equals 1,000,000 micro-units.
// Rate/day interpretation and storage fallback are inferred compatibility rules.
bool ImportCatalogFromDefinitions(
    const std::map<std::string, content::BuildingDefinition> &definitions,
    const CatalogImportOptions &options, economy::Catalog &out, ImportReport &report);

// Formats a deterministic summary, including the evidence status of assumptions.
std::string DescribeImport(const ImportReport &report);

} // namespace openrepublic::compat
