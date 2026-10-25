#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::runtime {

struct SaveSlotInfo {
    std::string name;
    std::filesystem::path path;
    std::uint64_t savedAtUnixSeconds{};
    std::uint64_t tick{};
    std::uint64_t buildingCount{};
    bool hasMetadata = false;
};

struct SaveSlotPolicy {
    std::size_t maxNamedSlots = 64;
    std::size_t rotatingSlots = 3;
};

bool IsValidSaveSlotName(std::string_view name) noexcept;
std::filesystem::path DefaultSaveDirectory();
bool WriteSaveSlot(const std::filesystem::path& directory, std::string_view name,
                   std::string_view checkpoint, std::uint64_t savedAtUnixSeconds,
                   std::uint64_t tick, std::uint64_t buildingCount, std::string& error);
bool ReadSaveSlot(const std::filesystem::path& path, std::string& checkpoint,
                  SaveSlotInfo* metadata, std::string& error);
bool ListSaveSlots(const std::filesystem::path& directory, std::vector<SaveSlotInfo>& slots,
                   std::string& error, SaveSlotPolicy policy = {});
bool WriteAutosave(const std::filesystem::path& directory, std::string_view checkpoint,
                   std::uint64_t savedAtUnixSeconds, std::uint64_t tick,
                   std::uint64_t buildingCount, std::size_t& nextSlot, std::string& error,
                   SaveSlotPolicy policy = {});

} // namespace openrepublic::runtime
