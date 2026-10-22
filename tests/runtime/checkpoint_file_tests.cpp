#include "openrepublic/runtime/checkpoint_file.h"
#include "openrepublic/economy/checkpoint.h"
#include "openrepublic/economy/economy.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace openrepublic;
namespace {
int failures = 0;
void Check(bool value, const char *message) {
    if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
std::string Read(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
}

int main() {
    const auto root = fs::temp_directory_path() / fs::path("OpenRepublic checkpoint file tests");
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);
    auto catalog = economy::MakeSyntheticCatalog();
    economy::World world(catalog, 8, 8);
    std::string payload;
    economy::ParseError parse;
    Check(economy::SaveCheckpoint(world, payload, parse), "create valid checkpoint fixture");
    const auto sourceHash = world.Snapshot().stateHash;
    std::string error;
    const auto unicode = root / fs::path(u8"世界-checkpoint.chk");
    auto collision = unicode;
    collision += ".tmp.openrepublic.0";
    { std::ofstream out(collision, std::ios::binary); out << "owned elsewhere"; }
    Check(!runtime::WriteCheckpointFileExclusive(unicode, payload, error), "preexisting temporary path is not reused");
    Check(Read(collision) == "owned elsewhere", "preexisting temporary file remains untouched");
    fs::remove(collision);
    Check(runtime::WriteCheckpointFileExclusive(unicode, payload, error), "publish valid checkpoint at Unicode path");
    Check(Read(unicode) == payload, "published bytes exactly match validated payload");
    economy::RestoredCheckpoint restored;
    Check(economy::RestoreCheckpoint(Read(unicode), restored, parse), "published checkpoint restores");
    Check(restored.world && restored.world->Snapshot().stateHash == sourceHash,
          "restored world hash matches source world");

    economy::World differentWorld(catalog, 8, 8);
    differentWorld.Step(1);
    std::string differentPayload;
    Check(economy::SaveCheckpoint(differentWorld, differentPayload, parse), "create different valid checkpoint fixture");
    error.clear();
    Check(!runtime::WriteCheckpointFileExclusive(unicode, differentPayload, error), "valid payload cannot replace existing output");
    Check(Read(unicode) == payload, "existing output remains byte-for-byte intact");
    const auto invalid = root / "invalid.chk";
    Check(!runtime::WriteCheckpointFileExclusive(invalid, "", error), "empty payload rejected");
    Check(!runtime::WriteCheckpointFileExclusive(invalid, "invalid", error), "malformed payload rejected");
    Check(!fs::exists(invalid), "invalid payload leaves no output");
    const auto missing = root / "missing" / "out.chk";
    Check(!runtime::WriteCheckpointFileExclusive(missing, payload, error), "missing parent fails");
    Check(!fs::exists(missing), "missing parent leaves no output");
    for (const auto &entry : fs::directory_iterator(root))
        Check(entry.path() == unicode || entry.path() == invalid, "no temporary file remains");
    fs::remove_all(root, ec);
    return failures ? 1 : 0;
}
