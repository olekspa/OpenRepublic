#include "openrepublic/content/nmf_geometry.h"

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
void Check(bool ok, const std::string& label) {
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << label << '\n';
    }
}
void WriteU16(std::string& b, std::size_t at, std::uint16_t v) {
    b[at] = static_cast<char>(v & 255u);
    b[at + 1] = static_cast<char>(v >> 8);
}
void WriteU32(std::string& b, std::size_t at, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
        b[at + i] = static_cast<char>((v >> (8 * i)) & 255u);
}
void WriteFloat(std::string& b, std::size_t at, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    WriteU32(b, at, bits);
}
void AppendU16(std::string& b, std::uint16_t v) {
    auto at = b.size();
    b.resize(at + 2);
    WriteU16(b, at, v);
}
void AppendU32(std::string& b, std::uint32_t v) {
    auto at = b.size();
    b.resize(at + 4);
    WriteU32(b, at, v);
}
void AppendFloat(std::string& b, float f) {
    std::uint32_t bits;
    std::memcpy(&bits, &f, 4);
    AppendU32(b, bits);
}

struct LodSpec {
    std::vector<NmfPosition> positions{{-1, 0, -1}, {1, 0, -1}, {0, 1, 1}};
    std::vector<std::uint16_t> indices{0, 1, 2};
    std::vector<NmfVec3Attribute> vec3Stream;
    std::uint32_t flags{};
    std::uint32_t field0x10{};
    std::uint32_t field0x18{};
    std::vector<std::vector<std::uint16_t>> subsets;
};
struct ObjectSpec {
    std::uint32_t type{};
    std::uint32_t sizeField{1}; // Deliberately not a byte span.
    std::string rawName{"fixture-node"};
    std::int16_t parent{-1};
    std::int16_t children{};
    std::vector<float> matrixA;
    std::vector<float> matrixB;
    std::vector<LodSpec> lods{LodSpec{}};
};

