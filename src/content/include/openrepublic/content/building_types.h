#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace openrepublic::content {

// Directives outside the recognized asset set are preserved verbatim and in
// order. Their meaning is not interpreted.
struct BuildingTypeDirective {
    std::string name;
    std::vector<std::string> arguments;
    std::size_t line = 0;
};

struct BuildingTypeModelReference {
    // Opaque content-root-relative path token as written in the data file.
    std::string path;
    // Second argument of MODEL_LOD / MODEL_LOD2 when present in the data.
    std::optional<double> trailingNumber;
    // Value the build-scoped loader uses: the trailing number when present,
    // otherwise its initial default (1000 for MODEL_LOD, 2000 for MODEL_LOD2).
    // The loader reads it with `%f`; what consumes it (presumably a LOD switch
    // distance, in unknown units) was not traced.
    double loaderNumber = 0.0;
};

struct BuildingTypeDefinition {
    std::string name;
    std::size_t line = 0;
    std::string model;                                   // MODEL
    std::optional<BuildingTypeModelReference> lod1;      // MODEL_LOD
    std::optional<BuildingTypeModelReference> lod2;      // MODEL_LOD2
    std::string material;                                // MATERIAL
    std::string emissiveModel;                           // MODELEMISSIVE
    std::string emissiveMaterial;                        // MATERIALEMISSIVE
    std::vector<BuildingTypeDirective> otherDirectives;
};

struct BuildingTypesDocument {
    std::vector<BuildingTypeDefinition> types;
    bool terminated = false;          // trailing `$END` seen
    // Lines whose first token starts with "--". The game has no comment syntax:
    // its token loop skips any unrecognized token, so such a line is skipped
    // token by token. OpenRepublic skips the whole line and counts it.
    std::size_t commentLikeLines = 0;
};

constexpr std::size_t MaxBuildingTypesInputBytes = 8u * 1024u * 1024u;
constexpr std::size_t MaxBuildingTypes = 8192;
constexpr std::size_t MaxBuildingTypeDirectives = 4096;

// Parses the line-oriented `$TYPE <name>` ... `END` block format observed in
// one WRSR build's building-type data file (OBSERVED, single build). Leading
// whitespace is ignored; blank lines are skipped; lines whose first token
// starts with "--" are skipped and counted (equivalent to the game skipping
// those unrecognized tokens). Recognized single-occurrence
// directives are MODEL, MODEL_LOD, MODEL_LOD2, MATERIAL, MODELEMISSIVE and
// MATERIALEMISSIVE; a repeat or wrong argument count is an error. Everything
// else inside a block is preserved. Duplicate type names, nesting, stray
// content, a missing `END`, or content after `$END` are errors. No game
// loader semantics are claimed. On failure output is unchanged.
bool ParseBuildingTypes(const std::string& text, BuildingTypesDocument& output,
                        std::string* error = nullptr);

// Reads `<contentRoot>/buildings/buildingtypes.ini` (bounded) and parses it.
bool LoadBuildingTypesFile(const std::filesystem::path& contentRoot, BuildingTypesDocument& output,
                           std::string* error = nullptr);

const BuildingTypeDefinition* FindBuildingType(const BuildingTypesDocument& document,
                                               const std::string& name);

struct BuildingTypeAssetPaths {
    std::filesystem::path model;
    std::filesystem::path material; // empty when the type has no MATERIAL
};

// Resolves a type's model (lod 0 = MODEL, 1 = MODEL_LOD, 2 = MODEL_LOD2) and
// MATERIAL against an absolute content root. Tokens must be relative without
// "..", drive prefixes, ':' or NULs; both files must exist as regular files
// whose canonical path stays inside the canonical root. Does not open files.
// On failure output is unchanged.
bool ResolveBuildingTypeAssets(const BuildingTypeDefinition& type, int lod,
                               const std::filesystem::path& contentRoot,
                               BuildingTypeAssetPaths& output,
                               std::string* error = nullptr);

} // namespace openrepublic::content
