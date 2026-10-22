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
        {"farm", 0.0, 0.0},
        {"MIRRORZ_farm", 20.0, 0.0},
        {"unknown", 24.0, 0.0},
        {"factory", 1.0, 0.0},
        {"farm", 1000.0, 0.0},
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
              firstReport.skippedOutOfBounds == 1,
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
              description.find("Roads are not part of this step") != std::string::npos,
          "description is a concise readable report");

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

    return failures == 0 ? 0 : 1;
}