void Sentinels(std::string& b, std::size_t n, unsigned char value) {
    b.append(n, static_cast<char>(value));
}
std::vector<float> Identity() {
    std::vector<float> m(16, 0.0f);
    for (int i = 0; i < 16; i += 5)
        m[i] = 1.0f;
    return m;
}
std::string Fixture(const std::vector<ObjectSpec>& objects) {
    std::string b(20, '\0');
    WriteU32(b, 12, static_cast<std::uint32_t>(objects.size()));
    for (const auto& o : objects) {
        const auto start = b.size();
        b.resize(start + 0xe8, '\0');
        WriteU32(b, start, o.type);
        WriteU32(b, start + 4, o.sizeField);
        for (std::size_t i = 0; i < o.rawName.size() && i < 64; ++i)
            b[start + 8 + i] = o.rawName[i];
        WriteU16(b, start + 0x48, static_cast<std::uint16_t>(o.parent));
        WriteU16(b, start + 0x4a, static_cast<std::uint16_t>(o.children));
        const auto matrixA = o.matrixA.empty() ? Identity() : o.matrixA;
        const auto matrixB = o.matrixB.empty() ? Identity() : o.matrixB;
        for (std::size_t i = 0; i < 16; ++i) {
            std::uint32_t a, bValue;
            std::memcpy(&a, &matrixA[i], 4);
            std::memcpy(&bValue, &matrixB[i], 4);
            WriteU32(b, start + 0x4c + i * 4, a);
            WriteU32(b, start + 0x8c + i * 4, bValue);
        }
        // AABB min and max; float components are finite and intentionally retained.
        const float bounds[6] = {-2, -3, -4, 2, 3, 4};
        for (std::size_t i = 0; i < 6; ++i) {
            std::uint32_t bits;
            std::memcpy(&bits, &bounds[i], 4);
            WriteU32(b, start + 0xcc + i * 4, bits);
        }
        WriteU32(b, start + 0xe4, static_cast<std::uint32_t>(o.lods.size()));
        for (const auto& lod : o.lods) {
            auto h = b.size();
            b.resize(h + 0x1c, '\0');
            WriteU32(b, h, 0x12345678);
            WriteU32(b, h + 4, static_cast<std::uint32_t>(lod.positions.size()));
            WriteU32(b, h + 8, static_cast<std::uint32_t>(lod.indices.size()));
            WriteU32(b, h + 12, static_cast<std::uint32_t>(lod.subsets.size()));
            WriteU32(b, h + 16, lod.field0x10);
            WriteU32(b, h + 20, lod.flags);
            WriteU32(b, h + 24, lod.field0x18);
            for (auto i : lod.indices)
                AppendU16(b, i);
            for (const auto& p : lod.positions) {
                AppendFloat(b, p.x);
                AppendFloat(b, p.y);
                AppendFloat(b, p.z);
            }
            unsigned char tag = 0x31;
            for (auto flag : {0x8u, 0x10u, 0x20u})
                if (lod.flags & flag) {
                    if (flag == 0x8 && lod.vec3Stream.size() == lod.positions.size()) {
                        for (const auto& v : lod.vec3Stream) {
                            AppendFloat(b, v.x);
                            AppendFloat(b, v.y);
                            AppendFloat(b, v.z);
                        }
                    } else
                        Sentinels(b, lod.positions.size() * 12, tag++);
                }
            for (auto flag : {0x100u, 0x200u, 0x400u, 0x800u, 0x20000u})
                if (lod.flags & flag)
                    Sentinels(b, lod.positions.size() * 8, tag++);
            if (lod.flags & 0x10000u)
                Sentinels(b, lod.positions.size() * 4, tag++);
            if (lod.flags & 0x40000u) {
                Sentinels(b, (lod.indices.size() / 3) * 16, 0xa1);
                Sentinels(b, (lod.indices.size() / 3) * 24, 0xb2);
            }
            for (const auto& subset : lod.subsets) {
                auto sh = b.size();
                b.resize(sh + 12, '\0');
                WriteU32(b, sh + 4, static_cast<std::uint32_t>(lod.indices.size()));
                WriteU16(b, sh + 10, static_cast<std::uint16_t>(subset.size()));
                for (auto i : subset)
                    AppendU16(b, i);
            }
        }
    }
    WriteU32(b, 16, static_cast<std::uint32_t>(b.size()));
    return b;
}
void UpdateLength(std::string& b) {
    WriteU32(b, 16, static_cast<std::uint32_t>(b.size()));
}
std::size_t FirstLodData() {
    return 20 + 0xe8 + 0x1c;
}
std::size_t FirstVec3Stream(const LodSpec& l) {
    return FirstLodData() + l.indices.size() * 2 + l.positions.size() * 12;
}

void ExpectFailure(std::string b, const std::string& label) {
    NmfStaticGeometry out;
    NmfStaticObject sentinel;
    sentinel.objectIndex = 71;
    sentinel.lod.indices = {77};
    out.objects.push_back(sentinel);
    std::string error;
    Check(!ParseNmfStaticGeometry(b, out, &error) && !error.empty(),
          label + " fails with diagnostic");
    Check(out.objects.size() == 1 && out.objects[0].objectIndex == 71 &&
              out.objects[0].lod.indices == std::vector<std::uint32_t>{77},
          label + " leaves output transactional");
}

