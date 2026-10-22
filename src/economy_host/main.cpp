#include "economy_protocol.h"

#include "openrepublic/compat/catalog_import.h"
#include "openrepublic/compat/region_import.h"
#include "openrepublic/compat/region_roads.h"
#include "openrepublic/compat/storage_profile.h"
#include "openrepublic/content/map_placement.h"
#include "openrepublic/content/road_network.h"
#include "openrepublic/economy/checkpoint.h"

#ifdef OPENREPUBLIC_HAVE_LUA_RULES
#include "openrepublic/scripting/economy_rules.h"
#endif

#include <fstream>
#include <iostream>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <set>
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
            << "       [--storage-profile <file> (only with --region)]\n"
            << "       [--utility-resource <name> (repeatable, only with --region)]\n"
            << "       [--role observer|controller] --checkpoint <file>\n"
            << "Reads protocolVersion 0 JSONL requests from stdin and writes responses to stdout.\n"
            << "--region imports a local WRSR map package's placements and roads into the world\n"
            << "(the catalog is built from <content-root>/buildings_types; local content only).\n"
            << "--checkpoint restores a bounded native simulation checkpoint; do not combine it with catalog, rules, region, storage-profile, or utility-resource options.\n"
            << "--storage-profile loads OpenRepublic-authored storage factors for a region import.\n"
            << "--utility-resource marks a discovered region catalog resource as supplied by utility connections.\n";
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

