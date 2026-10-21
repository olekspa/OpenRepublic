#include "openrepublic/content/nmf_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace openrepublic::content;

namespace {

int failures = 0;

void Check(bool condition, const std::string& label) {
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << label << '\n';
}

void WriteU16LE(std::string& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<char>(value & 0xffu);
    bytes[offset + 1] = static_cast<char>((value >> 8) & 0xffu);
}

void WriteU32LE(std::string& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<char>((value >> (i * 8)) & 0xffu);
}

void AppendU16LE(std::string& bytes, std::uint16_t value) {
    const std::size_t offset = bytes.size();
    bytes.resize(offset + 2);
    WriteU16LE(bytes, offset, value);
}

void AppendU32LE(std::string& bytes, std::uint32_t value) {
    const std::size_t offset = bytes.size();
    bytes.resize(offset + 4);
    WriteU32LE(bytes, offset, value);
}

void AppendFloatLE(std::string& bytes, float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "fixture expects 32-bit floats");
    std::memcpy(&bits, &value, sizeof(bits));
    AppendU32LE(bytes, bits);
}

struct SubmeshSpec {
    std::vector<NmfPosition> positions;
    std::vector<std::uint16_t> indices;
    std::uint32_t flags = 0;
    std::uint32_t extraVertexSetCount = 0;
    std::vector<std::vector<std::uint16_t>> partitionIndices;
};

struct ObjectSpec {
    std::uint32_t type = 0;
    std::vector<SubmeshSpec> submeshes;
};

void AppendZeroes(std::string& bytes, std::size_t count) {
    bytes.append(count, '\0');
}

void AppendSentinel(std::string& bytes, std::size_t count, unsigned char sentinel) {
    bytes.append(count, static_cast<char>(sentinel));
}

std::string BuildFixture(const std::vector<ObjectSpec>& objects,
                         const std::vector<std::string>& identifiers = {}) {
    std::string bytes(20, '\0');
    WriteU32LE(bytes, 8, static_cast<std::uint32_t>(identifiers.size()));
    WriteU32LE(bytes, 12, static_cast<std::uint32_t>(objects.size()));
    for (const std::string& identifier : identifiers) {
        const std::size_t offset = bytes.size();
        bytes.resize(offset + 64, '\0');
        bytes.replace(offset, identifier.size(), identifier);
    }

    for (const ObjectSpec& object : objects) {
        const std::size_t objectStart = bytes.size();
        bytes.resize(objectStart + 0xe4, '\0');
        WriteU32LE(bytes, objectStart, object.type);
        AppendU32LE(bytes, static_cast<std::uint32_t>(object.submeshes.size()));
        for (const SubmeshSpec& submesh : object.submeshes) {
            const std::size_t header = bytes.size();
            bytes.resize(header + 0x1c, '\0');
            WriteU32LE(bytes, header + 4, static_cast<std::uint32_t>(submesh.positions.size()));
            WriteU32LE(bytes, header + 8, static_cast<std::uint32_t>(submesh.indices.size()));
            WriteU32LE(bytes, header + 12, static_cast<std::uint32_t>(submesh.partitionIndices.size()));
            WriteU32LE(bytes, header + 16, submesh.extraVertexSetCount);
            WriteU32LE(bytes, header + 20, submesh.flags);

            for (const std::uint16_t index : submesh.indices) AppendU16LE(bytes, index);
            for (const NmfPosition& position : submesh.positions) {
                AppendFloatLE(bytes, position.x);
                AppendFloatLE(bytes, position.y);
                AppendFloatLE(bytes, position.z);
            }

            const auto appendArray = [&bytes](std::size_t stride, std::size_t count,
                                               unsigned char sentinel) {
                AppendSentinel(bytes, stride * count, sentinel);
            };
            unsigned char streamSentinel = 0x31;
            for (const std::uint32_t flag : {0x8u, 0x10u, 0x20u}) {
                if ((submesh.flags & flag) != 0)
                    appendArray(12, submesh.positions.size(), streamSentinel++);
            }
            for (const std::uint32_t flag : {0x100u, 0x200u, 0x400u, 0x800u, 0x20000u}) {
                if ((submesh.flags & flag) != 0)
                    appendArray(8, submesh.positions.size(), streamSentinel++);
            }
            if ((submesh.flags & 0x10000u) != 0)
                appendArray(4, submesh.positions.size(), streamSentinel++);
            if ((submesh.flags & 0x40000u) != 0) {
                const std::size_t triangleCount = submesh.indices.size() / 3;
                appendArray(16, triangleCount, 0xa1);
                appendArray(24, triangleCount, 0xb2);
            }

            for (const std::vector<std::uint16_t>& partition : submesh.partitionIndices) {
                const std::size_t partitionHeader = bytes.size();
                bytes.resize(partitionHeader + 12, '\0');
                WriteU32LE(bytes, partitionHeader + 4,
                    static_cast<std::uint32_t>(submesh.indices.size()));
                WriteU16LE(bytes, partitionHeader + 8, 0);
                WriteU16LE(bytes, partitionHeader + 10,
                    static_cast<std::uint16_t>(partition.size()));
                for (const std::uint16_t index : partition) AppendU16LE(bytes, index);
            }
        }
        WriteU32LE(bytes, objectStart + 4,
            static_cast<std::uint32_t>(bytes.size() - objectStart));
    }
    WriteU32LE(bytes, 16, static_cast<std::uint32_t>(bytes.size()));
    return bytes;
}

