#include "openrepublic/content/nmf_geometry.h"

#include "openrepublic/content/nmf_header.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

namespace openrepublic::content {
namespace {

constexpr std::size_t kHeaderSize = 20;
constexpr std::size_t kObjectHeaderSize = 0xe4;
constexpr std::size_t kObjectMeshPrefixSize = 0xe8;
constexpr std::size_t kSubmeshHeaderSize = 0x1c;
constexpr std::size_t kPartitionHeaderSize = 12;
constexpr std::uint32_t kFlagNormal = 0x8;
constexpr std::uint32_t kFlagTangent = 0x10;
constexpr std::uint32_t kFlagBitangent = 0x20;
constexpr std::uint32_t kFlagUv0 = 0x100;
constexpr std::uint32_t kFlagUv1 = 0x200;
constexpr std::uint32_t kFlagUv2 = 0x400;
constexpr std::uint32_t kFlagUv3 = 0x800;
constexpr std::uint32_t kFlagFourByteVertexStream = 0x10000;
constexpr std::uint32_t kFlagEightByteVertexStream = 0x20000;
constexpr std::uint32_t kFlagPerTriangleData = 0x40000;
// Bit zero appears in the local sample but has no associated stream branch in
// the reviewed loader; accept it without assigning a meaning.
constexpr std::uint32_t kSupportedFlags = 0x1 | kFlagNormal | kFlagTangent |
    kFlagBitangent | kFlagUv0 | kFlagUv1 | kFlagUv2 | kFlagUv3 |
    kFlagFourByteVertexStream | kFlagEightByteVertexStream | kFlagPerTriangleData;
constexpr std::size_t kMaxPartitionRecords = 65536;
constexpr std::size_t kMaxPartitionIndices = MaxNmfGeometryIndices;
constexpr float kMaxAbsoluteCoordinate = 1000000.0f;

void SetError(std::string* error, const char* message) {
    if (error != nullptr) *error = message;
}

bool Fail(std::string* error, const char* message) {
    SetError(error, message);
    return false;
}

bool CanRead(std::string_view bytes, std::size_t offset, std::size_t count,
             std::size_t limit) {
    return limit <= bytes.size() && offset <= limit && count <= limit - offset;
}

bool ReadU16LE(std::string_view bytes, std::size_t offset, std::size_t limit,
               std::uint16_t& value) {
    if (!CanRead(bytes, offset, sizeof(std::uint16_t), limit)) return false;
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data() + offset);
    value = static_cast<std::uint16_t>(data[0]) |
        (static_cast<std::uint16_t>(data[1]) << 8);
    return true;
}

bool ReadU32LE(std::string_view bytes, std::size_t offset, std::size_t limit,
               std::uint32_t& value) {
    if (!CanRead(bytes, offset, sizeof(std::uint32_t), limit)) return false;
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data() + offset);
    value = static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8) |
        (static_cast<std::uint32_t>(data[2]) << 16) |
        (static_cast<std::uint32_t>(data[3]) << 24);
    return true;
}

bool ReadFloatLE(std::string_view bytes, std::size_t offset, std::size_t limit,
                 float& value) {
    static_assert(sizeof(float) == sizeof(std::uint32_t), "NMF profile expects 32-bit floats");
    std::uint32_t bits = 0;
    if (!ReadU32LE(bytes, offset, limit, bits)) return false;
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

bool CheckedMultiply(std::size_t left, std::size_t right, std::size_t& result) {
    if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left) return false;
    result = left * right;
    return true;
}

bool SkipBytes(std::string_view bytes, std::size_t& cursor, std::size_t limit,
               std::size_t count) {
    if (!CanRead(bytes, cursor, count, limit)) return false;
    cursor += count;
    return true;
}

