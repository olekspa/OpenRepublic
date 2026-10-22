#include "openrepublic/content/text_table.h"

#include <iostream>
#include <string>

using namespace openrepublic::content;

namespace {

int failures = 0;

void Check(bool condition, const std::string& label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

void U32(std::string& out, std::uint32_t v) {
    for (int s = 24; s >= 0; s -= 8) out.push_back(static_cast<char>((v >> s) & 0xff));
}

void U16(std::string& out, std::uint16_t v) {
    out.push_back(static_cast<char>(v >> 8));
    out.push_back(static_cast<char>(v & 0xff));
}

// Two strings: id 7 "Farm" and id 9 "t/d" + e-acute + an emoji surrogate pair.
std::string MakeTable() {
    const std::u16string first = u"Farm";
    const std::u16string second = u"t/d\u00e9\U0001F600";
    std::string out;
    const std::uint32_t textStart = 12 + 2 * 10;
    const std::uint32_t total =
        textStart + static_cast<std::uint32_t>((first.size() + 1 + second.size() + 1) * 2);
    U32(out, 2);
    U32(out, total);
    U32(out, 0);
    U32(out, 7);
    U32(out, 0);
    U16(out, static_cast<std::uint16_t>(first.size()));
    U32(out, 9);
    U32(out, static_cast<std::uint32_t>(first.size() + 1));
    U16(out, static_cast<std::uint16_t>(second.size()));
    for (char16_t c : first) U16(out, c);
    U16(out, 0);
    for (char16_t c : second) U16(out, c);
    U16(out, 0);
    return out;
}

void TestParsesAndConverts() {
    std::map<std::uint32_t, std::string> table;
    std::string error;
    Check(ParseTextTable(MakeTable(), table, &error), "synthetic table parses: " + error);
    Check(table.size() == 2 && table[7] == "Farm", "ASCII text is decoded");
    const std::string expected = {'t', '/', 'd', char(0xC3), char(0xA9), char(0xF0), char(0x9F), char(0x98), char(0x80)};
    Check(table[9] == expected, "BMP and surrogate-pair characters become UTF-8");
}

void TestRejectsBadTables() {
    std::map<std::uint32_t, std::string> out{{1, "keep"}};
    const auto rejects = [&](const std::string& bytes, const std::string& label) {
        std::string error;
        Check(!ParseTextTable(bytes, out, &error) && !error.empty() && out.size() == 1 && out[1] == "keep",
              label);
    };
    rejects("short", "a short input is rejected");
    std::string wrongSize = MakeTable();
    wrongSize.push_back('x');
    rejects(wrongSize, "a size mismatch is rejected");
    std::string outside = MakeTable();
    outside[12 + 8] = char(0x7f); // high byte of record 0 length
    rejects(outside, "a record outside the text area is rejected");
    std::string duplicate = MakeTable();
    duplicate[12 + 10 + 3] = 7; // second id becomes 7
    rejects(duplicate, "a duplicate id is rejected");
    std::string hugeCount = MakeTable();
    hugeCount[1] = char(0xff);
    rejects(hugeCount, "an oversized entry count is rejected");
}

} // namespace

int main() {
    TestParsesAndConverts();
    TestRejectsBadTables();
    if (failures != 0) {
        std::cerr << failures << " text-table test(s) failed\n";
        return 1;
    }
    std::cout << "All text-table tests passed\n";
    return 0;
}
