#include "economy_protocol.h"

#include "openrepublic/compat/catalog_import.h"
#include "openrepublic/compat/region_import.h"
#include "openrepublic/compat/region_roads.h"
#include "openrepublic/content/map_placement.h"
#include "openrepublic/content/road_network.h"

#ifdef OPENREPUBLIC_HAVE_LUA_RULES
#include "openrepublic/scripting/economy_rules.h"
#endif

#include <fstream>
#include <iostream>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {
using namespace openrepublic;

enum class LineStatus { Line, End, TooLong };

LineStatus ReadBoundedLine(std::istream &input, std::string &line) {
  line.clear();
  bool sawByte = false;
  char ch{};
  while (input.get(ch)) {
    sawByte = true;
    if (ch == '\n')
      return LineStatus::Line;
    if (line.size() == economy_host::MaxRequestLineBytes)
      return LineStatus::TooLong;
    line.push_back(ch);
  }
  return sawByte ? LineStatus::Line : LineStatus::End;
}

void PrintUsage() {
  std::cerr << "Usage: OpenRepublicEconomyHost [--role observer|controller] [--catalog file]\n"
            << "       [--rules <economy_rules.lua>] [--region <map dir> --content-root <dir> [--center N] [--radius M]]\n"
            << "Reads protocolVersion 0 JSONL requests from stdin and writes responses to stdout.\n"
            << "--region imports a local WRSR map package's placements and roads into the world\n"
            << "(the catalog is built from <content-root>/buildings_types; local content only).\n";
}

bool ReadFile(const std::filesystem::path &path, std::string &text) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return false;
  std::ostringstream buffer;
  buffer << input.rdbuf();
  text = buffer.str();
  return true;
}

// Applies a sandboxed Lua tuning bundle to the catalog (no-op without a rules path).
bool ApplyRules(const std::filesystem::path &rulesPath, economy::Catalog &catalog, std::string &error) {
  if (rulesPath.empty())
    return true;
#ifdef OPENREPUBLIC_HAVE_LUA_RULES
  economy::Catalog tuned;
  if (!scripting::EconomyRules::ApplyFile(rulesPath, catalog, tuned, &error))
    return false;
  catalog = std::move(tuned);
  return true;
#else
  error = "this build has no Lua support";
  return false;
#endif
}

// Imports a map package region into a new world. All outputs stay owned by the caller.
bool BuildRegionWorld(const std::filesystem::path &contentRoot, const std::filesystem::path &mapDir,
                      long centerIndex, double radius, const std::filesystem::path &rulesPath,
                      economy::Catalog &catalog,
                      std::unique_ptr<economy::World> &world, std::string &description,
                      std::string &regionInfoJson, std::string &error) {
  std::map<std::string, content::BuildingDefinition> definitions;
  std::error_code ec;
  for (const auto &entry : std::filesystem::directory_iterator(contentRoot / "buildings_types", ec)) {
    if (entry.path().extension() != ".ini")
      continue;
    std::string text;
    content::BuildingDefinition definition;
    if (ReadFile(entry.path(), text) && content::ParseBuildingDefinition(text, definition, nullptr))
      definitions[entry.path().stem().string()] = std::move(definition);
  }
  compat::ImportReport catalogReport;
  if (definitions.empty() ||
      !compat::ImportCatalogFromDefinitions(definitions, {}, catalog, catalogReport)) {
    error = "could not build a catalog from <content-root>/buildings_types";
    return false;
  }
  if (!ApplyRules(rulesPath, catalog, error))
    return false;
  std::vector<content::PlacedBuilding> placements;
  if (!content::LoadMapPlacements(mapDir, placements, &error))
    return false;
  std::vector<compat::RoadLinkRef> links;
  std::string roadBytes;
  content::RoadNetwork roads;
  if (ReadFile(mapDir / "road.bin", roadBytes) && content::ExtractRoadNetwork(roadBytes, roads, nullptr)) {
    for (std::size_t i = 0; i < roads.nodes.size(); ++i) {
      for (const std::uint32_t j : roads.nodes[i].neighbors) {
        if (j > i && j < roads.nodes.size())
          links.push_back({roads.nodes[i].x, roads.nodes[i].z, roads.nodes[j].x, roads.nodes[j].z});
      }
    }
  }
  const std::size_t center = centerIndex >= 0 ? static_cast<std::size_t>(centerIndex)
                                              : content::FindDensestPlacement(placements, radius);
  std::vector<compat::PlacedBuildingRef> refs;
  for (const auto &placed : placements)
    refs.push_back({placed.typeName, placed.X(), placed.Z()});
  compat::RegionImportOptions options;
  options.radiusMeters = radius;
  compat::RegionImportReport imported;
  if (!compat::ImportRegionIntoWorld(refs, center, catalog, options, world, imported)) {
    error = "region import failed: " + compat::DescribeRegionImport(imported);
    return false;
  }
  compat::RegionRoadOptions roadOptions;
  roadOptions.maxConnectorCells = 120;
  compat::RegionRoadReport roadReport;
  if (!compat::ImportRoadsIntoWorld(*world, imported, options, links, roadOptions, roadReport)) {
    error = "road import failed";
    return false;
  }
  std::ostringstream summary;
  summary << "Imported map region (local WRSR data, experimental): centre placement " << center
          << " of " << placements.size() << ", radius " << radius << " m, cell size "
          << options.cellSizeMeters << " m. Cell (cx,cz) covers map x " << imported.originX
          << " + 4*cx .. +4, z " << imported.originZ << " + 4*cz .. +4.\n"
          << compat::DescribeRegionImport(imported) << "\n" << compat::DescribeRegionRoads(roadReport);
  description = summary.str();
  std::ostringstream info;
  info.precision(17);
  info << "{\"originX\":" << imported.originX << ",\"originZ\":" << imported.originZ
       << ",\"cellSizeMeters\":" << options.cellSizeMeters << ",\"gridWidth\":" << imported.gridWidth
       << ",\"gridDepth\":" << imported.gridDepth << ",\"centerPlacement\":" << center
       << ",\"radiusMeters\":" << radius << "}";
  regionInfoJson = info.str();
  return true;
}
} // namespace

