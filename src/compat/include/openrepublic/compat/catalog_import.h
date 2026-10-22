#pragma once

#include "openrepublic/content/building_definition.h"
#include "openrepublic/economy/economy.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace openrepublic::compat {

struct CatalogImportOptions {
    std::uint32_t ticksPerDay = 24;
    std::uint32_t defaultFootprint = 4;
    double storageDaysFallback = 10.0;
};

struct SkippedBuilding {
    std::string name;
    std::string reason;
};

struct ImportReport {
    CatalogImportOptions options;
    bool success{};
    std::string error;
    std::size_t definitionsSeen{};
    std::size_t resourcesImported{};
    std::size_t buildingsImported{};
    std::size_t buildingsSkipped{};
    std::size_t resourceSpecificStorageEntriesSeen{};
    std::vector<SkippedBuilding> skippedBuildings;
    std::vector<std::string> warnings;
};

// Economy amounts are integer micro-units: 1 unit equals 1,000,000 micro-units.
// Rate/day interpretation and storage fallback are inferred compatibility rules.
bool ImportCatalogFromDefinitions(
    const std::map<std::string, content::BuildingDefinition> &definitions,
    const CatalogImportOptions &options, economy::Catalog &out, ImportReport &report);

// Formats a deterministic summary, including the evidence status of assumptions.
std::string DescribeImport(const ImportReport &report);

} // namespace openrepublic::compat