bool SkipFlaggedVertexArrays(std::string_view bytes, std::size_t& cursor,
                             std::size_t limit, std::uint32_t vertexCount,
                             std::uint32_t flags) {
    constexpr std::uint32_t threeFloatFlags[] = {
        kFlagNormal, kFlagTangent, kFlagBitangent,
    };
    constexpr std::uint32_t eightByteFlags[] = {
        kFlagUv0, kFlagUv1, kFlagUv2, kFlagUv3, kFlagEightByteVertexStream,
    };

    std::size_t arrayBytes = 0;
    for (const std::uint32_t flag : threeFloatFlags) {
        if ((flags & flag) == 0) continue;
        if (!CheckedMultiply(vertexCount, 12, arrayBytes) ||
            !SkipBytes(bytes, cursor, limit, arrayBytes)) return false;
    }
    for (const std::uint32_t flag : eightByteFlags) {
        if ((flags & flag) == 0) continue;
        if (!CheckedMultiply(vertexCount, 8, arrayBytes) ||
            !SkipBytes(bytes, cursor, limit, arrayBytes)) return false;
    }
    if ((flags & kFlagFourByteVertexStream) != 0) {
        if (!CheckedMultiply(vertexCount, 4, arrayBytes) ||
            !SkipBytes(bytes, cursor, limit, arrayBytes)) return false;
    }
    return true;
}

} // namespace

