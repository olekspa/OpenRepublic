#include "economy_protocol.h"

#include "json.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace openrepublic;
using Json = nlohmann::json;

bool Check(bool value, const char *label) {
  if (!value)
    std::cerr << "FAIL: " << label << '\n';
  return value;
}

std::string Request(const char *id, const char *method, const Json &fields = Json::object()) {
  Json request = fields;
  request["id"] = id;
  request["protocolVersion"] = economy_host::ProtocolVersion;
  request["method"] = method;
  return request.dump();
}

Json Response(economy_host::Dispatcher &dispatcher, const std::string &line) {
  return Json::parse(dispatcher.HandleLine(line));
}

economy::Catalog MakeCatalog(std::uint64_t depotCapacity = 4,
                             std::uint64_t wideCapacity = 8) {
  economy::CatalogBuilder builder;
  economy::Catalog catalog;
  economy::CatalogErrorInfo error;
  economy::ResourceId grain{};
  economy::BuildingTypeId farm{}, depot{}, wideDepot{};
  economy::VehicleTypeId truck{};
  const bool built = builder.AddResource("grain", &grain, error) &&
      builder.AddBuildingType("farm", 1, 1, &farm, error) &&
      builder.AddBuildingType("depot", 1, 1, &depot, error) &&
      builder.AddBuildingType("wide_depot", 3, 1, &wideDepot, error) &&
      builder.AddStorage(farm, grain, 100, error) &&
      builder.AddStorage(depot, grain, depotCapacity, error) &&
      builder.AddStorage(wideDepot, grain, wideCapacity, error) &&
      builder.AddVehicleType("truck", 4, 1, &truck, error) &&
      builder.Build(catalog, error);
  if (!built)
    std::cerr << "FAIL: build synthetic adviser catalog: " << error.message << '\n';
  return built ? catalog : economy::Catalog{};
}

bool QueueRoad(economy::World &world, economy::Cell a, economy::Cell b) {
  return world.BuildRoad(a, b) != 0;
}

bool AddRoadPath(economy::World &world, const std::vector<economy::Cell> &path) {
  for (std::size_t i = 1; i < path.size(); ++i)
    if (!QueueRoad(world, path[i - 1], path[i]))
      return false;
  return true;
}

bool SetInventory(std::string &save, std::uint64_t building, std::uint64_t amount);

// IDs are stable and intentionally simple: farm 1, wide depot 3, full depot 4,
// undersized depot 5, and the disconnected depot 6. Building access is below
// the footprint center; the wide depot's access is (5,1), not its origin (4,2).
std::string BaseSave(const economy::Catalog &catalog, bool secondCurrent = false,
                     bool secondSource = false) {
  economy::World world(catalog, 24, 12);
  for (std::int32_t x = 1; x < 12; ++x)
    if (!QueueRoad(world, {x, 1}, {x + 1, 1})) return {};
  for (std::int32_t x = 13; x < 17; ++x)
    if (!QueueRoad(world, {x, 1}, {x + 1, 1})) return {};
  const bool queued = world.PlaceBuilding(0, {1, 2}) != 0 &&
      world.PlaceBuilding(1, {3, 2}) != 0 &&
      world.PlaceBuilding(2, {4, 2}) != 0 &&
      world.PlaceBuilding(1, {8, 2}) != 0 &&
      world.PlaceBuilding(1, {10, 2}) != 0 &&
      world.PlaceBuilding(1, {13, 2}) != 0;
  if (!queued || (secondCurrent && world.PlaceBuilding(1, {11, 2}) == 0) ||
      (secondSource && world.PlaceBuilding(0, {17, 2}) == 0) || !world.Step().accepted)
    return {};
  if (world.CreateHaulOrder(1, 2, 0, 0) == 0 ||
      (secondCurrent && world.CreateHaulOrder(1, 7, 0, 0) == 0) ||
      (secondSource && world.CreateHaulOrder(7, 3, 0, 0) == 0) || !world.Step().accepted)
    return {};
  auto save = economy::SaveWorld(world);
  // Keep the non-wide connected alternatives full or one unit short of cargo.
  if (!SetInventory(save, 4, 4) || !SetInventory(save, 5, 1)) return {};
  return save;
}

