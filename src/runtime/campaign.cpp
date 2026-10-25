#include "openrepublic/runtime/campaign.h"

#include "openrepublic/runtime/checkpoint_file.h"
#include "openrepublic/runtime/save_slots.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <set>
#include <string_view>

namespace openrepublic::runtime {
namespace {
constexpr std::string_view kCampaignMagic = "OPENREPUBLIC_CAMPAIGN 1";
constexpr std::string_view kProgressMagic = "OPENREPUBLIC_CAMPAIGN_PROGRESS 1\n";
constexpr std::size_t kMaxManifestBytes = 64 * 1024;
constexpr std::size_t kMaxProgressBytes = 16 * 1024;
constexpr std::size_t kMaxMissions = 128;

std::string_view Trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) text.remove_suffix(1);
    return text;
}

bool SafeId(std::string_view id) {
    if (id.empty() || id.size() > 48) return false;
    for (const unsigned char ch : id)
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '-' || ch == '_')) return false;
    return id != "." && id != "..";
}

bool SafeFilename(std::string_view filename) {
    if (filename.empty() || filename.size() > 128 || filename == "." || filename == "..") return false;
    for (const unsigned char ch : filename)
        if (ch < 0x20 || ch == 0x7f || ch == '/' || ch == '\\' || ch == ':' || ch == '|') return false;
    return true;
}

bool ReadBounded(const std::filesystem::path& path, std::size_t limit, std::string& bytes, std::string& error) {
    std::error_code ec;
    if (std::filesystem::is_symlink(path, ec) || ec) {
        error = ec ? "could not inspect file: " + ec.message() : "symbolic links are not accepted";
        return false;
    }
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size == 0 || size > limit) {
        error = ec ? "could not inspect file: " + ec.message() : "file is empty or exceeds its size limit";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) { error = "could not open file"; return false; }
    bytes.assign(std::istreambuf_iterator<char>(input), {});
    if (input.bad() || bytes.size() != size) { error = "file could not be read completely"; return false; }
    return true;
}

bool ParseUnsigned(std::string_view text, std::uint64_t& value) {
    if (text.empty() || (text.size() > 1 && text.front() == '0')) return false;
    std::uint64_t parsed{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) return false;
    value = parsed;
    return true;
}

bool SplitMission(std::string_view line, CampaignMission& mission) {
    std::string_view fields[7];
    for (std::size_t i = 0; i < 5; ++i) {
        const auto separator = line.find('|');
        if (separator == std::string_view::npos) return false;
        fields[i] = Trim(line.substr(0, separator));
        line.remove_prefix(separator + 1);
    }
    const auto treasurySeparator = line.find('|');
    fields[5] = Trim(line.substr(0, treasurySeparator));
    fields[6] = treasurySeparator == std::string_view::npos ? std::string_view{} : Trim(line.substr(treasurySeparator + 1));
    if (fields[6].find('|') != std::string_view::npos) return false;
    std::uint64_t par{}, startingTreasury{};
    if (!SafeId(fields[0]) || fields[1].empty() || fields[1].size() > 160 ||
        !SafeFilename(fields[2]) || !SafeFilename(fields[3]) || !SafeFilename(fields[4]) ||
        !ParseUnsigned(fields[5], par) ||
        (treasurySeparator != std::string_view::npos && !ParseUnsigned(fields[6], startingTreasury))) return false;
    mission = { std::string(fields[0]), std::string(fields[1]), std::string(fields[2]),
                std::string(fields[3]), std::string(fields[4]), par, startingTreasury };
    return true;
}

const CampaignMission* FindMission(const CampaignDefinition& campaign, std::string_view id) {
    const auto it = std::find_if(campaign.missions.begin(), campaign.missions.end(),
        [id](const auto& mission) { return mission.id == id; });
    return it == campaign.missions.end() ? nullptr : &*it;
}

bool ValidProgress(const CampaignDefinition& campaign, const CampaignProgress& progress) {
    if (progress.campaignId != campaign.id || progress.missions.size() != campaign.missions.size()) return false;
    for (std::size_t i = 0; i < campaign.missions.size(); ++i)
        if (progress.missions[i].first != campaign.missions[i].id) return false;
    return true;
}
}

