#include "openrepublic/content/map_header.h"

#include <cstdint>
#include <iostream>
#include <string>

using namespace openrepublic::content;

namespace {

int failures = 0;

void Check(bool condition, const std::string& label) {
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << label << '\n';
}

void WriteU32LE(std::string& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i)
        bytes[offset + i] = static_cast<char>((value >> (i * 8)) & 0xffu);
}

void WriteU16LE(std::string& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<char>(value & 0xffu);
    bytes[offset + 1] = static_cast<char>((value >> 8) & 0xffu);
}

std::string Fixture(const std::u16string& text, std::uint32_t leading = 121) {
    std::string bytes(4 + (text.size() + 1) * 2 + 12, '\0');
    WriteU32LE(bytes, 0, leading);
    for (std::size_t i = 0; i < text.size(); ++i)
        WriteU16LE(bytes, 4 + i * 2, static_cast<std::uint16_t>(text[i]));
    return bytes;
}

void ExpectFailure(const std::string& bytes, const std::string& label) {
    MapHeaderPrefix output;
    output.leadingValue = 999;
    output.leadingTextCandidate = u"sentinel";
    std::string error;
    const bool parsed = ParseMapHeaderPrefix(bytes, output, &error);
    Check(!parsed && !error.empty(), label + " rejects with a diagnostic");
    Check(output.leadingValue == 999 && output.leadingTextCandidate == u"sentinel",
          label + " leaves output unchanged");
}

void TestObservedPreambleShape() {
    const auto bytes = Fixture(u"synthetic-map", 124);
    MapHeaderPrefix output;
    std::string error = "stale";
    Check(ParseMapHeaderPrefix(bytes, output, &error), "bounded preamble parses");
    Check(error.empty(), "success clears diagnostic");
    Check(output.leadingValue == 124, "leading value is preserved without interpretation");
    Check(output.leadingTextCandidate == u"synthetic-map", "UTF-16LE candidate is preserved");

    const auto unicode = Fixture(u"map-\U0001f3d4");
    Check(ParseMapHeaderPrefix(unicode, output, &error), "valid surrogate pair parses");
    Check(output.leadingTextCandidate == u"map-\U0001f3d4", "surrogate pair is preserved");
}

void TestMalformedPrefixesAreTransactional() {
    ExpectFailure(std::string(7, '\0'), "short prefix");
    ExpectFailure(Fixture(u"x").substr(0, 6), "missing terminator");
    ExpectFailure(std::string(4, '\0') + std::string(4, '\0'), "empty text candidate");

    auto unpairedHigh = Fixture(u"x");
    WriteU16LE(unpairedHigh, 4, 0xd800);
    ExpectFailure(unpairedHigh, "unpaired high surrogate");

    auto unpairedLow = Fixture(u"x");
    WriteU16LE(unpairedLow, 4, 0xdc00);
    ExpectFailure(unpairedLow, "unpaired low surrogate");

    auto noBoundedTerminator = std::string(4 + 258 * 2, '\0');
    WriteU32LE(noBoundedTerminator, 0, 121);
    for (std::size_t i = 0; i < 257; ++i) WriteU16LE(noBoundedTerminator, 4 + i * 2, 'x');
    ExpectFailure(noBoundedTerminator, "string longer than the 256-code-unit limit");

    ExpectFailure(std::string(64 * 1024 + 1, '\0'), "oversized input");
}


void TestFixedBlockLayout() {
    std::string bytes(0x20c, '\0');
    WriteU32LE(bytes, 0, 124);
    WriteU16LE(bytes, 4, 'a');
    WriteU32LE(bytes, 0x184, 0x11111111u);
    WriteU32LE(bytes, 0x184 + 32 * 4, 0xabcdef01u);  // last word, ends at +0x207
    bytes[0x208] = 1; bytes[0x209] = 2; bytes[0x20a] = 3;
    WriteU32LE(bytes, 0x1a4, 0x3f800000u);  // 1.0f camera position x
    WriteU32LE(bytes, 0x1b8, 0x40000000u);  // 2.0f look-at z
    WriteU32LE(bytes, 0x1d8, 1);
    WriteU32LE(bytes, 0x1e0, 7);
    WriteU32LE(bytes, 0x1dc, 9);
    WriteU32LE(bytes, 0x198, 123);
    WriteU32LE(bytes, 0x19c, 1985);
    MapHeaderFixedBlock block;
    std::string error;
    Check(ParseMapHeaderFixedBlock(bytes, block, &error), "fixed block parses for version 124");
    Check(block.version == 124 && block.words[0] == 0x11111111u && block.words[32] == 0xabcdef01u,
          "fixed block first and last words");
    Check(block.cameraPosition[0] == 1.0f && block.cameraLookAt[2] == 2.0f, "camera vectors read as floats");
    Check(block.gameDay == 123 && block.gameYear == 1985, "game date slots");
    Check(block.extendedBlockFlag == 1 && block.firstRecordListCount == 7 && block.secondRecordListCount == 9,
          "extended flag and record list counts");
    Check(block.flagBytes[0] == 1 && block.flagBytes[1] == 2 && block.flagBytes[2] == 3, "fixed block flag bytes");

    MapHeaderFixedBlock untouched;
    untouched.version = 77;
    auto wrongVersion = bytes;
    WriteU32LE(wrongVersion, 0, 121);
    Check(!ParseMapHeaderFixedBlock(wrongVersion, untouched, &error) && untouched.version == 77,
          "other versions rejected without modifying output");
    Check(!ParseMapHeaderFixedBlock(bytes.substr(0, 0x20b), untouched, &error) && untouched.version == 77,
          "short input rejected without modifying output");
}

} // namespace

int main() {
    TestObservedPreambleShape();
    TestMalformedPrefixesAreTransactional();
    TestFixedBlockLayout();
    if (failures != 0) {
        std::cerr << failures << " map header prefix test(s) failed\n";
        return 1;
    }
    std::cout << "All map header prefix tests passed\n";
    return 0;
}