SubmeshSpec BasicSubmesh() {
    SubmeshSpec submesh;
    submesh.positions = {{-1.0f, 0.0f, -1.0f}, {1.0f, 0.0f, -1.0f},
        {0.0f, 1.0f, 1.0f}};
    submesh.indices = {0, 1, 2};
    return submesh;
}

void SetDeclaredLength(std::string& bytes) {
    WriteU32LE(bytes, 16, static_cast<std::uint32_t>(bytes.size()));
}

void SetObjectLength(std::string& bytes) {
    if (bytes.size() >= 20 + 0xe4)
        WriteU32LE(bytes, 24, static_cast<std::uint32_t>(bytes.size() - 20));
}

void SetDeclaredAndObjectLengths(std::string& bytes) {
    SetDeclaredLength(bytes);
    SetObjectLength(bytes);
}

std::size_t FirstSubmeshAuxiliaryStart(const SubmeshSpec& submesh) {
    return 20 + 0xe8 + 0x1c + submesh.indices.size() * sizeof(std::uint16_t) +
        submesh.positions.size() * 12;
}

void ExpectFailure(std::string bytes, const std::string& label) {
    NmfStaticGeometry output;
    NmfStaticSubmesh sentinel;
    sentinel.objectIndex = 71;
    sentinel.submeshIndex = 29;
    sentinel.positions = {{9.0f, 8.0f, 7.0f}};
    sentinel.indices = {0};
    output.submeshes.push_back(sentinel);
    std::string error;
    Check(!ParseNmfStaticGeometry(bytes, output, &error) && !error.empty(),
        label + " rejects with a diagnostic");
    Check(output.submeshes.size() == 1 && output.submeshes[0].objectIndex == 71 &&
        output.submeshes[0].positions.size() == 1 && output.submeshes[0].indices ==
        std::vector<std::uint32_t>{0}, label + " preserves caller output on failure");
}

void TestStaticProfileParsesMultipleObjectsAndAuxiliaryStreams() {
    SubmeshSpec first = BasicSubmesh();
    first.flags = 0x40139u;
    first.partitionIndices = {{0, 1, 2}};
    SubmeshSpec second = BasicSubmesh();
    second.positions[0].z = -3.0f;
    const auto bytes = BuildFixture({{0, {first}}, {0, {second}}}, {"synthetic wall"});

    NmfStaticGeometry output;
    std::string error = "stale";
    Check(ParseNmfStaticGeometry(bytes, output, &error),
        "bounded NMF type-0 geometry with recognized auxiliary streams parses");
    Check(error.empty(), "geometry parse clears a stale diagnostic");
    Check(output.submeshes.size() == 2, "multiple object submeshes are retained");
    if (output.submeshes.size() == 2) {
        Check(output.submeshes[0].objectIndex == 0 && output.submeshes[0].submeshIndex == 0,
            "first submesh retains source record indices");
        Check(output.submeshes[1].objectIndex == 1 && output.submeshes[1].submeshIndex == 0,
            "second object retains its source record index");
        Check(output.submeshes[0].positions.size() == 3 &&
            output.submeshes[0].indices == std::vector<std::uint32_t>({0, 1, 2}),
            "positions and 16-bit triangle indices normalize to owned vectors");
        Check(output.submeshes[1].positions[0].z == -3.0f,
            "later object bytes are found after bounded auxiliary streams");
    }
}

