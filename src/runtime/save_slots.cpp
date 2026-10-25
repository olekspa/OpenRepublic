#include "openrepublic/runtime/save_slots.h"

#include "openrepublic/economy/checkpoint.h"
#include "openrepublic/runtime/checkpoint_file.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>
#include <limits>
#include <cstdlib>
#include <chrono>
#include <utility>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace openrepublic::runtime {
namespace {
constexpr std::string_view kMagic = "OPENREPUBLIC_SAVE_SLOT 1\n";
constexpr std::size_t kMaxSlotBytes = economy::MaxCheckpointBytes + 256;

bool DecimalLine(std::string_view text, std::size_t& cursor, std::string_view label,
                 std::uint64_t& value) {
    if (text.substr(cursor, label.size()) != label) return false;
    cursor += label.size();
    const auto end = text.find('\n', cursor);
    if (end == std::string_view::npos || end == cursor) return false;
    const auto digits = text.substr(cursor, end - cursor);
    if (digits.size() > 1 && digits.front() == '0') return false;
    std::uint64_t parsed{};
    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size()) return false;
    cursor = end + 1;
    value = parsed;
    return true;
}

bool ParseSlot(std::string_view data, std::string_view& payload, SaveSlotInfo& info) {
    if (data.substr(0, kMagic.size()) != kMagic) return false;
    std::size_t cursor = kMagic.size();
    std::uint64_t length{};
    if (!DecimalLine(data, cursor, "SAVED_AT_UNIX ", info.savedAtUnixSeconds) ||
        !DecimalLine(data, cursor, "TICK ", info.tick) ||
        !DecimalLine(data, cursor, "BUILDINGS ", info.buildingCount) ||
        !DecimalLine(data, cursor, "CHECKPOINT_BYTES ", length) ||
        length > economy::MaxCheckpointBytes || length != data.size() - cursor) return false;
    payload = data.substr(cursor, static_cast<std::size_t>(length));
    info.hasMetadata = true;
    return true;
}

bool ReadBytes(const std::filesystem::path& path, std::string& bytes, std::string& error) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size == 0 || size > kMaxSlotBytes) {
        error = ec ? "could not inspect save slot: " + ec.message() : "save slot is empty or exceeds its size limit";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) { error = "could not open save slot"; return false; }
    bytes.assign(std::istreambuf_iterator<char>(input), {});
    if (input.bad() || bytes.size() != size) { error = "save slot could not be read completely"; return false; }
    return true;
}
}

bool IsValidSaveSlotName(std::string_view name) noexcept {
    if (name.empty() || name.size() > 48 || name == "." || name == "..") return false;
    for (const unsigned char ch : name) {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '-' || ch == '_')) return false;
    }
    const auto equals = [name](std::string_view expected) {
        if (name.size() != expected.size()) return false;
        for (std::size_t i = 0; i < name.size(); ++i) {
            const auto ch = static_cast<unsigned char>(name[i]);
            const char upper = static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch);
            if (upper != expected[i]) return false;
        }
        return true;
    };
    if (equals("CON") || equals("PRN") || equals("AUX") || equals("NUL")) return false;
    if (name.size() == 4 && (equals("COM1") || equals("COM2") || equals("COM3") || equals("COM4") ||
        equals("COM5") || equals("COM6") || equals("COM7") || equals("COM8") || equals("COM9") ||
        equals("LPT1") || equals("LPT2") || equals("LPT3") || equals("LPT4") || equals("LPT5") ||
        equals("LPT6") || equals("LPT7") || equals("LPT8") || equals("LPT9"))) return false;
    return true;
}

std::filesystem::path DefaultSaveDirectory() {
#ifdef _WIN32
    std::wstring local(32768, L'\0');
    DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local.data(), static_cast<DWORD>(local.size()));
    if (length > 0 && length < local.size()) {
        local.resize(length);
        return std::filesystem::path(local) / "OpenRepublic" / "saves";
    }
    std::wstring profile(32768, L'\0');
    length = GetEnvironmentVariableW(L"USERPROFILE", profile.data(), static_cast<DWORD>(profile.size()));
    if (length > 0 && length < profile.size()) {
        profile.resize(length);
        return std::filesystem::path(profile) / "AppData" / "Local" / "OpenRepublic" / "saves";
    }
    return {};