void TestMetadataAndSingleLodProfile() {
    ObjectSpec a;
    a.sizeField = 1;
    a.rawName = "first raw";
    ObjectSpec b;
    b.rawName = "second raw";
    b.lods[0].positions[0].z = -3;
    const auto bytes = Fixture({a, b});
    NmfStaticGeometry out;
    std::string error = "stale";
    Check(ParseNmfStaticGeometry(bytes, out, &error),
          "valid root nodes with tiny raw size field parse to exact EOF");
    Check(error.empty() && out.objects.size() == 2, "parser clears error and retains node order");
    if (out.objects.size() == 2) {
        Check(out.objects[0].objectIndex == 0 && out.objects[1].objectIndex == 1,
              "object indexes remain ordered");
        Check(out.objects[0].objectSizeField == 1,
              "unproven +4 field is preserved but not treated as span");
        Check(out.objects[0].nameBytes[0] == 'f' && out.objects[1].nameBytes[0] == 's',
              "name bytes stay raw and aligned");
        Check(out.objects[0].parentIndex == -1 && out.objects[0].childCount == 0,
              "root sentinel metadata is retained");
        Check(out.objects[0].nodeMatrixA[0] == 1 && out.objects[0].nodeMatrixB[15] == 1,
              "raw matrix components are retained");
        Check(out.objects[0].bounds.minimum.x == -2 && out.objects[0].bounds.maximum.z == 4,
              "AABB metadata is retained");
        Check(out.objects[0].lod.lodSizeField == 0x12345678 &&
                  out.objects[0].lod.positions.size() == 3,
              "one nested LOD retains header and geometry");
        Check(out.objects[1].lod.positions[0].z == -3,
              "following object begins after previous object's counted LOD data");
    }
    auto twoLods = Fixture({a});
    WriteU32(twoLods, 20 + 0xe4, 2);
    ExpectFailure(std::move(twoLods), "multiple LODs");
    auto nonIdentityA = a;
    nonIdentityA.matrixA = Identity();
    nonIdentityA.matrixA[12] = 5;
    ExpectFailure(Fixture({nonIdentityA}), "nonidentity matrix A outside preview profile");
    auto nonIdentityB = a;
    nonIdentityB.matrixB = Identity();
    nonIdentityB.matrixB[12] = 5;
    ExpectFailure(Fixture({nonIdentityB}), "nonidentity matrix B outside preview profile");
    auto nonRoot = a;
    nonRoot.parent = 0;
    ExpectFailure(Fixture({nonRoot}), "non-root parent outside preview profile");
    auto hasChildren = a;
    hasChildren.children = 1;
    ExpectFailure(Fixture({hasChildren}), "node with children outside preview profile");
}

void TestFlaggedVec3StreamAndAlignment() {
    ObjectSpec a;
    auto& l = a.lods[0];
    l.flags = 0x8 | 0x10 | 0x20 | 0x100;
    l.vec3Stream = {{.25f, -.5f, .75f}, {-.25f, .5f, -.75f}, {1, 0, 0}};
    ObjectSpec b;
    b.lods[0].positions[0] = {29, -37, 41};
    auto bytes = Fixture({a, b});
    NmfStaticGeometry out;
    std::string error;
    Check(ParseNmfStaticGeometry(bytes, out, &error),
          "flagged vec3 and later auxiliary streams parse");
    Check(out.objects.size() == 2 && out.objects[0].lod.attribute0x8.size() == 3,
          "flagged stream is retained vertex-aligned");
    if (out.objects.size() == 2) {
        const auto& v = out.objects[0].lod.attribute0x8;
        Check(v[0].x == .25f && v[0].y == -.5f && v[0].z == .75f && v[1].x == -.25f && v[2].x == 1,
              "vec3 values preserve source order");
        Check(out.objects[1].lod.positions[0].x == 29 && out.objects[1].lod.positions[0].y == -37 &&
                  out.objects[1].lod.positions[0].z == 41,
              "following object alignment accounts for flagged streams");
    }
    auto trunc = Fixture({a});
    trunc.resize(FirstVec3Stream(l) + l.vec3Stream.size() * 12 - 1);
    UpdateLength(trunc);
    ExpectFailure(std::move(trunc), "truncated vec3 stream");
    auto nan = Fixture({a});
    WriteU32(nan, FirstVec3Stream(l) + 4, 0x7fc00000);
    ExpectFailure(std::move(nan), "nonfinite flagged vec3 value");
    ObjectSpec noFlag;
    Check(ParseNmfStaticGeometry(Fixture({noFlag}), out, &error) &&
              out.objects[0].lod.attribute0x8.empty(),
          "unflagged vec3 data is absent");
}