std::string TieSave(const economy::Catalog &catalog) {
  economy::World world(catalog, 10, 8);
  for (std::int32_t x = 1; x < 5; ++x)
    if (!QueueRoad(world, {x, 1}, {x + 1, 1})) return {};
  // The right-hand candidate receives the lower ID, so equal road distances
  // must resolve by building ID rather than iteration or geometry.
  const bool queued = world.PlaceBuilding(0, {1, 2}) != 0 &&
      world.PlaceBuilding(1, {3, 2}) != 0 &&
      world.PlaceBuilding(1, {4, 2}) != 0 &&
      world.PlaceBuilding(1, {2, 2}) != 0;
  if (!queued || !world.Step().accepted || world.CreateHaulOrder(1, 2, 0, 0) == 0 ||
      !world.Step().accepted)
    return {};
  return economy::SaveWorld(world);
}

std::string DetourSave(const economy::Catalog &catalog) {
  economy::World world(catalog, 12, 12);
  // Current destination access at (3,1), nearer wide-depot access at (6,1),
  // and a farther access at (2,6). The latter has the shorter enabled-road route.
  const std::vector<economy::Cell> route{
      {3, 1}, {3, 0}, {2, 0}, {1, 0}, {0, 0}, {0, 1}, {0, 2}, {0, 3},
      {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3}, {6, 3}, {7, 3}, {8, 3},
      {8, 2}, {8, 1}, {7, 1}, {6, 1}};
  if (!AddRoadPath(world, route) ||
      !AddRoadPath(world, {{1, 1}, {2, 1}, {3, 1}}) ||
      !AddRoadPath(world, {{3, 1}, {2, 1}, {2, 2}, {2, 3}, {2, 4}, {2, 5}, {2, 6}})
      || world.PlaceBuilding(0, {1, 2}) == 0 || world.PlaceBuilding(1, {3, 2}) == 0 ||
      world.PlaceBuilding(2, {5, 2}) == 0 || world.PlaceBuilding(1, {2, 7}) == 0 ||
      !world.Step().accepted || world.CreateHaulOrder(1, 2, 0, 0) == 0 || !world.Step().accepted)
    return {};
  return economy::SaveWorld(world);
}

std::vector<std::string> Split(const std::string &line) {
  std::vector<std::string> fields;
  std::size_t begin = 0;
  while (begin <= line.size()) {
    const auto end = line.find(' ', begin);
    fields.push_back(line.substr(begin, end == std::string::npos ? end : end - begin));
    if (end == std::string::npos) break;
    begin = end + 1;
  }
  return fields;
}

std::string Join(const std::vector<std::string> &fields) {
  std::string result;
  for (const auto &field : fields) {
    if (!result.empty()) result.push_back(' ');
    result += field;
  }
  return result;
}

bool SetRecordField(std::string &save, const char *tag, std::size_t idColumn,
                    std::uint64_t id, std::size_t field, const std::string &value) {
  std::size_t begin = 0;
  while (begin < save.size()) {
    const auto end = save.find('\n', begin);
    const auto length = (end == std::string::npos ? save.size() : end) - begin;
    auto fields = Split(save.substr(begin, length));
    if (fields.size() > idColumn && fields[0] == tag && fields[idColumn] == std::to_string(id)) {
      if (field >= fields.size()) return false;
      fields[field] = value;
      const auto replacement = Join(fields);
      save.replace(begin, length, replacement);
      return true;
    }
    if (end == std::string::npos) break;
    begin = end + 1;
  }
  return false;
}

bool Stall(std::string &save, std::uint64_t orderId, std::uint64_t destination,
           std::int32_t x, std::int32_t z, std::uint64_t cargo = 4) {
  return SetRecordField(save, "O", 1, orderId, 3, std::to_string(destination)) &&
      SetRecordField(save, "O", 1, orderId, 7, "3") && // Waiting
      SetRecordField(save, "O", 1, orderId, 8, "3") && // DestinationFull
      SetRecordField(save, "O", 1, orderId, 9, std::to_string(cargo)) &&
      SetRecordField(save, "O", 1, orderId, 10, std::to_string(x)) &&
      SetRecordField(save, "O", 1, orderId, 11, std::to_string(z)) &&
      SetRecordField(save, "O", 1, orderId, 12, "1");
}