int main(int argc, char **argv) {
  economy_host::Role role = economy_host::Role::Observer;
  std::string catalogPath;
  std::filesystem::path regionDir;
  std::filesystem::path contentRoot;
  std::filesystem::path rulesPath;
  long centerIndex = -1;
  double radius = 400.0;
  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);
    if (arg == "--role" && i + 1 < argc) {
      const std::string value(argv[++i]);
      if (value == "observer")
        role = economy_host::Role::Observer;
      else if (value == "controller")
        role = economy_host::Role::Controller;
      else {
        std::cerr << "Invalid role: " << value << "\n";
        PrintUsage();
        return 2;
      }
    } else if (arg == "--catalog" && i + 1 < argc) {
      catalogPath = argv[++i];
    } else if (arg == "--region" && i + 1 < argc) {
      regionDir = argv[++i];
    } else if (arg == "--content-root" && i + 1 < argc) {
      contentRoot = argv[++i];
    } else if (arg == "--rules" && i + 1 < argc) {
      rulesPath = argv[++i];
    } else if (arg == "--center" && i + 1 < argc) {
      centerIndex = std::strtol(argv[++i], nullptr, 10);
    } else if (arg == "--radius" && i + 1 < argc) {
      radius = std::strtod(argv[++i], nullptr);
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return 0;
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      PrintUsage();
      return 2;
    }
  }

  economy::Catalog catalog = economy::MakeSyntheticCatalog();
  if (!catalogPath.empty()) {
    std::ifstream input(catalogPath, std::ios::binary);
    if (!input) {
      std::cerr << "Could not open catalog file.\n";
      return 2;
    }
    const std::string text((std::istreambuf_iterator<char>(input)), {});
    economy::Catalog parsed;
    economy::ParseError error;
    if (!economy::ParseCatalogText(text, parsed, error)) {
      std::cerr << "Catalog parse error at line " << error.line << ": " << error.message << "\n";
      return 2;
    }
    catalog = std::move(parsed);
  }

  if (regionDir.empty()) {
    std::string rulesError;
    if (!ApplyRules(rulesPath, catalog, rulesError)) {
      std::cerr << "Lua rules rejected: " << rulesError << "\n";
      return 2;
    }
  }
  std::unique_ptr<economy::World> regionWorld;
  std::string regionDescription;
  std::string regionInfoJson;
  if (!regionDir.empty()) {
    if (contentRoot.empty() || !catalogPath.empty() || !(radius >= 10.0 && radius <= 1000.0)) {
      std::cerr << "--region needs --content-root, excludes --catalog, and a radius of 10..1000 m.\n";
      return 2;
    }
    std::string error;
    if (!BuildRegionWorld(contentRoot, regionDir, centerIndex, radius, rulesPath, catalog, regionWorld,
                          regionDescription, regionInfoJson, error)) {
      std::cerr << "Region import failed: " << error << "\n";
      return 2;
    }
  }
  economy_host::Dispatcher dispatcher = regionWorld ? economy_host::Dispatcher(role, *regionWorld)
                                                    : economy_host::Dispatcher(role, catalog);
  if (regionWorld)
    dispatcher.SetRegionDescription(regionDescription);
  if (regionWorld)
    dispatcher.SetRegionInfoJson(regionInfoJson);
  std::string line;
  std::size_t requests = 0;
  while (!dispatcher.ShutdownRequested()) {
    const auto status = ReadBoundedLine(std::cin, line);
    if (status == LineStatus::End)
      break;
    if (requests >= economy_host::MaxRequestsPerProcess) {
      std::cout << "{\"id\":null,\"protocolVersion\":0,\"ok\":false,\"error\":{\"code\":\"request_limit_"
                   "exceeded\",\"message\":\"process request limit reached\"}}\n";
      return 3;
    }
    ++requests;
    if (status == LineStatus::TooLong) {
      std::cout << "{\"id\":null,\"protocolVersion\":0,\"ok\":false,\"error\":{\"code\":\"request_too_"
                   "large\",\"message\":\"request line exceeds the configured byte limit\"}}\n";
      std::cout.flush();
      return std::cout ? 5 : 4;
    }
    std::cout << dispatcher.HandleLine(line) << '\n';
    std::cout.flush();
    if (!std::cout)
      return 4;
  }
  return 0;
}
