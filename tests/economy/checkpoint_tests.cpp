#include "openrepublic/economy/checkpoint.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace openrepublic::economy;

namespace {
int failures = 0;

void Check(bool ok, const std::string &message) {
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::vector<std::string> Lines(std::string_view text) {
    std::istringstream in{std::string(text)};
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);)
        lines.push_back(std::move(line));
    return lines;
}

std::string ReplaceOnce(std::string text, const std::string &from, const std::string &to) {
    const auto at = text.find(from);
    if (at == std::string::npos) {
        Check(false, "catalog fixture contains expected text: " + from);
        return text;
    }
    text.replace(at, from.size(), to);
    return text;
}

std::string ReplaceAll(std::string text, const std::string &from, const std::string &to) {
    std::size_t at = 0;
    bool changed = false;
    while ((at = text.find(from, at)) != std::string::npos) {
        text.replace(at, from.size(), to);
        at += to.size();
        changed = true;
    }
    Check(changed, "catalog fixture contains expected text: " + from);
    return text;
}

std::string Frame(std::string_view catalog, std::string_view world) {
    return "OPENREPUBLIC_ECONOMY_CHECKPOINT 1\nCATALOG_BYTES " +
           std::to_string(catalog.size()) + "\nWORLD_BYTES " + std::to_string(world.size()) + "\n" +
           std::string(catalog) + std::string(world);
}

std::string HeaderReplace(std::string checkpoint, const std::string &linePrefix,
                          const std::string &replacement) {
    const auto at = checkpoint.find(linePrefix);
    if (at == std::string::npos) {
        Check(false, "checkpoint fixture contains expected header field: " + linePrefix);
        return checkpoint;
    }
    const auto end = checkpoint.find('\n', at);
    checkpoint.replace(at, end - at, replacement);
    return checkpoint;
}

Catalog CatalogFromText(const std::string &text) {
    Catalog catalog;
    ParseError error;
    Check(ParseCatalogText(text, catalog, error), "catalog mismatch fixture parses");
    return catalog;
}

void CheckRestoreRejectsTransactionally(const std::string &bad, RestoredCheckpoint &out,
                                        const std::string &label) {
    const auto *catalogBefore = out.catalog.get();
    const auto *worldBefore = out.world.get();
    const auto catalogTextBefore = WriteCatalogText(*out.catalog);
    const auto worldTextBefore = SaveWorld(*out.world);
    ParseError error;
    const bool restored = RestoreCheckpoint(bad, out, error);
    Check(!restored, label + " is rejected");
    Check(error.code != ParseErrorCode::None, label + " reports a parse error");
    Check(out.catalog.get() == catalogBefore && out.world.get() == worldBefore &&
              WriteCatalogText(*out.catalog) == catalogTextBefore && SaveWorld(*out.world) == worldTextBefore,
          label + " leaves prior restored checkpoint unchanged");
}

std::string LoadedCargoWithPendingRetarget(const Catalog &catalog, CommandId &retargetId) {
    World world(catalog, 48, 8);
    for (int x = 1; x < 40; ++x)
        world.BuildRoad({x, 1}, {x + 1, 1});
    world.PlaceBuilding(0, {1, 2});
    world.PlaceBuilding(1, {28, 2});
    world.PlaceBuilding(1, {40, 2});
    world.Step();
    const auto orderCommand = world.CreateHaulOrder(1, 2, 0, 0);
    world.Step();
    Check(world.GetCommandResult(orderCommand).state == CommandState::Accepted,
          "checkpoint fixture haul order is accepted");
    bool loaded = false;
    for (unsigned i = 0; i < 80; ++i) {
        world.Step();
        const auto snapshot = world.Snapshot();
        if (!snapshot.orders.empty() && snapshot.orders[0].cargo > 0) {
            loaded = true;
            break;
        }
    }
    Check(loaded, "checkpoint fixture captures cargo on a moving truck");
    retargetId = world.RetargetHaulOrder(1, 3);
    Check(retargetId != 0 && world.GetCommandResult(retargetId).state == CommandState::Queued,
          "checkpoint fixture has a pending retarget command");
    return SaveWorld(world);
}