void TestEachAcceptedAuxiliaryStreamBranch() {
    struct StreamCase {
        std::uint32_t flag;
        const char* label;
    };
    constexpr StreamCase cases[] = {
        {0x10000u, "four-byte per-vertex stream"},
        {0x20000u, "additional eight-byte per-vertex stream"},
        {0x200u, "UV1 stream"},
        {0x400u, "UV2 stream"},
        {0x800u, "UV3 stream"},
        {0x40000u, "per-triangle 16-byte and 24-byte streams"},
    };

    for (const StreamCase& streamCase : cases) {
        SubmeshSpec first = BasicSubmesh();
        first.flags = streamCase.flag;
        if (streamCase.flag == 0x40000u) {
            first.indices = {0, 1, 2, 2, 1, 0};
        }
        SubmeshSpec following = BasicSubmesh();
        following.positions[0] = {17.0f, 23.0f, -31.0f};
        const auto bytes = BuildFixture({{0, {first, following}}});

        NmfStaticGeometry output;
        std::string error;
        Check(ParseNmfStaticGeometry(bytes, output, &error),
            std::string(streamCase.label) + " parses before a following submesh");
        Check(output.submeshes.size() == 2,
            std::string(streamCase.label) + " retains both submeshes");
        if (output.submeshes.size() == 2) {
            Check(output.submeshes[1].positions.size() == 3 &&
                output.submeshes[1].positions[0].x == 17.0f &&
                output.submeshes[1].positions[0].y == 23.0f &&
                output.submeshes[1].positions[0].z == -31.0f,
                std::string(streamCase.label) + " consumes the expected sentinel-sized bytes");
        }

        auto truncated = BuildFixture({{0, {first}}});
        truncated.pop_back();
        SetDeclaredAndObjectLengths(truncated);
        ExpectFailure(std::move(truncated),
            std::string("truncated ") + streamCase.label);
    }

    SubmeshSpec twoTriangles = BasicSubmesh();
    twoTriangles.flags = 0x40000u;
    twoTriangles.indices = {0, 1, 2, 2, 1, 0};
    const auto completeFaceStreams = BuildFixture({{0, {twoTriangles}}});
    const std::size_t faceStreamStart = FirstSubmeshAuxiliaryStart(twoTriangles);

    auto truncatedSixteenByteFaceStream = completeFaceStreams;
    truncatedSixteenByteFaceStream.resize(faceStreamStart + 2 * 16 - 1);
    SetDeclaredAndObjectLengths(truncatedSixteenByteFaceStream);
    ExpectFailure(std::move(truncatedSixteenByteFaceStream),
        "truncated 16-byte per-triangle stream before the 24-byte stream");

    auto truncatedTwentyFourByteFaceStream = completeFaceStreams;
    truncatedTwentyFourByteFaceStream.resize(faceStreamStart + 2 * 16 + 2 * 24 - 1);
    SetDeclaredAndObjectLengths(truncatedTwentyFourByteFaceStream);
    ExpectFailure(std::move(truncatedTwentyFourByteFaceStream),
        "truncated 24-byte per-triangle stream after complete 16-byte stream");
}