bool LoadCampaign(const std::filesystem::path& manifestPath,
                  CampaignDefinition& campaign, std::string& error) {
    error.clear();
    std::string bytes;
    if (!ReadBounded(manifestPath, kMaxManifestBytes, bytes, error)) return false;
    std::istringstream input(bytes);
    std::string line;
    if (!std::getline(input, line) || Trim(line) != kCampaignMagic) {
        error = "campaign manifest has an unsupported header";
        return false;
    }
    CampaignDefinition parsed;
    parsed.directory = manifestPath.parent_path();
    bool sawId = false, sawTitle = false;
    std::set<std::string> missionIds;
    std::size_t lineNumber = 1;
    while (std::getline(input, line)) {
        ++lineNumber;
        const auto content = Trim(line);
        if (content.empty() || content.front() == '#') continue;
        if (content.rfind("id=", 0) == 0) {
            if (sawId || !SafeId(content.substr(3))) { error = "invalid or duplicate campaign id at line " + std::to_string(lineNumber); return false; }
            parsed.id = std::string(content.substr(3)); sawId = true;
        } else if (content.rfind("title=", 0) == 0) {
            const auto title = Trim(content.substr(6));
            if (sawTitle || title.empty() || title.size() > 160 || title.find('|') != std::string_view::npos) {
                error = "invalid or duplicate campaign title at line " + std::to_string(lineNumber); return false;
            }
            parsed.title = std::string(title); sawTitle = true;
        } else if (content.rfind("mission=", 0) == 0) {
            if (parsed.missions.size() >= kMaxMissions) { error = "campaign exceeds the mission limit"; return false; }
            CampaignMission mission;
            if (!SplitMission(content.substr(8), mission) || !missionIds.insert(mission.id).second) {
                error = "invalid or duplicate mission at line " + std::to_string(lineNumber); return false;
            }
            parsed.missions.push_back(std::move(mission));
        } else {
            error = "unknown campaign manifest field at line " + std::to_string(lineNumber);
            return false;
        }
    }
    if (!sawId || !sawTitle || parsed.missions.empty()) { error = "campaign must include id, title, and at least one mission"; return false; }
    for (const auto& mission : parsed.missions) {
        for (const auto* filename : { &mission.briefingFile, &mission.scenarioFile, &mission.objectivesFile }) {
            const auto path = parsed.directory / *filename;
            std::error_code ec;
            if (std::filesystem::is_symlink(path, ec) || ec || !std::filesystem::is_regular_file(path, ec) || ec) {
                error = "campaign mission file is missing, not regular, or a symbolic link: " + *filename;
                return false;
            }
        }
    }
    campaign = std::move(parsed);
    return true;
}

bool ListCampaigns(const std::filesystem::path& campaignRoot,
                   std::vector<std::string>& ids, std::string& error) {
    error.clear();
    std::error_code ec;
    if (!std::filesystem::exists(campaignRoot, ec)) {
        if (ec) { error = "could not inspect campaign directory: " + ec.message(); return false; }
        ids.clear(); return true;
    }
    std::vector<std::string> result;
    std::size_t scanned = 0;
    for (std::filesystem::directory_iterator it(campaignRoot, ec), end; !ec && it != end; it.increment(ec)) {
        if (++scanned > 256) { error = "campaign directory has too many entries"; return false; }
        if (it->is_symlink(ec) || ec) { ec.clear(); continue; }
        if (!it->is_directory(ec) || ec) { ec.clear(); continue; }
        CampaignDefinition campaign;
        std::string loadError;
        if (LoadCampaign(it->path() / "campaign.txt", campaign, loadError)) result.push_back(campaign.id);
    }
    if (ec) { error = "could not enumerate campaign directory: " + ec.message(); return false; }
    std::sort(result.begin(), result.end());
    ids = std::move(result);
    return true;
}

CampaignProgress EmptyCampaignProgress(const CampaignDefinition& campaign) {
    CampaignProgress result;
    result.campaignId = campaign.id;
    for (const auto& mission : campaign.missions) result.missions.emplace_back(mission.id, CampaignMissionProgress{});
    return result;
}