Catalog MakeOversizedCanonicalCatalog() {
    // Keep each recipe line below the catalog parser's line limit while making
    // the canonical catalog exceed the checkpoint component byte limit.
    CatalogBuilder builder;
    CatalogErrorInfo error;
    std::vector<ResourceId> resources;
    resources.reserve(96);
    for (unsigned i = 0; i < 96; ++i) {
        ResourceId id{};
        char name[16];
        std::snprintf(name, sizeof(name), "r%03u", i);
        Check(builder.AddResource(name, &id, error), "oversize fixture adds a resource");
        resources.push_back(id);
    }
    std::vector<RecipeAmount> inputs;
    for (auto id : resources)
        inputs.push_back({id, 1});
    for (unsigned i = 0; i < 800; ++i) {
        BuildingTypeId type{};
        char name[24];
        std::snprintf(name, sizeof(name), "building_%04u", i);
        Check(builder.AddBuildingType(name, 1, 1, &type, error), "oversize fixture adds a building type");
        for (auto id : resources)
            Check(builder.AddStorage(type, id, 10, error), "oversize fixture adds storage");
        char recipeName[24];
        std::snprintf(recipeName, sizeof(recipeName), "recipe_%04u", i);
        Check(builder.AddRecipe(type, {recipeName, 1, inputs, {}}, error),
              "oversize fixture adds a long canonical recipe");
    }
    Catalog catalog;
    Check(builder.Build(catalog, error), "oversize fixture catalog builds");
    return catalog;
}

Catalog MakeNonRoundtrippableLongRecipeCatalog() {
    CatalogBuilder builder;
    CatalogErrorInfo error;
    std::vector<ResourceId> resources;
    std::vector<RecipeAmount> inputs;
    resources.reserve(600);
    inputs.reserve(600);
    for (unsigned i = 0; i < 600; ++i) {
        ResourceId id{};
        char name[16];
        std::snprintf(name, sizeof(name), "x%04u", i);
        Check(builder.AddResource(name, &id, error), "long-line fixture adds resource");
        resources.push_back(id);
        inputs.push_back({id, 1});
    }
    BuildingTypeId type{};
    Check(builder.AddBuildingType("longrecipe", 1, 1, &type, error),
          "long-line fixture adds building type");
    for (auto id : resources)
        Check(builder.AddStorage(type, id, 10, error), "long-line fixture adds storage");
    Check(builder.AddRecipe(type, {"many_inputs", 1, inputs, {}}, error),
          "long-line fixture adds recipe beyond parser line limit");
    Catalog catalog;
    Check(builder.Build(catalog, error), "long-line fixture catalog builds");
    return catalog;
}

