#pragma once

#include "openrepublic/content/building_definition.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace openrepublic::content {

enum class BuildingConstructionOperationKind {
    Work,
    Resource,
    AutomaticResource,
    MultiplyExisting,
    Unsupported
};

enum class BuildingConstructionDiagnosticCode {
    MalformedArguments,
    OrphanPhase,
    UnsupportedDirective,
    UnresolvedAutomaticRecipe,
    UnexpectedContinuation
};

struct BuildingConstructionDiagnostic {
    BuildingConstructionDiagnosticCode code = BuildingConstructionDiagnosticCode::MalformedArguments;
    std::size_t line = 0;
    std::string directive;
    std::string message;
};

struct BuildingConstructionContinuationRow {
    // Original row text, excluding the line ending; comments are retained.
    std::string text;
    std::size_t line = 0;
};

struct BuildingConstructionOperation {
    BuildingConstructionOperationKind kind = BuildingConstructionOperationKind::Unsupported;
    std::string directive;
    std::vector<std::string> arguments;
    std::size_t line = 0;
    // Index of the most recent syntactically valid WORK operation, where applicable.
    std::optional<std::size_t> workOperationIndex;
    // Numeric tokens are retained as authored. Parsed values only prove finite lexical
    // conversion; units, ranges, and gameplay meaning remain unknown.
    std::optional<double> numericValue;
    std::vector<BuildingConstructionContinuationRow> retainedContinuation;
};

struct BuildingConstructionProfile {
    // All $COST_* declarations, in source order, including opaque selectors.
    std::vector<BuildingConstructionOperation> operations;
    std::vector<BuildingConstructionDiagnostic> diagnostics;
};

constexpr std::size_t MaxBuildingConstructionContinuationRows = 65536;

// Parses a bounded construction declaration profile from original building text.
// The existing BuildingDefinition parser validates the complete input first.
// This retains source order and raw arguments without assigning gameplay units
// or deriving final construction quantities. On hard input failure, output is
// unchanged; malformed optional COST_* declarations are retained with diagnostics.
bool ParseBuildingConstructionProfile(const std::string& text,
                                      BuildingConstructionProfile& output,
                                      std::string* error = nullptr);

} // namespace openrepublic::content