#endif
    if (const char* home = std::getenv("HOME"); home && *home)
        return std::filesystem::u8path(home) / ".local" / "share" / "OpenRepublic" / "saves";
    return std::filesystem::current_path() / "OpenRepublic" / "saves";
}

bool WriteSaveSlot(const std::filesystem::path& directory, std::string_view name,
                   std::string_view checkpoint, std::uint64_t savedAtUnixSeconds,
                   std::uint64_t tick, std::uint64_t buildingCount, std::string& error) {
    error.clear();
    if (directory.empty()) { error = "user-data save directory is unavailable"; return false; }
    if (!IsValidSaveSlotName(name)) { error = "save slot name must use 1-48 letters, digits, '-' or '_'"; return false; }
    if (checkpoint.empty() || checkpoint.size() > economy::MaxCheckpointBytes) { error = "checkpoint payload is empty or too large"; return false; }
    economy::RestoredCheckpoint restored;
    economy::ParseError parseError;
    if (!economy::RestoreCheckpoint(checkpoint, restored, parseError)) { error = "invalid checkpoint: " + parseError.message; return false; }
    const auto snapshot = restored.world->Snapshot();
    if (snapshot.tick != tick || snapshot.buildings.size() != buildingCount) {
        error = "save slot metadata does not match the checkpoint world";
        return false;
    }
    std::string wrapped(kMagic);
    wrapped += "SAVED_AT_UNIX " + std::to_string(savedAtUnixSeconds) + "\n";
    wrapped += "TICK " + std::to_string(tick) + "\n";
    wrapped += "BUILDINGS " + std::to_string(buildingCount) + "\n";
    wrapped += "CHECKPOINT_BYTES " + std::to_string(checkpoint.size()) + "\n";
    wrapped.append(checkpoint);
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) { error = "could not create save directory: " + ec.message(); return false; }
    return ReplaceFileAtomically(directory / (std::string(name) + ".orsave"), wrapped, kMaxSlotBytes, error);
}

bool ReadSaveSlot(const std::filesystem::path& path, std::string& checkpoint,
                  SaveSlotInfo* metadata, std::string& error) {
    error.clear();
    std::string bytes;
    if (!ReadBytes(path, bytes, error)) return false;
    std::string_view payload(bytes);
    SaveSlotInfo info;
    if (ParseSlot(bytes, payload, info)) {
        economy::RestoredCheckpoint restored;
        economy::ParseError parseError;
        if (!economy::RestoreCheckpoint(payload, restored, parseError)) { error = "save slot checkpoint is invalid: " + parseError.message; return false; }
        const auto snapshot = restored.world->Snapshot();
        if (snapshot.tick != info.tick || snapshot.buildings.size() != info.buildingCount) {
            error = "save slot metadata does not match its checkpoint world";
            return false;
        }
    } else {
        // Legacy raw checkpoints remain loadable. Their tick/count are intentionally unknown
        // to the slot list because the old format contains no metadata header.
        economy::RestoredCheckpoint restored;
        economy::ParseError parseError;
        if (!economy::RestoreCheckpoint(bytes, restored, parseError)) { error = "save slot is neither a supported slot nor a legacy checkpoint: " + parseError.message; return false; }
        info.hasMetadata = false;
    }
    info.name = path.stem().string();
    info.path = path;
    checkpoint.assign(payload);
    if (metadata) *metadata = std::move(info);
    return true;
}