void TestRoundTripAndReplay(const Catalog &catalog) {
    CommandId retargetId{};
    const std::string native = LoadedCargoWithPendingRetarget(catalog, retargetId);
    World source(catalog, 1, 1);
    ParseError error;
    Check(LoadWorld(native, catalog, source, error), "pending loaded-cargo native world fixture loads");
    const auto baseline = source.Snapshot();
    Check(!baseline.orders.empty() && baseline.orders[0].cargo > 0 && baseline.orders[0].state == HaulState::ToDestination,
          "checkpoint baseline has loaded cargo in transit");

    std::string checkpoint;
    Check(SaveCheckpoint(source, checkpoint, error), "checkpoint serializes canonical catalog and world");
    Check(!checkpoint.empty() && checkpoint.find("OPENREPUBLIC_ECONOMY_CHECKPOINT 1\n") == 0,
          "checkpoint has the strict versioned framing header");

    RestoredCheckpoint restored;
    Check(RestoreCheckpoint(checkpoint, restored, error), "checkpoint restores owned catalog and world");
    Check(restored.catalog && restored.world && &restored.world->GetCatalog() == restored.catalog.get(),
          "restored world references its owned catalog");
    Check(restored.world->Tick() == source.Tick() && restored.world->Revision() == source.Revision() &&
              restored.world->Snapshot().stateHash == source.Snapshot().stateHash &&
              SaveWorld(*restored.world) == SaveWorld(source),
          "checkpoint preserves tick, revision, hash, and canonical native world text");
    Check(restored.world->GetCommandResult(retargetId).state == CommandState::Queued,
          "pending retarget survives checkpoint restore");
    std::string sameCheckpoint;
    Check(SaveCheckpoint(*restored.world, sameCheckpoint, error) && sameCheckpoint == checkpoint,
          "restore and resave reproduces byte-identical canonical checkpoint");

    auto *catalogAddress = restored.catalog.get();
    auto *worldAddress = restored.world.get();
    RestoredCheckpoint moved(std::move(restored));
    Check(moved.catalog.get() == catalogAddress && moved.world.get() == worldAddress &&
              &moved.world->GetCatalog() == moved.catalog.get(),
          "moving restored checkpoint preserves catalog/world pointer lifetime relationship");
    RestoredCheckpoint assigned;
    assigned.catalog = std::make_unique<Catalog>(catalog);
    assigned.world = std::make_unique<World>(*assigned.catalog, 2, 2);
    Check(RestoreCheckpoint(checkpoint, assigned, error),
          "move assignment replaces an already populated owned checkpoint transactionally");
    Check(assigned.catalog && assigned.world && &assigned.world->GetCatalog() == assigned.catalog.get() &&
              assigned.world->Snapshot().stateHash == moved.world->Snapshot().stateHash,
          "move-assigned checkpoint retains a live owned catalog reference");
    RestoredCheckpoint incoming;
    Check(RestoreCheckpoint(checkpoint, incoming, error), "incoming move-assignment fixture restores");
    const auto *incomingCatalogAddress = incoming.catalog.get();
    const auto *incomingWorldAddress = incoming.world.get();
    assigned = std::move(incoming);
    Check(!incoming.catalog && !incoming.world && assigned.catalog.get() == incomingCatalogAddress &&
              assigned.world.get() == incomingWorldAddress &&
              &assigned.world->GetCatalog() == assigned.catalog.get(),
          "move assignment over populated owner preserves incoming catalog lifetime");
    const auto *assignedCatalogAddress = assigned.catalog.get();
    const auto *assignedWorldAddress = assigned.world.get();
    assigned = std::move(assigned);
    Check(assigned.catalog.get() == assignedCatalogAddress && assigned.world.get() == assignedWorldAddress &&
              &assigned.world->GetCatalog() == assigned.catalog.get(),
          "self move assignment preserves owned checkpoint and catalog lifetime");

    for (unsigned tick = 0; tick < 12; ++tick) {
        Check(source.Step().accepted && moved.world->Step().accepted, "checkpoint replay tick advances");
        const auto left = source.Snapshot();
        const auto right = moved.world->Snapshot();
        Check(left.tick == right.tick && left.revision == right.revision && left.stateHash == right.stateHash &&
                  SaveWorld(source) == SaveWorld(*moved.world),
              "checkpoint continuation is deterministic at each tick");
    }

    // Both worlds must continue allocating fresh IDs after moving the restored
    // owner. The command and resulting building IDs must agree exactly.
    const auto leftCommand = source.PlaceBuilding(0, {44, 4});
    const auto rightCommand = moved.world->PlaceBuilding(0, {44, 4});
    Check(leftCommand != 0 && leftCommand == rightCommand, "restored command counter continues without reuse");
    source.Step();
    moved.world->Step();
    Check(source.GetCommandResult(leftCommand).state == CommandState::Accepted &&
              moved.world->GetCommandResult(rightCommand).state == CommandState::Accepted &&
              source.GetCommandResult(leftCommand).building == moved.world->GetCommandResult(rightCommand).building &&
              source.GetCommandResult(leftCommand).building == 4 &&
              source.Snapshot().stateHash == moved.world->Snapshot().stateHash,
          "restored entity IDs continue uniquely and replay identically");
}

