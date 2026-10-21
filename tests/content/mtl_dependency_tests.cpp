#include "openrepublic/content/mtl_dependencies.h"
#include "openrepublic/content/mtl_syntax.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>
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
            ("openrepublic-mtl-dependency-test-" + std::to_string(nonce) + "-" +
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
    output << "synthetic texture fixture\n";
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

std::string OneTexture(const std::string& path) {
    return "$SUBMATERIAL synthetic_section\n$TEXTURE 007 " + path + "\n$END\n";
}

void TestOrderAndOpaqueTokens() {
    TempDirectory temporary;
    const auto root = temporary.Path() / "root";
    Check(WriteFixture(root / "textures" / "a.dds"), "write synthetic in-root a texture");
    Check(WriteFixture(root / "textures" / "b.dds"), "write synthetic in-root b texture");

    const auto document = Parse(
        "$SUBMATERIAL first_token\n"
        "$TEXTURE 007 textures/a.dds\n"
        "$TEXTURE 1 textures/missing.dds\n"
        "$TEXTURE 07 textures/a.dds\n"
        "$SUBMATERIAL second_token\n"
        "$TEXTURE 03 textures/b.dds\n"
        "$TEXTURE_MTL 9 alternate.dds\n"
        "$END\n");
    MtlDependencyManifest manifest;
    std::string error = "stale";
    const auto result = BuildMtlDependencyManifest(document, {root}, manifest, &error);
    Check(result == MtlDependencyManifestError::None && error.empty(),
        "valid document returns a complete manifest and clears the diagnostic");
    Check(manifest.roots.size() == 1 && manifest.roots[0].status == MtlDependencyRootStatus::Valid,
        "absolute existing directory is recorded as a valid root");
    Check(manifest.textures.size() == 4, "all repeated texture directives are retained");
    if (manifest.textures.size() == 4) {
        Check(manifest.textures[0].submaterialIndex == 0 &&
            manifest.textures[0].submaterialNameToken == "first_token" &&
            manifest.textures[0].sourceLine == 2 &&
            manifest.textures[0].textureIndexToken == "007" &&
            manifest.textures[0].rawPathToken == "textures/a.dds" &&
            manifest.textures[0].status == MtlDependencyStatus::Resolved &&
            manifest.textures[0].resolvedPath == fs::canonical(root / "textures" / "a.dds"),
            "first entry preserves source order, section, line, opaque index/path, and contained resolved path");
        Check(manifest.textures[1].textureIndexToken == "1" &&
            manifest.textures[1].rawPathToken == "textures/missing.dds" &&
            manifest.textures[1].status == MtlDependencyStatus::Missing &&
            manifest.textures[1].resolvedPath.empty(),
            "missing texture is reported without selecting a path");
        Check(manifest.textures[2].textureIndexToken == "07" &&
            manifest.textures[2].rawPathToken == "textures/a.dds" &&
            manifest.textures[2].status == MtlDependencyStatus::Resolved,
            "duplicate source reference remains a distinct ordered dependency");
        Check(manifest.textures[3].submaterialIndex == 1 &&
            manifest.textures[3].submaterialNameToken == "second_token" &&
            manifest.textures[3].sourceLine == 6 &&
            manifest.textures[3].textureIndexToken == "03" &&
            manifest.textures[3].status == MtlDependencyStatus::Resolved,
            "later sections preserve their original order and exact index token");
        Check(std::none_of(manifest.textures.begin(), manifest.textures.end(), [](const auto& entry) {
                return entry.rawPathToken == "alternate.dds";
            }),
            "unknown texture-like $TEXTURE_MTL remains outside the recognized manifest");
    }
}

void TestAmbiguityIndependentOfRootOrder() {
    TempDirectory temporary;
    const auto first = temporary.Path() / "first";
    const auto second = temporary.Path() / "second";
    Check(WriteFixture(first / "same.dds") && WriteFixture(second / "same.dds"),
        "write same synthetic texture under two roots");
    const auto document = Parse(OneTexture("same.dds"));

    MtlDependencyManifest forward;
    MtlDependencyManifest reverse;
    Check(BuildMtlDependencyManifest(document, {first, second}, forward) == MtlDependencyManifestError::None,
        "resolve forward root order");
    Check(BuildMtlDependencyManifest(document, {second, first}, reverse) == MtlDependencyManifestError::None,
        "resolve reverse root order");
    Check(forward.textures.size() == 1 && reverse.textures.size() == 1 &&
        forward.textures[0].status == MtlDependencyStatus::Ambiguous &&
        reverse.textures[0].status == MtlDependencyStatus::Ambiguous &&
        forward.textures[0].resolvedPath.empty() && reverse.textures[0].resolvedPath.empty(),
        "same relative asset in multiple roots is ambiguous regardless of root order");

    MtlDependencyManifest duplicateRoot;
    Check(BuildMtlDependencyManifest(document, {first, fs::canonical(first)}, duplicateRoot) ==
            MtlDependencyManifestError::None && duplicateRoot.textures.size() == 1 &&
            duplicateRoot.textures[0].status == MtlDependencyStatus::Resolved,
        "duplicate spellings of the same canonical root are counted once");
}

void TestRejectedPathForms() {
    TempDirectory temporary;
    const auto root = temporary.Path() / "root";
    Check(WriteFixture(root / "inside.dds"), "write safe path fixture");
    const std::vector<std::string> unsafe = {
        "../outside.dds",
        "folder/../outside.dds",
        R"(folder\..\outside.dds)",
        "/outside.dds",
        R"(\rooted\outside.dds)",
        R"(C:\outside.dds)",
        "C:outside.dds",
        "textures/file.dds:alternate-stream",
        R"(\\server\share\outside.dds)",
    };
    for (const auto& path : unsafe) {
        const auto document = Parse(OneTexture(path));
        MtlDependencyManifest manifest;
        Check(BuildMtlDependencyManifest(document, {root}, manifest) == MtlDependencyManifestError::None &&
            manifest.textures.size() == 1 &&
            manifest.textures[0].status == MtlDependencyStatus::RejectedPath &&
            manifest.textures[0].resolvedPath.empty(),
            "reject rooted, drive, UNC, or parent traversal path form");
    }

    auto nulDocument = Parse(OneTexture("inside.dds"));
    nulDocument.submaterials[0].directives[0].arguments[1] = std::string("inside\0.dds", 10);
    MtlDependencyManifest nulManifest;
    Check(BuildMtlDependencyManifest(nulDocument, {root}, nulManifest) == MtlDependencyManifestError::None &&
        nulManifest.textures.size() == 1 &&
        nulManifest.textures[0].status == MtlDependencyStatus::RejectedPath,
        "embedded NUL in a fabricated source path token is rejected");
}

void TestInvalidRootsAndFailureTransactionality() {
    TempDirectory temporary;
    const auto root = temporary.Path() / "root";
    const auto invalidRoot = temporary.Path() / "not-created";
    Check(WriteFixture(root / "inside.dds"), "write invalid-root fixture");
    const auto document = Parse(OneTexture("inside.dds"));

    MtlDependencyManifest invalidRootManifest;
    Check(BuildMtlDependencyManifest(document, {invalidRoot, root}, invalidRootManifest) ==
            MtlDependencyManifestError::None && invalidRootManifest.roots.size() == 2 &&
            invalidRootManifest.roots[0].status == MtlDependencyRootStatus::InvalidRoot &&
            invalidRootManifest.roots[1].status == MtlDependencyRootStatus::Valid &&
            invalidRootManifest.textures.size() == 1 &&
            invalidRootManifest.textures[0].status == MtlDependencyStatus::InvalidRoot,
        "one invalid supplied root makes resolution incomplete instead of selecting another root");

    MtlDependencyManifest noRoots;
    Check(BuildMtlDependencyManifest(document, {}, noRoots) == MtlDependencyManifestError::None &&
        noRoots.textures.size() == 1 && noRoots.textures[0].status == MtlDependencyStatus::InvalidRoot,
        "empty root set is reported as an invalid-root outcome");

    MtlDependencyManifest unchanged;
    unchanged.roots.push_back({MtlDependencyRootStatus::FileSystemError, {"sentinel-root"}});
    MtlTextureDependency sentinel;
    sentinel.submaterialIndex = 99;
    sentinel.submaterialNameToken = "sentinel";
    sentinel.sourceLine = 77;
    sentinel.textureIndexToken = "sentinel-index";
    sentinel.rawPathToken = "sentinel-path";
    sentinel.status = MtlDependencyStatus::Ambiguous;
    unchanged.textures.push_back(sentinel);

    auto malformed = document;
    malformed.terminated = false;
    std::string error;
    Check(BuildMtlDependencyManifest(malformed, {root}, unchanged, &error) ==
            MtlDependencyManifestError::InvalidDocument && !error.empty() &&
            unchanged.roots.size() == 1 &&
            unchanged.roots[0].canonicalPath == fs::path("sentinel-root") &&
            unchanged.textures.size() == 1 && unchanged.textures[0].submaterialIndex == 99 &&
            unchanged.textures[0].rawPathToken == "sentinel-path" &&
            unchanged.textures[0].status == MtlDependencyStatus::Ambiguous,
        "invalid document returns a generic error without changing caller output");

    std::vector<fs::path> tooManyRoots(MaxMtlDependencyRoots + 1, root);
    Check(BuildMtlDependencyManifest(document, tooManyRoots, unchanged, &error) ==
            MtlDependencyManifestError::LimitsExceeded &&
            unchanged.textures.size() == 1 && unchanged.textures[0].submaterialIndex == 99,
        "root-limit error also leaves caller output unchanged");

    auto excessive = document;
    excessive.submaterials.resize(MaxMtlDependencySections + 1, excessive.submaterials.front());
    Check(BuildMtlDependencyManifest(excessive, {root}, unchanged) ==
            MtlDependencyManifestError::LimitsExceeded &&
            unchanged.textures.size() == 1 && unchanged.textures[0].submaterialIndex == 99,
        "document section-limit error leaves caller output unchanged");
}

void TestAggregateCandidateProbeLimitIsTransactional() {
    TempDirectory temporary;
    std::vector<fs::path> roots;
    roots.reserve(MaxMtlDependencyRoots);
    for (std::size_t i = 0; i < MaxMtlDependencyRoots; ++i) {
        const auto root = temporary.Path() / ("root-" + std::to_string(i));
        std::error_code error;
        fs::create_directories(root, error);
        Check(!error, "create synthetic candidate-budget root");
        roots.push_back(root);
    }

    // 8193 safe references across 32 distinct roots exceed 262144 potential
    // inspections by 32. Build the parsed-document shape directly to keep the
    // test focused on resolver preflight rather than parser throughput.
    MtlDocument document;
    document.terminated = true;
    MtlSubmaterial section;
    section.nameToken = "budget_fixture";
    section.directives.reserve(8193);
    for (std::size_t i = 0; i < 8193; ++i) {
        MtlDirective directive;
        directive.name = "$TEXTURE";
        directive.kind = MtlDirectiveKind::Texture;
        directive.arguments = {"0", "synthetic/missing-" + std::to_string(i) + ".dds"};
        directive.line = i + 1;
        section.directives.push_back(std::move(directive));
    }
    document.submaterials.push_back(std::move(section));

    MtlDependencyManifest unchanged;
    unchanged.roots.push_back({MtlDependencyRootStatus::FileSystemError, {"sentinel-root"}});
    MtlTextureDependency sentinel;
    sentinel.submaterialIndex = 99;
    sentinel.submaterialNameToken = "sentinel";
    sentinel.sourceLine = 77;
    sentinel.textureIndexToken = "sentinel-index";
    sentinel.rawPathToken = "sentinel-path";
    sentinel.status = MtlDependencyStatus::Ambiguous;
    unchanged.textures.push_back(sentinel);

    std::string error;
    Check(BuildMtlDependencyManifest(document, roots, unchanged, &error) ==
            MtlDependencyManifestError::LimitsExceeded && !error.empty() &&
            unchanged.roots.size() == 1 &&
            unchanged.roots[0].canonicalPath == fs::path("sentinel-root") &&
            unchanged.textures.size() == 1 && unchanged.textures[0].submaterialIndex == 99 &&
            unchanged.textures[0].rawPathToken == "sentinel-path" &&
            unchanged.textures[0].status == MtlDependencyStatus::Ambiguous,
        "aggregate candidate-probe limit is rejected before resolution and preserves caller output");
}

void TestSymlinkContainmentAndFilesystemError() {
    TempDirectory temporary;
    const auto root = temporary.Path() / "root";
    const auto outside = temporary.Path() / "outside";
    Check(WriteFixture(outside / "secret.dds"), "write external synthetic texture");
    std::error_code error;
    fs::create_directories(root / "textures", error);
    if (error) {
        Check(false, "create synthetic root directories for symlink test");
        return;
    }
    error.clear();
    fs::create_directory_symlink(outside, root / "textures" / "linked", error);
    if (error) {
        std::cout << "SKIP: directory symlink creation is unavailable on this host\n";
        return;
    }
    const auto escapeDocument = Parse(OneTexture("textures/linked/secret.dds"));
    MtlDependencyManifest escaped;
    Check(BuildMtlDependencyManifest(escapeDocument, {root}, escaped) == MtlDependencyManifestError::None &&
        escaped.textures.size() == 1 && escaped.textures[0].status == MtlDependencyStatus::RejectedPath &&
        escaped.textures[0].resolvedPath.empty(),
        "existing directory symlink cannot resolve a path outside the supplied root");

    error.clear();
    fs::create_symlink(outside / "secret.dds", root / "textures" / "final-link.dds", error);
    if (error) {
        std::cout << "SKIP: file symlink creation is unavailable on this host\n";
    } else {
        const auto finalLinkDocument = Parse(OneTexture("textures/final-link.dds"));
        MtlDependencyManifest finalLinkResult;
        Check(BuildMtlDependencyManifest(finalLinkDocument, {root}, finalLinkResult) ==
                MtlDependencyManifestError::None && finalLinkResult.textures.size() == 1 &&
                finalLinkResult.textures[0].status == MtlDependencyStatus::RejectedPath &&
                finalLinkResult.textures[0].resolvedPath.empty(),
            "final-file symlink cannot resolve a file outside the supplied root");
    }

    error.clear();
    fs::create_directory_symlink(root / "loop", root / "loop", error);
    if (error) {
        std::cout << "SKIP: symlink-loop creation is unavailable on this host\n";
        return;
    }
    const auto loopDocument = Parse(OneTexture("loop/missing.dds"));
    MtlDependencyManifest loopResult;
    Check(BuildMtlDependencyManifest(loopDocument, {root}, loopResult) == MtlDependencyManifestError::None &&
        loopResult.textures.size() == 1 &&
        loopResult.textures[0].status == MtlDependencyStatus::FileSystemError,
        "filesystem resolution errors are reported without selecting a path");
}

void TestWindowsUnicodeRootIdentity() {
#ifdef _WIN32
    TempDirectory temporary;
    const auto upperCaseRoot = temporary.Path() / fs::path(L"Root\u00C4");
    const auto lowerCaseVariant = temporary.Path() / fs::path(L"root\u00E4");
    Check(WriteFixture(upperCaseRoot / "unicode.dds"), "write fixture under Unicode root");
    const auto document = Parse(OneTexture("unicode.dds"));
    MtlDependencyManifest manifest;
    Check(BuildMtlDependencyManifest(document, {upperCaseRoot, lowerCaseVariant}, manifest) ==
            MtlDependencyManifestError::None && manifest.roots.size() == 2 &&
            manifest.roots[0].status == MtlDependencyRootStatus::Valid &&
            manifest.roots[1].status == MtlDependencyRootStatus::Valid &&
            manifest.textures.size() == 1 &&
            manifest.textures[0].status == MtlDependencyStatus::Resolved,
        "Unicode case-variant spellings of one Windows root are compared ordinally and deduplicated");
#endif
}

} // namespace

int main() {
    TestOrderAndOpaqueTokens();
    TestAmbiguityIndependentOfRootOrder();
    TestRejectedPathForms();
    TestInvalidRootsAndFailureTransactionality();
    TestAggregateCandidateProbeLimitIsTransactional();
    TestSymlinkContainmentAndFilesystemError();
    TestWindowsUnicodeRootIdentity();
    if (failures != 0) {
        std::cerr << failures << " MTL dependency manifest test(s) failed\n";
        return 1;
    }
    std::cout << "All MTL dependency manifest tests passed\n";
    return 0;
}
