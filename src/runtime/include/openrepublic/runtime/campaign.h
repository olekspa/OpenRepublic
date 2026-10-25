#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace openrepublic::runtime {

struct CampaignMission {
    std::string id;
    std::string title;
    std::string briefingFile;
    std::string scenarioFile;
    std::string objectivesFile;
    std::uint64_t parScore{};
    std::uint64_t startingTreasury{};
};

struct CampaignDefinition {
    std::string id;
    std::string title;
    std::filesystem::path directory;
    std::vector<CampaignMission> missions;
};

struct CampaignMissionProgress {
    bool completed{};
    std::uint64_t bestScore{};
};

struct CampaignProgress {
    std::string campaignId;
    std::vector<std::pair<std::string, CampaignMissionProgress>> missions;
};

// Loads a bounded UTF-8 text manifest named campaign.txt. Its first line is
// "OPENREPUBLIC_CAMPAIGN 1", followed by id=, title= and mission= records.
// A mission record is id|title|briefing-file|scenario-file|objectives-file|par-score
// with an optional |starting-treasury field (uint64, defaults to zero).
bool LoadCampaign(const std::filesystem::path& manifestPath,
                  CampaignDefinition& campaign, std::string& error);
bool ListCampaigns(const std::filesystem::path& campaignRoot,
                   std::vector<std::string>& ids, std::string& error);

CampaignProgress EmptyCampaignProgress(const CampaignDefinition& campaign);
bool LoadCampaignProgress(const std::filesystem::path& path,
                          const CampaignDefinition& campaign,
                          CampaignProgress& progress, std::string& error);
bool SaveCampaignProgressAtomic(const std::filesystem::path& path,
                                const CampaignDefinition& campaign,
                                const CampaignProgress& progress,
                                std::string& error);
bool RecordCampaignCompletion(const CampaignDefinition& campaign,
                              CampaignProgress& progress,
                              std::string_view missionId,
                              std::uint64_t score);
std::filesystem::path DefaultCampaignProgressPath(std::string_view campaignId);

} // namespace openrepublic::runtime
