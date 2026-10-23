#include "openrepublic/content/building_construction.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace openrepublic::content {
namespace {

bool IsWhitespace(char value) {
    return value == ' ' || value == '\t' || value == '\v' || value == '\f';
}

std::vector<std::string> SplitTokens(const std::string& line) {
    std::vector<std::string> tokens;
    std::size_t cursor = 0;
    while (cursor < line.size()) {
        while (cursor < line.size() && IsWhitespace(line[cursor])) ++cursor;
        if (cursor == line.size()) break;
        const auto start = cursor;
        while (cursor < line.size() && !IsWhitespace(line[cursor])) ++cursor;
        tokens.emplace_back(line.data() + start, cursor - start);
    }
    return tokens;
}

bool ParseFinite(const std::string& token, double& value) {
    if (token.empty()) return false;
    errno = 0;
    char* end = nullptr;
    value = std::strtod(token.c_str(), &end);
    return end == token.c_str() + token.size() && errno != ERANGE && std::isfinite(value);
}

bool IsCostDirective(const std::string& name) {
    return name.rfind("$COST_", 0) == 0;
}

bool AddContinuation(BuildingConstructionProfile& profile, std::size_t operationIndex,
                     const std::string& line, std::size_t lineNumber, std::size_t& retainedRows,
                     std::string* error) {
    if (retainedRows >= MaxBuildingConstructionContinuationRows) {
        if (error) *error = "building construction profile exceeds the retained continuation row limit";
        return false;
    }
    auto& operation = profile.operations[operationIndex];
    operation.retainedContinuation.push_back({line, lineNumber});
    ++retainedRows;
    return true;
}

void AddDiagnostic(BuildingConstructionProfile& profile,
                   BuildingConstructionDiagnosticCode code,
                   const BuildingConstructionOperation& operation,
                   std::string message) {
    profile.diagnostics.push_back({code, operation.line, operation.directive, std::move(message)});
}

} // namespace

bool ParseBuildingConstructionProfile(const std::string& text,
                                      BuildingConstructionProfile& output,
                                      std::string* error) {
    BuildingDefinition validated;
    if (!ParseBuildingDefinition(text, validated, error)) return false;

    BuildingConstructionProfile candidate;
    std::optional<std::size_t> latestWork;
    std::optional<std::size_t> activeOperation;
    std::size_t retainedRows = 0;
    std::size_t lineNumber = 0;
    std::istringstream input(text);
    std::string originalLine;

    while (std::getline(input, originalLine)) {
        ++lineNumber;
        if (!originalLine.empty() && originalLine.back() == '\r') originalLine.pop_back();
        std::string semanticLine = originalLine;
        const auto comment = semanticLine.find("//");
        if (comment != std::string::npos) semanticLine.erase(comment);
        std::size_t first = 0;
        while (first < semanticLine.size() && IsWhitespace(semanticLine[first])) ++first;
        if (first == semanticLine.size()) {
            if (activeOperation) {
                if (!AddContinuation(candidate, *activeOperation, originalLine, lineNumber,
                                     retainedRows, error)) return false;
            }
            continue;
        }

        if (semanticLine[first] != '$') {
            if (activeOperation) {
                auto& operation = candidate.operations[*activeOperation];
                if (!AddContinuation(candidate, *activeOperation, originalLine, lineNumber,
                                     retainedRows, error)) return false;
                if (operation.kind != BuildingConstructionOperationKind::Unsupported) {
                    AddDiagnostic(candidate, BuildingConstructionDiagnosticCode::UnexpectedContinuation,
                                  operation, "continuation row is retained but invalidates typed usability");
                    if (operation.kind == BuildingConstructionOperationKind::Work && latestWork == *activeOperation)
                        latestWork.reset();
                }
            }
            continue;
        }

        activeOperation.reset();
        const auto tokens = SplitTokens(semanticLine);
        if (tokens.empty() || !IsCostDirective(tokens.front())) continue;

        BuildingConstructionOperation operation;
        operation.directive = tokens.front();
        operation.arguments.assign(tokens.begin() + 1, tokens.end());
        operation.line = lineNumber;
        bool valid = false;
        double number = 0.0;

        if (operation.directive == "$COST_WORK") {
            operation.kind = BuildingConstructionOperationKind::Work;
            valid = operation.arguments.size() == 2 && ParseFinite(operation.arguments[1], number);
            if (valid) {
                operation.numericValue = number;
                latestWork = candidate.operations.size();
            } else {
                // A malformed phase declaration breaks association to an earlier phase.
                latestWork.reset();
                AddDiagnostic(candidate, BuildingConstructionDiagnosticCode::MalformedArguments,
                              operation, "$COST_WORK requires a phase token and finite numeric token");
            }
        } else if (operation.directive == "$COST_RESOURCE" ||
                   operation.directive == "$COST_RESOURCE_AUTO") {
            const bool automatic = operation.directive == "$COST_RESOURCE_AUTO";
            operation.kind = automatic ? BuildingConstructionOperationKind::AutomaticResource
                                       : BuildingConstructionOperationKind::Resource;
            // Source context exists independently of the numeric token's validity.
            operation.workOperationIndex = latestWork;
            valid = operation.arguments.size() == 2 && ParseFinite(operation.arguments[1], number);
            if (valid) {
                operation.numericValue = number;
            } else {
                AddDiagnostic(candidate, BuildingConstructionDiagnosticCode::MalformedArguments,
                              operation, operation.directive + " requires a selector token and finite numeric token");
            }
            if (!latestWork) {
                AddDiagnostic(candidate, BuildingConstructionDiagnosticCode::OrphanPhase,
                              operation, "declaration has no preceding syntactically valid $COST_WORK");
            }
            if (automatic) {
                AddDiagnostic(candidate, BuildingConstructionDiagnosticCode::UnresolvedAutomaticRecipe,
                              operation, "automatic recipe expansion is unresolved; selector and numeric token remain raw");
            }
        } else if (operation.directive == "$COST_ALL_MULTIPLY") {
            operation.kind = BuildingConstructionOperationKind::MultiplyExisting;
            valid = operation.arguments.size() == 1 && ParseFinite(operation.arguments[0], number);
            if (valid) operation.numericValue = number;
            else AddDiagnostic(candidate, BuildingConstructionDiagnosticCode::MalformedArguments,
                               operation, "$COST_ALL_MULTIPLY requires one finite numeric token");
        } else {
            operation.kind = BuildingConstructionOperationKind::Unsupported;
            AddDiagnostic(candidate, BuildingConstructionDiagnosticCode::UnsupportedDirective,
                          operation, "directive selector is retained as opaque source data");
            const bool phaseScoped = operation.directive == "$COST_WORK_BUILDING_NODE" ||
                                     operation.directive == "$COST_WORK_BUILDING_ALL" ||
                                     operation.directive == "$COST_WORK_BUILDING_KEYWORD" ||
                                     operation.directive == "$COST_WORK_VEHICLE_STATION" ||
                                     operation.directive == "$COST_WORK_VEHICLE_STATION_ACCORDING_NODE";
            if (phaseScoped) {
                operation.workOperationIndex = latestWork;
                if (!latestWork)
                    AddDiagnostic(candidate, BuildingConstructionDiagnosticCode::OrphanPhase,
                                  operation, "selector has no preceding syntactically valid $COST_WORK");
            }
        }

        candidate.operations.push_back(std::move(operation));
        activeOperation = candidate.operations.size() - 1;
    }

    output = std::move(candidate);
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::content
