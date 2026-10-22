#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace openrepublic::compat {

using StorageCapacityFactors = std::map<std::pair<std::string, std::string>, double>;

// Parses an OpenRepublic-authored mapping from (transport class, resource) to
// micro-units per raw storage amount. On failure, factors is left unchanged.
bool ParseStorageCapacityProfile(std::string_view text, StorageCapacityFactors &factors,
                                 std::string &error);

// Reads at most the profile byte limit plus one byte before parsing. On
// failure, factors is left unchanged.
bool LoadStorageCapacityProfile(const std::filesystem::path &path, StorageCapacityFactors &factors,
                                std::string &error);

} // namespace openrepublic::compat
