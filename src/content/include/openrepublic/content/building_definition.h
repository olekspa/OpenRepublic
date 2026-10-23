#pragma once

#include <array>
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

// Raw numeric tokens from economically relevant declarations that do not yet
// have ledger-supported gameplay semantics. Non-numeric arguments are retained
// as nullopt in numericArguments; source spelling/order and line are preserved.
struct BuildingEconomicRawDirective {
    std::string name;
    std::vector<std::string> arguments;
    std::vector<std::optional<double>> numericArguments;
    std::size_t line = 0;
};

// Source declaration only. Directive names and coordinate order are retained
// exactly as written; `_ONLY` and the coordinates have no interpreted meaning.
// A malformed declaration remains available with `valid == false` and a
// diagnostic so optional connection data cannot reject economic definitions.
struct BuildingElectricalConnection {
    std::string directive;
    std::size_t line = 0;
    std::array<std::optional<std::array<double, 3>>, 2> points{};
    bool valid = false;
    std::string diagnostic;
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
    // Candidate construction/utility/waste declarations kept as raw typed
    // arguments only. They are not consumed by economy rules.
    std::vector<BuildingEconomicRawDirective> economicRaw;
    // Recognized electrical connection declarations, in source order. This
    // retains source geometry only; it does not identify a native port.
    std::vector<BuildingElectricalConnection> electricalConnections;
    // Other `$` directives, including electrical headers for legacy consumers,
    // preserved in order and uninterpreted.
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
// `$CONSUMPTION_PER_SECOND`, `$STORAGE*`, and the eight
// `$CONNECTION_ELETRIC_*` electrical declaration spellings. Malformed optional
// connection geometry is retained with diagnostics and does not fail otherwise
// valid typed data. A malformed economic typed directive is an error. No gameplay or native-port
// semantics are claimed. On failure output is unchanged.
bool ParseBuildingDefinition(const std::string& text, BuildingDefinition& output,
                             std::string* error = nullptr);

} // namespace openrepublic::content
