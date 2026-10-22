#include "building_record_walk.h"
#include "openrepublic/content/map_placement.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using openrepublic::content::detail::WalkBuildingRecordOffsets;

namespace {

int failures = 0;

void Check(bool condition, const std::string& label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

void PutI32(std::string& bytes, std::size_t offset, std::int32_t value) {
    const auto raw = static_cast<std::uint32_t>(value);
    bytes[offset] = static_cast<char>(raw & 0xff);
    bytes[offset + 1] = static_cast<char>((raw >> 8) & 0xff);
    bytes[offset + 2] = static_cast<char>((raw >> 16) & 0xff);
    bytes[offset + 3] = static_cast<char>((raw >> 24) & 0xff);
}

void PutFloat(std::string& bytes, std::size_t offset, float value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void AppendZeros(std::string& bytes, std::size_t count) { bytes.append(count, '\0'); }

void AppendArray(std::string& bytes, std::size_t count, std::size_t stride) {
    AppendZeros(bytes, count * stride);
}

std::string MinimalRecord() {
    std::string record(0x548 + 400, '\0');
    PutI32(record, 0x514, 1);
    AppendZeros(record, 128);
    return record;
}

void SetCount(std::string& record, bool extension, std::size_t field, std::int32_t count = 1) {
    const std::size_t base = extension ? 0x548 : 0;
    PutI32(record, base + field, count);
}

void AppendHelperA(std::string& out) {
    std::string h(32, '\0');
    PutI32(h, 0, 1); PutI32(h, 20, 1); PutI32(h, 24, 1); PutI32(h, 28, 1);
    out += h;
    AppendArray(out, 1, 72); AppendArray(out, 1, 188);
    AppendArray(out, 1, 12); AppendArray(out, 1, 12);
}

void AppendHelperB(std::string& out) {
    std::string h(16, '\0'); PutI32(h, 0, 1); out += h;
    AppendArray(out, 1, 72);
}

void AppendHelperC(std::string& out) { AppendZeros(out, 60); }

void AppendHelperD(std::string& out) {
    std::string h(44, '\0');
    for (const auto field : {16u, 12u, 24u, 20u, 28u, 32u, 36u, 40u}) PutI32(h, field, 1);
    out += h;
    AppendArray(out, 1, 4); AppendArray(out, 1, 8); AppendArray(out, 1, 8);
    AppendArray(out, 1, 4); AppendArray(out, 1, 4); AppendArray(out, 1, 12);
    AppendArray(out, 1, 4); AppendArray(out, 1, 4);
}

void AppendHelperE(std::string& out) {
    std::string h(36, '\0'); PutI32(h, 4, 1); PutI32(h, 8, 1); PutI32(h, 12, 1); out += h;
    AppendArray(out, 1, 8); AppendArray(out, 1, 8); AppendArray(out, 1, 4);
}

void AppendHelperF(std::string& out) {
    std::string h(24, '\0'); PutI32(h, 8, 1); out += h; AppendArray(out, 1, 72);
}

void AppendHelperG(std::string& out) {
    std::string h(40, '\0'); PutI32(h, 36, 1); out += h; AppendArray(out, 1, 72);
}

std::string FullRecord(std::size_t* utfTerminator = nullptr) {
    std::string r(0x548 + 400, '\0');
    SetCount(r, false, 0x3f0);
    std::string first(28, '\0'); PutI32(first, 20, 1); r += first;
    AppendZeros(r, 0x2000);
    std::string count(4, '\0'); PutI32(count, 0, 1); r += count;
    AppendArray(r, 1, 32); AppendArray(r, 1, 72);

    for (const auto& f : {std::pair<std::size_t, std::size_t>{0x400, 188}, {0x404, 4},
                          {0x40c, 188}, {0x408, 4}}) {
        SetCount(r, false, f.first); AppendArray(r, 1, f.second);
    }
    SetCount(r, false, 0x418); AppendHelperA(r);
    SetCount(r, true, 0x1c, 0x20);
    SetCount(r, false, 0x418); AppendArray(r, 1, 4);
    SetCount(r, false, 0x514, 1);
    SetCount(r, false, 0x418); AppendHelperB(r);
    r[0x548 + 0x97] = 2; AppendZeros(r, 2);

    SetCount(r, false, 0x424); AppendHelperC(r);
    SetCount(r, false, 0x428); AppendHelperC(r);
    for (const auto& f : {std::pair<std::size_t, std::size_t>{0x42c, 32}, {0x450, 32},
                          {0x478, 4}, {0x47c, 4}, {0x480, 4}, {0x484, 4}}) {
        SetCount(r, false, f.first); AppendArray(r, 1, f.second);
    }
    SetCount(r, true, 0xe8); AppendArray(r, 1, 4);
    SetCount(r, false, 0x488); AppendArray(r, 1, 4);
    SetCount(r, false, 0x48c); AppendHelperD(r);
    SetCount(r, false, 0x50c); AppendHelperE(r);
    for (const auto& f : {std::pair<std::size_t, std::size_t>{0x4c8, 12}, {0x504, 12},
                          {0x4cc, 28}, {0x4dc, 12}, {0x4e0, 12}, {0x4e4, 12}}) {
        SetCount(r, false, f.first); AppendArray(r, 1, f.second);
    }
    SetCount(r, true, 0x2c); AppendArray(r, 1, 12);
    for (const auto& f : {std::pair<std::size_t, std::size_t>{0x4e8, 8}, {0x4ec, 8},
                          {0x414, 4}, {0x4f4, 4}}) {
        SetCount(r, false, f.first); AppendArray(r, 1, f.second);
    }
    SetCount(r, true, 0xb4); AppendArray(r, 1, 4);
    SetCount(r, false, 0x4f8);
    std::string conditionalCount(4, '\0'); PutI32(conditionalCount, 0, 1); r += conditionalCount;
    AppendArray(r, 1, 32);
    conditionalCount.assign(4, '\0'); PutI32(conditionalCount, 0, 1); r += conditionalCount;
    AppendArray(r, 1, 16); AppendZeros(r, 20);
    SetCount(r, false, 0x524); AppendArray(r, 1, 4);
    SetCount(r, false, 0x52c); AppendZeros(r, 256);
    SetCount(r, false, 0x530);
    for (unsigned i = 0; i < 4; ++i) AppendHelperF(r);
    SetCount(r, false, 0x534);
    AppendZeros(r, 4); AppendHelperF(r); AppendHelperF(r);
    SetCount(r, false, 0x540); AppendArray(r, 1, 4);
    AppendZeros(r, 128);
    SetCount(r, false, 0x538, 2);
    if (utfTerminator != nullptr) *utfTerminator = r.size() + 4;
    AppendZeros(r, 6); // two UTF-16 units + terminating zero unit
    SetCount(r, false, 0x544); AppendArray(r, 1, 4);
    SetCount(r, true, 0x0c); AppendArray(r, 1, 136);
    SetCount(r, true, 0x18, 9); AppendArray(r, 9, 4);
    SetCount(r, true, 0x38); AppendArray(r, 1, 16);
    SetCount(r, true, 0x3c); AppendArray(r, 1, 192);
    SetCount(r, true, 0x40); AppendArray(r, 1, 4);
    SetCount(r, true, 0x4c); AppendArray(r, 1, 4);
    SetCount(r, true, 0x98); AppendZeros(r, 128);
    SetCount(r, true, 0xec); AppendHelperG(r);
    SetCount(r, true, 0xf0); AppendArray(r, 1, 4);
    SetCount(r, true, 0x108); AppendArray(r, 1, 16);
    return r;
}

std::string MakeFile(const std::vector<std::string>& records) {
    std::string bytes(4, '\0');
    PutI32(bytes, 0, static_cast<std::int32_t>(records.size()));
    for (const auto& record : records) bytes += record;
    return bytes;
}

void TestProfilesAndOffsets() {
    const std::string minimal = MinimalRecord();
    std::string decoy(0x548, '\0');
    std::memcpy(decoy.data(), "fake_building", sizeof("fake_building"));
    for (const auto at : {0u, 5u, 10u, 15u}) PutFloat(decoy, 0x31c + at * 4, 1.0f);
    std::string withTail(0x548 + 400, '\0');
    PutI32(withTail, 0x514, 1);
    PutI32(withTail, 0x400, 8);
    // Place decoy fixed-header bytes immediately in the variable stream. The
    // strict walker must still identify the next record by the declared stride.
    std::string variable = decoy;
    variable.resize(8 * 188, '\0');
    withTail += variable;
    AppendZeros(withTail, 128);
    const std::string file = MakeFile({withTail, minimal});
    std::vector<std::size_t> offsets{999};
    std::string error;
    Check(WalkBuildingRecordOffsets(file, 121, offsets, &error), "profile 121 exact walk: " + error);
    Check(offsets.size() == 2 && offsets[0] == 4 && offsets[1] == 4 + withTail.size(),
          "record offsets follow consumed variable streams, ignoring a decoy fixed header");
    Check(WalkBuildingRecordOffsets(file, 124, offsets, &error), "profile 124 exact walk: " + error);
    Check(WalkBuildingRecordOffsets(MakeFile({}), 121, offsets, &error) && offsets.empty(),
          "zero-record file ends exactly after its count");
    Check(!WalkBuildingRecordOffsets(file, 122, offsets, &error), "unlisted profile is rejected");
}

void TestNestedBranchesAndHelpers() {
    const std::string file = MakeFile({FullRecord(), MinimalRecord()});
    std::vector<std::size_t> offsets;
    std::string error;
    Check(WalkBuildingRecordOffsets(file, 124, offsets, &error),
          "all helper families and optional stream branches walk: " + error);
    Check(offsets.size() == 2 && offsets[0] == 4 && offsets[1] == 4 + FullRecord().size(),
          "every branch length determines the next record offset");
}

void TestFailuresAreTransactional() {
    std::string error;
    const auto keepOutput = std::vector<std::size_t>{41, 42};
    auto fails = [&](std::string bytes, std::uint32_t version, const std::string& label) {
        auto output = keepOutput;
        const bool ok = WalkBuildingRecordOffsets(bytes, version, output, &error);
        Check(!ok, label + " rejects");
        Check(output == keepOutput, label + " preserves output on failure");
        Check(!error.empty(), label + " reports an error");
    };

    auto negativeTop = MakeFile({}); PutI32(negativeTop, 0, -1);
    fails(negativeTop, 121, "negative record count");
    auto tooMany = MakeFile({}); PutI32(tooMany, 0, 65537);
    fails(tooMany, 121, "oversized record count");
    fails(MakeFile({MinimalRecord()}).substr(0, 4 + 0x548 + 399), 121, "fixed-header truncation");
    fails(MakeFile({MinimalRecord()}).substr(0, 4 + 0x548 + 400 + 127), 121, "trailing-block truncation");
    fails(MakeFile({MinimalRecord()}) + "x", 121, "trailing bytes after exact EOF");
    auto negativeArray = MakeFile({MinimalRecord()}); PutI32(negativeArray, 4 + 0x400, -1);
    fails(negativeArray, 121, "negative nested array count");
    auto hugeArray = MakeFile({MinimalRecord()}); PutI32(hugeArray, 4 + 0x400, 1 << 20);
    fails(hugeArray, 121, "array count that exceeds available bytes");
    auto invalidFlag = MakeFile({MinimalRecord()}); PutI32(invalidFlag, 4 + 0x514, 2);
    fails(invalidFlag, 121, "unknown fixed-header flag");
    auto invalidReserved = MakeFile({FullRecord()});
    // Locate the second helper group after its preceding fixed arrays.
    const std::size_t helperBOffset = 4 + 0x548 + 400 + 28 + 0x2000 + 4 + 32 + 72 +
        188 + 4 + 188 + 4 + 32 + 72 + 188 + 12 + 12 + 4;
    invalidReserved[helperBOffset + 4] = 1;
    fails(invalidReserved, 121, "unknown reserved helper byte");
    std::size_t stringTerminator = 0;
    auto badUtf = MakeFile({FullRecord(&stringTerminator)});
    badUtf[4 + stringTerminator] = 1;
    fails(badUtf, 121, "missing UTF-16 terminator");
}

void TestSignedAndBoundaryFields() {
    auto packed = MakeFile({FullRecord()});
    packed[4 + 0x548 + 0x97] = static_cast<char>(0xff);
    std::vector<std::size_t> offsets;
    std::string error;
    Check(!WalkBuildingRecordOffsets(packed, 124, offsets, &error), "negative packed stream length rejects");

    auto scalar = MakeFile({MinimalRecord()}); PutI32(scalar, 4 + 0x548 + 0x18, 10);
    Check(!WalkBuildingRecordOffsets(scalar, 124, offsets, &error), "scalar count above nine rejects");

    auto flag = MakeFile({MinimalRecord()}); PutI32(flag, 4 + 0x52c, 2);
    Check(!WalkBuildingRecordOffsets(flag, 124, offsets, &error), "nonbinary optional stream flag rejects");

    auto truncatedHelper = MakeFile({FullRecord()});
    truncatedHelper.resize(truncatedHelper.size() - 1);
    Check(!WalkBuildingRecordOffsets(truncatedHelper, 121, offsets, &error),
          "truncation in final nested helper rejects");
}

int RunCorpusCli(int argc, char** argv) {
    if (argc == 1) return -1;
    if (argc != 3) {
        std::cerr << "usage: OpenRepublicBuildingRecordWalkTests <buildings_game.bin> <121|124>\n";
        return 2;
    }
    const std::string path(argv[1]);
    const std::uint32_t version = static_cast<std::uint32_t>(std::stoul(argv[2]));
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "placement input could not be opened\n";
        return 2;
    }
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0 || static_cast<std::uint64_t>(size) > 512ull * 1024ull * 1024ull) {
        std::cerr << "placement input exceeds the 512 MiB bound\n";
        return 2;
    }
    input.seekg(0, std::ios::beg);
    const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    std::vector<openrepublic::content::PlacedBuilding> buildings;
    std::string error;
    if (!openrepublic::content::ExtractPlacedBuildingsExact(bytes, version, buildings, &error)) {
        std::cerr << "placement walk failed: " << error << '\n';
        return 1;
    }
    std::cout << "PASS exact placement walk: " << buildings.size() << " records\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const int cli = RunCorpusCli(argc, argv);
    if (cli >= 0) return cli;
    TestProfilesAndOffsets();
    TestNestedBranchesAndHelpers();
    TestFailuresAreTransactional();
    TestSignedAndBoundaryFields();
    if (failures == 0) std::cout << "All building-record walk tests passed\n";
    return failures == 0 ? 0 : 1;
}
