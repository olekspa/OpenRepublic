#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace openrepublic::content {

struct BuildingResourceRate {
    std::string resource;
    // Raw data value in the loader's internal rate unit. The unit is INFERRED
    // to be per game day (see the evidence ledger); efficiency scaling is unknown.
    double value = 0.0;
    std::size_t line = 0;
    // True when a positive `$WORKERS_NEEDED` had already been read. The
    // build-scoped loader (SOVIET64.exe, static review) multiplies the value by
    // the worker count at that moment for `$CONSUMPTION` and (except one type
    // code) `$PRODUCTION`, so such rates are per worker. `$WORKERS_NEEDED` also
    // rescales earlier entries for one further type code; that is not modelled.
    bool workersDeclaredBefore = false;
};

struct BuildingStorageEntry {
    std::string directive;      // for example "$STORAGE_IMPORT_SPECIAL"
    std::string transportClass; // opaque token such as RESOURCE_TRANSPORT_COVERED
    double amount = 0.0;
    std::string resource;       // only for the three-argument forms
    std::size_t line = 0;
};

struct BuildingDefinitionDirective {
    std::string name;
    std::vector<std::string> arguments;
    std::size_t line = 0;
};

struct BuildingDefinition {
    // `$NAME` numeric token (an opaque text identifier).
    std::optional<std::string> nameId;
    // Names of `$TYPE_*` directives without the prefix, in file order.
    std::vector<std::string> typeFlags;
    std::optional<double> workersNeeded;
    std::optional<double> workingVehiclesNeeded;
    std::vector<BuildingResourceRate> production;
    std::vector<BuildingResourceRate> consumption;
    std::vector<BuildingResourceRate> consumptionPerSecond;
    std::vector<BuildingStorageEntry> storage;
    // Every other `$` directive, preserved in order and uninterpreted.
    std::vector<BuildingDefinitionDirective> other;
    // Non-blank lines that did not start with `$` after comment removal.
    std::size_t ignoredLines = 0;
};

constexpr std::size_t MaxBuildingDefinitionBytes = 4u * 1024u * 1024u;
constexpr std::size_t MaxBuildingDefinitionDirectives = 65536;

// Parses one building-definition text file from the observed line-oriented
// format (OBSERVED, one WRSR build): `$DIRECTIVE arg ...` lines, `//` starts a
// comment that runs to the end of the line, and non-directive lines (separator
// rules, data rows following a directive) are counted and ignored. Typed
// extraction covers `$NAME`, `$TYPE_*`, `$WORKERS_NEEDED`,
// `$WORKING_VEHICLES_NEEDED`, `$PRODUCTION`, `$CONSUMPTION`,
// `$CONSUMPTION_PER_SECOND` and the `$STORAGE*` family; a typed directive with
// the wrong argument count or a non-finite number is an error. No gameplay
// semantics are claimed. On failure output is unchanged.
bool ParseBuildingDefinition(const std::string& text, BuildingDefinition& output,
                             std::string* error = nullptr);

} // namespace openrepublic::content
