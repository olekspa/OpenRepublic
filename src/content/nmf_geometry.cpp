#include "openrepublic/content/nmf_geometry.h"

#include "openrepublic/content/nmf_header.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

namespace openrepublic::content {
namespace {

constexpr std::size_t kHeaderSize = 20;
constexpr std::size_t kObjectRecordSize = 0xe8;
constexpr std::size_t kLodHeaderSize = 0x1c;
constexpr std::size_t kPartitionHeaderSize = 12;
// The loader has no byte-consumption branch for bit 0x1; its meaning is unknown.
constexpr std::uint32_t kFlagNoStreamBit0x1 = 0x1;
constexpr std::uint32_t kFlagAttribute0x8 = 0x8;
constexpr std::uint32_t kFlagStream0x10Vec3 = 0x10;
constexpr std::uint32_t kFlagStream0x20Vec3 = 0x20;
constexpr std::uint32_t kFlagStream0x100EightBytes = 0x100;
constexpr std::uint32_t kFlagStream0x200EightBytes = 0x200;
constexpr std::uint32_t kFlagStream0x400EightBytes = 0x400;
constexpr std::uint32_t kFlagStream0x800EightBytes = 0x800;
constexpr std::uint32_t kFlagStream0x10000FourBytes = 0x10000;
constexpr std::uint32_t kFlagStream0x20000EightBytes = 0x20000;
constexpr std::uint32_t kFlagStream0x40000 = 0x40000;
constexpr std::size_t kStream0x40000FirstBytesPerTriangle = 16;
constexpr std::size_t kStream0x40000SecondBytesPerTriangle = 24;
constexpr std::uint32_t kSupportedFlags =
    kFlagNoStreamBit0x1 | kFlagAttribute0x8 | kFlagStream0x10Vec3 | kFlagStream0x20Vec3 |
    kFlagStream0x100EightBytes | kFlagStream0x200EightBytes | kFlagStream0x400EightBytes |
    kFlagStream0x800EightBytes | kFlagStream0x10000FourBytes | kFlagStream0x20000EightBytes |
    kFlagStream0x40000;
constexpr std::size_t kMaxPartitionRecords = 65536;
constexpr std::size_t kMaxPartitionIndices = MaxNmfGeometryIndices;
constexpr float kMaxAbsoluteCoordinate = 1000000.0f;

void SetError(std::string* error, const char* message) {
    if (error != nullptr)
        *error = message;
}

bool Fail(std::string* error, const char* message) {
    SetError(error, message);
    return false;
}

bool CanRead(std::string_view bytes, std::size_t offset, std::size_t count, std::size_t limit) {
    return limit <= bytes.size() && offset <= limit && count <= limit - offset;
}

bool ReadU16LE(std::string_view bytes, std::size_t offset, std::size_t limit,
               std::uint16_t& value) {
    if (!CanRead(bytes, offset, 2, limit))
        return false;
    const auto* p = reinterpret_cast<const unsigned char*>(bytes.data() + offset);
    value = static_cast<std::uint16_t>(p[0]) | (static_cast<std::uint16_t>(p[1]) << 8);
    return true;
}

bool ReadU32LE(std::string_view bytes, std::size_t offset, std::size_t limit,
               std::uint32_t& value) {
    if (!CanRead(bytes, offset, 4, limit))
        return false;
    const auto* p = reinterpret_cast<const unsigned char*>(bytes.data() + offset);
    value = static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
            (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
    return true;
}

bool ReadFloatLE(std::string_view bytes, std::size_t offset, std::size_t limit, float& value) {
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    std::uint32_t bits = 0;
    if (!ReadU32LE(bytes, offset, limit, bits))
        return false;
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

bool CheckedMultiply(std::size_t a, std::size_t b, std::size_t& result) {
    if (a != 0 && b > std::numeric_limits<std::size_t>::max() / a)
        return false;
    result = a * b;
    return true;
}

bool Skip(std::string_view bytes, std::size_t& cursor, std::size_t limit, std::size_t n) {
    if (!CanRead(bytes, cursor, n, limit))
        return false;
    cursor += n;
    return true;
}

bool ReadVector3Array(std::string_view bytes, std::size_t& cursor, std::size_t limit,
                      std::uint32_t count, std::vector<NmfNormal>& output) {
    std::size_t byteCount = 0;
    if (!CheckedMultiply(count, 12, byteCount) || !CanRead(bytes, cursor, byteCount, limit))
        return false;
    output.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto at = cursor + static_cast<std::size_t>(i) * 12;
        NmfNormal value;
        if (!ReadFloatLE(bytes, at, limit, value.x) ||
            !ReadFloatLE(bytes, at + 4, limit, value.y) ||
            !ReadFloatLE(bytes, at + 8, limit, value.z) || !std::isfinite(value.x) ||
            !std::isfinite(value.y) || !std::isfinite(value.z))
            return false;
        output.push_back(value);
    }
    cursor += byteCount;
    return true;
}

bool SkipAuxiliaryVertexStreams(std::string_view bytes, std::size_t& cursor, std::size_t limit,
                                std::uint32_t count, std::uint32_t flags) {
    for (const auto flag : {kFlagStream0x10Vec3, kFlagStream0x20Vec3}) {
        if ((flags & flag) == 0)
            continue;
        std::size_t n = 0;
        if (!CheckedMultiply(count, 12, n) || !Skip(bytes, cursor, limit, n))
            return false;
    }
    for (const auto flag :
         {kFlagStream0x100EightBytes, kFlagStream0x200EightBytes, kFlagStream0x400EightBytes,
          kFlagStream0x800EightBytes, kFlagStream0x20000EightBytes}) {
        if ((flags & flag) == 0)
            continue;
        std::size_t n = 0;
        if (!CheckedMultiply(count, 8, n) || !Skip(bytes, cursor, limit, n))
            return false;
    }
    if ((flags & kFlagStream0x10000FourBytes) != 0) {
        std::size_t n = 0;
        if (!CheckedMultiply(count, 4, n) || !Skip(bytes, cursor, limit, n))
            return false;
    }
    return true;
}

bool ReadRawNodeMetadata(std::string_view bytes, std::size_t start, NmfStaticObject& object) {
    if (!ReadU32LE(bytes, start, bytes.size(), object.objectType) ||
        !ReadU32LE(bytes, start + 4, bytes.size(), object.objectSizeField))
        return false;
    std::memcpy(object.nameBytes.data(), bytes.data() + start + 8, object.nameBytes.size());
    std::uint16_t parent = 0, children = 0;
    if (!ReadU16LE(bytes, start + 0x48, bytes.size(), parent) ||
        !ReadU16LE(bytes, start + 0x4a, bytes.size(), children))
        return false;
    const auto asSigned16 = [](std::uint16_t value) {
        const std::int32_t signedValue = (value & 0x8000u) != 0
                                             ? static_cast<std::int32_t>(value) - 0x10000
                                             : static_cast<std::int32_t>(value);
        return static_cast<std::int16_t>(signedValue);
    };
    object.parentIndex = asSigned16(parent);
    object.childCount = asSigned16(children);
    for (std::size_t i = 0; i < 16; ++i) {
        if (!ReadFloatLE(bytes, start + 0x4c + i * 4, bytes.size(), object.nodeMatrixA[i]) ||
            !std::isfinite(object.nodeMatrixA[i]) ||
            !ReadFloatLE(bytes, start + 0x8c + i * 4, bytes.size(), object.nodeMatrixB[i]) ||
            !std::isfinite(object.nodeMatrixB[i]))
            return false;
    }
    float* bounds[] = {&object.bounds.minimum.x, &object.bounds.minimum.y,
                       &object.bounds.minimum.z, &object.bounds.maximum.x,
                       &object.bounds.maximum.y, &object.bounds.maximum.z};
    for (std::size_t i = 0; i < 6; ++i)
        if (!ReadFloatLE(bytes, start + 0xcc + i * 4, bytes.size(), *bounds[i]) ||
            !std::isfinite(*bounds[i]))
            return false;
    return true;
}

} // namespace

bool ParseNmfStaticGeometry(std::string_view bytes, NmfStaticGeometry& output, std::string* error) {
    if (bytes.size() < kHeaderSize)
        return Fail(error, "NMF geometry profile requires at least 20 header bytes");
    if (bytes.size() > MaxNmfGeometryBytes)
        return Fail(error, "NMF geometry input exceeds the 256 MiB profile limit");
    NmfHeaderProfile header;
    if (!ParseNmfHeaderProfile(bytes, header, error))
        return false;
    std::uint32_t declared = 0;
    if (!ReadU32LE(bytes, 16, bytes.size(), declared) || declared != bytes.size())
        return Fail(error, "NMF geometry byte-length field does not match the provided input");
    if (header.objectRecordCount == 0 || header.objectRecordCount > MaxNmfGeometryObjects)
        return Fail(error, "NMF geometry profile contains no objects or exceeds its object limit");

    std::size_t cursor = kHeaderSize + header.submaterialIdentifiers.size() * 64u;
    NmfStaticGeometry candidate;
    candidate.submaterialIdentifiers = header.submaterialIdentifiers;
    candidate.objects.reserve(header.objectRecordCount);
    std::size_t totalVertices = 0, totalIndices = 0, totalPartitions = 0, totalPartitionIndices = 0;

    for (std::uint32_t objectIndex = 0; objectIndex < header.objectRecordCount; ++objectIndex) {
        if (!CanRead(bytes, cursor, kObjectRecordSize, bytes.size()))
            return Fail(error, "NMF object record is truncated");
        NmfStaticObject object;
        if (!ReadRawNodeMetadata(bytes, cursor, object))
            return Fail(error, "NMF node metadata is truncated or non-finite");
        object.objectIndex = objectIndex;
        if (object.objectType != 0)
            return Fail(error, "NMF geometry profile supports only static type-0 mesh objects");
        if (object.parentIndex != -1 || object.childCount != 0)
            return Fail(error, "NMF preview profile supports only root nodes without children");
        const auto isIdentity = [](const std::array<float, 16>& m) {
            for (std::size_t i = 0; i < m.size(); ++i) {
                const float expected = (i % 5 == 0) ? 1.0f : 0.0f;
                if (m[i] != expected)
                    return false;
            }
            return true;
        };
        if (!isIdentity(object.nodeMatrixA) || !isIdentity(object.nodeMatrixB))
            return Fail(error, "NMF preview profile requires identity node matrices");

        cursor += 0xe4;
        std::uint32_t lodCount = 0;
        if (!ReadU32LE(bytes, cursor, bytes.size(), lodCount))
            return Fail(error, "NMF LOD count is truncated");
        cursor += 4;
        if (lodCount != 1)
            return Fail(error, "NMF preview profile supports exactly one LOD per node");
        if (!CanRead(bytes, cursor, kLodHeaderSize, bytes.size()))
            return Fail(error, "NMF LOD header is truncated");
        auto& lod = object.lod;
        std::uint32_t vertexCount = 0, indexCount = 0;
        if (!ReadU32LE(bytes, cursor, bytes.size(), lod.lodSizeField) ||
            !ReadU32LE(bytes, cursor + 4, bytes.size(), vertexCount) ||
            !ReadU32LE(bytes, cursor + 8, bytes.size(), indexCount) ||
            !ReadU32LE(bytes, cursor + 12, bytes.size(), lod.subsetCount) ||
            !ReadU32LE(bytes, cursor + 16, bytes.size(), lod.headerField0x10) ||
            !ReadU32LE(bytes, cursor + 20, bytes.size(), lod.vertexFlags) ||
            !ReadU32LE(bytes, cursor + 24, bytes.size(), lod.headerField0x18))
            return Fail(error, "NMF LOD fields are truncated");
        // +0x10 and +0x18 are inferred as morph metadata for this build-scoped profile.
        if (lod.headerField0x10 != 0 || lod.headerField0x18 != 0)
            return Fail(error,
                        "NMF inferred morph metadata is unsupported by this preview profile");
        if ((lod.vertexFlags & ~kSupportedFlags) != 0)
            return Fail(error, "NMF LOD uses unsupported vertex-stream flags");
        if (vertexCount < 3 || vertexCount > 65536 || indexCount < 3 || indexCount % 3 != 0)
            return Fail(error, "NMF LOD is outside the bounded triangle-count profile");
        if (lod.subsetCount > kMaxPartitionRecords - totalPartitions)
            return Fail(error, "NMF subset count exceeds the bounded profile limit");
        if (vertexCount > MaxNmfGeometryVertices - totalVertices ||
            indexCount > MaxNmfGeometryIndices - totalIndices)
            return Fail(error, "NMF aggregate geometry counts exceed the bounded profile limits");
        cursor += kLodHeaderSize;

        std::size_t indexBytes = 0;
        if (!CheckedMultiply(indexCount, 2, indexBytes) ||
            !CanRead(bytes, cursor, indexBytes, bytes.size()))
            return Fail(error, "NMF LOD index stream is truncated");
        lod.indices.reserve(indexCount);
        for (std::uint32_t i = 0; i < indexCount; ++i) {
            std::uint16_t index = 0;
            if (!ReadU16LE(bytes, cursor + static_cast<std::size_t>(i) * 2, bytes.size(), index) ||
                index >= vertexCount)
                return Fail(error, "NMF LOD index is outside its vertex array");
            lod.indices.push_back(index);
        }
        cursor += indexBytes;
        std::size_t positionBytes = 0;
        if (!CheckedMultiply(vertexCount, 12, positionBytes) ||
            !CanRead(bytes, cursor, positionBytes, bytes.size()))
            return Fail(error, "NMF LOD position stream is truncated");
        lod.positions.reserve(vertexCount);
        for (std::uint32_t i = 0; i < vertexCount; ++i) {
            const auto at = cursor + static_cast<std::size_t>(i) * 12;
            NmfPosition p;
            if (!ReadFloatLE(bytes, at, bytes.size(), p.x) ||
                !ReadFloatLE(bytes, at + 4, bytes.size(), p.y) ||
                !ReadFloatLE(bytes, at + 8, bytes.size(), p.z) || !std::isfinite(p.x) ||
                !std::isfinite(p.y) || !std::isfinite(p.z) ||
                std::abs(p.x) > kMaxAbsoluteCoordinate || std::abs(p.y) > kMaxAbsoluteCoordinate ||
                std::abs(p.z) > kMaxAbsoluteCoordinate)
                return Fail(error, "NMF LOD contains a non-finite or out-of-profile position");
            lod.positions.push_back(p);
        }
        cursor += positionBytes;
        if ((lod.vertexFlags & kFlagAttribute0x8) != 0 &&
            !ReadVector3Array(bytes, cursor, bytes.size(), vertexCount, lod.vertexNormals))
            return Fail(error, "NMF flagged normal stream is truncated or non-finite");
        if (!SkipAuxiliaryVertexStreams(bytes, cursor, bytes.size(), vertexCount, lod.vertexFlags))
            return Fail(error, "NMF auxiliary vertex stream is truncated");
        if ((lod.vertexFlags & kFlagStream0x40000) != 0) {
            const std::size_t triangleCount = indexCount / 3;
            std::size_t firstStreamBytes = 0;
            std::size_t secondStreamBytes = 0;
            if (!CheckedMultiply(triangleCount, kStream0x40000FirstBytesPerTriangle,
                                 firstStreamBytes) ||
                !Skip(bytes, cursor, bytes.size(), firstStreamBytes))
                return Fail(error, "NMF first per-triangle auxiliary stream is truncated");
            if (!CheckedMultiply(triangleCount, kStream0x40000SecondBytesPerTriangle,
                                 secondStreamBytes) ||
                !Skip(bytes, cursor, bytes.size(), secondStreamBytes))
                return Fail(error, "NMF second per-triangle auxiliary stream is truncated");
        }
        totalPartitions += lod.subsetCount;
        lod.subsets.reserve(lod.subsetCount);
        for (std::uint32_t i = 0; i < lod.subsetCount; ++i) {
            if (!CanRead(bytes, cursor, kPartitionHeaderSize, bytes.size()))
                return Fail(error, "NMF subset header is truncated");
            NmfGeometrySubset subset;
            if (!ReadU32LE(bytes, cursor, bytes.size(), subset.indexOffset) ||
                !ReadU32LE(bytes, cursor + 4, bytes.size(), subset.indexCount) ||
                !ReadU16LE(bytes, cursor + 8, bytes.size(), subset.submaterialIndex) ||
                !ReadU16LE(bytes, cursor + 10, bytes.size(), subset.auxiliaryIndexCount))
                return Fail(error, "NMF subset record fields are truncated");
            if (subset.indexOffset > lod.indices.size() ||
                subset.indexCount > lod.indices.size() - subset.indexOffset)
                return Fail(error, "NMF subset draw range is outside its LOD index array");
            // Each observed subset is submitted as its own indexed triangle draw,
            // so its start offset need not align with a triangle boundary in the
            // concatenated LOD index array. Preserve the exact per-draw offset.
            if (subset.submaterialIndex >= header.submaterialIdentifiers.size())
                return Fail(error, "NMF subset references a missing submaterial-table entry");
            cursor += kPartitionHeaderSize;
            std::size_t n = 0;
            if (!CheckedMultiply(subset.auxiliaryIndexCount, 2, n) ||
                !CanRead(bytes, cursor, n, bytes.size()))
                return Fail(error, "NMF subset indices are truncated");
            subset.auxiliaryIndices.reserve(subset.auxiliaryIndexCount);
            for (std::uint16_t j = 0; j < subset.auxiliaryIndexCount; ++j) {
                std::uint16_t index = 0;
                if (!ReadU16LE(bytes, cursor + static_cast<std::size_t>(j) * 2, bytes.size(),
                               index))
                    return Fail(error, "NMF subset auxiliary index stream is truncated");
                subset.auxiliaryIndices.push_back(index);
            }
            if (subset.auxiliaryIndexCount > kMaxPartitionIndices - totalPartitionIndices)
                return Fail(error, "NMF aggregate subset indices exceed the bounded profile limit");
            totalPartitionIndices += subset.auxiliaryIndexCount;
            cursor += n;
            lod.subsets.push_back(std::move(subset));
        }
        totalVertices += vertexCount;
        totalIndices += indexCount;
        candidate.objects.push_back(std::move(object));
    }
    if (cursor != bytes.size())
        return Fail(error, "NMF geometry profile has trailing or unclaimed bytes");
    output = std::move(candidate);
    if (error != nullptr)
        error->clear();
    return true;
}

bool LoadNmfStaticGeometry(const std::filesystem::path& path, NmfStaticGeometry& output,
                           std::string* error) {
    std::error_code ec;
    const auto fileBytes = std::filesystem::file_size(path, ec);
    if (ec || fileBytes < kHeaderSize || fileBytes > MaxNmfGeometryBytes)
        return Fail(error,
                    "NMF input is missing, unreadable, or outside the 256 MiB profile limit");
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        return Fail(error, "NMF input could not be opened");
    std::string bytes(static_cast<std::size_t>(fileBytes), '\0');
    stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream || stream.peek() != std::char_traits<char>::eof())
        return Fail(error, "NMF input changed during read or could not be read completely");
    return ParseNmfStaticGeometry(bytes, output, error);
}

} // namespace openrepublic::content