bool LoadCampaignProgress(const std::filesystem::path& path, const CampaignDefinition& campaign,
                          CampaignProgress& progress, std::string& error) {
    error.clear();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        if (ec) { error = "could not inspect campaign progress: " + ec.message(); return false; }
        progress = EmptyCampaignProgress(campaign);
        return true;
    }
    std::string bytes;
    if (!ReadBounded(path, kMaxProgressBytes, bytes, error)) return false;
    if (bytes.rfind(kProgressMagic, 0) != 0) { error = "campaign progress has an unsupported header"; return false; }
    std::istringstream input(bytes.substr(kProgressMagic.size()));
    std::string line;
    if (!std::getline(input, line) || line.rfind("campaign=", 0) != 0 || line.substr(9) != campaign.id) {
        error = "campaign progress belongs to a different campaign"; return false;
    }
    auto parsed = EmptyCampaignProgress(campaign);
    std::set<std::string> seen;
    while (std::getline(input, line)) {
        const auto separator = line.find('|');
        const auto scoreSeparator = separator == std::string::npos ? std::string::npos : line.find('|', separator + 1);
        if (separator == std::string::npos || scoreSeparator == std::string::npos ||
            line.find('|', scoreSeparator + 1) != std::string::npos) { error = "malformed campaign progress record"; return false; }
        const std::string id = line.substr(0, separator);
        const auto* mission = FindMission(campaign, id);
        if (!mission || !seen.insert(id).second) { error = "unknown or duplicate mission in campaign progress"; return false; }
        const auto completedText = std::string_view(line).substr(separator + 1, scoreSeparator - separator - 1);
        std::uint64_t score{};
        if ((completedText != "0" && completedText != "1") || !ParseUnsigned(std::string_view(line).substr(scoreSeparator + 1), score)) {
            error = "invalid campaign progress value"; return false;
        }
        const auto index = static_cast<std::size_t>(mission - campaign.missions.data());
        parsed.missions[index].second = { completedText == "1", score };
    }
    if (seen.size() != campaign.missions.size()) { error = "campaign progress is missing mission records"; return false; }
    progress = std::move(parsed);
    return true;
}

bool SaveCampaignProgressAtomic(const std::filesystem::path& path, const CampaignDefinition& campaign,
                                const CampaignProgress& progress, std::string& error) {
    error.clear();
    if (path.empty() || !ValidProgress(campaign, progress)) { error = "campaign progress does not match its definition"; return false; }
    std::string bytes(kProgressMagic);
    bytes += "campaign=" + campaign.id + "\n";
    for (const auto& item : progress.missions)
        bytes += item.first + "|" + (item.second.completed ? "1" : "0") + "|" + std::to_string(item.second.bestScore) + "\n";
    if (bytes.size() > kMaxProgressBytes) { error = "campaign progress exceeds the size limit"; return false; }
    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) { error = "could not create user-data directory: " + ec.message(); return false; }
    }
    const auto progressStatus = std::filesystem::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory) ec.clear();
    else if (ec) { error = "could not inspect campaign progress path: " + ec.message(); return false; }
    if (!ec && std::filesystem::is_symlink(progressStatus)) {
        error = "campaign progress path cannot be a symbolic link";
        return false;
    }
    return ReplaceFileAtomically(path, bytes, kMaxProgressBytes, error);
}

bool RecordCampaignCompletion(const CampaignDefinition& campaign, CampaignProgress& progress,
                              std::string_view missionId, std::uint64_t score) {
    if (!ValidProgress(campaign, progress)) return false;
    const auto* mission = FindMission(campaign, missionId);
    if (!mission) return false;
    const auto index = static_cast<std::size_t>(mission - campaign.missions.data());
    auto& record = progress.missions[index].second;
    record.completed = true;
    record.bestScore = std::max(record.bestScore, score);
    return true;
}

std::filesystem::path DefaultCampaignProgressPath() {
    return DefaultSaveDirectory() / "campaign-progress.dat";
}

} // namespace openrepublic::runtime
