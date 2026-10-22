#include "openrepublic/compat/storage_profile.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
using openrepublic::compat::ParseStorageCapacityProfile;
using openrepublic::compat::StorageCapacityFactors;

int failures = 0;

void Check(bool condition, const char *message) {
    if (condition)
        return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

void Reject(std::string text, const char *message) {
    StorageCapacityFactors factors{{{"KEEP", "grain"}, 2.5}};
    const auto before = factors;
    std::string error;
    Check(!ParseStorageCapacityProfile(text, factors, error), message);
    Check(factors == before, "rejected profile leaves the output map unchanged");
    Check(!error.empty(), "rejected profile provides an error");
}

} // namespace

int main() {
    StorageCapacityFactors factors;
    std::string error;
    const std::string valid =
        "OPENREPUBLIC_STORAGE_PROFILE 1\r\n"
        "\r\n"
        "# OpenRepublic-authored synthetic factors\r\n"
        "FACTOR COVERED grain 1250000.5\r\n"
        "FACTOR OPEN steel .25\r\n";
    Check(ParseStorageCapacityProfile(valid, factors, error), "CRLF profile with comments parses");
    Check(factors.size() == 2, "valid profile contains both factor mappings");
    Check(factors.at({"COVERED", "grain"}) == 1250000.5 &&
              factors.at({"OPEN", "steel"}) == 0.25,
          "parsed factors retain their exact pair keys and decimal values");
    Check(error.empty(), "successful parse clears a previous error");

    Reject("FACTOR COVERED grain 1\n", "missing header is rejected");
    Reject("OPENREPUBLIC_STORAGE_PROFILE 2\n", "unsupported header version is rejected");
    Reject("OPENREPUBLIC_STORAGE_PROFILE 1\nFACTOR COVERED grain 1 extra\n",
           "trailing tokens are rejected");
    Reject("OPENREPUBLIC_STORAGE_PROFILE 1\nFACTOR COVERED grain 1\nFACTOR COVERED grain 2\n",
           "duplicate pairs are rejected");
    Reject("OPENREPUBLIC_STORAGE_PROFILE 1\nFACTOR COVERED grain 0\n",
           "zero factor is rejected");
    Reject("OPENREPUBLIC_STORAGE_PROFILE 1\nFACTOR COVERED grain -1\n",
           "negative factor is rejected");
    Reject("OPENREPUBLIC_STORAGE_PROFILE 1\nFACTOR COVERED grain nan\n",
           "NaN factor is rejected");
    Reject("OPENREPUBLIC_STORAGE_PROFILE 1\nFACTOR COVERED grain inf\n",
           "infinite factor is rejected");
    Reject("OPENREPUBLIC_STORAGE_PROFILE 1\nFACTOR bad/name grain 1\n",
           "invalid names are rejected");
    Reject("OPENREPUBLIC_STORAGE_PROFILE 1\nFACTOR COVERED grain 1e3\n",
           "non-decimal exponent notation is rejected");

    std::string tooLarge = "OPENREPUBLIC_STORAGE_PROFILE 1\n";
    tooLarge.append(64u * 1024u, ' ');
    Reject(tooLarge, "profiles larger than 64 KiB are rejected");

    constexpr char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string tooMany = "OPENREPUBLIC_STORAGE_PROFILE 1\n";
    for (int i = 0; i < 4097; ++i) {
        const std::string className(1, alphabet[i % 62]);
        const int resourceIndex = i / 62;
        const std::string resourceName{alphabet[resourceIndex / 62], alphabet[resourceIndex % 62]};
        tooMany += "FACTOR " + className + " " + resourceName + " 1\n";
    }
    Reject(tooMany, "profiles with more than 4096 rows are rejected");

    const auto temp = std::filesystem::temp_directory_path() / "openrepublic-storage-profile-test.txt";
    {
        std::ofstream output(temp, std::ios::binary | std::ios::trunc);
        output << "OPENREPUBLIC_STORAGE_PROFILE 1\nFACTOR COVERED grain 3.5\n";
    }
    factors.clear();
    Check(openrepublic::compat::LoadStorageCapacityProfile(temp.string(), factors, error),
          "profile loader reads a bounded file");
    Check(factors.size() == 1 && factors.at({"COVERED", "grain"}) == 3.5,
          "file loader returns parsed factors");
    std::filesystem::remove(temp);

    const auto oversized = std::filesystem::temp_directory_path() / "openrepublic-storage-profile-oversized.txt";
    {
        std::ofstream output(oversized, std::ios::binary | std::ios::trunc);
        output << "OPENREPUBLIC_STORAGE_PROFILE 1\n";
        std::string padding(64u * 1024u, ' ');
        output.write(padding.data(), static_cast<std::streamsize>(padding.size()));
    }
    factors = {{{"KEEP", "grain"}, 2.5}};
    const auto before = factors;
    Check(!openrepublic::compat::LoadStorageCapacityProfile(oversized.string(), factors, error),
          "file loader rejects an oversized input before parsing");
    Check(factors == before, "oversized file leaves output map unchanged");
    std::filesystem::remove(oversized);

    std::cout << (failures == 0 ? "PASS" : "FAIL") << ": storage profile tests ("
              << failures << " failures)\n";
    return failures == 0 ? 0 : 1;
}
