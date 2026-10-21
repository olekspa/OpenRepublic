#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::content {

constexpr std::uint32_t MaxNmfHeaderSubmaterials = 4096;
constexpr std::uint32_t MaxNmfHeaderObjectRecords = 4096;

struct NmfHeaderProfile {
    std::uint32_t objectRecordCount{};
    std::vector<std::string> submaterialIdentifiers;
};

// Reads only the 20-byte prefix and the observed 64-byte submaterial identifier
// table. It does not validate or decode object geometry, animations, or materials.
// On failure, output is unchanged.
bool ParseNmfHeaderProfile(std::string_view bytes, NmfHeaderProfile& output,
                           std::string* error = nullptr);

} // namespace openrepublic::content
