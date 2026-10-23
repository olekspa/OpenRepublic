// Opt-in read-only real-region workload. Paths are supplied by the caller and
// are never printed or copied; only OpenRepublic reports and timings are emitted.
#include "openrepublic/compat/catalog_import.h"
#include "openrepublic/compat/region_import.h"
#include "openrepublic/compat/region_roads.h"
#include "openrepublic/content/map_placement.h"
#include "openrepublic/economy_api/economy_api.h"

#ifdef OPENREPUBLIC_ECONOMY_HOST_AVAILABLE
#include "economy_protocol.h"
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {
using namespace openrepublic;
using Clock = std::chrono::steady_clock;

bool ReadFile(const std::filesystem::path& path, std::string& bytes) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return false;
    bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return input.good() || input.eof();
}

bool LoadCatalog(const std::filesystem::path& contentRoot, economy::Catalog& catalog,
                 std::size_t& parsedDefinitions, compat::ImportReport& report, std::string& error) {
    std::map<std::string, content::BuildingDefinition> definitions;
    std::error_code ec;
    const auto directory = contentRoot / "buildings_types";
    for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->path().extension() != ".ini")
            continue;
        std::string bytes;
        content::BuildingDefinition definition;
        if (ReadFile(it->path(), bytes) && content::ParseBuildingDefinition(bytes, definition, nullptr))
            definitions[it->path().stem().string()] = std::move(definition);
    }
    if (ec || definitions.empty()) {
        error = "no parseable building definitions were found under the content root";
        return false;
    }
    parsedDefinitions = definitions.size();
    if (!compat::ImportCatalogFromDefinitions(definitions, {}, catalog, report)) {
        error = "the parsed building definitions did not produce a compatible economy catalog";
        return false;
    }
    return true;
}

std::string BaseTypeName(const std::string& name) {
    constexpr char prefix[] = "MIRRORZ_";
    return name.compare(0, sizeof(prefix) - 1, prefix) == 0
        ? name.substr(sizeof(prefix) - 1) : name;
}

std::size_t SampleCenter(const std::vector<content::PlacedBuilding>& placements,
                         const economy::Catalog& catalog, double radius) {
    if (placements.empty())
        return 0;
    const auto candidateCount = std::min<std::size_t>(placements.size(), 256);
    std::size_t best = 0;
    std::size_t bestCount = 0;
    for (std::size_t sample = 0; sample < candidateCount; ++sample) {
        const std::size_t index = candidateCount == 1 ? 0 :
            sample * (placements.size() - 1) / (candidateCount - 1);
        const auto& center = placements[index];
        std::size_t count = 0;
        for (const auto& candidate : placements) {
            economy::BuildingTypeId type{};
            if (!catalog.FindBuildingType(BaseTypeName(candidate.typeName), type))
                continue;
            const double dx = candidate.X() - center.X();
            const double dz = candidate.Z() - center.Z();
            count += dx * dx + dz * dz <= radius * radius;
        }
        if (count > bestCount) {
            best = index;
            bestCount = count;
        }
    }
    return best;
}

double Milliseconds(Clock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}