bool SetInventory(std::string &save, std::uint64_t building, std::uint64_t amount) {
  return SetRecordField(save, "I", 1, building, 3, std::to_string(amount));
}

bool Load(economy::World &world, const std::string &save) {
  economy::ParseError error;
  const bool loaded = economy::LoadWorld(save, world.GetCatalog(), world, error);
  if (!loaded)
    std::cerr << "FAIL: synthetic save rejected: " << error.message << '\n';
  return loaded;
}

Json Advise(economy::World &world, economy_host::Role role = economy_host::Role::Controller,
            bool canControl = true) {
  economy_api::EconomyApi api(world, {true, canControl});
  economy_host::Dispatcher dispatcher(role, api, false);
  const auto response = Response(dispatcher, Request("advice", "advise"));
  if (!response.value("ok", false))
    std::cerr << "FAIL: advise request rejected: " << response.dump() << '\n';
  return response.value("result", Json::object());
}

std::string ManyOrdersSave(const economy::Catalog &catalog) {
  economy::World world(catalog, 80, 8);
  world.PlaceBuilding(0, {1, 2});
  for (int x = 3; x <= 35; ++x) world.PlaceBuilding(1, {x, 2});
  for (int x = 40; x <= 72; ++x) world.PlaceBuilding(1, {x, 2});
  for (int x = 1; x < 73; ++x) world.BuildRoad({x, 1}, {x + 1, 1});
  if (!world.Step().accepted || world.Snapshot().buildings.size() != 67) return {};
  for (unsigned i = 0; i < 33; ++i) world.CreateHaulOrder(1, i + 2, 0, 0);
  if (!world.Step().accepted || world.Snapshot().orders.size() != 33) return {};
  auto save = economy::SaveWorld(world);
  for (unsigned i = 0; i < 33; ++i)
    if (!Stall(save, i + 1, i + 2, static_cast<int>(i + 3), 1) ||
        !SetInventory(save, i + 2, 4)) return {};
  return save;
}

bool LoadAndAdvise(const economy::Catalog &catalog, const std::string &save,
                   Json &result, economy::World &world) {
  return Load(world, save) && (result = Advise(world), true);
}

} // namespace