void TestExpectedCatalogMatching(const Catalog &catalog) {
    World source(catalog, 8, 8);
    source.PlaceBuilding(0, {2, 2});
    source.Step();
    std::string checkpoint;
    ParseError error;
    Check(SaveCheckpoint(source, checkpoint, error), "expected-catalog fixture checkpoint saves");

    World target(catalog, 8, 8);
    target.PlaceBuilding(1, {4, 2});
    target.Step();
    Check(LoadCheckpoint(checkpoint, catalog, target, error) && SaveWorld(target) == SaveWorld(source),
          "exact expected catalog loads checkpoint world");

    const std::string canonical = WriteCatalogText(catalog);
    std::vector<std::pair<std::string, std::string>> variants;
    variants.emplace_back("resource name", ReplaceAll(canonical, "grain", "grains"));
    variants.emplace_back("recipe amount", ReplaceOnce(canonical, "in grain:1 out flour:1", "in grain:2 out flour:1"));
    variants.emplace_back("recipe timer", ReplaceOnce(canonical, "recipe harvest every 2", "recipe harvest every 3"));
    variants.emplace_back("vehicle definition", ReplaceOnce(canonical, "vehicle truck capacity 4 speed 1",
                                                              "vehicle truck capacity 5 speed 1"));
    auto reordered = canonical;
    const auto grainAt = reordered.find("resource grain\n");
    const auto flourAt = reordered.find("resource flour\n");
    Check(grainAt != std::string::npos && flourAt != std::string::npos,
          "canonical fixture has two resource declarations to reorder");
    if (grainAt != std::string::npos && flourAt != std::string::npos) {
        const std::string first = "resource grain\n";
        const std::string second = "resource flour\n";
        reordered.replace(flourAt, second.size(), first);
        reordered.replace(grainAt, first.size(), second);
    }
    variants.emplace_back("resource order", std::move(reordered));

    for (const auto &variant : variants) {
        Catalog changed = CatalogFromText(variant.second);
        Check(WriteCatalogText(changed) != canonical, variant.first + " changes canonical catalog identity");
        const auto before = SaveWorld(target);
        Check(!LoadCheckpoint(checkpoint, changed, target, error), variant.first + " mismatch is rejected");
        Check(SaveWorld(target) == before, variant.first + " mismatch leaves target world unchanged");
    }
}

