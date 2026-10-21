#include "openrepublic/content/nmf_header.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

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

std::string Fixture(const std::vector<std::string>& identifiers, std::uint32_t objectCount = 1) {
    std::string bytes(20 + identifiers.size() * 64, '\0');
    WriteU32LE(bytes, 8, static_cast<std::uint32_t>(identifiers.size()));
    WriteU32LE(bytes, 12, objectCount);
    WriteU32LE(bytes, 16, 0xdecafbad); // Opaque in this header-only profile.
    for (std::size_t i = 0; i < identifiers.size(); ++i)
        bytes.replace(20 + i * 64, identifiers[i].size(), identifiers[i]);
    return bytes;
}

void ExpectFailure(std::string bytes, const std::string& label) {
    NmfHeaderProfile output;
    output.objectRecordCount = 123;
    output.submaterialIdentifiers = {"sentinel"};
    std::string error;
    const bool parsed = ParseNmfHeaderProfile(bytes, output, &error);
    Check(!parsed && !error.empty(), label + " rejects with a diagnostic");
    Check(output.objectRecordCount == 123 &&
        output.submaterialIdentifiers == std::vector<std::string>{"sentinel"},
        label + " leaves caller output unchanged");
}

void TestValidHeaderProfile() {
    const auto bytes = Fixture({"synthetic brick", "synthetic_window"}, 2);
    NmfHeaderProfile output;
    std::string error = "stale";
    Check(ParseNmfHeaderProfile(bytes, output, &error), "bounded synthetic header parses");
    Check(error.empty(), "success clears diagnostic");
    Check(output.objectRecordCount == 2, "observed object-record count is retained");
    Check(output.submaterialIdentifiers ==
        std::vector<std::string>({"synthetic brick", "synthetic_window"}),
        "64-byte identifier records preserve their exact order");
}

void TestMalformedHeadersAreTransactional() {
    ExpectFailure(std::string(19, '\0'), "short fixed header");
    ExpectFailure(Fixture({"complete"}).substr(0, 20 + 63), "truncated identifier record");

    auto excessiveSubmaterials = Fixture({});
    WriteU32LE(excessiveSubmaterials, 8, MaxNmfHeaderSubmaterials + 1);
    ExpectFailure(excessiveSubmaterials, "excessive submaterial count");

    auto excessiveObjects = Fixture({});
    WriteU32LE(excessiveObjects, 12, MaxNmfHeaderObjectRecords + 1);
    ExpectFailure(excessiveObjects, "excessive object-record count");

    auto missingTerminator = Fixture({"identifier"});
    std::fill(missingTerminator.begin() + 20, missingTerminator.begin() + 84, 'x');
    ExpectFailure(missingTerminator, "identifier without a terminator");

    auto emptyIdentifier = Fixture({"x"});
    emptyIdentifier[20] = '\0';
    ExpectFailure(emptyIdentifier, "empty identifier");

    auto controlByte = Fixture({"x"});
    controlByte[20] = '\x01';
    ExpectFailure(controlByte, "identifier with a control byte");

    auto nonAscii = Fixture({"x"});
    nonAscii[20] = static_cast<char>(0x80);
    ExpectFailure(nonAscii, "identifier outside printable ASCII");
}

} // namespace

int main() {
    TestValidHeaderProfile();
    TestMalformedHeadersAreTransactional();
    if (failures != 0) {
        std::cerr << failures << " NMF header test(s) failed\n";
        return 1;
    }
    std::cout << "All NMF header profile tests passed\n";
    return 0;
}
