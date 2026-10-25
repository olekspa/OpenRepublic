#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace openrepublic::runtime {

/** Validates and durably publishes a checkpoint without replacing an existing path. */
bool WriteCheckpointFileExclusive(const std::filesystem::path &path,
                                  std::string_view checkpoint,
                                  std::string &error);

/** Atomically publishes already-validated bounded bytes without replacing an existing path. */
bool WriteFileExclusive(const std::filesystem::path& path, std::string_view bytes,
                        std::size_t maximumBytes, std::string& error);

/** Atomically replaces a file after writing and validating the complete new bytes. */
bool ReplaceFileAtomically(const std::filesystem::path& path, std::string_view bytes,
                           std::size_t maximumBytes, std::string& error);

} // namespace openrepublic::runtime