bool ReadCheckpointFile(const std::filesystem::path &path, std::string &text, std::string &error) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    error = "could not open checkpoint file";
    return false;
  }
  const std::size_t probeSize = economy::MaxCheckpointBytes + 1;
  text.resize(probeSize);
  input.read(text.data(), static_cast<std::streamsize>(probeSize));
  const std::streamsize count = input.gcount();
  if (input.bad() || (input.fail() && !input.eof())) {
    text.clear();
    error = "checkpoint file read failed";
    return false;
  }
  if (count < 0 || static_cast<std::size_t>(count) > economy::MaxCheckpointBytes) {
    text.clear();
    error = "checkpoint file exceeds the " + std::to_string(economy::MaxCheckpointBytes) + " byte limit";
    return false;
  }
  text.resize(static_cast<std::size_t>(count));
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
                      const compat::StorageCapacityFactors &storageCapacityFactors,
                      const std::set<std::string> &utilityResources,
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
  compat::CatalogImportOptions catalogOptions;
  catalogOptions.storageCapacityFactors = storageCapacityFactors;
  catalogOptions.utilityResources = utilityResources;
  if (definitions.empty() ||
      !compat::ImportCatalogFromDefinitions(definitions, catalogOptions, catalog, catalogReport)) {
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
          << compat::DescribeImport(catalogReport) << "\n"
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
  std::filesystem::path checkpointPath;
  bool checkpointSpecified = false;
  bool catalogSpecified = false;
  bool rulesSpecified = false;
  bool regionSpecified = false;
  bool contentRootSpecified = false;
  bool centerSpecified = false;
  bool radiusSpecified = false;
  bool storageProfileSpecified = false;
  std::set<std::string> utilityResources;
  std::filesystem::path regionDir;
  std::filesystem::path contentRoot;
  std::filesystem::path rulesPath;
  std::filesystem::path storageProfilePath;
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
      catalogSpecified = true;
      catalogPath = argv[++i];
    } else if (arg == "--checkpoint") {
      checkpointSpecified = true;
      if (i + 1 >= argc || std::string(argv[i + 1]).rfind("--", 0) == 0) {
        std::cerr << "--checkpoint needs a file path.\n";
        PrintUsage();
        return 2;
      }
      checkpointPath = argv[++i];
    } else if (arg == "--region" && i + 1 < argc) {
      regionSpecified = true;
      regionDir = argv[++i];
    } else if (arg == "--content-root" && i + 1 < argc) {
      contentRootSpecified = true;
      contentRoot = argv[++i];
    } else if (arg == "--rules" && i + 1 < argc) {
      rulesSpecified = true;
      rulesPath = argv[++i];
    } else if (arg == "--storage-profile") {
      if (storageProfileSpecified) {
        std::cerr << "--storage-profile may be specified only once.\n";
        PrintUsage();
        return 2;
      }
      storageProfileSpecified = true;
      if (i + 1 >= argc || std::string(argv[i + 1]).rfind("--", 0) == 0) {
        std::cerr << "--storage-profile needs a file path.\n";
        PrintUsage();
        return 2;
      }
      storageProfilePath = argv[++i];
      if (storageProfilePath.empty()) {
        std::cerr << "--storage-profile needs a non-empty file path.\n";
        PrintUsage();
        return 2;
      }
    } else if (arg == "--utility-resource") {
      if (i + 1 >= argc || std::string(argv[i + 1]).rfind("--", 0) == 0 ||
          std::string(argv[i + 1]).empty()) {
        std::cerr << "--utility-resource needs a non-empty resource name.\n";
        PrintUsage();
        return 2;
      }
      const std::string utilityName(argv[++i]);
      if (!utilityResources.insert(utilityName).second) {
        std::cerr << "--utility-resource may not repeat the same resource name.\n";
        PrintUsage();
        return 2;
      }
    } else if (arg == "--center" && i + 1 < argc) {
      centerSpecified = true;
      centerIndex = std::strtol(argv[++i], nullptr, 10);
    } else if (arg == "--radius" && i + 1 < argc) {
      radiusSpecified = true;
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

  if (checkpointSpecified &&
      (catalogSpecified || rulesSpecified || regionSpecified || contentRootSpecified ||
       centerSpecified || radiusSpecified || storageProfileSpecified || !utilityResources.empty())) {
    std::cerr << "--checkpoint cannot be combined with explicit catalog, rules, region, content-root, center, radius, storage-profile, or utility-resource options.\n";
    PrintUsage();
    return 2;
  }
  if (!utilityResources.empty() && (!regionSpecified || catalogSpecified || checkpointSpecified)) {
    std::cerr << "--utility-resource requires --region and cannot be combined with --catalog or --checkpoint.\n";
    PrintUsage();
    return 2;
  }
  if (checkpointSpecified && checkpointPath.empty()) {
    std::cerr << "--checkpoint needs a non-empty file path.\n";
    PrintUsage();
    return 2;
  }
  if (storageProfileSpecified && (!regionSpecified || catalogSpecified || checkpointSpecified)) {
    std::cerr << "--storage-profile requires --region and cannot be combined with --catalog or --checkpoint.\n";
    PrintUsage();
    return 2;
  }

  compat::StorageCapacityFactors storageCapacityFactors;
  if (storageProfileSpecified) {
    std::string profileError;
    if (!compat::LoadStorageCapacityProfile(storageProfilePath, storageCapacityFactors,
                                            profileError)) {
      std::cerr << "Storage profile rejected: " << profileError << ".\n";
      return 2;
    }
  }

  economy::Catalog catalog = economy::MakeSyntheticCatalog();
  economy::RestoredCheckpoint restoredCheckpoint;
  if (checkpointSpecified) {
    std::string checkpointText;
    std::string readError;
    if (!ReadCheckpointFile(checkpointPath, checkpointText, readError)) {
      std::cerr << "Checkpoint restore failed: " << readError << ".\n";
      return 2;
    }
    economy::ParseError parseError;
    if (!economy::RestoreCheckpoint(checkpointText, restoredCheckpoint, parseError) ||
        !restoredCheckpoint.catalog || !restoredCheckpoint.world) {
      std::cerr << "Checkpoint restore failed: "
                << (parseError.message.empty() ? "invalid checkpoint contents" : parseError.message)
                << ".\n";
      return 2;
    }
  } else if (!catalogPath.empty()) {
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

  if (!checkpointSpecified && regionDir.empty()) {
    std::string rulesError;
    if (!ApplyRules(rulesPath, catalog, rulesError)) {
      std::cerr << "Lua rules rejected: " << rulesError << "\n";
      return 2;
    }
  }
  std::unique_ptr<economy::World> regionWorld;
  std::string regionDescription;
  std::string regionInfoJson;
  if (!checkpointSpecified && !regionDir.empty()) {
    if (contentRoot.empty() || !catalogPath.empty() || !(radius >= 10.0 && radius <= 1000.0)) {
      std::cerr << "--region needs --content-root, excludes --catalog, and a radius of 10..1000 m.\n";
      return 2;
    }
    std::string error;
    if (!BuildRegionWorld(contentRoot, regionDir, centerIndex, radius, rulesPath,
                          storageCapacityFactors, utilityResources, catalog, regionWorld, regionDescription,
                          regionInfoJson, error)) {
      std::cerr << "Region import failed: " << error << "\n";
      return 2;
    }
  }
  economy_host::Dispatcher dispatcher = restoredCheckpoint.world
                                            ? economy_host::Dispatcher(role, *restoredCheckpoint.world)
                                        : regionWorld ? economy_host::Dispatcher(role, *regionWorld)
                                                      : economy_host::Dispatcher(role, catalog);
  if (regionWorld)
    dispatcher.SetRegionDescription(regionDescription);
  if (regionWorld)
    dispatcher.SetRegionInfoJson(regionInfoJson);
#ifdef OPENREPUBLIC_HAVE_LUA_RULES
  {
    // Rules are always applied to the untuned base so repeated reloads do not compound.
    const economy::Catalog &baseCatalog = restoredCheckpoint.catalog
                                              ? *restoredCheckpoint.catalog
                                          : regionWorld ? regionWorld->GetCatalog()
                                                        : catalog;
    const auto baseCopy = std::make_shared<economy::Catalog>(baseCatalog);
    dispatcher.SetRulesBuilder([baseCopy](std::string_view text, economy::Catalog &out, std::string &error) {
      return scripting::EconomyRules::ApplyText(text, *baseCopy, out, &error);
    });
  }
#endif
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
