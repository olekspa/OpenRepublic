#include "openrepublic/content/mtl_dependencies.h"
#include "openrepublic/content/mtl_preview_texture.h"
#include "openrepublic/content/mtl_syntax.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

using namespace openrepublic::content;
namespace fs = std::filesystem;

namespace {

int failures = 0;

void Check(bool condition, const std::string& label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

class TempDirectory {
public:
    TempDirectory() {
        static std::atomic<unsigned long long> sequence{0};
        const auto nonce = static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        path_ = fs::temp_directory_path() /
            ("openrepublic-mtl-preview-texture-test-" + std::to_string(nonce) + "-" +
                std::to_string(sequence.fetch_add(1)));
        fs::create_directories(path_);
    }

    ~TempDirectory() {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }

    const fs::path& Path() const { return path_; }

private:
    fs::path path_;
};

bool WriteFixture(const fs::path& path) {
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    if (error) return false;
    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    output << "synthetic image bytes are not decoded by this test\n";
    return static_cast<bool>(output);
}

MtlDocument Parse(const std::string& source) {
    MtlDocument document;
    std::string error;
    if (!ParseMtlSyntax(source, document, &error)) {
        std::cerr << "fixture parse failed: " << error << '\n';
        ++failures;
    }
    return document;
}

std::vector<MtlPreviewBaseColorTexture> Build(
    const MtlDocument& document, const std::vector<fs::path>& roots) {
    MtlDependencyManifest dependencies;
    std::string error;
    if (BuildMtlDependencyManifest(document, roots, dependencies, &error) !=
        MtlDependencyManifestError::None) {
        std::cerr << "dependency fixture failed: " << error << '\n';
        ++failures;
        return {};
    }

    std::vector<MtlPreviewBaseColorTexture> textures;
    if (!BuildMtlPreviewBaseColorTextures(document, dependencies, textures, &error)) {
        std::cerr << "preview texture fixture failed: " << error << '\n';
        ++failures;
    }
    return textures;
}

void TestUniqueResolvedSlotZeroAndUnmappedSlots() {
    TempDirectory temporary;
    const auto root = temporary.Path() / "root";
    Check(WriteFixture(root / "wall.img") && WriteFixture(root / "normal.img"),
        "write synthetic image-path fixtures");
    const auto document = Parse(
        "$SUBMATERIAL wall\n"
        "$TEXTURE 00 wall.img\n"
        "$TEXTURE 1 normal.img\n"
        "$SUBMATERIAL roof\n"
        "$TEXTURE 1 missing.img\n"
        "$SUBMATERIAL trim\n"
        "$END\n");

    const auto textures = Build(document, {root});
    Check(textures.size() == 3, "one preview binding is returned for every submaterial section");
    if (textures.size() == 3) {
        Check(textures[0].status == MtlPreviewBaseColorTextureStatus::Resolved &&
            textures[0].resolvedPath == fs::canonical(root / "wall.img"),
            "a unique resolved integer slot zero becomes a local preview candidate");
        Check(textures[1].status == MtlPreviewBaseColorTextureStatus::NoSlotZero &&
            textures[1].resolvedPath.empty(),
            "nonzero texture slots remain unmapped in this preview adapter");
        Check(textures[2].status == MtlPreviewBaseColorTextureStatus::NoSlotZero &&
            textures[2].resolvedPath.empty(),
            "missing nonzero-slot references do not become slot-zero candidates");
    }
}

void TestAmbiguousMissingAndInvalidSlotZero() {
    TempDirectory temporary;
    const auto root = temporary.Path() / "root";
    Check(WriteFixture(root / "a.img") && WriteFixture(root / "b.img") &&
        WriteFixture(root / "c.img"),
        "write synthetic ambiguity fixtures");

    const auto duplicate = Parse(
        "$SUBMATERIAL duplicated\n"
        "$TEXTURE 0 a.img\n"
        "$TEXTURE 00 a.img\n"
        "$END\n");
    const auto duplicateTextures = Build(duplicate, {root});
    Check(duplicateTextures.size() == 1 &&
        duplicateTextures[0].status == MtlPreviewBaseColorTextureStatus::Ambiguous &&
        duplicateTextures[0].resolvedPath.empty(),
        "duplicate slot-zero references remain ambiguous even when they name the same file");

    const auto malformedCompanion = Parse(
        "$SUBMATERIAL malformed_companion\n"
        "$TEXTURE 0 a.img\n"
        "$TEXTURE 0.0 c.img\n"
        "$END\n");
    const auto malformedCompanionTextures = Build(malformedCompanion, {root});
    Check(malformedCompanionTextures.size() == 1 &&
        malformedCompanionTextures[0].status == MtlPreviewBaseColorTextureStatus::Ambiguous &&
        malformedCompanionTextures[0].resolvedPath.empty(),
        "a malformed texture index prevents claiming a sole unique slot-zero candidate");

    const auto missing = Parse(
        "$SUBMATERIAL missing\n"
        "$TEXTURE 0 absent.img\n"
        "$END\n");
    const auto missingTextures = Build(missing, {root});
    Check(missingTextures.size() == 1 &&
        missingTextures[0].status == MtlPreviewBaseColorTextureStatus::Unresolved &&
        missingTextures[0].resolvedPath.empty(),
        "unresolved slot-zero references never return a path");

    const auto invalidIndex = Parse(
        "$SUBMATERIAL decimal\n"
        "$TEXTURE 0.0 a.img\n"
        "$END\n");
    const auto invalidTextures = Build(invalidIndex, {root});
    Check(invalidTextures.size() == 1 &&
        invalidTextures[0].status == MtlPreviewBaseColorTextureStatus::InvalidSlotIndex &&
        invalidTextures[0].resolvedPath.empty(),
        "a numeric but non-integer texture index is not guessed as slot zero");
}

void TestManifestMismatchIsTransactional() {
    TempDirectory temporary;
    const auto root = temporary.Path() / "root";
    Check(WriteFixture(root / "a.img"), "write transactionality fixture");
    const auto document = Parse(
        "$SUBMATERIAL valid\n"
        "$TEXTURE 0 a.img\n"
        "$END\n");
    MtlDependencyManifest dependencies;
    Check(BuildMtlDependencyManifest(document, {root}, dependencies) ==
        MtlDependencyManifestError::None, "build matching dependency manifest");
    if (!dependencies.textures.empty())
        dependencies.textures[0].rawPathToken = "other.img";

    std::vector<MtlPreviewBaseColorTexture> output = {
        {MtlPreviewBaseColorTextureStatus::Ambiguous, "sentinel"}};
    std::string error;
    Check(!BuildMtlPreviewBaseColorTextures(document, dependencies, output, &error) &&
        !error.empty() && output.size() == 1 &&
        output[0].status == MtlPreviewBaseColorTextureStatus::Ambiguous &&
        output[0].resolvedPath == fs::path("sentinel"),
        "a mismatched manifest fails without replacing caller output");
}

} // namespace

int main() {
    TestUniqueResolvedSlotZeroAndUnmappedSlots();
    TestAmbiguousMissingAndInvalidSlotZero();
    TestManifestMismatchIsTransactional();
    if (failures != 0) {
        std::cerr << failures << " MTL preview texture check(s) failed\n";
        return 1;
    }
    std::cout << "MTL preview texture checks passed\n";
    return 0;
}
