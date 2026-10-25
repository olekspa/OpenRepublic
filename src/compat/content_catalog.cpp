#include "openrepublic/compat/catalog_import.h"

#include <algorithm>
#include <fstream>
#include <system_error>

namespace openrepublic::compat {
namespace {

constexpr std::size_t kMaxDefinitionFiles = 8192;

bool ReadDefinition(const std::filesystem::path &path, std::string &text, std::string &error) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        error = "could not inspect a building definition file: " + ec.message();
        return false;
    }
    if (size > content::MaxBuildingDefinitionBytes) {
        error = "a building definition file exceeds the 4 MiB limit";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "could not open a building definition file";
        return false;
    }
    text.resize(static_cast<std::size_t>(size));
    if (size != 0)
        input.read(text.data(), static_cast<std::streamsize>(size));
    if (input.bad() || static_cast<std::uintmax_t>(input.gcount()) != size) {
        error = "could not read a building definition file";
        return false;
    }
    if (input.peek() != std::char_traits<char>::eof()) {
        error = "a building definition file changed while being read";
        return false;
    }
    return true;
}

bool Fail(std::string &error, const std::string &message) {
    error = message;
    return false;
}

} // namespace

bool ImportCatalogFromContentDirectory(const std::filesystem::path &inputRoot,
                                      const CatalogImportOptions &options,
                                      economy::Catalog &out, ImportReport &report,
                                      std::string &error) {
    error.clear();
    std::error_code ec;
    if (!std::filesystem::is_directory(inputRoot, ec) || ec)
        return Fail(error, "content definition path is not a readable directory");

    auto root = inputRoot;
    const auto mediaDefinitions = inputRoot / "media_soviet" / "buildings_types";
    const auto definitions = inputRoot / "buildings_types";
    if (std::filesystem::is_directory(mediaDefinitions, ec) && !ec)
        root = mediaDefinitions;
    else {
        ec.clear();
        if (std::filesystem::is_directory(definitions, ec) && !ec)
            root = definitions;
        else if (inputRoot.filename() != "buildings_types")
            return Fail(error, "could not find buildings_types below the supplied content root");
    }

    std::vector<std::filesystem::path> files;
    std::filesystem::directory_iterator iterator(root, ec), end;
    if (ec)
        return Fail(error, "could not enumerate the building definitions directory: " + ec.message());
    for (; iterator != end; iterator.increment(ec)) {
        if (ec)
            return Fail(error, "could not enumerate the building definitions directory: " + ec.message());
        const auto status = iterator->symlink_status(ec);
        if (ec)
            return Fail(error, "could not inspect an entry in the building definitions directory: " + ec.message());
        if (std::filesystem::is_regular_file(status) && iterator->path().extension() == ".ini") {
            if (files.size() == kMaxDefinitionFiles)
                return Fail(error, "building definitions directory exceeds the 8192 .ini file limit");
            files.push_back(iterator->path());
        }
    }
    std::sort(files.begin(), files.end());
    if (files.empty())
        return Fail(error, "building definitions directory contains no regular .ini files");

    std::map<std::string, content::BuildingDefinition> parsed;
    std::vector<SkippedBuilding> outsideAllowlist;
    for (const auto &path : files) {
        const std::string name = path.stem().u8string();
        if (name.empty())
            return Fail(error, "building definition file has an empty filename stem");
        if (!options.buildingWhitelist.empty() && !options.buildingWhitelist.count(name)) {
            outsideAllowlist.push_back(
                {name, "building is outside the configured exact-name allowlist; file was not read"});
            continue;
        }
        if (parsed.count(name))
            return Fail(error, "building definition directory contains duplicate filename stems");
        std::string text;
        if (!ReadDefinition(path, text, error))
            return false;
        content::BuildingDefinition definition;
        std::string parseError;
        if (!content::ParseBuildingDefinition(text, definition, &parseError))
            return Fail(error, "invalid building definition '" + name + "': " + parseError);
        parsed.emplace(name, std::move(definition));
    }

    if (!ImportCatalogFromDefinitions(parsed, options, out, report)) {
        error = report.error;
        return false;
    }
    report.definitionsSeen = files.size();
    report.buildingsSkipped += outsideAllowlist.size();
    report.skippedBuildings.insert(report.skippedBuildings.end(), outsideAllowlist.begin(),
                                   outsideAllowlist.end());
    std::sort(report.skippedBuildings.begin(), report.skippedBuildings.end(),
              [](const SkippedBuilding &left, const SkippedBuilding &right) {
                  return left.name < right.name;
              });
    for (const auto &name : options.buildingWhitelist)
        if (!parsed.count(name))
            report.warnings.push_back("allowlisted building definition file not found: '" +
                                      name + "'");
    return true;
}

} // namespace openrepublic::compat
