#include "openrepublic/runtime/campaign.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;
namespace {
int failures = 0;
void Check(bool value, const char* message) {
    if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
fs::path TempDirectory() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("OpenRepublic-campaign-" + std::to_string(nonce));
    std::error_code ec;
    fs::create_directories(root, ec);
    return ec ? fs::path{} : root;
}
void Write(const fs::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
}
std::string Read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return { std::istreambuf_iterator<char>(input), {} };
}
}

int main() {
    using namespace openrepublic::runtime;
    const auto root = TempDirectory();
    Check(!root.empty(), "create temporary campaign root");
    if (root.empty()) return 1;
    const auto firstDir = root / "first";
    const auto secondDir = root / "second";
    fs::create_directories(firstDir);
    fs::create_directories(secondDir);
    for (const auto& dir : { firstDir, secondDir }) {
        Write(dir / "briefing.txt", "Build a synthetic republic.\n");
        Write(dir / "scenario.json", "{}\n");
        Write(dir / "objectives.json", "{}\n");
    }
    Write(firstDir / "campaign.txt",
          "OPENREPUBLIC_CAMPAIGN 1\n"
          "id=first\n"
          "title=First Campaign\n"
          "mission=coal|Coal to heat|briefing.txt|scenario.json|objectives.json|250|0\n"
          "mission=town|Growing town|briefing.txt|scenario.json|objectives.json|500\n");
    Write(secondDir / "campaign.txt",
          "OPENREPUBLIC_CAMPAIGN 1\n"
          "id=second\n"
          "title=Second Campaign\n"
          "mission=logistics|Trade|briefing.txt|scenario.json|objectives.json|0\n");

    CampaignDefinition campaign;
    std::string error;
    Check(LoadCampaign(firstDir / "campaign.txt", campaign, error), "load valid campaign manifest");
    Check(campaign.id == "first" && campaign.title == "First Campaign" && campaign.missions.size() == 2,
          "campaign preserves ordered manifest metadata");
    Check(campaign.missions[0].id == "coal" && campaign.missions[0].parScore == 250 &&
          campaign.missions[0].startingTreasury == 0 && campaign.missions[1].id == "town" &&
          campaign.missions[1].startingTreasury == 0,
          "mission metadata, optional starting treasury default, and order are retained");

    std::vector<std::string> ids;
    Check(ListCampaigns(root, ids, error) && ids == std::vector<std::string>({ "first", "second" }),
          "list valid direct child campaigns in stable order");
    auto progress = EmptyCampaignProgress(campaign);
    Check(progress.missions.size() == 2 && !progress.missions[0].second.completed,
          "new progress starts all missions incomplete");
    Check(RecordCampaignCompletion(campaign, progress, "town", 420), "record a mission completion");
    Check(progress.missions[1].second.completed && progress.missions[1].second.bestScore == 420,
          "completion records score at the matching mission index");
    Check(RecordCampaignCompletion(campaign, progress, "town", 390) &&
          progress.missions[1].second.bestScore == 420, "later lower scores preserve best score");
    Check(!RecordCampaignCompletion(campaign, progress, "missing", 999), "unknown mission cannot alter progress");

    const auto progressPath = root / "user-data" / "campaign-progress.dat";
    Check(SaveCampaignProgressAtomic(progressPath, campaign, progress, error), "atomically save progress under user data");
    auto loaded = EmptyCampaignProgress(campaign);
    Check(LoadCampaignProgress(progressPath, campaign, loaded, error), "load persisted progress");
    Check(loaded.missions.size() == progress.missions.size() &&
          loaded.missions[0].first == progress.missions[0].first &&
          loaded.missions[0].second.completed == progress.missions[0].second.completed &&
          loaded.missions[0].second.bestScore == progress.missions[0].second.bestScore &&
          loaded.missions[1].first == progress.missions[1].first &&
          loaded.missions[1].second.completed == progress.missions[1].second.completed &&
          loaded.missions[1].second.bestScore == progress.missions[1].second.bestScore,
          "progress round-trips completed flag and best scores");
    Check(SaveCampaignProgressAtomic(progressPath, campaign, loaded, error), "atomic replacement updates existing progress");
    Check(!LoadCampaignProgress(progressPath, CampaignDefinition{ "other", "Other", firstDir, campaign.missions }, loaded, error),
          "progress cannot be loaded for a different campaign id");

    const auto beforeCorruptWrite = Read(progressPath);
    Write(progressPath, "OPENREPUBLIC_CAMPAIGN_PROGRESS 1\ncampaign=first\ncoal|yes|01\n");
    auto unchanged = EmptyCampaignProgress(campaign);
    Check(!LoadCampaignProgress(progressPath, campaign, unchanged, error), "reject malformed/noncanonical progress values");
    Check(unchanged.missions.size() == campaign.missions.size() && !unchanged.missions[0].second.completed,
          "failed progress load leaves the output at its prior caller value");
    Write(progressPath, beforeCorruptWrite);

    Write(firstDir / "bad.txt",
          "OPENREPUBLIC_CAMPAIGN 1\nid=bad\ntitle=Bad\n"
          "mission=escape|Bad|../briefing.txt|scenario.json|objectives.json|1\n");
    CampaignDefinition sentinel{ "sentinel", "keep", {}, {} };
    Check(!LoadCampaign(firstDir / "bad.txt", sentinel, error), "reject path traversal in mission file references");
    Check(sentinel.id == "sentinel", "failed campaign load is transactional");
    Write(firstDir / "bad.txt",
          "OPENREPUBLIC_CAMPAIGN 1\nid=bad\ntitle=Bad\n"
          "mission=dup|One|briefing.txt|scenario.json|objectives.json|1\n"
          "mission=dup|Two|briefing.txt|scenario.json|objectives.json|2\n");
    Check(!LoadCampaign(firstDir / "bad.txt", sentinel, error), "reject duplicate mission IDs");
    Write(firstDir / "bad.txt",
          "OPENREPUBLIC_CAMPAIGN 1\nid=bad\ntitle=Bad\n"
          "mission=treasury|Bad|briefing.txt|scenario.json|objectives.json|1|18446744073709551615\n");
    Check(LoadCampaign(firstDir / "bad.txt", sentinel, error) &&
          sentinel.missions[0].startingTreasury == UINT64_MAX,
          "parse maximum uint64 starting treasury without narrowing");
    Write(firstDir / "bad.txt",
          "OPENREPUBLIC_CAMPAIGN 1\nid=bad\ntitle=Bad\n"
          "mission=treasury|Bad|briefing.txt|scenario.json|objectives.json|1|18446744073709551616\n");
    Check(!LoadCampaign(firstDir / "bad.txt", sentinel, error), "reject starting treasury overflow");

    std::error_code ec;
    fs::remove_all(root, ec);
    Check(!ec, "remove test-owned campaign and user-data files");
    return failures ? 1 : 0;
}