void TestUnknownBitOneDoesNotAdvanceTheStream() {
    ObjectSpec first;
    first.lods[0].flags = 0x1;
    ObjectSpec following;
    following.lods[0].positions[0] = {17, 23, -31};
    const auto bytes = Fixture({first, following});
    NmfStaticGeometry out;
    std::string error;
    Check(ParseNmfStaticGeometry(bytes, out, &error),
          "build-scoped flag 0x1 without a stream branch parses");
    Check(out.objects.size() == 2 && out.objects[1].lod.positions[0].x == 17,
          "flag 0x1 consumes no bytes in this profile");
}

void TestAllAuxiliaryBranchesAndTruncation() {
    for (const auto flag : {0x10000u, 0x20000u, 0x200u, 0x400u, 0x800u, 0x40000u}) {
        ObjectSpec a;
        a.lods[0].flags = flag;
        if (flag == 0x40000u)
            a.lods[0].indices = {0, 1, 2, 2, 1, 0};
        ObjectSpec b;
        b.lods[0].positions[0] = {17, 23, -31};
        auto bytes = Fixture({a, b});
        NmfStaticGeometry out;
        std::string error;
        Check(ParseNmfStaticGeometry(bytes, out, &error), "auxiliary stream branch parses");
        Check(out.objects.size() == 2 && out.objects[1].lod.positions[0].x == 17,
              "auxiliary stream consumes expected bytes");
        auto shortBytes = Fixture({a});
        shortBytes.pop_back();
        UpdateLength(shortBytes);
        ExpectFailure(std::move(shortBytes), "truncated auxiliary stream");
    }

    ObjectSpec withSubset;
    withSubset.lods[0].subsets = {{0, 1, 2}};
    NmfStaticGeometry out;
    std::string error;
    Check(ParseNmfStaticGeometry(Fixture({withSubset}), out, &error),
          "subset header and index branch parses");
    auto badSubset = Fixture({withSubset});
    // LOD header + base indices + positions reaches the first 12-byte subset record.
    const auto subsetHeader = FirstLodData() + 3 * 2 + 3 * 12;
    WriteU16(badSubset, subsetHeader + 12 + 4, 3);
    ExpectFailure(std::move(badSubset), "subset index outside vertex array");
    auto shortSubset = Fixture({withSubset});
    shortSubset.resize(shortSubset.size() - 1);
    UpdateLength(shortSubset);
    ExpectFailure(std::move(shortSubset), "truncated subset indices");

    ObjectSpec twoTriangles;
    twoTriangles.lods[0].flags = 0x40000u;
    twoTriangles.lods[0].indices = {0, 1, 2, 2, 1, 0};
    const auto faceBytes = Fixture({twoTriangles});
    const auto faceStart = FirstLodData() + 6 * 2 + 3 * 12;
    auto short16ByteFace = faceBytes;
    short16ByteFace.resize(faceStart + 2 * 16 - 1);
    UpdateLength(short16ByteFace);
    ExpectFailure(std::move(short16ByteFace), "truncated first per-triangle auxiliary array");
    auto short24ByteFace = faceBytes;
    short24ByteFace.resize(faceStart + 2 * 16 + 2 * 24 - 1);
    UpdateLength(short24ByteFace);
    ExpectFailure(std::move(short24ByteFace), "truncated second per-triangle auxiliary array");
}

