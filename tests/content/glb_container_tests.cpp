#include "openrepublic/content/glb_container.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t kMagic = 0x46546C67;
constexpr std::uint32_t kJson = 0x4E4F534A;
constexpr std::uint32_t kBin = 0x004E4942;

void AppendU32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}

void PatchU32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) bytes[offset + shift / 8] = static_cast<std::uint8_t>(value >> shift);
}

void AppendChunk(std::vector<std::uint8_t>& bytes, std::uint32_t type, const std::vector<std::uint8_t>& payload) {
    AppendU32(bytes, static_cast<std::uint32_t>(payload.size()));
    AppendU32(bytes, type);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
}

std::vector<std::uint8_t> MakeGlb(const std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>>& chunks) {
    std::vector<std::uint8_t> bytes;
    AppendU32(bytes, kMagic);
    AppendU32(bytes, 2);
    AppendU32(bytes, 0);
    for (const auto& chunk : chunks) AppendChunk(bytes, chunk.first, chunk.second);
    PatchU32(bytes, 8, static_cast<std::uint32_t>(bytes.size()));
    return bytes;
}

bool Check(const char* name, const std::vector<std::uint8_t>& bytes, bool expected,
           const std::string& expectedErrorPart = {}, std::size_t maxBytes = openrepublic::content::kMaxGlbContainerBytes) {
    openrepublic::content::GlbContainerInfo info;
    info.chunkCount = 77;
    std::string error;
    const bool result = openrepublic::content::PreflightGlbContainer(bytes, info, &error, maxBytes);
    const bool outputUnchangedOnFailure = expected || info.chunkCount == 77;
    const bool diagnosticMatches = expectedErrorPart.empty() || error.find(expectedErrorPart) != std::string::npos;
    if (result != expected || !outputUnchangedOnFailure || !diagnosticMatches) {
        std::cerr << "FAIL: " << name << " result=" << result << " error='" << error << "'\n";
        return false;
    }
    return true;
}

bool CheckPath(const char* name, const std::filesystem::path& path, bool expected,
               const std::string& expectedErrorPart = {}, std::size_t maxBytes = openrepublic::content::kMaxGlbContainerBytes) {
    openrepublic::content::GlbContainerInfo info;
    info.chunkCount = 77;
    std::string error;
    const bool result = openrepublic::content::PreflightGlbContainer(path, info, &error, maxBytes);
    const bool outputUnchangedOnFailure = expected || info.chunkCount == 77;
    const bool diagnosticMatches = expectedErrorPart.empty() || error.find(expectedErrorPart) != std::string::npos;
    if (result != expected || !outputUnchangedOnFailure || !diagnosticMatches) {
        std::cerr << "FAIL: " << name << " result=" << result << " error='" << error << "'\n";
        return false;
    }
    return true;
}

} // namespace

int main() {
    const std::vector<std::uint8_t> json{'{', '}', ' ', ' '};
    const std::vector<std::uint8_t> bin{0, 0, 0, 0};
    const auto valid = MakeGlb({{kJson, json}, {kBin, bin}});
    bool ok = true;
    ok &= Check("valid JSON then BIN framing", valid, true);
    ok &= Check("valid JSON-only framing", MakeGlb({{kJson, json}}), true);

    auto invalid = valid;
    PatchU32(invalid, 0, 0);
    ok &= Check("invalid magic", invalid, false, "invalid magic");
    invalid = valid;
    PatchU32(invalid, 4, 1);
    ok &= Check("invalid version", invalid, false, "version is not 2");
    invalid = valid;
    PatchU32(invalid, 8, static_cast<std::uint32_t>(valid.size() - 4));
    ok &= Check("declared length mismatch", invalid, false, "does not match actual");
    invalid = valid;
    invalid.pop_back();
    PatchU32(invalid, 8, static_cast<std::uint32_t>(invalid.size()));
    ok &= Check("truncated non-aligned file", invalid, false, "truncated");
    invalid = valid;
    PatchU32(invalid, 12, 0xFFFFFFFCu);
    ok &= Check("truncated chunk payload", invalid, false, "truncated");
    invalid = MakeGlb({{kBin, bin}, {kJson, json}});
    ok &= Check("misordered JSON chunk", invalid, false, "first chunk must be JSON");
    invalid = MakeGlb({{0x12345678u, bin}});
    ok &= Check("missing JSON chunk", invalid, false, "first chunk must be JSON");
    invalid = MakeGlb({{kJson, json}, {kJson, json}});
    ok &= Check("duplicate JSON chunk", invalid, false, "duplicate or misordered JSON");
    invalid = MakeGlb({{kJson, json}, {0x12345678u, bin}, {kBin, bin}});
    ok &= Check("misordered BIN chunk", invalid, false, "immediately follow");
    invalid = MakeGlb({{kJson, json}});
    ok &= Check("oversize configured bound", invalid, false, "byte limit", invalid.size() - 1);

    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path tempPath = std::filesystem::temp_directory_path() /
        ("openrepublic-glb-container-" + std::to_string(unique) + ".glb");
    const std::filesystem::path missingPath = tempPath.string() + ".missing";
    {
        std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
        if (!file) {
            std::cerr << "FAIL: could not create temporary GLB fixture.\n";
            return 1;
        }
        file.write(reinterpret_cast<const char*>(valid.data()), static_cast<std::streamsize>(valid.size()));
        if (!file) {
            std::cerr << "FAIL: could not write temporary GLB fixture.\n";
            return 1;
        }
    }
    ok &= CheckPath("path overload reads valid GLB", tempPath, true);
    ok &= CheckPath("path overload applies byte bound", tempPath, false, "byte limit", valid.size() - 1);
    ok &= CheckPath("path overload reports missing input", missingPath, false, "determine GLB file size");
    std::error_code cleanupError;
    std::filesystem::remove(tempPath, cleanupError);

    if (!ok) return 1;
    std::cout << "OpenRepublic GLB container framing tests passed.\n";
    return 0;
}
