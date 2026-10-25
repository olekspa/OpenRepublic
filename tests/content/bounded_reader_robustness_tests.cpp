#include "openrepublic/compat/catalog_import.h"
#include "openrepublic/content/building_definition.h"
#include "openrepublic/content/map_header.h"
#include "openrepublic/content/road_network.h"
#include "openrepublic/content/terrain_dds.h"
#include "building_record_walk.h"
#include "workers_record_walk.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace openrepublic;

namespace {
int failures = 0;
void Check(bool ok, const char* label) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}

void Exercise(const std::string& bytes) {
    content::RoadNetwork road;
    road.segmentCount = 0x1234;
    std::string error;
    const bool roadOk = content::ExtractRoadNetwork(bytes, road, &error);
    if (!roadOk) Check(road.segmentCount == 0x1234, "road failure is transactional");

    content::MapHeaderPrefix prefix;
    prefix.leadingValue = 0x1234;
    const bool prefixOk = content::ParseMapHeaderPrefix(bytes, prefix, &error);
    if (!prefixOk) Check(prefix.leadingValue == 0x1234, "map prefix failure is transactional");
    content::MapHeaderFixedBlock fixed;
    fixed.version = 0x1234;
    const bool fixedOk = content::ParseMapHeaderFixedBlock(bytes, fixed, &error);
    if (!fixedOk) Check(fixed.version == 0x1234, "map fixed block failure is transactional");

    std::vector<std::uint8_t> terrainBytes(bytes.begin(), bytes.end());
    content::Heightmap height;
    height.width = 0x1234;
    const bool heightOk = content::LoadLegacyR32F(terrainBytes, height, &error);
    if (!heightOk) Check(height.width == 0x1234, "heightmap failure is transactional");
    content::Rgba8Raster raster;
    raster.width = 0x1234;
    const bool rasterOk = content::LoadLegacyRgba8(terrainBytes, raster, &error);
    if (!rasterOk) Check(raster.width == 0x1234, "terrain raster failure is transactional");
    content::TerrainMetadata metadata;
    metadata.gridSize = 1234.0f;
    content::Heightmap metadataHeight;
    metadataHeight.width = 2;
    metadataHeight.height = 2;
    const bool metadataOk = content::ParseTerrainMetadata(bytes, metadataHeight, metadata, &error);
    if (!metadataOk) Check(metadata.gridSize == 1234.0f, "terrain metadata failure is transactional");

    content::BuildingDefinition definition;
    definition.ignoredLines = 0x1234;
    const bool definitionOk = content::ParseBuildingDefinition(bytes, definition, &error);
    if (!definitionOk) Check(definition.ignoredLines == 0x1234, "definition failure is transactional");

    std::vector<std::size_t> buildingOffsets{{0x1234}};
    const bool buildingOk = content::detail::WalkBuildingRecordOffsets(bytes, 124, buildingOffsets, &error);
    if (!buildingOk) Check(buildingOffsets == std::vector<std::size_t>{0x1234}, "building walk failure is transactional");
    std::vector<content::detail::WorkerRecordSpan> workerSpans{{0x1234, 0x5678}};
    const bool workersOk = content::detail::WalkWorkerRecordOffsets(bytes, workerSpans, &error);
    if (!workersOk) Check(workerSpans.size() == 1 && workerSpans[0].offset == 0x1234,
                          "worker walk failure is transactional");
}

void TestDeterministicMutationCorpus() {
    // Exhaust every truncation length of a bounded header-like buffer, then
    // flip every bit in deterministic short buffers. These are crash/transaction
    // probes; acceptance of a byte sequence does not assign it game semantics.
    std::string seed(320, '\0');
    for (std::size_t length = 0; length <= seed.size(); ++length)
        Exercise(seed.substr(0, length));
    for (std::size_t position = 0; position < 128; ++position) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            std::string mutated = seed.substr(0, 128);
            mutated[position] = static_cast<char>(1u << bit);
            Exercise(mutated);
        }
    }
    // Oversized-count patterns must reject promptly and preserve caller state.
    for (const auto count : {0x7fffffffu, 0xffffffffu}) {
        std::string oversized(32, '\0');
        for (unsigned i = 0; i < 4; ++i) oversized[i] = static_cast<char>(count >> (8u * i));
        Exercise(oversized);
    }
}

void TestCatalogImportTransaction() {
    economy::Catalog output;
    compat::ImportReport report;
    report.definitionsSeen = 0x1234;
    compat::CatalogImportOptions invalid;
    invalid.ticksPerDay = 0;
    Check(!compat::ImportCatalogFromDefinitions({}, invalid, output, report),
          "invalid catalog import options reject");
    Check(!report.success && report.error.find("ticksPerDay") != std::string::npos,
          "catalog importer returns a bounded diagnostic for invalid options");
}
} // namespace

int main() {
    TestDeterministicMutationCorpus();
    TestCatalogImportTransaction();
    return failures == 0 ? 0 : 1;
}