void TestFramingAndCanonicalValidation(const Catalog &catalog) {
    World source(catalog, 8, 8);
    source.PlaceBuilding(0, {2, 2});
    source.Step();
    std::string valid;
    ParseError error;
    Check(SaveCheckpoint(source, valid, error), "framing fixture saves");
    RestoredCheckpoint out;
    Check(RestoreCheckpoint(valid, out, error), "framing fixture primes transactional output");
    const auto lines = Lines(valid);
    Check(lines.size() > 3, "checkpoint has payload after three header lines");

    auto malformed = [&](const std::string &label, const std::string &candidate, ParseErrorCode code) {
        CheckRestoreRejectsTransactionally(candidate, out, label);
        ParseError local;
        Check(!RestoreCheckpoint(candidate, out, local) && local.code == code,
              label + " reports the expected parse category");
    };
    malformed("unsupported version", ReplaceOnce(valid, "CHECKPOINT 1", "CHECKPOINT 2"),
              ParseErrorCode::InvalidHeader);
    malformed("negative catalog length", HeaderReplace(valid, "CATALOG_BYTES ", "CATALOG_BYTES -1"),
              ParseErrorCode::InvalidNumber);
    malformed("signed catalog length", HeaderReplace(valid, "CATALOG_BYTES ", "CATALOG_BYTES +1"),
              ParseErrorCode::InvalidNumber);
    malformed("leading-zero catalog length", HeaderReplace(valid, "CATALOG_BYTES ", "CATALOG_BYTES 01"),
              ParseErrorCode::InvalidNumber);
    malformed("zero catalog length", HeaderReplace(valid, "CATALOG_BYTES ", "CATALOG_BYTES 0"),
              ParseErrorCode::InvalidNumber);
    malformed("overflow catalog length",
              HeaderReplace(valid, "CATALOG_BYTES ", "CATALOG_BYTES 18446744073709551616"),
              ParseErrorCode::InvalidNumber);
    malformed("truncated checkpoint payload", valid.substr(0, valid.size() - 1),
              ParseErrorCode::InvalidHeader);
    malformed("declared payload truncation", HeaderReplace(valid, "WORLD_BYTES ",
                                                             "WORLD_BYTES " +
                                                                 std::to_string(SaveWorld(source).size() + 1)),
              ParseErrorCode::InvalidHeader);
    malformed("trailing payload bytes", valid + "X", ParseErrorCode::InvalidHeader);
    malformed("malformed header", ReplaceOnce(valid, "CATALOG_BYTES ", "CATALOG_SIZE "),
              ParseErrorCode::InvalidHeader);

    const auto canonicalCatalog = WriteCatalogText(catalog);
    auto noncanonicalCatalog = "# noncanonical padding\n" + canonicalCatalog;
    const auto native = SaveWorld(source);
    malformed("noncanonical embedded catalog", Frame(noncanonicalCatalog, native), ParseErrorCode::CatalogInvalid);
    malformed("invalid embedded world", Frame(canonicalCatalog, "OPENREPUBLIC_ECONOMY_WORLD 99\n"),
              ParseErrorCode::InvalidHeader);
    malformed("catalog component over byte limit",
              Frame(std::string(MaxCheckpointCatalogBytes + 1, 'x'), SaveWorld(source)),
              ParseErrorCode::LimitExceeded);
    malformed("checkpoint total over byte limit",
              std::string(MaxCheckpointBytes + 1, 'x'), ParseErrorCode::LimitExceeded);
    Check(out.world && out.catalog, "transactional malformed checks keep restored output available");
}

void TestOversizeCatalog(const Catalog &ordinary) {
    Catalog oversized = MakeOversizedCanonicalCatalog();
    const auto canonical = WriteCatalogText(oversized);
    Check(canonical.size() > MaxCheckpointCatalogBytes,
          "long-recipe catalog fixture exceeds the checkpoint catalog byte limit");
    World world(oversized, 8, 8);
    world.PlaceBuilding(0, {2, 2});
    world.Step();
    std::string output = "preserve-me";
    ParseError error;
    Check(!SaveCheckpoint(world, output, error), "oversized canonical recipe catalog explicitly fails to save");
    Check(error.code != ParseErrorCode::None, "oversized checkpoint save reports a diagnostic");
    Check(output == "preserve-me", "oversized checkpoint save leaves caller output unchanged");

    Catalog longLine = MakeNonRoundtrippableLongRecipeCatalog();
    const auto longCanonical = WriteCatalogText(longLine);
    Check(longCanonical.size() < MaxCheckpointCatalogBytes,
          "long recipe roundtrip fixture remains below checkpoint catalog byte limit");
    World longLineWorld(longLine, 4, 4);
    output = "preserve-me-too";
    Check(!SaveCheckpoint(longLineWorld, output, error),
          "non-roundtrippable long canonical recipe explicitly fails checkpoint save");
    Check(error.code != ParseErrorCode::None && output == "preserve-me-too",
          "long recipe failure reports an error and preserves caller output");

    World ordinaryWorld(ordinary, 8, 8);
    std::string ordinaryCheckpoint;
    Check(SaveCheckpoint(ordinaryWorld, ordinaryCheckpoint, error) && !ordinaryCheckpoint.empty(),
          "ordinary catalog checkpoint remains saveable after oversize rejection");
}
} // namespace

int main() {
    const Catalog catalog = MakeSyntheticCatalog();
    TestRoundTripAndReplay(catalog);
    TestExpectedCatalogMatching(catalog);
    TestFramingAndCanonicalValidation(catalog);
    TestOversizeCatalog(catalog);
    if (failures != 0) {
        std::cerr << failures << " economy checkpoint checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "economy checkpoint checks passed\n";
    return EXIT_SUCCESS;
}
