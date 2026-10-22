#include "openrepublic/content/tree_placements.h"

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
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

void PutU32(std::string& bytes, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<char>((value >> (i * 8)) & 0xff));
}

void PutFloat(std::string& bytes, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    PutU32(bytes, bits);
}

void PutGroup(std::string& bytes, const std::string& name,
              const std::vector<std::array<float, 3>>& points) {
    std::string fixedName(64, '\0');
    std::memcpy(fixedName.data(), name.data(), name.size());
    bytes += fixedName;
    PutU32(bytes, static_cast<std::uint32_t>(points.size()));
    for (std::size_t i = 0; i < points.size(); ++i) {
        for (float value : points[i]) PutFloat(bytes, value);
        // Preserve unknown bytes; tests ensure the decoder does not mislabel them.
        bytes.append({static_cast<char>(i + 1), '\x12', '\x34', '\x56', '\x78'});
    }
}

std::string MakeFile() {
    std::string bytes;
    PutU32(bytes, 2);
    PutU32(bytes, 0x12345678); // opaque header word
    PutGroup(bytes, "pine1", {{{-123.5f, 14.25f, 900.0f}, {10.0f, 0.0f, -20.0f}}});
    PutGroup(bytes, "topol", {{{-9999.0f, 2.0f, 9999.0f}}});
    PutU32(bytes, 0xabcdef01); // opaque trailer
    return bytes;
}

void TestExtraction() {
    TreePlacements decoded;
    std::string error;
    Check(ExtractTreePlacements(MakeFile(), decoded, &error), "observed-profile groups decode: " + error);
    Check(decoded.species.size() == 2 && decoded.species[0].modelName == "pine1" &&
              decoded.species[1].modelName == "topol", "species names retain order");
    Check(decoded.species.size() == 2 && decoded.species[0].placementCount == 2 &&
              decoded.species[1].placementCount == 1, "species counts are retained");
    Check(decoded.placements.size() == 3 && decoded.placements[0].typeIndex == 0 &&
              decoded.placements[2].typeIndex == 1, "placements reference species-table indices");
    Check(decoded.placements.size() == 3 && decoded.placements[0].x == -123.5 &&
              decoded.placements[0].y == 14.25 && decoded.placements[0].z == 900.0,
          "first twelve record bytes decode as XYZ floats");
    Check(decoded.placements.size() == 3 && !decoded.placements[0].yawRadians &&
              !decoded.placements[0].scale && decoded.diagnostic.find("remain unknown") != std::string::npos,
          "unresolved record fields remain unset and are diagnosed");
    Check(decoded.opaqueHeaderValue == 0x12345678 && decoded.opaqueTrailer == 0xabcdef01,
          "opaque header and trailer values are preserved");
    Check(error.empty(), "successful decode clears error output");

    std::string empty;
    PutU32(empty, 0);
    PutU32(empty, 0);
    PutU32(empty, 0);
    TreePlacements emptyDecoded;
    Check(ExtractTreePlacements(empty, emptyDecoded, &error) && emptyDecoded.species.empty() &&
              emptyDecoded.placements.empty(), "empty-map zero-species profile is accepted");
}

void TestFailClosed() {
    TreePlacements keep;
    keep.species.push_back({"keep", 0});
    std::string error;
    std::string shortFile = MakeFile();
    shortFile.pop_back();
    Check(!ExtractTreePlacements(shortFile, keep, &error) && !error.empty() &&
              keep.species.size() == 1 && keep.species[0].modelName == "keep",
          "truncated payload fails transactionally");

    std::string oversized;
    PutU32(oversized, MaxTreeSpecies + 1);
    PutU32(oversized, 0);
    Check(!ExtractTreePlacements(oversized, keep, &error), "excessive species count is rejected");

    std::string nonFinite = MakeFile();
    const std::uint32_t nanBits = 0x7fc00000;
    for (int i = 0; i < 4; ++i) nonFinite[76 + i] = static_cast<char>((nanBits >> (i * 8)) & 0xff);
    Check(!ExtractTreePlacements(nonFinite, keep, &error), "non-finite XYZ is rejected");

    std::string invalidName = MakeFile();
    invalidName[8] = '!';
    Check(!ExtractTreePlacements(invalidName, keep, &error), "invalid species name is rejected");

    std::string opaquePrefix = MakeFile();
    opaquePrefix[8 + 20] = '\x7f';
    TreePlacements prefixDecoded;
    Check(ExtractTreePlacements(opaquePrefix, prefixDecoded, &error),
          "unknown species-prefix bytes are retained without interpretation");

    std::string extra = MakeFile();
    extra.push_back('\0');
    Check(!ExtractTreePlacements(extra, keep, &error), "unexpected trailing bytes are rejected");
}

void ValidatePackageTreeBins(const std::filesystem::path& root) {
    std::size_t files = 0;
    std::uint64_t placements = 0;
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(root, ec), end;
         it != end && !ec; it.increment(ec)) {
        if (!it->is_regular_file() || it->path().filename() != "trees.bin") continue;
        TreePlacements decoded;
        std::string error;
        Check(LoadTreePlacements(it->path().parent_path(), decoded, &error),
              "package decode " + it->path().string() + ": " + error);
        ++files;
        placements += decoded.placements.size();
    }
    Check(!ec, "package inventory traversal completed: " + ec.message());
    Check(files > 0, "package validation found trees.bin files under the supplied root");
    if (failures == 0)
        std::cout << "Validated " << files << " trees.bin files and " << placements << " placements\n";
}

} // namespace

int main(int argc, char** argv) {
    TestExtraction();
    TestFailClosed();
    if (argc > 1) ValidatePackageTreeBins(argv[1]);
    return failures == 0 ? 0 : 1;
}