void PrintUsage() {
    std::cerr << "Usage: OpenRepublicEconomyScaleBench <content-root> <map-directory> [radius-meters]\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) {
        PrintUsage();
        return 2;
    }
    const std::filesystem::path contentRoot = argv[1];
    const std::filesystem::path mapDirectory = argv[2];
    const double radius = argc == 4 ? std::stod(argv[3]) : 1000.0;
    if (!std::isfinite(radius) || radius <= 0.0) {
        PrintUsage();
        return 2;
    }

    const auto importStart = Clock::now();
    economy::Catalog catalog;
    std::size_t definitionCount = 0;
    compat::ImportReport catalogReport;
    std::string error;
    const auto catalogStart = Clock::now();
    if (!LoadCatalog(contentRoot, catalog, definitionCount, catalogReport, error)) {
        std::cerr << "Catalog load failed: " << error << '\n';
        return 1;
    }
    const auto catalogDuration = Clock::now() - catalogStart;

    const auto placementStart = Clock::now();
    std::vector<content::PlacedBuilding> placements;
    if (!content::LoadMapPlacements(mapDirectory, placements, &error) || placements.empty()) {
        std::cerr << "Placement load failed: " << (error.empty() ? "no placements" : error) << '\n';
        return 1;
    }
    const auto placementDuration = Clock::now() - placementStart;
    const auto resolvablePlacements = static_cast<std::size_t>(std::count_if(
        placements.begin(), placements.end(), [&catalog](const auto& placement) {
            economy::BuildingTypeId type{};
            return catalog.FindBuildingType(BaseTypeName(placement.typeName), type);
        }));
    const auto centerStart = Clock::now();
    const std::size_t center = SampleCenter(placements, catalog, radius);
    const auto centerDuration = Clock::now() - centerStart;

    std::vector<compat::PlacedBuildingRef> refs;
    refs.reserve(placements.size());
    for (const auto& placement : placements)
        refs.push_back({placement.typeName, placement.X(), placement.Z(), std::nullopt,
                        placement.sourceBuildingTableIndex});

    compat::RegionImportOptions regionOptions;
    regionOptions.radiusMeters = radius;
    std::unique_ptr<economy::World> world;
    compat::RegionImportReport imported;
    const auto regionImportStart = Clock::now();
    if (!compat::ImportRegionIntoWorld(refs, center, catalog, regionOptions, world, imported)) {
        std::cerr << "Region import failed: " << compat::DescribeRegionImport(imported) << '\n';
        return 1;
    }
    const auto regionImportDuration = Clock::now() - regionImportStart;

    const auto roadImportStart = Clock::now();
    std::string roadBytes;
    content::RoadNetwork roadNetwork;
    std::vector<compat::RoadLinkRef> links;
    if (ReadFile(mapDirectory / "road.bin", roadBytes) &&
        content::ExtractRoadNetwork(roadBytes, roadNetwork, &error)) {
        for (std::size_t i = 0; i < roadNetwork.nodes.size(); ++i) {
            const auto& node = roadNetwork.nodes[i];
            for (const auto neighbor : node.neighbors) {
                if (neighbor > i && neighbor < roadNetwork.nodes.size()) {
                    const auto& other = roadNetwork.nodes[neighbor];
                    links.push_back({node.x, node.z, other.x, other.z});
                }
            }
        }
    }
    compat::RegionRoadReport roadReport;
    compat::RegionRoadOptions roadOptions;
    roadOptions.maxConnectorCells = 120;
    if (!compat::ImportRoadsIntoWorld(*world, imported, regionOptions, links, roadOptions, roadReport)) {
        std::cerr << "Road import failed: " << compat::DescribeRegionRoads(roadReport) << '\n';
        return 1;
    }
    const auto roadImportDuration = Clock::now() - roadImportStart;
    const auto importDuration = Clock::now() - importStart;

    economy_api::EconomyApi api(*world, {true, true});
    const std::string beforeHash = std::to_string(world->Snapshot().stateHash);
    const auto tickStart = Clock::now();
    for (int i = 0; i < 10; ++i) {
        if (api.Step() != economy_api::ErrorCode::None) {
            std::cerr << "Economy API tick failed\n";
            return 1;
        }
    }
    const auto tickDuration = Clock::now() - tickStart;

    std::cout << "catalog_definitions=" << definitionCount
              << " catalog_types=" << catalog.BuildingTypes().size()
              << " catalog_resources=" << catalog.Resources().size()
              << " catalog_imported_definitions=" << catalogReport.buildingsImported
              << " source_placements=" << placements.size()
              << " resolvable_placements=" << resolvablePlacements
              << " selected_center_index=" << center
              << " requested_radius_m=" << radius
              << " imported_buildings=" << imported.placed
              << " skipped_unknown=" << imported.skippedUnknownType
              << " grid=" << imported.gridWidth << 'x' << imported.gridDepth
              << " road_links=" << roadReport.linksConsidered
              << " roads_accepted=" << roadReport.segmentsAccepted
              << " import_ms=" << Milliseconds(importDuration)
              << " catalog_ms=" << Milliseconds(catalogDuration)
              << " placements_ms=" << Milliseconds(placementDuration)
              << " center_ms=" << Milliseconds(centerDuration)
              << " region_ms=" << Milliseconds(regionImportDuration)
              << " roads_ms=" << Milliseconds(roadImportDuration)
              << " api_tick_ms=" << Milliseconds(tickDuration) / 10.0
              << " hash_before_ticks=" << beforeHash
              << " hash_after_ticks=" << world->Snapshot().stateHash << '\n';

