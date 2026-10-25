#include "openrepublic/runtime/save_slots.h"

#include "openrepublic/economy/checkpoint.h"
#include "openrepublic/economy/economy.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace openrepublic;
namespace {
int failures = 0;
void Check(bool value, const char* message) {
    if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
fs::path TempDirectory() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = fs::temp_directory_path() / ("OpenRepublic-save-slots-" + std::to_string(nonce));
    std::error_code ec;
    fs::create_directories(path, ec);
    return ec ? fs::path{} : path;
}
std::string Read(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
}

int main() {
    Check(runtime::IsValidSaveSlotName("before-shift_01"), "accept safe slot name");
    for (const auto bad : {"", "..", "../escape", "has space", "semi;colon", "a.b", "CON", "Lpt1"})
        Check(!runtime::IsValidSaveSlotName(bad), "reject unsafe slot name");

    const auto root = TempDirectory();
    Check(!root.empty(), "create temporary save directory");
    if (root.empty()) return 1;
    auto catalog = economy::MakeSyntheticCatalog();
    economy::World world(catalog, 8, 8);
    world.Step(7);
    std::string checkpoint;
    economy::ParseError parseError;
    Check(economy::SaveCheckpoint(world, checkpoint, parseError), "create native checkpoint");
    const auto hash = world.Snapshot().stateHash;
    std::string error;
    Check(runtime::WriteSaveSlot(root, "manual-1", checkpoint, 1234, 7, 0, error), "write named slot");
    Check(!runtime::WriteSaveSlot(root, "bad-metadata", checkpoint, 1, 8, 0, error),
          "reject metadata that disagrees with checkpoint tick");
    std::vector<runtime::SaveSlotInfo> slots;
    Check(runtime::ListSaveSlots(root, slots, error), "list save slot headers");
    Check(slots.size() == 1 && slots[0].hasMetadata && slots[0].savedAtUnixSeconds == 1234 &&
          slots[0].tick == 7 && slots[0].buildingCount == 0, "header metadata lists timestamp, tick and count");
    std::string loaded;
    runtime::SaveSlotInfo metadata;
    const auto namedPath = root / "manual-1.orsave";
    Check(runtime::ReadSaveSlot(namedPath, loaded, &metadata, error), "load selected save slot");
    economy::RestoredCheckpoint restored;
    Check(economy::RestoreCheckpoint(loaded, restored, parseError) &&
          restored.world->Snapshot().stateHash == hash, "loaded slot restores exact world state");

    const auto legacy = root / "legacy.orsave";
    { std::ofstream out(legacy, std::ios::binary); out.write(checkpoint.data(), static_cast<std::streamsize>(checkpoint.size())); }
    Check(runtime::ReadSaveSlot(legacy, loaded, &metadata, error) && !metadata.hasMetadata,
          "legacy raw checkpoint remains loadable without metadata");
    Check(runtime::ListSaveSlots(root, slots, error), "list legacy checkpoint by filename metadata");
    Check(std::any_of(slots.begin(), slots.end(), [](const auto& slot) {
        return slot.name == "legacy" && !slot.hasMetadata;
    }), "legacy checkpoint appears in load list without fabricated tick/count");

    const auto corruptWorld = root / "corrupt-world.orsave";
    Check(runtime::WriteSaveSlot(root, "corrupt-world", checkpoint, 1235, 7, 0, error), "write corrupt-world fixture");
    auto corruptedBytes = Read(corruptWorld);
    const auto checkpointMagic = corruptedBytes.find("OPENREPUBLIC_ECONOMY_CHECKPOINT 1");
    Check(checkpointMagic != std::string::npos, "find nested checkpoint for corruption fixture");
    if (checkpointMagic != std::string::npos)
        corruptedBytes[checkpointMagic + std::string("OPENREPUBLIC_ECONOMY_CHECKPOINT 1").size() - 1] = '2';
    { std::ofstream out(corruptWorld, std::ios::binary | std::ios::trunc); out.write(corruptedBytes.data(), static_cast<std::streamsize>(corruptedBytes.size())); }
    Check(runtime::ListSaveSlots(root, slots, error), "list metadata without restoring checkpoint worlds");
    Check(std::any_of(slots.begin(), slots.end(), [](const auto& slot) { return slot.name == "corrupt-world"; }),
          "header-only list retains slot whose world validation is deferred until load");
    Check(!runtime::ReadSaveSlot(corruptWorld, loaded, &metadata, error), "reject corrupted nested checkpoint on selected load");
    const auto badMetadata = root / "bad-header-metadata.orsave";
    Check(runtime::WriteSaveSlot(root, "bad-header-metadata", checkpoint, 1236, 7, 0, error), "write bad-metadata fixture");
    auto badMetadataBytes = Read(badMetadata);
    const auto tickLine = badMetadataBytes.find("TICK 7\n");
    Check(tickLine != std::string::npos, "find tick metadata for corruption fixture");
    if (tickLine != std::string::npos) badMetadataBytes[tickLine + 5] = '8';
    { std::ofstream out(badMetadata, std::ios::binary | std::ios::trunc); out.write(badMetadataBytes.data(), static_cast<std::streamsize>(badMetadataBytes.size())); }
    Check(!runtime::ReadSaveSlot(badMetadata, loaded, &metadata, error), "reject header metadata that disagrees on selected load");
    const auto truncated = root / "truncated.orsave";
    { std::ofstream out(truncated, std::ios::binary); out << "OPENREPUBLIC_SAVE_SLOT 1\nSAVED_AT_UNIX 1\nTICK 7\nBUILDINGS 0\nCHECKPOINT_BYTES 12\nshort"; }
    Check(!runtime::ReadSaveSlot(truncated, loaded, &metadata, error), "reject truncated slot payload");
    const auto corrupt = root / "corrupt.orsave";
    { std::ofstream out(corrupt, std::ios::binary); out << "OPENREPUBLIC_SAVE_SLOT 1\nSAVED_AT_UNIX 01\n"; }
    Check(!runtime::ReadSaveSlot(corrupt, loaded, &metadata, error), "reject noncanonical metadata number");

    std::size_t next = 0;
    for (std::uint64_t i = 0; i < 5; ++i) {
        economy::World autosaveWorld(catalog, 8, 8);
        autosaveWorld.Step(7 + i);
        std::string autosaveCheckpoint;
        Check(economy::SaveCheckpoint(autosaveWorld, autosaveCheckpoint, parseError), "create autosave checkpoint fixture");
        Check(runtime::WriteAutosave(root, autosaveCheckpoint, 2000 + i, 7 + i, 0, next, error), "rotate autosave ring");
    }
    Check(next == 2, "autosave ring advances modulo three");
    Check(runtime::ListSaveSlots(root, slots, error), "list after autosave rotation");
    std::size_t autosaves = 0;
    for (const auto& slot : slots) {
        if (slot.name.rfind("autosave-", 0) == 0) ++autosaves;
    }
    Check(autosaves == 3, "autosave rotation retains exactly three slots");

    std::error_code ec;
    fs::remove_all(root, ec);
    Check(!ec, "remove test-owned temporary directory");
    return failures ? 1 : 0;
}
