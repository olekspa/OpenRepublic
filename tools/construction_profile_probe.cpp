// Aggregate-only, read-only inventory. Never prints source paths or payloads.
#include "openrepublic/content/building_construction.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
using namespace openrepublic::content;
struct Totals {
    std::uint64_t files{}, definitions{}, profiles{}, withCosts{}, failures{};
    std::uint64_t readFailures{}, definitionFailures{}, profileFailures{}, unnamedSkipped{}, withDiagnostics{};
    std::uint64_t operations{}, work{}, resources{}, automatic{}, multipliers{}, opaque{}, rows{};
    std::uint64_t malformed{}, orphan{}, unsupported{}, unresolved{}, unexpectedRows{};
};

bool ReadBounded(const std::filesystem::path& path, std::string& text) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > MaxBuildingDefinitionBytes) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    text.resize(static_cast<std::size_t>(size));
    if (size) input.read(text.data(), static_cast<std::streamsize>(size));
    // Detect truncation or growth while reading rather than inspecting a partial file.
    return (size == 0 || input.gcount() == static_cast<std::streamsize>(size)) &&
           !input.bad() && input.peek() == std::char_traits<char>::eof() && !input.bad();
}

void Inspect(const std::filesystem::path& path, Totals& totals) {
    ++totals.files;
    std::string text;
    BuildingDefinition definition;
    if (!ReadBounded(path, text)) {
        ++totals.readFailures;
        ++totals.failures;
        return;
    }
    if (!ParseBuildingDefinition(text, definition)) {
        ++totals.definitionFailures;
        ++totals.failures;
        return;
    }
    if (!definition.nameId) {
        ++totals.unnamedSkipped;
        return; // inventory selects named building definitions
    }
    ++totals.definitions;
    BuildingConstructionProfile profile;
    if (!ParseBuildingConstructionProfile(text, profile)) {
        ++totals.profileFailures;
        ++totals.failures;
        return;
    }
    ++totals.profiles;
    if (!profile.operations.empty()) ++totals.withCosts;
    if (!profile.diagnostics.empty()) ++totals.withDiagnostics;
    for (const auto& operation : profile.operations) {
        ++totals.operations;
        totals.rows += operation.retainedContinuation.size();
        switch (operation.kind) {
        case BuildingConstructionOperationKind::Work: ++totals.work; break;
        case BuildingConstructionOperationKind::Resource: ++totals.resources; break;
        case BuildingConstructionOperationKind::AutomaticResource: ++totals.automatic; break;
        case BuildingConstructionOperationKind::MultiplyExisting: ++totals.multipliers; break;
        case BuildingConstructionOperationKind::Unsupported: ++totals.opaque; break;
        }
    }
    for (const auto& diagnostic : profile.diagnostics) {
        switch (diagnostic.code) {
        case BuildingConstructionDiagnosticCode::MalformedArguments: ++totals.malformed; break;
        case BuildingConstructionDiagnosticCode::OrphanPhase: ++totals.orphan; break;
        case BuildingConstructionDiagnosticCode::UnsupportedDirective: ++totals.unsupported; break;
        case BuildingConstructionDiagnosticCode::UnresolvedAutomaticRecipe: ++totals.unresolved; break;
        case BuildingConstructionDiagnosticCode::UnexpectedContinuation: ++totals.unexpectedRows; break;
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 3 || std::string(argv[1]) != "--root") {
        std::cerr << "Usage: OpenRepublicConstructionProfileProbe --root <definition-directory>\n";
        return 2;
    }
    Totals totals;
    try {
        const std::filesystem::path root(argv[2]);
        if (!std::filesystem::is_directory(root)) {
            std::cerr << "Input directory is unavailable.\n";
            return 2;
        }
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".ini") continue;
            if (totals.files == 100000) {
                std::cerr << "Inventory file limit exceeded.\n";
                return 2;
            }
            Inspect(entry.path(), totals);
        }
    } catch (const std::filesystem::filesystem_error&) {
        std::cerr << "Inventory filesystem operation failed.\n";
        return 2;
    }
    std::cout << "{\"schemaVersion\":1,\"costImportSupported\":false"
              << ",\"files\":" << totals.files << ",\"definitions\":" << totals.definitions
              << ",\"profiles\":" << totals.profiles << ",\"withCosts\":" << totals.withCosts
              << ",\"failures\":" << totals.failures << ",\"operations\":" << totals.operations
              << ",\"readFailures\":" << totals.readFailures << ",\"definitionParseFailures\":" << totals.definitionFailures
              << ",\"profileParseFailures\":" << totals.profileFailures << ",\"unnamedSkipped\":" << totals.unnamedSkipped
              << ",\"profilesWithDiagnostics\":" << totals.withDiagnostics
              << ",\"work\":" << totals.work << ",\"resources\":" << totals.resources
              << ",\"automaticRecipes\":" << totals.automatic << ",\"multipliers\":" << totals.multipliers
              << ",\"opaqueSelectors\":" << totals.opaque << ",\"continuationRows\":" << totals.rows
              << ",\"malformed\":" << totals.malformed << ",\"orphan\":" << totals.orphan
              << ",\"unsupported\":" << totals.unsupported << ",\"unresolvedRecipes\":" << totals.unresolved
              << ",\"unexpectedContinuations\":" << totals.unexpectedRows << "}\n";
    if (!totals.definitions) {
        std::cerr << "No named building definitions found.\n";
        return 1;
    }
    return totals.failures ? 1 : 0;
}