bool ParseNmfStaticGeometry(std::string_view bytes, NmfStaticGeometry& output,
                            std::string* error) {
    if (bytes.size() < kHeaderSize)
        return Fail(error, "NMF geometry profile requires at least 20 header bytes");
    if (bytes.size() > MaxNmfGeometryBytes)
        return Fail(error, "NMF geometry input exceeds the 256 MiB profile limit");

    NmfHeaderProfile header;
    if (!ParseNmfHeaderProfile(bytes, header, error)) return false;

    std::uint32_t declaredByteLength = 0;
    if (!ReadU32LE(bytes, 16, bytes.size(), declaredByteLength) ||
        declaredByteLength != bytes.size())
        return Fail(error, "NMF geometry byte-length field does not match the provided input");
    if (header.objectRecordCount == 0)
        return Fail(error, "NMF geometry profile contains no object records");

    std::size_t cursor = kHeaderSize + header.submaterialIdentifiers.size() * 64u;
    NmfStaticGeometry candidate;
    candidate.submeshes.reserve(std::min<std::size_t>(header.objectRecordCount,
        MaxNmfGeometrySubmeshes));
    std::size_t totalVertices = 0;
    std::size_t totalIndices = 0;
    std::size_t totalPartitions = 0;
    std::size_t totalPartitionIndices = 0;

    for (std::uint32_t objectIndex = 0;
         objectIndex < header.objectRecordCount; ++objectIndex) {
        if (!CanRead(bytes, cursor, kObjectHeaderSize, bytes.size()))
            return Fail(error, "NMF object fixed header is truncated");

        std::uint32_t objectType = 0;
        std::uint32_t objectByteLength = 0;
        if (!ReadU32LE(bytes, cursor, bytes.size(), objectType) ||
            !ReadU32LE(bytes, cursor + 4, bytes.size(), objectByteLength))
            return Fail(error, "NMF object header fields are truncated");
        if (objectType != 0)
            return Fail(error, "NMF geometry profile supports only static type-0 mesh objects");
        if (objectByteLength < kObjectMeshPrefixSize ||
            !CanRead(bytes, cursor, objectByteLength, bytes.size()))
            return Fail(error, "NMF object byte span is outside the provided input");
        const std::size_t objectEnd = cursor + objectByteLength;
        std::size_t objectCursor = cursor + kObjectHeaderSize;

        std::uint32_t submeshCount = 0;
        if (!ReadU32LE(bytes, objectCursor, objectEnd, submeshCount))
            return Fail(error, "NMF object submesh count is truncated");
        objectCursor += sizeof(std::uint32_t);
        if (submeshCount > MaxNmfHeaderObjectRecords ||
            submeshCount > MaxNmfGeometrySubmeshes - candidate.submeshes.size())
            return Fail(error, "NMF submesh count exceeds the bounded profile limit");

        for (std::uint32_t submeshIndex = 0; submeshIndex < submeshCount; ++submeshIndex) {
            if (!CanRead(bytes, objectCursor, kSubmeshHeaderSize, objectEnd))
                return Fail(error, "NMF submesh header is truncated");

            std::uint32_t vertexCount = 0;
            std::uint32_t indexCount = 0;
            std::uint32_t partitionCount = 0;
            std::uint32_t extraVertexSetCount = 0;
            std::uint32_t flags = 0;
            if (!ReadU32LE(bytes, objectCursor + 4, objectEnd, vertexCount) ||
                !ReadU32LE(bytes, objectCursor + 8, objectEnd, indexCount) ||
                !ReadU32LE(bytes, objectCursor + 12, objectEnd, partitionCount) ||
                !ReadU32LE(bytes, objectCursor + 16, objectEnd, extraVertexSetCount) ||
                !ReadU32LE(bytes, objectCursor + 20, objectEnd, flags))
                return Fail(error, "NMF submesh fields are truncated");
            if ((flags & ~kSupportedFlags) != 0)
                return Fail(error, "NMF submesh uses unsupported vertex-stream flags");
            if (extraVertexSetCount != 0)
                return Fail(error, "NMF profile does not decode additional vertex sets");
            if (vertexCount < 3 || vertexCount > 65536 || indexCount < 3 || indexCount % 3 != 0)
                return Fail(error, "NMF submesh is outside the bounded triangle-count profile");
            if (partitionCount > kMaxPartitionRecords - totalPartitions)
                return Fail(error, "NMF material-partition count exceeds the bounded profile limit");
            if (vertexCount > MaxNmfGeometryVertices - totalVertices ||
                indexCount > MaxNmfGeometryIndices - totalIndices)
                return Fail(error, "NMF aggregate geometry counts exceed the bounded profile limits");

            objectCursor += kSubmeshHeaderSize;
            std::size_t indexBytes = 0;
            if (!CheckedMultiply(indexCount, sizeof(std::uint16_t), indexBytes) ||
                !CanRead(bytes, objectCursor, indexBytes, objectEnd))
                return Fail(error, "NMF submesh index stream is truncated");

            NmfStaticSubmesh mesh;
            mesh.objectIndex = objectIndex;
            mesh.submeshIndex = submeshIndex;
            mesh.positions.reserve(vertexCount);
            mesh.indices.reserve(indexCount);
            for (std::uint32_t i = 0; i < indexCount; ++i) {
                std::uint16_t index = 0;
                if (!ReadU16LE(bytes, objectCursor + static_cast<std::size_t>(i) * 2u,
                    objectEnd, index) || index >= vertexCount)
                    return Fail(error, "NMF submesh index is outside its vertex array");
                mesh.indices.push_back(index);
            }
            objectCursor += indexBytes;

            std::size_t positionBytes = 0;
            if (!CheckedMultiply(vertexCount, sizeof(NmfPosition), positionBytes) ||
                !CanRead(bytes, objectCursor, positionBytes, objectEnd))
                return Fail(error, "NMF submesh position stream is truncated");
            for (std::uint32_t i = 0; i < vertexCount; ++i) {
                const std::size_t offset = objectCursor + static_cast<std::size_t>(i) * 12u;
                NmfPosition position;
                if (!ReadFloatLE(bytes, offset, objectEnd, position.x) ||
                    !ReadFloatLE(bytes, offset + 4, objectEnd, position.y) ||
                    !ReadFloatLE(bytes, offset + 8, objectEnd, position.z) ||
                    !std::isfinite(position.x) || !std::isfinite(position.y) ||
                    !std::isfinite(position.z) ||
                    std::abs(position.x) > kMaxAbsoluteCoordinate ||
                    std::abs(position.y) > kMaxAbsoluteCoordinate ||
                    std::abs(position.z) > kMaxAbsoluteCoordinate)
                    return Fail(error, "NMF submesh contains a non-finite or out-of-profile position");
                mesh.positions.push_back(position);
            }
            objectCursor += positionBytes;

            if (!SkipFlaggedVertexArrays(bytes, objectCursor, objectEnd, vertexCount, flags))
                return Fail(error, "NMF auxiliary vertex stream is truncated");

            if ((flags & kFlagPerTriangleData) != 0) {
                std::size_t triangleDataBytes = 0;
                if (!CheckedMultiply(indexCount / 3u, 40u, triangleDataBytes) ||
                    !SkipBytes(bytes, objectCursor, objectEnd, triangleDataBytes))
                    return Fail(error, "NMF per-triangle auxiliary stream is truncated");
            }

            totalPartitions += partitionCount;
            for (std::uint32_t partitionIndex = 0;
                 partitionIndex < partitionCount; ++partitionIndex) {
                if (!CanRead(bytes, objectCursor, kPartitionHeaderSize, objectEnd))
                    return Fail(error, "NMF submesh partition header is truncated");
                std::uint16_t partitionIndexCount = 0;
                if (!ReadU16LE(bytes, objectCursor + 10, objectEnd, partitionIndexCount))
                    return Fail(error, "NMF submesh partition index count is truncated");
                std::size_t partitionIndexBytes = 0;
                if (!CheckedMultiply(partitionIndexCount, sizeof(std::uint16_t), partitionIndexBytes) ||
                    !SkipBytes(bytes, objectCursor, objectEnd, kPartitionHeaderSize) ||
                    !CanRead(bytes, objectCursor, partitionIndexBytes, objectEnd))
                    return Fail(error, "NMF submesh partition indices are truncated");
                for (std::uint16_t i = 0; i < partitionIndexCount; ++i) {
                    std::uint16_t index = 0;
                    if (!ReadU16LE(bytes, objectCursor + static_cast<std::size_t>(i) * 2u,
                        objectEnd, index) || index >= vertexCount)
                        return Fail(error, "NMF material-partition index is outside its vertex array");
                }
                if (partitionIndexCount > kMaxPartitionIndices - totalPartitionIndices)
                    return Fail(error, "NMF aggregate partition indices exceed the bounded profile limit");
                totalPartitionIndices += partitionIndexCount;
                objectCursor += partitionIndexBytes;
            }

            totalVertices += vertexCount;
            totalIndices += indexCount;
            candidate.submeshes.push_back(std::move(mesh));
        }

        if (objectCursor != objectEnd)
            return Fail(error, "NMF object span contains unparsed bytes in this profile");
        cursor = objectEnd;
    }

    if (candidate.submeshes.empty())
        return Fail(error, "NMF geometry profile contains no renderable triangle submeshes");
    if (cursor != bytes.size())
        return Fail(error, "NMF geometry profile has trailing or unclaimed bytes");

    output = std::move(candidate);
    if (error != nullptr) error->clear();
    return true;
}

bool LoadNmfStaticGeometry(const std::filesystem::path& path,
                           NmfStaticGeometry& output,
                           std::string* error) {
    std::error_code fileError;
    const std::uintmax_t fileBytes = std::filesystem::file_size(path, fileError);
    if (fileError || fileBytes < kHeaderSize || fileBytes > MaxNmfGeometryBytes)
        return Fail(error, "NMF input is missing, unreadable, or outside the 256 MiB profile limit");

    std::ifstream stream(path, std::ios::binary);
    if (!stream) return Fail(error, "NMF input could not be opened");
    std::string bytes(static_cast<std::size_t>(fileBytes), '\0');
    stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream || stream.peek() != std::char_traits<char>::eof())
        return Fail(error, "NMF input changed during read or could not be read completely");

    return ParseNmfStaticGeometry(bytes, output, error);
}

} // namespace openrepublic::content