int main() {
  bool ok = true;
  auto catalog = MakeCatalog();
  ok &= Check(catalog.Resources().size() == 1, "synthetic adviser catalog built");
  if (!ok) return 1;

  auto base = BaseSave(catalog);
  ok &= Check(!base.empty(), "base canonical SaveWorld fixture created");
  auto stalled = base;
  ok &= Check(Stall(stalled, 1, 2, 3, 1) && SetInventory(stalled, 2, 4),
              "token helper creates a loaded DestinationFull order");
  economy::World baseWorld(catalog, 1, 1);
  Json result;
  ok &= Check(LoadAndAdvise(catalog, stalled, result, baseWorld), "loaded stalled fixture accepted");
  if (result.is_object() && result.contains("retargets")) {
    const auto &suggestions = result["retargets"];
    ok &= Check(suggestions.size() == 1, "reachable alternative is suggested despite full, small, and disconnected candidates");
    if (!suggestions.empty()) {
      const auto &s = suggestions.front();
      ok &= Check(s["orderId"] == 1 && s["vehicleId"] == 1 && s["resource"] == "grain" &&
                      s["cargo"] == "4" && s["destination"] == 3 && s["distanceCells"] == 2,
                  "suggestion carries IDs, decimal cargo, destination, and enabled-road distance");
      ok &= Check(s["command"] == Json{{"kind", "retarget_haul_order"}, {"orderId", 1}, {"destination", 3}},
                  "suggestion command has the exact retarget protocol shape");
    }
    ok &= Check(result.value("retargetsTruncated", true) == false,
                "complete single suggestion is not marked truncated");
  } else ok &= Check(false, "advice has retarget suggestions array");

  const auto canonicalBefore = economy::SaveWorld(baseWorld);
  const auto hashBefore = baseWorld.Snapshot().stateHash;
  const auto repeat = Advise(baseWorld);
  ok &= Check(economy::SaveWorld(baseWorld) == canonicalBefore && baseWorld.Snapshot().stateHash == hashBefore,
              "adviser query preserves canonical save and state hash");
  ok &= Check(repeat["retargets"] == result["retargets"], "repeated adviser output is deterministic");
  economy::World observerWorld(catalog, 1, 1);
  ok &= Check(Load(observerWorld, stalled), "observer fixture loaded");
  const auto observerHash = observerWorld.Snapshot().stateHash;
  const auto observerAdvice = Advise(observerWorld, economy_host::Role::Observer, false);
  ok &= Check(observerAdvice["retargets"] == result["retargets"] &&
                  observerWorld.Snapshot().stateHash == observerHash,
              "observer can read identical retarget advice without mutation");

  auto exactCatalog = MakeCatalog(4, 4);
  auto exactSave = BaseSave(exactCatalog);
  Stall(exactSave, 1, 2, 3, 1);
  SetInventory(exactSave, 2, 4);
  economy::World exactWorld(exactCatalog, 1, 1);
  Json exactResult;
  ok &= Check(LoadAndAdvise(exactCatalog, exactSave, exactResult, exactWorld) &&
                  exactResult["retargets"].size() == 1 && exactResult["retargets"][0]["destination"] == 3,
              "candidate with exact whole-cargo room is eligible");
  auto shortCatalog = MakeCatalog(4, 3);
  auto shortSave = BaseSave(shortCatalog);
  Stall(shortSave, 1, 2, 3, 1);
  SetInventory(shortSave, 2, 4);
  economy::World shortWorld(shortCatalog, 1, 1);
  Json shortResult;
  ok &= Check(LoadAndAdvise(shortCatalog, shortSave, shortResult, shortWorld) &&
                  shortResult["retargets"].empty(),
              "candidate one unit short of whole cargo is excluded");

  auto inboundCatalog = MakeCatalog(4, 7);
  auto inboundSave = BaseSave(inboundCatalog, false, true);
  Stall(inboundSave, 1, 2, 3, 1);
  SetInventory(inboundSave, 2, 4);
  // Order 2 is already loaded for destination 3. It reserves four of seven units.
  SetRecordField(inboundSave, "O", 1, 2, 3, "3");
  SetRecordField(inboundSave, "O", 1, 2, 7, "1"); // ToDestination
  SetRecordField(inboundSave, "O", 1, 2, 8, "0");
  SetRecordField(inboundSave, "O", 1, 2, 9, "4");
  SetRecordField(inboundSave, "O", 1, 2, 10, "17");
  SetRecordField(inboundSave, "O", 1, 2, 11, "1");
  SetRecordField(inboundSave, "O", 1, 2, 12, "1");
  economy::World inboundWorld(inboundCatalog, 1, 1);
  Json inboundResult;
  ok &= Check(LoadAndAdvise(inboundCatalog, inboundSave, inboundResult, inboundWorld) &&
                  inboundResult["retargets"].empty(),
              "all other loaded inbound cargo reserves candidate capacity");

  auto twoSave = BaseSave(catalog, true);
  Stall(twoSave, 1, 2, 3, 1);
  Stall(twoSave, 2, 7, 11, 1);
  SetInventory(twoSave, 2, 4);
  SetInventory(twoSave, 7, 4);
  auto reservationCatalog = MakeCatalog(4, 7);
  // Catalog structure is equal, but capacity is part of the catalog used by the save fixture.
  twoSave = BaseSave(reservationCatalog, true);
  Stall(twoSave, 1, 2, 3, 1);
  Stall(twoSave, 2, 7, 11, 1);
  SetInventory(twoSave, 2, 4);
  SetInventory(twoSave, 7, 4);
  economy::World reservationWorld(reservationCatalog, 1, 1);
  Json reservationResult;
  ok &= Check(LoadAndAdvise(reservationCatalog, twoSave, reservationResult, reservationWorld) &&
                  reservationResult["retargets"].size() == 1 && reservationResult["retargets"][0]["orderId"] == 1,
              "earlier order reserves candidate space and later order cannot overbook it");
  auto duplicateCatalog = MakeCatalog(4, 8);
  auto duplicateSave = BaseSave(duplicateCatalog, true);
  Stall(duplicateSave, 1, 2, 3, 1);
  Stall(duplicateSave, 2, 7, 11, 1);
  SetInventory(duplicateSave, 2, 4);
  SetInventory(duplicateSave, 7, 4);
  economy::World duplicateWorld(duplicateCatalog, 1, 1);
  Json duplicateResult;
  ok &= Check(LoadAndAdvise(duplicateCatalog, duplicateSave, duplicateResult, duplicateWorld) &&
                  duplicateResult["retargets"].size() == 1,
              "same source destination resource tuple is not suggested twice");

  auto staleSave = stalled;
  SetInventory(staleSave, 2, 0);
  economy::World staleWorld(catalog, 1, 1);
  Json staleResult;
  ok &= Check(LoadAndAdvise(catalog, staleSave, staleResult, staleWorld) && staleResult["retargets"].empty(),
              "stale DestinationFull failure is skipped when current destination can fit cargo");
  auto stateSave = stalled;
  SetRecordField(stateSave, "O", 1, 1, 7, "4"); // NoRoute
  economy::World stateWorld(catalog, 1, 1);
  Json stateResult;
  ok &= Check(LoadAndAdvise(catalog, stateSave, stateResult, stateWorld) && stateResult["retargets"].empty(),
              "non-Waiting order is not advised");

  auto tieCatalog = MakeCatalog(4, 4);
  auto tieSave = TieSave(tieCatalog);
  Stall(tieSave, 1, 2, 3, 1);
  SetInventory(tieSave, 2, 4);
  economy::World tieWorld(tieCatalog, 1, 1);
  Json tieResult;
  ok &= Check(LoadAndAdvise(tieCatalog, tieSave, tieResult, tieWorld) &&
                  tieResult["retargets"].size() == 1 && tieResult["retargets"][0]["destination"] == 3 &&
                  tieResult["retargets"][0]["distanceCells"] == 1,
              "equal shortest routes resolve by destination ID");

  auto detourCatalog = MakeCatalog(4, 8);
  auto detourSave = DetourSave(detourCatalog);
  Stall(detourSave, 1, 2, 3, 1);
  SetInventory(detourSave, 2, 4);
  economy::World detourWorld(detourCatalog, 1, 1);
  Json detourResult;
  ok &= Check(LoadAndAdvise(detourCatalog, detourSave, detourResult, detourWorld) &&
                  detourResult["retargets"].size() == 1 && detourResult["retargets"][0]["destination"] == 4 &&
                  detourResult["retargets"][0]["distanceCells"] == 6,
              "candidate selection uses enabled-road distance rather than Manhattan distance");

  auto disabledSave = stalled;
  SetRecordField(disabledSave, "R", 1, 3, 6, "0"); // (3,1)--(4,1)
  economy::World disabledWorld(catalog, 1, 1);
  Json disabledResult;
  ok &= Check(LoadAndAdvise(catalog, disabledSave, disabledResult, disabledWorld) &&
                  disabledResult["retargets"].empty(), "disabled road cannot connect a candidate");

  auto existingSave = BaseSave(catalog, true);
  Stall(existingSave, 1, 2, 3, 1);
  SetInventory(existingSave, 2, 4);
  SetInventory(existingSave, 7, 4);
  SetRecordField(existingSave, "O", 1, 2, 3, "3"); // Empty existing order already claims the pair.
  economy::World existingWorld(catalog, 1, 1);
  Json existingResult;
  ok &= Check(LoadAndAdvise(catalog, existingSave, existingResult, existingWorld) &&
                  existingResult["retargets"].empty(), "existing empty order still prevents duplicate tuple");

  // Being at the candidate access is a valid zero-edge route, even if isolated.
  auto zeroSave = disabledSave;
  SetRecordField(zeroSave, "O", 1, 1, 10, "5");
  SetRecordField(zeroSave, "R", 1, 4, 6, "0");
  SetRecordField(zeroSave, "R", 1, 5, 6, "0");
  economy::World zeroWorld(catalog, 1, 1);
  Json zeroResult;
  ok &= Check(LoadAndAdvise(catalog, zeroSave, zeroResult, zeroWorld) &&
                  zeroResult["retargets"].size() == 1 &&
                  zeroResult["retargets"][0]["distanceCells"] == 0,
              "candidate access at the truck cell needs no road edge");

  const auto manySave = ManyOrdersSave(catalog);
  economy::World manyWorld(catalog, 1, 1);
  Json manyResult;
  ok &= Check(!manySave.empty() && LoadAndAdvise(catalog, manySave, manyResult, manyWorld) &&
                  manyResult["retargets"].size() == 32 && manyResult["retargetsTruncated"] == true &&
                  manyResult["retargets"][0]["orderId"] == 1 &&
                  manyResult["retargets"][31]["orderId"] == 32,
              "32 suggestion cap is deterministic and reports unexamined eligible work");
  auto lateSave = manySave;
  SetRecordField(lateSave, "R", 1, 34, 6, "0"); // First 32 trucks cannot reach alternatives.
  economy::World lateWorld(catalog, 1, 1);
  Json lateResult;
  ok &= Check(LoadAndAdvise(catalog, lateSave, lateResult, lateWorld) &&
                  lateResult["retargets"].size() == 1 && lateResult["retargets"][0]["orderId"] == 33 &&
                  lateResult["retargetsTruncated"] == false,
              "infeasible early orders do not starve a later useful suggestion");

  {
    economy::CatalogBuilder builder;
    economy::Catalog largeCatalog;
    economy::CatalogErrorInfo error;
    const bool built = builder.AddResource("grain", nullptr, error) &&
        builder.AddBuildingType("depot", 1, 1, nullptr, error) &&
        builder.AddStorage(0, 0, UINT64_MAX, error) &&
        builder.AddVehicleType("truck", UINT64_MAX, 1, nullptr, error) &&
        builder.Build(largeCatalog, error);
    ok &= Check(built, "large-amount synthetic catalog built");
    economy::World largeWorld(largeCatalog, 12, 8);
    for (int x = 1; x <= 5; ++x) largeWorld.PlaceBuilding(0, {x, 2});
    for (int x = 1; x < 5; ++x) largeWorld.BuildRoad({x, 1}, {x + 1, 1});
    largeWorld.Step();
    largeWorld.CreateHaulOrder(1, 4, 0, 0);
    largeWorld.CreateHaulOrder(2, 5, 0, 0);
    largeWorld.CreateHaulOrder(3, 5, 0, 0);
    largeWorld.Step();
    auto largeSave = economy::SaveWorld(largeWorld);
    Stall(largeSave, 1, 4, 4, 1, 4);
    SetInventory(largeSave, 2, UINT64_MAX);
    SetInventory(largeSave, 3, UINT64_MAX);
    SetInventory(largeSave, 4, UINT64_MAX);
    for (unsigned id : {2u, 3u}) {
      SetRecordField(largeSave, "O", 1, id, 7, "1");
      SetRecordField(largeSave, "O", 1, id, 8, "0");
      SetRecordField(largeSave, "O", 1, id, 9, std::to_string(UINT64_MAX));
    }
    SetRecordField(largeSave, "O", 1, 3, 9, "4"); // MAX+4 would wrap to 3 without saturation.
    Json largeResult;
    ok &= Check(LoadAndAdvise(largeCatalog, largeSave, largeResult, largeWorld) &&
                    largeResult["retargets"].empty(),
                "overflowing inbound reservations saturate rather than exposing false free room");
  }

  if (ok) std::cout << "Haul adviser retarget contracts passed\n";
  return ok ? 0 : 1;
}
