#include "openrepublic/content/building_types.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace openrepublic::content;

namespace {

int failures = 0;

void Check(bool condition, const std::string& label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

std::string Lines(std::initializer_list<const char*> lines) {
    std::string text;
    for (const char* line : lines) {
        text += line;
        text += "\r\n";
    }
    return text;
}

const std::string kSynthetic = Lines({
    "$TYPE alpha",
    " MODEL content/alpha.nmf",
    " MODEL_LOD content/alpha_lod.nmf 1000.000000",
    " MODEL_LOD2 content/alpha_lod2.nmf",
    " MATERIAL content/alpha.mtl",
    " MODELEMISSIVE  content/alpha_e.nmf",
    " MATERIALEMISSIVE  content/alpha_e.mtl",
    " LIFE 3800.000000",
    "--LIGHT 1 2 3 4",
    " LIGHT 1 2 3 4",
    " END",
    "",
    "  $TYPE beta",
    " MODEL content/beta.nmf",
    " MATERIALEMISSIEV content/typo.mtl",
    " END",
    "",
    "$END",
});

void TestParsesSyntheticDocument() {
    BuildingTypesDocument document;
    std::string error;
    Check(ParseBuildingTypes(kSynthetic, document, &error), "synthetic document parses: " + error);
    Check(document.types.size() == 2 && document.terminated && document.commentLikeLines == 1,
          "two types, a terminator and one comment-like line are recorded");
    const auto* alpha = FindBuildingType(document, "alpha");
    Check(alpha != nullptr && alpha->model == "content/alpha.nmf" &&
              alpha->material == "content/alpha.mtl" && alpha->emissiveModel == "content/alpha_e.nmf" &&
              alpha->emissiveMaterial == "content/alpha_e.mtl",
          "recognized single directives are stored");
    Check(alpha != nullptr && alpha->lod1 && alpha->lod1->path == "content/alpha_lod.nmf" &&
              alpha->lod1->trailingNumber && *alpha->lod1->trailingNumber == 1000.0 && alpha->lod2 &&
              !alpha->lod2->trailingNumber,
          "LOD references keep their optional trailing number");
    Check(alpha != nullptr && alpha->otherDirectives.size() == 2 &&
              alpha->otherDirectives[0].name == "LIFE" && alpha->otherDirectives[1].name == "LIGHT",
          "unrecognized directives are preserved in order and comment-like lines are skipped");
    const auto* beta = FindBuildingType(document, "beta");
    Check(beta != nullptr && beta->material.empty() && beta->otherDirectives.size() == 1 &&
              beta->otherDirectives[0].name == "MATERIALEMISSIEV",
          "a misspelled directive is preserved, not interpreted");
    Check(FindBuildingType(document, "gamma") == nullptr, "unknown type names are not found");
}

void TestRejectsMalformedDocuments() {
    BuildingTypesDocument document;
    BuildingTypesDocument unchanged;
    unchanged.commentLikeLines = 77;
    document = unchanged;
    const auto rejects = [&](const std::string& text, const std::string& label) {
        std::string error;
        Check(!ParseBuildingTypes(text, document, &error) && !error.empty() &&
                  document.commentLikeLines == 77,
              label);
    };
    rejects(Lines({"$TYPE a", " MODEL x.nmf"}), "missing END is rejected");
    rejects(Lines({" MODEL x.nmf"}), "directive outside a block is rejected");
    rejects(Lines({"$TYPE a", "$TYPE b", " END"}), "nested $TYPE is rejected");
    rejects(Lines({"$TYPE a", " END", "$TYPE a", " END"}), "duplicate type names are rejected");
    rejects(Lines({"$TYPE a", " MODEL x", " MODEL y", " END"}), "repeated MODEL is rejected");
    rejects(Lines({"$TYPE a", " MODEL", " END"}), "MODEL without a path is rejected");
    rejects(Lines({"$TYPE a", " MODEL_LOD x nan", " END"}), "non-finite LOD number is rejected");
    rejects(Lines({"$TYPE a", " END", "$END", "$TYPE b"}), "content after $END is rejected");
    rejects(Lines({" END"}), "END outside a block is rejected");
    rejects(std::string("$TYPE a\0b", 9), "embedded NUL is rejected");
    rejects(std::string(MaxBuildingTypesInputBytes + 1, 'x'), "oversized input is rejected");
    rejects("$TYPE a\r\n " + std::string(5000, 'x') + "\r\n END\r\n", "over-long lines are rejected");
}

void TestResolvesAssetsInsideRoot() {
    namespace fs = std::filesystem;
    const auto base = fs::temp_directory_path() /
                      ("openrepublic_building_types_" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(base / "root" / "content");
    fs::create_directories(base / "outside");
    for (const char* name : {"root/content/alpha.nmf", "root/content/alpha.mtl",
                             "root/content/alpha_lod.nmf", "outside/secret.nmf"}) {
        std::ofstream(base / name) << "x";
    }

    BuildingTypeDefinition type;
    type.name = "alpha";
    type.model = "content/alpha.nmf";
    type.material = "content/alpha.mtl";
    type.lod1 = BuildingTypeModelReference{"content/alpha_lod.nmf", 5.0};

    BuildingTypeAssetPaths paths;
    std::string error;
    Check(ResolveBuildingTypeAssets(type, 0, base / "root", paths, &error) &&
              paths.model.filename() == "alpha.nmf" && paths.material.filename() == "alpha.mtl",
          "level 0 resolves model and material: " + error);
    Check(ResolveBuildingTypeAssets(type, 1, base / "root", paths, &error) &&
              paths.model.filename() == "alpha_lod.nmf",
          "level 1 selects MODEL_LOD");

    const BuildingTypeAssetPaths before = paths;
    Check(!ResolveBuildingTypeAssets(type, 2, base / "root", paths, &error) && paths.model == before.model,
          "a missing level fails and leaves output unchanged");
    Check(!ResolveBuildingTypeAssets(type, 3, base / "root", paths, &error), "level 3 is rejected");
    Check(!ResolveBuildingTypeAssets(type, 0, fs::path("relative"), paths, &error),
          "a relative root is rejected");

    BuildingTypeDefinition escaping = type;
    escaping.model = "../outside/secret.nmf";
    Check(!ResolveBuildingTypeAssets(escaping, 0, base / "root", paths, &error),
          "parent-directory traversal is rejected");
    escaping.model = "C:/windows/x.nmf";
    Check(!ResolveBuildingTypeAssets(escaping, 0, base / "root", paths, &error),
          "drive-prefixed paths are rejected");
    escaping.model = "content/missing.nmf";
    Check(!ResolveBuildingTypeAssets(escaping, 0, base / "root", paths, &error),
          "a missing file is rejected");

    std::error_code cleanup;
    fs::remove_all(base, cleanup);
}

} // namespace

int main() {
    TestParsesSyntheticDocument();
    TestRejectsMalformedDocuments();
    TestResolvesAssetsInsideRoot();
    if (failures != 0) {
        std::cerr << failures << " building-type test(s) failed\n";
        return 1;
    }
    std::cout << "All building-type tests passed\n";
    return 0;
}