#ifdef OPENREPUBLIC_ECONOMY_HOST_AVAILABLE
    economy_host::Dispatcher host(economy_host::Role::Controller, api, false);
    const auto queryHashBefore = world->Snapshot().stateHash;
    const auto fullStart = Clock::now();
    const auto fullResponse = host.HandleLine("{\"id\":\"world-full\",\"protocolVersion\":0,\"method\":\"world\"}");
    const auto fullDuration = Clock::now() - fullStart;
    const auto revision = std::to_string(world->Revision());
    const std::string pageRequest = "{\"id\":\"world-page\",\"protocolVersion\":0,\"method\":\"world\",\"revision\":\"" +
        revision + "\",\"kind\":\"buildings\",\"offset\":0,\"limit\":32}";
    const auto pageStart = Clock::now();
    const auto pageResponse = host.HandleLine(pageRequest);
    const auto pageDuration = Clock::now() - pageStart;
    const auto adviseStart = Clock::now();
    const auto adviseResponse = host.HandleLine("{\"id\":\"advise\",\"protocolVersion\":0,\"method\":\"advise\"}");
    const auto adviseDuration = Clock::now() - adviseStart;
    const auto queryHashAfter = world->Snapshot().stateHash;
    const auto jsonTickStart = Clock::now();
    const auto tickResponse = host.HandleLine("{\"id\":\"tick\",\"protocolVersion\":0,\"method\":\"step\",\"ticks\":1}");
    const auto jsonTickDuration = Clock::now() - jsonTickStart;
    if (fullResponse.empty() || pageResponse.empty() || adviseResponse.empty() || tickResponse.empty()) {
        std::cerr << "Host dispatcher returned an empty response\n";
        return 1;
    }
    std::cout << "host_world_json_ms=" << Milliseconds(fullDuration)
              << " world_json_bytes=" << fullResponse.size()
              << " host_page_json_ms=" << Milliseconds(pageDuration)
              << " page_json_bytes=" << pageResponse.size()
              << " advise_ms=" << Milliseconds(adviseDuration)
              << " advise_json_bytes=" << adviseResponse.size()
              << " query_hash_before=" << queryHashBefore
              << " query_hash_after=" << queryHashAfter
              << " query_hashes_identical=" << (queryHashBefore == queryHashAfter ? "true" : "false")
              << " host_step_json_ms=" << Milliseconds(jsonTickDuration) << '\n';
#else
    const auto queryStart = Clock::now();
    const auto full = api.QueryWorld();
    const auto fullDuration = Clock::now() - queryStart;
    const auto pageStart = Clock::now();
    const auto page = api.QueryWorld(world->Revision(), "buildings", 0, economy_api::MaxEntityPageSize);
    const auto pageDuration = Clock::now() - pageStart;
    std::cout << "host_json=unavailable typed_world_ms=" << Milliseconds(fullDuration)
              << " typed_world_buildings=" << full.world.buildings.size()
              << " typed_page_ms=" << Milliseconds(pageDuration)
              << " typed_page_buildings=" << page.page.buildings.size() << '\n';
#endif
    return 0;
}
