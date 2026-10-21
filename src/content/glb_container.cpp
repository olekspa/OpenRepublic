#include "openrepublic/content/glb_container.h"

#include <fstream>
#include <limits>
#include <utility>

namespace openrepublic::content {
namespace {

constexpr std::uint32_t kGlbMagic = 0x46546C67; // ASCII "glTF" in little endian
constexpr std::uint32_t kGlbVersion = 2;
constexpr std::uint32_t kJsonChunk = 0x4E4F534A; // ASCII "JSON"
constexpr std::uint32_t kBinChunk = 0x004E4942;  // ASCII "BIN\0"
constexpr std::size_t kHeaderBytes = 12;
constexpr std::size_t kChunkHeaderBytes = 8;

std::uint32_t ReadU32LE(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

bool Fail(std::string* error, const char* message) {
    if (error != nullptr) *error = message;
    return false;
}

} // namespace

bool PreflightGlbContainer(const std::vector<std::uint8_t>& bytes,
                           GlbContainerInfo& output,
                           std::string* error,
                           std::size_t maxBytes) {
    if (bytes.size() > maxBytes) return Fail(error, "GLB container exceeds configured byte limit.");
    if (bytes.size() < kHeaderBytes) return Fail(error, "GLB container is shorter than its 12-byte header.");
    if (ReadU32LE(bytes.data()) != kGlbMagic) return Fail(error, "GLB container has invalid magic; expected 'glTF'.");
    if (ReadU32LE(bytes.data() + 4) != kGlbVersion) return Fail(error, "GLB container version is not 2.");

    const std::uint32_t declaredLength = ReadU32LE(bytes.data() + 8);
    if (declaredLength != bytes.size()) return Fail(error, "GLB declared length does not match actual file length.");
    if (declaredLength < kHeaderBytes + kChunkHeaderBytes) {
        return Fail(error, "GLB container has no complete JSON chunk header.");
    }

    GlbContainerInfo candidate;
    std::size_t cursor = kHeaderBytes;
    bool sawJson = false;
    bool sawBin = false;
    while (cursor < bytes.size()) {
        if (candidate.chunkCount >= kMaxGlbChunks) return Fail(error, "GLB container exceeds configured chunk-count limit.");
        if (bytes.size() - cursor < kChunkHeaderBytes) return Fail(error, "GLB container ends inside a chunk header.");

        const std::uint32_t chunkLength = ReadU32LE(bytes.data() + cursor);
        const std::uint32_t chunkType = ReadU32LE(bytes.data() + cursor + 4);
        cursor += kChunkHeaderBytes;

        if ((chunkLength & 3u) != 0) return Fail(error, "GLB chunk length is not 4-byte aligned.");
        if (static_cast<std::size_t>(chunkLength) > bytes.size() - cursor) {
            return Fail(error, "GLB chunk payload is truncated or exceeds the declared container.");
        }

        if (!sawJson) {
            if (chunkType != kJsonChunk) return Fail(error, "GLB first chunk must be JSON.");
            if (chunkLength == 0) return Fail(error, "GLB JSON chunk must not be empty.");
            sawJson = true;
            candidate.jsonChunkBytes = chunkLength;
            candidate.jsonChunkOffset = cursor;
        } else if (chunkType == kJsonChunk) {
            return Fail(error, "GLB contains a duplicate or misordered JSON chunk.");
        } else if (chunkType == kBinChunk) {
            if (candidate.chunkCount != 1) return Fail(error, "GLB BIN chunk must immediately follow the JSON chunk.");
            if (sawBin) return Fail(error, "GLB contains more than one BIN chunk.");
            sawBin = true;
            candidate.binaryChunkBytes = chunkLength;
        }

        ++candidate.chunkCount;
        cursor += static_cast<std::size_t>(chunkLength);
    }

    if (!sawJson) return Fail(error, "GLB container is missing its required JSON chunk.");
    if (cursor != bytes.size()) return Fail(error, "GLB chunk framing does not end at the container boundary.");
    output = candidate;
    if (error != nullptr) error->clear();
    return true;
}

bool ReadBoundedGlbContainer(const std::filesystem::path& path,
                             std::vector<std::uint8_t>& output,
                             std::string* error,
                             std::size_t maxBytes) {
    std::error_code ec;
    const auto fileSize = std::filesystem::file_size(path, ec);
    if (ec) return Fail(error, "Unable to determine GLB file size.");
    if (fileSize > maxBytes) return Fail(error, "GLB container exceeds configured byte limit.");
    if (fileSize > std::numeric_limits<std::size_t>::max()) return Fail(error, "GLB file size is not representable on this platform.");
    if (fileSize > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        return Fail(error, "GLB file size exceeds the stream read limit on this platform.");
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) return Fail(error, "Unable to open GLB file.");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
    if (!bytes.empty()) {
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (stream.gcount() != static_cast<std::streamsize>(bytes.size())) return Fail(error, "GLB file changed or ended during bounded read.");
    }
    if (stream.peek() != std::char_traits<char>::eof()) return Fail(error, "GLB file grew during bounded read.");
    if (stream.bad()) return Fail(error, "GLB file read failed.");
    output = std::move(bytes);
    if (error != nullptr) error->clear();
    return true;
}

bool PreflightGlbContainer(const std::filesystem::path& path,
                           GlbContainerInfo& output,
                           std::string* error,
                           std::size_t maxBytes) {
    std::vector<std::uint8_t> bytes;
    if (!ReadBoundedGlbContainer(path, bytes, error, maxBytes)) return false;
    return PreflightGlbContainer(bytes, output, error, maxBytes);
}

} // namespace openrepublic::content
