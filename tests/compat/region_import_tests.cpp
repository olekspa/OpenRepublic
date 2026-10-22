#include "openrepublic/compat/catalog_import.h"
#include "openrepublic/compat/region_import.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace openrepublic;

namespace {

int failures = 0;

void Check(bool condition, const char *message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << "\n";
    }
}

content::BuildingDefinition Producer() {
    content::BuildingDefinition definition;
    definition.production.push_back({"goods", 1.0, 1, false});
    return definition;
}

economy::Catalog FootprintCatalog(std::uint32_t width, std::uint32_t depth) {
    economy::CatalogBuilder builder;
    economy::CatalogErrorInfo error;
    economy::BuildingTypeId type{};
    Check(builder.AddBuildingType("farm", width, depth, &type, error),
          "farm footprint type is valid");
    Check(builder.AddBuildingType("factory", width, depth, &type, error),
          "factory footprint type is valid");
    economy::Catalog catalog;
    Check(builder.Build(catalog, error), "synthetic footprint catalog builds");
    return catalog;
}

} // namespace

int main() {
    std::map<std::string, content::BuildingDefinition> definitions;
    definitions.emplace("farm", Producer());
    definitions.emplace("factory", Producer());
    economy::Catalog catalog;
    compat::ImportReport catalogReport;
    Check(compat::ImportCatalogFromDefinitions(definitions, {}, catalog, catalogReport),
          "synthetic definitions build a catalog");

    const std::vector<compat::PlacedBuildingRef> placements = {
        {"farm", 0.0, 0.0, 1001},
        {"MIRRORZ_farm", 20.0, 0.0, 1002},
        {"unknown", 24.0, 0.0, 1003},
        {"factory", 1.0, 0.0, 1004},
        {"farm", 1000.0, 0.0, 1005},
    };
    const compat::RegionImportOptions options{4.0, 50.0};
    std::unique_ptr<economy::World> firstWorld;
    std::unique_ptr<economy::World> secondWorld;
    compat::RegionImportReport firstReport;
    compat::RegionImportReport secondReport;
    Check(compat::ImportRegionIntoWorld(placements, 0, catalog, options, firstWorld, firstReport),
          "region import succeeds");
    Check(compat::ImportRegionIntoWorld(placements, 0, catalog, options, secondWorld, secondReport),
          "repeated region import succeeds");
    Check(firstWorld && secondWorld, "successful imports return worlds");
    Check(firstReport.considered == placements.size() && firstReport.placed == 2 &&
              firstReport.skippedUnknownType == 1 && firstReport.skippedOverlap == 1 &&
              firstReport.skippedAccessConflict == 0 && firstReport.skippedOutOfBounds == 1,
          "selected, unknown, overlapping, and out-of-radius counts are reported");
    Check(firstReport.originX == -16.0 && firstReport.originZ == -16.0 &&
              firstReport.gridWidth <= 512 && firstReport.gridDepth <= 512,
          "grid origin uses four-cell margin and dimensions are capped");
    Check(firstReport.notes.size() == 1 &&
              firstReport.notes[0].find("Roads are not part of this step") != std::string::npos,
          "report notes roads are omitted");
    Check(firstReport.placedBuildings.size() == 2 &&
              firstReport.placedBuildings[0].first == "farm" &&
              firstReport.placedBuildings[0].second == 1 &&
              firstReport.placedBuildings[1].first == "farm" &&
              firstReport.placedBuildings[1].second == 2,
          "source order, base mirrored type, and accepted building IDs are deterministic");
    Check(firstReport.placedSourceBuildings.size() == 2 &&
              firstReport.placedSourceBuildings[0].sourcePlacementKey == 1001 &&
              firstReport.placedSourceBuildings[0].typeName == "farm" &&
              firstReport.placedSourceBuildings[0].buildingId == 1 &&
              firstReport.placedSourceBuildings[1].sourcePlacementKey == 1002 &&
              firstReport.placedSourceBuildings[1].typeName == "farm" &&
              firstReport.placedSourceBuildings[1].buildingId == 2,
          "accepted source keys map exactly to economy IDs, including mirrored base type");
    Check(secondReport.placedSourceBuildings.size() == firstReport.placedSourceBuildings.size() &&
              secondReport.placedSourceBuildings[0].sourcePlacementKey ==
                  firstReport.placedSourceBuildings[0].sourcePlacementKey &&
              secondReport.placedSourceBuildings[0].buildingId ==
                  firstReport.placedSourceBuildings[0].buildingId &&
              secondReport.placedSourceBuildings[1].sourcePlacementKey ==
                  firstReport.placedSourceBuildings[1].sourcePlacementKey &&
              secondReport.placedSourceBuildings[1].buildingId ==
                  firstReport.placedSourceBuildings[1].buildingId,
          "source mapping is deterministic across repeated imports");
    if (firstWorld && secondWorld) {
        const auto firstSnapshot = firstWorld->Snapshot();
        const auto secondSnapshot = secondWorld->Snapshot();
        Check(firstSnapshot.stateHash == secondSnapshot.stateHash,
              "repeated import produces the same deterministic world hash");
        Check(firstSnapshot.gridWidth == firstReport.gridWidth &&
                  firstSnapshot.gridDepth == firstReport.gridDepth &&
                  firstSnapshot.buildings.size() == 2 &&
                  firstSnapshot.buildings[0].origin == economy::Cell{4, 4} &&
                  firstSnapshot.buildings[1].origin == economy::Cell{9, 4},
              "snapshot contains expected cells and dimensions");
        const auto *firstType = catalog.BuildingType(firstSnapshot.buildings[0].type);
        const auto *secondType = catalog.BuildingType(firstSnapshot.buildings[1].type);
        Check(firstType && secondType && firstType->name == "farm" && secondType->name == "farm",
              "world snapshot lists placed buildings by type");
    }
    const std::string description = compat::DescribeRegionImport(firstReport);
    Check(description.find("2 placed") != std::string::npos &&
              description.find("0 access conflict") != std::string::npos &&
              description.find("Roads are not part of this step") != std::string::npos,
          "description is a concise readable report");

    const std::vector<compat::PlacedBuildingRef> duplicateKeys = {
        {"farm", 0.0, 0.0, 77},
        {"unknown", 1000.0, 0.0, 77},
    };
    economy::World *const originalWorld = firstWorld.get();
    compat::RegionImportReport duplicateReport;
    Check(!compat::ImportRegionIntoWorld(duplicateKeys, 0, catalog, options, firstWorld,
                                         duplicateReport),
          "duplicate source keys reject import even when one placement would be skipped");
    Check(firstWorld.get() == originalWorld && duplicateReport.placedSourceBuildings.empty() &&
              duplicateReport.notes.size() == 1 &&
              duplicateReport.notes[0].find("duplicate source placement key") != std::string::npos,
          "duplicate-key failure is transactional and reports the reason");

    const std::vector<compat::PlacedBuildingRef> wideRegion = {
        {"farm", 0.0, 0.0},
        {"farm", 3000.0, 0.0},
    };
    std::unique_ptr<economy::World> cappedWorld;
    compat::RegionImportReport cappedReport;
    Check(compat::ImportRegionIntoWorld(wideRegion, 0, catalog, {4.0, 3000.0}, cappedWorld,
                                        cappedReport),
          "oversized requested radius imports after clamping");
    Check(cappedReport.gridWidth <= 512 && cappedReport.gridDepth <= 512 &&
              cappedReport.skippedOutOfBounds == 1 && cappedReport.notes.size() == 2 &&
              cappedReport.notes[0].find("radius reduced") != std::string::npos,
          "grid cap shrinks selection radius and records the adjustment");

    // A 3x3 earlier building occupies the later building's prospective access
    // cell while leaving the two footprints edge-adjacent and non-overlapping.
    const auto wideCatalog = FootprintCatalog(3, 3);
    const std::vector<compat::PlacedBuildingRef> accessInsideEarlierFootprint = {
        {"farm", 0.0, 0.0, 201},
        {"factory", 0.0, 12.0, 202},
    };
    std::unique_ptr<economy::World> accessWorld;
    compat::RegionImportReport accessReport;
    Check(compat::ImportRegionIntoWorld(accessInsideEarlierFootprint, 0, wideCatalog,
                                        {4.0, 50.0, 8}, accessWorld, accessReport),
          "later access-cell conflict does not abort region import");
    Check(accessReport.placed == 1 && accessReport.skippedOverlap == 0 &&
              accessReport.skippedAccessConflict == 1 &&
              accessReport.placedSourceBuildings.size() == 1 &&
              accessReport.placedSourceBuildings[0].sourcePlacementKey == 201,
          "prospective access inside an earlier footprint is counted separately and only accepted key maps");
    Check(accessReport.notes.size() <= 8 &&
              compat::DescribeRegionImport(accessReport).find("1 access conflict") !=
                  std::string::npos,
          "access conflict is visible in bounded notes and report summary");

    // With 1x1 footprints, the second building can occupy the first building's
    // access cell without colliding with the first footprint.
    const auto unitCatalog = FootprintCatalog(1, 1);
    const std::vector<compat::PlacedBuildingRef> laterFootprintCoversPriorAccess = {
        {"farm", 0.0, 0.0, 301},
        {"factory", 0.0, -4.0, 302},
    };
    std::unique_ptr<economy::World> reverseAccessWorld;
    compat::RegionImportReport reverseAccessReport;
    Check(compat::ImportRegionIntoWorld(laterFootprintCoversPriorAccess, 0, unitCatalog,
                                        {4.0, 50.0, 8}, reverseAccessWorld, reverseAccessReport),
          "later footprint covering earlier access does not abort region import");
    Check(reverseAccessReport.placed == 1 && reverseAccessReport.skippedOverlap == 0 &&
              reverseAccessReport.skippedAccessConflict == 1 &&
              reverseAccessReport.placedSourceBuildings.size() == 1 &&
              reverseAccessReport.placedSourceBuildings[0].sourcePlacementKey == 301,
          "later footprint covering prior access is classified separately and rejected key is unmapped");
    if (reverseAccessReport.placed != 1 || reverseAccessReport.skippedAccessConflict != 1)
        std::cerr << compat::DescribeRegionImport(reverseAccessReport) << '\n';

    // Repeating the same source row order must retain the same winner, mapping,
    // and economy IDs even when an access conflict is present.
    std::unique_ptr<economy::World> repeatedAccessWorld;
    compat::RegionImportReport repeatedAccessReport;
    Check(compat::ImportRegionIntoWorld(accessInsideEarlierFootprint, 0, wideCatalog,
                                        {4.0, 50.0, 8}, repeatedAccessWorld, repeatedAccessReport),
          "repeated access-conflict import succeeds");
    Check(accessWorld && repeatedAccessWorld &&
              accessWorld->Snapshot().stateHash == repeatedAccessWorld->Snapshot().stateHash &&
              accessReport.placedSourceBuildings.size() ==
                  repeatedAccessReport.placedSourceBuildings.size() &&
              accessReport.placedSourceBuildings[0].sourcePlacementKey ==
                  repeatedAccessReport.placedSourceBuildings[0].sourcePlacementKey &&
              accessReport.placedSourceBuildings[0].buildingId ==
                  repeatedAccessReport.placedSourceBuildings[0].buildingId &&
              accessReport.skippedAccessConflict == repeatedAccessReport.skippedAccessConflict,
          "same source row order yields deterministic accepted IDs and access-conflict reporting");

    // More conflicts than the configured note budget still produce exact
    // counters and a compact summary.
    std::vector<compat::PlacedBuildingRef> manyConflicts = {
        {"farm", 0.0, 0.0, 401},
    };
    for (std::uint64_t i = 0; i < 6; ++i)
        manyConflicts.push_back({"factory", 0.0, 12.0, 402 + i});
    std::unique_ptr<economy::World> boundedWorld;
    compat::RegionImportReport boundedReport;
    Check(compat::ImportRegionIntoWorld(manyConflicts, 0, wideCatalog, {4.0, 50.0, 2},
                                        boundedWorld, boundedReport),
          "many access conflicts import with bounded diagnostics");
    Check(boundedReport.skippedAccessConflict == 6 && boundedReport.notes.size() <= 2 &&
              compat::DescribeRegionImport(boundedReport).find("6 access conflict") !=
                  std::string::npos,
          "diagnostic notes respect maxDiagnosticNotes while counters remain exact");
    std::unique_ptr<economy::World> noNotesWorld;
    compat::RegionImportReport noNotesReport;
    Check(compat::ImportRegionIntoWorld(manyConflicts, 0, wideCatalog, {4.0, 50.0, 0},
                                        noNotesWorld, noNotesReport) && noNotesReport.notes.empty() &&
              noNotesReport.placed == 1 && noNotesReport.skippedAccessConflict == 6,
          "zero diagnostic budget preserves exact acceptance and conflict counts");

    // Duplicate source keys remain transactional even when the keyed row would
    // also be rejected for an access conflict.
    const std::vector<compat::PlacedBuildingRef> duplicateAccessKeys = {
        {"farm", 0.0, 0.0, 501},
        {"factory", 0.0, 12.0, 501},
    };
    economy::World *const beforeDuplicateAccess = accessWorld.get();
    compat::RegionImportReport duplicateAccessReport;
    Check(!compat::ImportRegionIntoWorld(duplicateAccessKeys, 0, wideCatalog,
                                         {4.0, 50.0, 2}, accessWorld, duplicateAccessReport),
          "duplicate key wins over otherwise skippable access conflict");
    Check(accessWorld.get() == beforeDuplicateAccess && duplicateAccessReport.placed == 0 &&
              duplicateAccessReport.placedSourceBuildings.empty() &&
              duplicateAccessReport.notes.size() <= 2,
          "duplicate access-key failure preserves prior world and bounded report");

    return failures == 0 ? 0 : 1;
}
