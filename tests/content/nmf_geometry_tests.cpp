#include "openrepublic/content/nmf_geometry.h"
#include "openrepublic/content/nmf_preview_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
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
    std::vector<NmfNormal> vec3Stream;
    std::vector<NmfTextureCoordinate> textureCoordinates0;
    std::vector<NmfTextureCoordinate> textureCoordinates1;
    std::uint32_t flags{};
    std::uint32_t field0x10{};
    std::uint32_t field0x18{};
    struct Subset {
        std::uint32_t indexOffset{};
        std::uint32_t indexCount{3};
        std::uint16_t submaterialIndex{};
        std::vector<std::uint16_t> auxiliaryIndices;
    };
    std::vector<Subset> subsets;
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
    std::uint32_t submaterialCount = 0;
    for (const auto& object : objects)
        for (const auto& lod : object.lods)
            for (const auto& subset : lod.subsets)
                submaterialCount = std::max(submaterialCount,
                    static_cast<std::uint32_t>(subset.submaterialIndex) + 1);
    WriteU32(b, 8, submaterialCount);
    WriteU32(b, 12, static_cast<std::uint32_t>(objects.size()));
    for (std::uint32_t i = 0; i < submaterialCount; ++i) {
        const std::string identifier = "fixture-submaterial-" + std::to_string(i);
        const auto record = b.size();
        b.resize(record + 64, '\0');
        std::memcpy(b.data() + record, identifier.data(), identifier.size());
    }
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
            for (const auto flag : {0x100u, 0x200u}) {
                if ((lod.flags & flag) == 0)
                    continue;
                const auto& coordinates = flag == 0x100u
                    ? lod.textureCoordinates0
                    : lod.textureCoordinates1;
                if (coordinates.size() == lod.positions.size()) {
                    for (const auto& uv : coordinates) {
                        AppendFloat(b, uv.u);
                        AppendFloat(b, uv.v);
                    }
                } else {
                    Sentinels(b, lod.positions.size() * 8, tag++);
                }
            }
            for (auto flag : {0x400u, 0x800u, 0x20000u})
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
                WriteU32(b, sh, subset.indexOffset);
                WriteU32(b, sh + 4, subset.indexCount);
                WriteU16(b, sh + 8, subset.submaterialIndex);
                WriteU16(b, sh + 10, static_cast<std::uint16_t>(subset.auxiliaryIndices.size()));
                for (auto i : subset.auxiliaryIndices)
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
std::size_t FirstLodData(bool hasSubmaterials = false) {
    return 20 + (hasSubmaterials ? 64 : 0) + 0xe8 + 0x1c;
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
    out.submaterialIdentifiers = {"sentinel-submaterial"};
    std::string error;
    Check(!ParseNmfStaticGeometry(b, out, &error) && !error.empty(),
          label + " fails with diagnostic");
    Check(out.objects.size() == 1 && out.objects[0].objectIndex == 71 &&
              out.objects[0].lod.indices == std::vector<std::uint32_t>{77} &&
              out.submaterialIdentifiers == std::vector<std::string>{"sentinel-submaterial"},
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

void TestFlaggedNormalStreamAndAlignment() {
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
          "flagged normal and later auxiliary streams parse");
    Check(out.objects.size() == 2 && out.objects[0].lod.vertexNormals.size() == 3,
          "build-scoped flag 0x8 normals are retained vertex-aligned");
    if (out.objects.size() == 2) {
        const auto& v = out.objects[0].lod.vertexNormals;
        Check(v[0].x == .25f && v[0].y == -.5f && v[0].z == .75f && v[1].x == -.25f && v[2].x == 1,
              "raw normal values preserve source order");
        Check(out.objects[1].lod.positions[0].x == 29 && out.objects[1].lod.positions[0].y == -37 &&
                  out.objects[1].lod.positions[0].z == 41,
              "following object alignment accounts for the normal stream");
    }
    auto trunc = Fixture({a});
    trunc.resize(FirstVec3Stream(l) + l.vec3Stream.size() * 12 - 1);
    UpdateLength(trunc);
    ExpectFailure(std::move(trunc), "truncated normal stream");
    auto nan = Fixture({a});
    WriteU32(nan, FirstVec3Stream(l) + 4, 0x7fc00000);
    ExpectFailure(std::move(nan), "nonfinite flagged normal value");
    ObjectSpec noFlag;
    Check(ParseNmfStaticGeometry(Fixture({noFlag}), out, &error) &&
              out.objects[0].lod.vertexNormals.empty(),
          "unflagged source normals are absent");
}

void TestTextureCoordinateStreamsRetainedAndAligned() {
    ObjectSpec first;
    auto& lod = first.lods[0];
    lod.flags = 0x100u | 0x200u | 0x400u;
    lod.textureCoordinates0 = {{-1.25f, 0.25f}, {0.5f, 1.5f}, {2.0f, -0.75f}};
    lod.textureCoordinates1 = {{0.125f, 0.875f}, {1.25f, -0.5f}, {-2.0f, 3.0f}};
    ObjectSpec following;
    following.lods[0].positions[0] = {29, -37, 41};

    NmfStaticGeometry out;
    std::string error;
    Check(ParseNmfStaticGeometry(Fixture({first, following}), out, &error),
          "build-scoped _TC0/_TC1 streams and following object parse");
    Check(out.objects.size() == 2 && out.objects[0].lod.textureCoordinates0.size() == 3 &&
              out.objects[0].lod.textureCoordinates1.size() == 3,
          "both texture-coordinate streams remain vertex-aligned");
    if (out.objects.size() == 2) {
        const auto& uv0 = out.objects[0].lod.textureCoordinates0;
        const auto& uv1 = out.objects[0].lod.textureCoordinates1;
        Check(uv0[0].u == -1.25f && uv0[0].v == 0.25f && uv0[2].u == 2.0f &&
                  uv0[2].v == -0.75f,
              "_TC0 values are preserved without clamping to a unit square");
        Check(uv1[0].u == 0.125f && uv1[1].v == -0.5f && uv1[2].u == -2.0f &&
                  uv1[2].v == 3.0f,
              "_TC1 values preserve stream and vertex order");
        Check(out.objects[1].lod.positions[0].x == 29 &&
                  out.objects[1].lod.positions[0].y == -37 &&
                  out.objects[1].lod.positions[0].z == 41,
              "following object cursor includes both retained coordinate streams and skipped _TC2");
    }

    auto onlyUv0 = first;
    onlyUv0.lods[0].flags = 0x100u;
    onlyUv0.lods[0].textureCoordinates1.clear();
    auto shortUv0 = Fixture({onlyUv0});
    shortUv0.resize(FirstVec3Stream(onlyUv0.lods[0]) + 3 * 8 - 1);
    UpdateLength(shortUv0);
    ExpectFailure(std::move(shortUv0), "truncated _TC0 stream");

    auto onlyUv1 = first;
    onlyUv1.lods[0].flags = 0x200u;
    onlyUv1.lods[0].textureCoordinates0.clear();
    auto shortUv1 = Fixture({onlyUv1});
    shortUv1.resize(FirstVec3Stream(onlyUv1.lods[0]) + 3 * 8 - 1);
    UpdateLength(shortUv1);
    ExpectFailure(std::move(shortUv1), "truncated _TC1 stream");

    auto nonFiniteUv = Fixture({onlyUv0});
    WriteU32(nonFiniteUv, FirstVec3Stream(onlyUv0.lods[0]) + 4, 0x7fc00000);
    ExpectFailure(std::move(nonFiniteUv), "non-finite _TC0 component");
}

void TestCombinedTextureCoordinateStreamsStayVertexAligned() {
    ObjectSpec uv0;
    uv0.lods[0].flags = 0x100u;
    uv0.lods[0].textureCoordinates0 = {{1, 2}, {3, 4}, {5, 6}};
    ObjectSpec uv1;
    uv1.lods[0].flags = 0x200u;
    uv1.lods[0].textureCoordinates1 = {{7, 8}, {9, 10}, {11, 12}};
    ObjectSpec both;
    both.lods[0].flags = 0x100u | 0x200u;
    both.lods[0].textureCoordinates0 = {{13, 14}, {15, 16}, {17, 18}};
    both.lods[0].textureCoordinates1 = {{19, 20}, {21, 22}, {23, 24}};
    ObjectSpec plain;
    plain.lods[0].positions[0].x = 31;

    const auto parse = [](std::vector<ObjectSpec> objects, NmfStaticGeometry& geometry) {
        std::string error;
        const bool parsed = ParseNmfStaticGeometry(Fixture(objects), geometry, &error);
        Check(parsed, "mixed-stream aggregation fixture parses: " + error);
        return parsed;
    };

    NmfStaticGeometry geometry;
    NmfTextureCoordinateStreams streams;
    if (parse({uv0, plain}, geometry)) {
        Check(BuildNmfTextureCoordinateStreams(geometry, streams),
              "_TC0 aggregation succeeds when a later object omits the set");
        Check(streams.hasTextureCoordinates0 && !streams.hasTextureCoordinates1 &&
                  streams.textureCoordinates0.size() == 6 && streams.textureCoordinates1.empty(),
              "_TC0-only aggregate has the expected presence and aligned length");
        if (streams.textureCoordinates0.size() == 6) {
            Check(streams.textureCoordinates0[0].u == 1 && streams.textureCoordinates0[2].v == 6 &&
                      streams.textureCoordinates0[3].u == 0 && streams.textureCoordinates0[5].v == 0,
                  "_TC0 values retain object/vertex order and missing vertices are zero-filled");
        }
    }

    geometry = {};
    if (parse({plain, uv0}, geometry)) {
        Check(BuildNmfTextureCoordinateStreams(geometry, streams),
              "_TC0 aggregation succeeds when an earlier object omits the set");
        if (streams.textureCoordinates0.size() == 6) {
            Check(streams.textureCoordinates0[0].u == 0 && streams.textureCoordinates0[2].v == 0 &&
                      streams.textureCoordinates0[3].u == 1 && streams.textureCoordinates0[5].v == 6,
                  "_TC0 aggregation zero-fills and preserves values when object order is reversed");
        }
    }

    geometry = {};
    if (parse({uv1, plain}, geometry)) {
        Check(BuildNmfTextureCoordinateStreams(geometry, streams),
              "_TC1 aggregation succeeds when a later object omits the set");
        Check(!streams.hasTextureCoordinates0 && streams.hasTextureCoordinates1 &&
                  streams.textureCoordinates0.empty() && streams.textureCoordinates1.size() == 6,
              "_TC1-only aggregate has the expected presence and aligned length");
        if (streams.textureCoordinates1.size() == 6) {
            Check(streams.textureCoordinates1[0].u == 7 && streams.textureCoordinates1[2].v == 12 &&
                      streams.textureCoordinates1[3].u == 0 && streams.textureCoordinates1[5].v == 0,
                  "_TC1 values retain order and missing vertices are zero-filled");
        }
    }

    geometry = {};
    if (parse({plain, uv1}, geometry)) {
        Check(BuildNmfTextureCoordinateStreams(geometry, streams),
              "_TC1 aggregation succeeds when an earlier object omits the set");
        if (streams.textureCoordinates1.size() == 6) {
            Check(streams.textureCoordinates1[0].u == 0 && streams.textureCoordinates1[2].v == 0 &&
                      streams.textureCoordinates1[3].u == 7 && streams.textureCoordinates1[5].v == 12,
                  "_TC1 aggregation zero-fills and preserves values when object order is reversed");
        }
    }

    geometry = {};
    if (parse({both, plain}, geometry)) {
        Check(BuildNmfTextureCoordinateStreams(geometry, streams),
              "dual-set aggregation succeeds when a later object omits both sets");
        Check(streams.hasTextureCoordinates0 && streams.hasTextureCoordinates1 &&
                  streams.textureCoordinates0.size() == 6 && streams.textureCoordinates1.size() == 6,
              "dual-set aggregates both remain aligned to the same six vertices");
        if (streams.textureCoordinates0.size() == 6 && streams.textureCoordinates1.size() == 6) {
            Check(streams.textureCoordinates0[0].u == 13 && streams.textureCoordinates0[2].v == 18 &&
                      streams.textureCoordinates1[0].u == 19 && streams.textureCoordinates1[2].v == 24 &&
                      streams.textureCoordinates0[3].u == 0 && streams.textureCoordinates1[5].v == 0,
                  "dual-set aggregation preserves both streams and zero-fills both missing tails");
        }
    }

    geometry = {};
    if (parse({plain, both}, geometry)) {
        Check(BuildNmfTextureCoordinateStreams(geometry, streams),
              "dual-set aggregation succeeds when an earlier object omits both sets");
        if (streams.textureCoordinates0.size() == 6 && streams.textureCoordinates1.size() == 6) {
            Check(streams.textureCoordinates0[0].u == 0 && streams.textureCoordinates1[2].v == 0 &&
                      streams.textureCoordinates0[3].u == 13 && streams.textureCoordinates0[5].v == 18 &&
                      streams.textureCoordinates1[3].u == 19 && streams.textureCoordinates1[5].v == 24,
                  "dual-set aggregation zero-fills both prefixes when object order is reversed");
        }
    }

    geometry = {};
    if (!parse({uv0, plain}, geometry))
        return;
    streams.hasTextureCoordinates0 = false;
    streams.hasTextureCoordinates1 = true;
    streams.textureCoordinates0 = {{99, 98}};
    streams.textureCoordinates1 = {{97, 96}};
    geometry.objects[0].lod.textureCoordinates0.pop_back();
    const auto previous = streams;
    std::string error;
    Check(!BuildNmfTextureCoordinateStreams(geometry, streams, &error),
          "misaligned per-object stream is rejected");
    Check(!error.empty() && streams.hasTextureCoordinates0 == previous.hasTextureCoordinates0 &&
              streams.hasTextureCoordinates1 == previous.hasTextureCoordinates1 &&
              streams.textureCoordinates0.size() == previous.textureCoordinates0.size() &&
              streams.textureCoordinates0[0].u == previous.textureCoordinates0[0].u &&
              streams.textureCoordinates1[0].v == previous.textureCoordinates1[0].v,
          "failed texture-coordinate aggregation leaves caller output unchanged");

    geometry = {};
    if (!parse({uv0}, geometry))
        return;
    geometry.objects[0].lod.textureCoordinates0[1].v = std::numeric_limits<float>::quiet_NaN();
    const auto previousForNonFinite = streams;
    error.clear();
    Check(!BuildNmfTextureCoordinateStreams(geometry, streams, &error),
          "non-finite constructed texture-coordinate data is rejected");
    Check(!error.empty() && streams.hasTextureCoordinates0 == previousForNonFinite.hasTextureCoordinates0 &&
              streams.hasTextureCoordinates1 == previousForNonFinite.hasTextureCoordinates1 &&
              streams.textureCoordinates0.size() == previousForNonFinite.textureCoordinates0.size() &&
              streams.textureCoordinates0[0].u == previousForNonFinite.textureCoordinates0[0].u,
          "non-finite aggregation failure leaves caller output unchanged");
}

void TestBuildScopedNonStreamBitsDoNotAdvanceTheStream() {
    for (const auto flag : {0x1u, 0x2u}) {
        ObjectSpec first;
        first.lods[0].flags = flag;
        ObjectSpec following;
        following.lods[0].positions[0] = {17, 23, -31};
        const auto bytes = Fixture({first, following});
        NmfStaticGeometry out;
        std::string error;
        Check(ParseNmfStaticGeometry(bytes, out, &error),
              "build-scoped non-stream flag parses without a payload");
        Check(out.objects.size() == 2 && out.objects[0].lod.vertexFlags == flag &&
                  out.objects[1].lod.positions[0].x == 17,
              "raw non-stream flag is preserved without advancing the next object cursor");
    }
}

void TestAllAuxiliaryBranchesAndTruncation() {
    for (const auto flag : {0x10000u, 0x20000u, 0x400u, 0x800u, 0x40000u}) {
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
    withSubset.lods[0].subsets = {{0, 3, 0, {0, 1, 2}}};
    NmfStaticGeometry out;
    std::string error;
    Check(ParseNmfStaticGeometry(Fixture({withSubset}), out, &error),
          "subset header and index branch parses");
    Check(out.submaterialIdentifiers == std::vector<std::string>{"fixture-submaterial-0"},
          "submaterial identifiers are retained");
    Check(out.objects.size() == 1 && out.objects[0].lod.subsets.size() == 1 &&
              out.objects[0].lod.subsets[0].indexOffset == 0 &&
              out.objects[0].lod.subsets[0].indexCount == 3 &&
              out.objects[0].lod.subsets[0].submaterialIndex == 0 &&
              out.objects[0].lod.subsets[0].auxiliaryIndices == std::vector<std::uint16_t>{0, 1, 2},
          "subset draw range, table index, and opaque auxiliary indices are retained");
    ObjectSpec twoSubmaterials;
    twoSubmaterials.lods[0].positions = {{-1, 0, -1}, {1, 0, -1}, {1, 1, 1}, {-1, 1, 1}};
    twoSubmaterials.lods[0].indices = {0, 1, 2, 0, 2, 3};
    twoSubmaterials.lods[0].subsets = {{0, 3, 0, {}}, {3, 3, 1, {}}};
    Check(ParseNmfStaticGeometry(Fixture({twoSubmaterials}), out, &error) &&
              out.submaterialIdentifiers ==
                  std::vector<std::string>{"fixture-submaterial-0", "fixture-submaterial-1"} &&
              out.objects[0].lod.subsets.size() == 2 &&
              out.objects[0].lod.subsets[1].indexOffset == 3 &&
              out.objects[0].lod.subsets[1].indexCount == 3 &&
              out.objects[0].lod.subsets[1].submaterialIndex == 1,
          "multiple submaterial index ranges retain their table mapping");
    ObjectSpec unalignedSubsetStart;
    unalignedSubsetStart.lods[0].indices = {0, 1, 2, 2, 1, 0};
    unalignedSubsetStart.lods[0].subsets = {{1, 3, 0, {}}};
    Check(ParseNmfStaticGeometry(Fixture({unalignedSubsetStart}), out, &error) &&
              out.objects[0].lod.subsets.size() == 1 &&
              out.objects[0].lod.subsets[0].indexOffset == 1,
          "subset start index is preserved without assuming top-level triangle alignment");
    auto opaqueAuxiliary = withSubset;
    opaqueAuxiliary.lods[0].subsets[0].auxiliaryIndices = {65535};
    Check(ParseNmfStaticGeometry(Fixture({opaqueAuxiliary}), out, &error) &&
              out.objects[0].lod.subsets[0].auxiliaryIndices ==
                  std::vector<std::uint16_t>{65535},
          "opaque auxiliary subset values are retained without assuming vertex-index semantics");
    // LOD header + base indices + positions reaches the first 12-byte subset record.
    const auto subsetHeader = FirstLodData(true) + 3 * 2 + 3 * 12;
    auto badSubsetRange = Fixture({withSubset});
    WriteU32(badSubsetRange, subsetHeader, 1);
    ExpectFailure(std::move(badSubsetRange), "subset draw range outside index array");
    auto partialTriangleRange = withSubset;
    partialTriangleRange.lods[0].indices = {0, 1, 2, 2, 1, 0};
    partialTriangleRange.lods[0].subsets = {{1, 4, 0, {}}};
    Check(ParseNmfStaticGeometry(Fixture({partialTriangleRange}), out, &error) &&
              out.objects[0].lod.subsets[0].indexOffset == 1 &&
              out.objects[0].lod.subsets[0].indexCount == 4,
          "unaligned subset range and trailing native-ignored index are preserved exactly");
    auto missingSubmaterial = Fixture({withSubset});
    WriteU16(missingSubmaterial, subsetHeader + 8, 1);
    ExpectFailure(std::move(missingSubmaterial), "subset references missing submaterial");
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

void TestNmfPreviewGeometrySourceUnitsAndMirror() {
    NmfStaticGeometry geometry;
    geometry.submaterialIdentifiers = {"m"};
    NmfStaticObject object;
    object.lod.positions = {{1, 2, 3}, {4, 2, 3}, {1, 6, 5}};
    object.lod.vertexNormals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
    object.lod.indices = {0, 1, 2};
    object.lod.subsets = {{0, 3, 0, 0, {}}};
    geometry.objects.push_back(object);

    NmfPreviewGeometry plain;
    std::string error;
    Check(BuildNmfPreviewGeometry(geometry, plain, &error, true), "source-units geometry builds: " + error);
    Check(plain.displayScale == 1.0 && plain.positions.size() == 3 && plain.positions[0].x == 1 &&
              plain.positions[0].y == 2 && plain.positions[0].z == 3 && plain.positions[2].z == 5,
          "source units keep positions unscaled, uncentred and unshifted");
    Check(plain.indices == std::vector<std::uint32_t>{0, 2, 1}, "the usual winding swap still applies");

    NmfPreviewGeometry mirrored;
    Check(BuildNmfPreviewGeometry(geometry, mirrored, &error, true, true),
          "mirrored geometry builds: " + error);
    Check(mirrored.positions[0].z == -3 && mirrored.positions[2].z == -5 && mirrored.positions[0].x == 1 &&
              mirrored.sourceNormals[0].z == -1.0f && mirrored.sourceMaximum.z == -3.0f,
          "mirroring negates Z for positions, normals and the recorded bounds");
    Check(mirrored.indices == std::vector<std::uint32_t>{0, 1, 2},
          "mirroring keeps the source triangle order so faces stay outward");
}

void TestNmfPreviewGeometryNormalizationAndSourceOrder() {
    NmfStaticGeometry geometry;
    geometry.submaterialIdentifiers = {"preview-material"};
    NmfStaticObject object;
    object.lod.positions = {{-2, 2, 4}, {2, 2, 4}, {-2, 6, 4}};
    object.lod.vertexNormals = {{0.25f, -0.5f, 0.75f}, {0, 1, 0}, {-1, 0, 0}};
    object.lod.indices = {0, 2, 1};
    object.lod.subsets = {{0, 3, 0, 0, {}}};
    geometry.objects.push_back(object);

    NmfPreviewGeometry preview;
    std::string error;
    Check(BuildNmfPreviewGeometry(geometry, preview, &error),
          "bounded NMF preview geometry is built: " + error);
    if (preview.positions.size() != 3)
        return;

    Check(preview.positions[0].x == 3 && preview.positions[0].y == 0 && preview.positions[0].z == 0 &&
              preview.positions[1].x == 13 && preview.positions[1].y == 0 && preview.positions[1].z == 0 &&
              preview.positions[2].x == 3 && preview.positions[2].y == 10 && preview.positions[2].z == 0,
          "positive uniform scale preserves axes while centering X/Z and moving minimum Y to zero");
    Check(preview.displayScale == 2.5 && preview.cameraTarget.x == 8 &&
              preview.cameraTarget.y == 5 && preview.cameraTarget.z == 0 &&
              preview.cameraDistance == 16.5f,
          "preview scale and framing metadata are derived from vertex extents");
    Check(preview.indices == std::vector<std::uint32_t>{0, 1, 2},
          "flattening reverses triangle winding by exchanging the second and third index");
    Check(preview.drawRanges.size() == 1 && preview.drawRanges[0].indexOffset == 0 &&
              preview.drawRanges[0].indexCount == 3 && preview.drawRanges[0].submaterialIndex == 0,
          "subset draw range and submaterial index are preserved");
    Check(preview.sourceNormals.size() == 3 && preview.sourceNormalMask ==
              std::vector<std::uint8_t>{1, 1, 1} && preview.sourceNormals[0].x == 0.25f &&
              preview.sourceNormals[0].y == -0.5f && preview.sourceNormals[0].z == 0.75f,
          "source normals are retained without normalization or reflection");
    Check(preview.sourceMinimum.x == -2 && preview.sourceMinimum.y == 2 &&
              preview.sourceMinimum.z == 4 && preview.sourceMaximum.x == 2 &&
              preview.sourceMaximum.y == 6 && preview.sourceMaximum.z == 4,
          "source-position extents are recorded independently of stored object bounds");
}

void TestNmfPreviewGeometryRebasesObjectsAndPreservesSubsetOffsets() {
    NmfStaticGeometry geometry;
    geometry.submaterialIdentifiers = {"material-a", "material-b"};
    NmfStaticObject first;
    first.lod.positions = {{-1, 0, 0}, {1, 0, 0}, {-1, 1, 0}};
    first.lod.vertexNormals = {{0.1f, 0.2f, 0.3f}, {0.4f, 0.5f, 0.6f}, {0.7f, 0.8f, 0.9f}};
    first.lod.indices = {2, 1, 0, 0, 1, 2, 1, 0, 2};
    first.lod.subsets = {{0, 4, 0, 0, {}}, {4, 5, 1, 0, {}}};
    NmfStaticObject second;
    second.lod.positions = {{10, 0, 0}, {10, 2, 0}, {11, 0, 0}};
    second.lod.indices = {2, 0, 1};
    geometry.objects = {first, second};

    NmfPreviewGeometry preview;
    std::string error;
    Check(BuildNmfPreviewGeometry(geometry, preview, &error),
          "multi-object preview geometry is built: " + error);
    Check(preview.indices == std::vector<std::uint32_t>{2, 0, 1, 0, 2, 1, 1, 2, 0, 5, 4, 3},
          "second object's indices are rebased while both objects retain triangle order and reversed winding");
    Check(preview.drawRanges.size() == 3 &&
              preview.drawRanges[0].indexOffset == 0 && preview.drawRanges[0].indexCount == 3 &&
              preview.drawRanges[0].submaterialIndex == 0 &&
              preview.drawRanges[1].indexOffset == 4 && preview.drawRanges[1].indexCount == 3 &&
              preview.drawRanges[1].submaterialIndex == 1 &&
              preview.drawRanges[2].indexOffset == 9 && preview.drawRanges[2].indexCount == 3 &&
              preview.drawRanges[2].submaterialIndex == NoNmfPreviewSubmaterialIndex,
          "subset offsets stay relative to the flattened index stream and tails are omitted per draw");
    Check(preview.ignoredSubsetIndexCount == 3 && preview.drawRangeTriangleCount == 3,
          "trailing per-subset indices and draw-range triangles are counted explicitly");
    Check(preview.sourceNormals.size() == preview.positions.size() &&
              preview.sourceNormalMask.size() == preview.positions.size() &&
              preview.sourceNormalMask == std::vector<std::uint8_t>{1, 1, 1, 0, 0, 0} &&
              preview.sourceNormals[0].x == 0.1f && preview.sourceNormals[2].z == 0.9f &&
              preview.sourceNormals[3].x == 0 && preview.sourceNormals[5].z == 0,
          "mixed source-normal presence remains aligned across object rebasing for renderer fallback");
}

void TestNmfPreviewGeometryStructuralDiagnosticsAndTransactionalFailures() {
    NmfStaticGeometry degenerate;
    NmfStaticObject line;
    line.lod.positions = {{-1, 0, 0}, {0, 0, 0}, {1, 0, 0}};
    line.lod.vertexNormals = {{1, 0, 0}, {2, 0, 0}, {3, 0, 0}};
    line.lod.indices = {0, 1, 2};
    degenerate.objects.push_back(line);
    NmfPreviewGeometry preview;
    std::string error;
    Check(BuildNmfPreviewGeometry(degenerate, preview, &error),
          "diagnostic preview geometry is built: " + error);
    Check(preview.zeroAreaTriangleCount == 1 && preview.drawRangeTriangleCount == 1,
          "exact zero-area draw-range triangles are reported without changing the draw list");
    Check(preview.sourceNormalCount == 3 && preview.sourceNormalLengthMinimum == 1 &&
              preview.sourceNormalLengthMean == 2 && preview.sourceNormalLengthMaximum == 3,
          "source-normal magnitude min/mean/max are reported without imposing a unit-length rule");

    NmfPreviewGeometry unchanged;
    unchanged.positions = {{42, 43, 44}};
    unchanged.indices = {7, 8, 9};
    unchanged.sourceNormalCount = 123;
    NmfStaticGeometry badIndex = degenerate;
    badIndex.objects[0].lod.indices[2] = 3;
    error.clear();
    Check(!BuildNmfPreviewGeometry(badIndex, unchanged, &error) && !error.empty(),
          "invalid flattened index is rejected");
    Check(unchanged.positions.size() == 1 && unchanged.positions[0].x == 42 &&
              unchanged.indices == std::vector<std::uint32_t>{7, 8, 9} &&
              unchanged.sourceNormalCount == 123,
          "failed preview conversion leaves caller output unchanged");

    NmfStaticGeometry badRange = degenerate;
    badRange.submaterialIdentifiers = {"material"};
    badRange.objects[0].lod.subsets = {{1, 3, 0, 0, {}}};
    error.clear();
    Check(!BuildNmfPreviewGeometry(badRange, unchanged, &error) && !error.empty(),
          "subset range outside the source index stream is rejected");

    NmfStaticGeometry nonFinite = degenerate;
    nonFinite.objects[0].lod.positions[1].y = std::numeric_limits<float>::quiet_NaN();
    error.clear();
    Check(!BuildNmfPreviewGeometry(nonFinite, unchanged, &error) && !error.empty(),
          "non-finite constructed geometry is rejected before rendering");
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
    WriteU32(unknownFlags, 20 + 0xe8 + 20, 0x4);
    ExpectFailure(std::move(unknownFlags), "unsupported vertex flag");
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
    TestFlaggedNormalStreamAndAlignment();
    TestTextureCoordinateStreamsRetainedAndAligned();
    TestCombinedTextureCoordinateStreamsStayVertexAligned();
    TestBuildScopedNonStreamBitsDoNotAdvanceTheStream();
    TestAllAuxiliaryBranchesAndTruncation();
    TestNmfPreviewGeometrySourceUnitsAndMirror();
    TestNmfPreviewGeometryNormalizationAndSourceOrder();
    TestNmfPreviewGeometryRebasesObjectsAndPreservesSubsetOffsets();
    TestNmfPreviewGeometryStructuralDiagnosticsAndTransactionalFailures();
    TestMalformedInputs();
    if (failures) {
        std::cerr << failures << " NMF geometry test(s) failed\n";
        return 1;
    }
    std::cout << "All NMF geometry profile tests passed\n";
    return 0;
}
