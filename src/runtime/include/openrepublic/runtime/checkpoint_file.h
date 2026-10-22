#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace openrepublic::runtime {

/** Validates and durably publishes a checkpoint without replacing an existing path. */
bool WriteCheckpointFileExclusive(const std::filesystem::path &path,
                                  std::string_view checkpoint,
                                  std::string &error);

} // namespace openrepublic::runtime