bool ListSaveSlots(const std::filesystem::path& directory, std::vector<SaveSlotInfo>& slots,
                   std::string& error, SaveSlotPolicy policy) {
    error.clear();
    if (directory.empty()) { error = "user-data save directory is unavailable"; return false; }
    std::vector<SaveSlotInfo> result;
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec)) { if (ec) { error = ec.message(); return false; } slots.clear(); return true; }
    std::size_t scanned = 0;
    for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
        if (++scanned > 256) { error = "save directory has too many entries to list safely"; return false; }
        if (it->is_symlink(ec)) { ec.clear(); continue; }
        if (!it->is_regular_file(ec) || ec) { ec.clear(); continue; }
        const auto extension = it->path().extension();
        if (extension != ".orsave" && extension != ".checkpoint") continue;
        std::string bytes, readError;
        if (!ReadBytes(it->path(), bytes, readError)) continue;
        std::string_view payload;
        SaveSlotInfo info;
        if (!ParseSlot(bytes, payload, info)) {
            // Legacy checkpoint files have no slot envelope. Recognize the old
            // checkpoint magic only; do not restore a world while listing.
            if (bytes.rfind("OPENREPUBLIC_ECONOMY_CHECKPOINT ", 0) != 0) continue;
            payload = bytes;
            info.hasMetadata = false;
            const auto fileTime = it->last_write_time(ec);
            if (!ec) {
                const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    fileTime - decltype(fileTime)::clock::now() + std::chrono::system_clock::now());
                const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(systemTime.time_since_epoch()).count();
                if (seconds > 0) info.savedAtUnixSeconds = static_cast<std::uint64_t>(seconds);
            }
            ec.clear();
        }
        info.name = it->path().stem().string();
        info.path = it->path();
        // Listing reads only bounded slot metadata and the payload length. The checkpoint
        // catalog/world are not parsed or restored until the user selects Load.
        if (payload.empty()) continue;
        result.push_back(std::move(info));
        if (result.size() > policy.maxNamedSlots + policy.rotatingSlots) { error = "save directory exceeds the slot count limit"; return false; }
    }
    if (ec) { error = "could not enumerate save directory: " + ec.message(); return false; }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        if (a.savedAtUnixSeconds != b.savedAtUnixSeconds) return a.savedAtUnixSeconds > b.savedAtUnixSeconds;
        return a.name < b.name;
    });
    slots = std::move(result);
    return true;
}

bool WriteAutosave(const std::filesystem::path& directory, std::string_view checkpoint,
                   std::uint64_t savedAtUnixSeconds, std::uint64_t tick,
                   std::uint64_t buildingCount, std::size_t& nextSlot, std::string& error,
                   SaveSlotPolicy policy) {
    if (directory.empty()) { error = "user-data save directory is unavailable"; return false; }
    if (policy.rotatingSlots == 0 || policy.rotatingSlots > 16) { error = "autosave slot count is outside 1..16"; return false; }
    nextSlot %= policy.rotatingSlots;
    const auto target = directory / ("autosave-" + std::to_string(nextSlot + 1) + ".orsave");
    economy::RestoredCheckpoint restored;
    economy::ParseError parseError;
    if (!economy::RestoreCheckpoint(checkpoint, restored, parseError)) { error = "invalid checkpoint: " + parseError.message; return false; }
    const auto snapshot = restored.world->Snapshot();
    if (snapshot.tick != tick || snapshot.buildings.size() != buildingCount) {
        error = "autosave metadata does not match the checkpoint world";
        return false;
    }
    std::string wrapped(kMagic);
    wrapped += "SAVED_AT_UNIX " + std::to_string(savedAtUnixSeconds) + "\n";
    wrapped += "TICK " + std::to_string(tick) + "\n";
    wrapped += "BUILDINGS " + std::to_string(buildingCount) + "\n";
    wrapped += "CHECKPOINT_BYTES " + std::to_string(checkpoint.size()) + "\n";
    wrapped.append(checkpoint);
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) { error = "could not create save directory: " + ec.message(); return false; }
    if (!ReplaceFileAtomically(target, wrapped, kMaxSlotBytes, error)) return false;
    nextSlot = (nextSlot + 1) % policy.rotatingSlots;
    return true;
}

} // namespace openrepublic::runtime
