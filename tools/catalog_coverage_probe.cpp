// Local-only aggregate probe. It prints no source paths, names of definitions,
// or source payloads; the supplied directory is read-only.
#include "openrepublic/compat/catalog_import.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using namespace openrepublic;

int main(int argc, char **argv) {
    if (argc != 3 || std::string(argv[1]) != "--root") {
        std::cerr << "Usage: OpenRepublicCatalogCoverageProbe --root <buildings_types-directory>\n";
        return 2;
    }
    const fs::path root(argv[2]);
    std::map<std::string, content::BuildingDefinition> definitions;
    std::size_t readFailures = 0, parseFailures = 0, unnamed = 0, duplicates = 0;
    try {
        if (!fs::is_directory(root)) return 2;
        for (const auto &entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".ini") continue;
            if (definitions.size() + readFailures + parseFailures + unnamed + duplicates >= 100000) return 2;
            std::error_code ec;
            const auto size = fs::file_size(entry.path(), ec);
            if (ec || size > content::MaxBuildingDefinitionBytes) { ++readFailures; continue; }
            std::ifstream input(entry.path(), std::ios::binary);
            std::string text(static_cast<std::size_t>(size), '\0');
            if (!input || (size && !input.read(text.data(), static_cast<std::streamsize>(size)))) {
                ++readFailures; continue;
            }
            content::BuildingDefinition definition;
            if (!content::ParseBuildingDefinition(text, definition)) { ++parseFailures; continue; }
            if (!definition.nameId) { ++unnamed; continue; }
            const auto name = entry.path().stem().string();
            if (!definitions.emplace(name, std::move(definition)).second) ++duplicates;
        }
    } catch (const fs::filesystem_error &) { return 2; }

    economy::Catalog catalog;
    compat::ImportReport report;
    compat::CatalogImportOptions options;
    if (!compat::ImportCatalogFromDefinitions(definitions, options, catalog, report)) {
        std::cerr << "Catalog import failed: " << report.error << "\n";
        return 1;
    }
    std::cout << "Definitions parsed: " << definitions.size() << "\n"
              << "Valid economy building types: " << report.buildingsImported << "\n"
              << "Skipped: " << report.buildingsSkipped << "\n"
              << "Definition read/parse/unnamed/duplicate counts: " << readFailures << "/"
              << parseFailures << "/" << unnamed << "/" << duplicates << "\n\n"
              << "Skip reasons:\n";
    std::map<std::string, std::size_t> reasons;
    for (const auto &item : report.skippedBuildings) ++reasons[item.reason];
    for (const auto &item : reasons) std::cout << item.second << "\t" << item.first << "\n";
    std::cout << "\n" << compat::DescribeImport(report);
    return 0;
}