void TestUnsupportedAndMalformedRecordsFailTransactionally() {
    ExpectFailure(std::string(19, '\0'), "short header");

    auto declaredMismatch = BuildFixture({{0, {BasicSubmesh()}}});
    WriteU32LE(declaredMismatch, 16, static_cast<std::uint32_t>(declaredMismatch.size() - 1));
    ExpectFailure(std::move(declaredMismatch), "declared file length mismatch");

    auto tooLarge = BuildFixture({{0, {BasicSubmesh()}}});
    WriteU32LE(tooLarge, 16, static_cast<std::uint32_t>(MaxNmfGeometryBytes + 1));
    ExpectFailure(std::move(tooLarge), "declared file length mismatch");

    auto unsupportedType = BuildFixture({{1, {BasicSubmesh()}}});
    ExpectFailure(std::move(unsupportedType), "non-type-0 object");

    auto badObjectSpan = BuildFixture({{0, {BasicSubmesh()}}});
    WriteU32LE(badObjectSpan, 24, static_cast<std::uint32_t>(badObjectSpan.size()));
    ExpectFailure(std::move(badObjectSpan), "object span extends beyond its input");

    auto badSubmeshCount = BuildFixture({{0, {BasicSubmesh()}}});
    WriteU32LE(badSubmeshCount, 20 + 0xe4, 4097);
    ExpectFailure(std::move(badSubmeshCount), "excessive submesh count");

    auto unknownFlags = BuildFixture({{0, {BasicSubmesh()}}});
    WriteU32LE(unknownFlags, 20 + 0xe8 + 20, 0x2);
    ExpectFailure(std::move(unknownFlags), "unknown vertex-stream flag");

    auto additionalVertexSet = BuildFixture({{0, {BasicSubmesh()}}});
    WriteU32LE(additionalVertexSet, 20 + 0xe8 + 16, 1);
    ExpectFailure(std::move(additionalVertexSet), "unsupported extra vertex set");

    auto excessiveVertices = BuildFixture({{0, {BasicSubmesh()}}});
    WriteU32LE(excessiveVertices, 20 + 0xe8 + 4, 65537);
    ExpectFailure(std::move(excessiveVertices), "vertex count above 16-bit index profile");

    auto nonTriangleIndexCount = BuildFixture({{0, {BasicSubmesh()}}});
    WriteU32LE(nonTriangleIndexCount, 20 + 0xe8 + 8, 4);
    ExpectFailure(std::move(nonTriangleIndexCount), "index count not divisible by three");

    auto outOfRangeIndex = BuildFixture({{0, {BasicSubmesh()}}});
    WriteU16LE(outOfRangeIndex, 20 + 0xe8 + 0x1c + 4, 3);
    ExpectFailure(std::move(outOfRangeIndex), "base index beyond its vertex array");

    auto outOfRangePartitionIndex = BuildFixture({{0, {[] {
        auto submesh = BasicSubmesh();
        submesh.partitionIndices = {{0, 1, 3}};
        return submesh;
    }()}}});
    ExpectFailure(std::move(outOfRangePartitionIndex), "partition index beyond its vertex array");

    auto nonFinitePosition = BuildFixture({{0, {BasicSubmesh()}}});
    const std::size_t firstPosition = 20 + 0xe8 + 0x1c + 3 * 2;
    WriteU32LE(nonFinitePosition, firstPosition + 4, 0x7fc00000u);
    ExpectFailure(std::move(nonFinitePosition), "non-finite position");

    auto truncatedGeometry = BuildFixture({{0, {BasicSubmesh()}}});
    truncatedGeometry.pop_back();
    SetDeclaredAndObjectLengths(truncatedGeometry);
    ExpectFailure(std::move(truncatedGeometry), "truncated position stream");

    auto trailingBytes = BuildFixture({{0, {BasicSubmesh()}}});
    trailingBytes.push_back('\0');
    SetDeclaredLength(trailingBytes);
    ExpectFailure(std::move(trailingBytes), "unclaimed trailing bytes");
}

} // namespace

int main() {
    TestStaticProfileParsesMultipleObjectsAndAuxiliaryStreams();
    TestEachAcceptedAuxiliaryStreamBranch();
    TestUnsupportedAndMalformedRecordsFailTransactionally();
    if (failures != 0) {
        std::cerr << failures << " NMF geometry test(s) failed\n";
        return 1;
    }
    std::cout << "All NMF geometry profile tests passed\n";
    return 0;
}