void TestMalformedInputs() {
    ExpectFailure(std::string(19, '\0'), "short header");
    auto excessiveObjects = Fixture({ObjectSpec{}});
    WriteU32(excessiveObjects, 12, 4097);
    ExpectFailure(std::move(excessiveObjects), "object count exceeds header profile limit");
    auto badLength = Fixture({ObjectSpec{}});
    WriteU32(badLength, 16, static_cast<std::uint32_t>(badLength.size() - 1));
    ExpectFailure(std::move(badLength), "file length mismatch");
    ObjectSpec badType;
    badType.type = 1;
    ExpectFailure(Fixture({badType}), "unsupported object type");
    auto multiLod = Fixture({ObjectSpec{}});
    WriteU32(multiLod, 20 + 0xe4, 0);
    ExpectFailure(std::move(multiLod), "zero LOD count");
    auto headerField0x10 = Fixture({ObjectSpec{}});
    WriteU32(headerField0x10, 20 + 0xe8 + 16, 1);
    ExpectFailure(std::move(headerField0x10), "nonzero raw LOD field +0x10");
    auto headerField0x18 = Fixture({ObjectSpec{}});
    WriteU32(headerField0x18, 20 + 0xe8 + 24, 1);
    ExpectFailure(std::move(headerField0x18), "nonzero raw LOD field +0x18");
    auto unknownFlags = Fixture({ObjectSpec{}});
    WriteU32(unknownFlags, 20 + 0xe8 + 20, 0x2);
    ExpectFailure(std::move(unknownFlags), "unknown vertex flag");
    auto badIndex = Fixture({ObjectSpec{}});
    WriteU16(badIndex, FirstLodData() + 4, 3);
    ExpectFailure(std::move(badIndex), "base index out of bounds");
    auto excessiveVertexCount = Fixture({ObjectSpec{}});
    WriteU32(excessiveVertexCount, 20 + 0xe8 + 4, 65537);
    ExpectFailure(std::move(excessiveVertexCount), "vertex count exceeds 16-bit index profile");
    auto nonTriangleIndexCount = Fixture({ObjectSpec{}});
    WriteU32(nonTriangleIndexCount, 20 + 0xe8 + 8, 4);
    ExpectFailure(std::move(nonTriangleIndexCount), "index count not divisible by three");
    auto nanPosition = Fixture({ObjectSpec{}});
    WriteU32(nanPosition, FirstLodData() + 6 + 4, 0x7fc00000);
    ExpectFailure(std::move(nanPosition), "nonfinite position");
    auto outOfProfilePosition = Fixture({ObjectSpec{}});
    WriteFloat(outOfProfilePosition, FirstLodData() + 6, 1000001.0f);
    ExpectFailure(std::move(outOfProfilePosition), "position outside the numeric profile bound");
    auto nanMatrix = Fixture({ObjectSpec{}});
    WriteU32(nanMatrix, 20 + 0x4c, 0x7fc00000);
    ExpectFailure(std::move(nanMatrix), "nonfinite matrix component");
    auto nanBounds = Fixture({ObjectSpec{}});
    WriteU32(nanBounds, 20 + 0xcc + 8, 0x7fc00000);
    ExpectFailure(std::move(nanBounds), "nonfinite AABB component");
    auto shortLodHeader = Fixture({ObjectSpec{}});
    shortLodHeader.resize(20 + 0xe8 + 0x1c - 1);
    UpdateLength(shortLodHeader);
    ExpectFailure(std::move(shortLodHeader), "truncated LOD header");
    auto trailing = Fixture({ObjectSpec{}});
    trailing.push_back('\0');
    UpdateLength(trailing);
    ExpectFailure(std::move(trailing), "trailing unclaimed byte");
    auto truncated = Fixture({ObjectSpec{}});
    truncated.resize(truncated.size() - 2);
    UpdateLength(truncated);
    ExpectFailure(std::move(truncated), "truncated counted geometry");
}
} // namespace

int main() {
    TestMetadataAndSingleLodProfile();
    TestFlaggedVec3StreamAndAlignment();
    TestUnknownBitOneDoesNotAdvanceTheStream();
    TestAllAuxiliaryBranchesAndTruncation();
    TestMalformedInputs();
    if (failures) {
        std::cerr << failures << " NMF geometry test(s) failed\n";
        return 1;
    }
    std::cout << "All NMF geometry profile tests passed\n";
    return 0;
}
