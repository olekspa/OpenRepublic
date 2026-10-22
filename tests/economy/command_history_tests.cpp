#include "openrepublic/economy/economy.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

using namespace openrepublic::economy;

namespace {
int failures = 0;
void Check(bool condition, const std::string &message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::string ReplaceLine(const std::string &text, const std::string &prefix, const std::string &replacement) {
    auto out = text;
    const auto begin = out.find(prefix);
    if (begin == std::string::npos)
        return {};
    const auto end = out.find('\n', begin);
    if (end == std::string::npos)
        return {};
    out.replace(begin, end - begin, replacement);
    return out;
}

std::string V1Fixture() {
    return "OPENREPUBLIC_ECONOMY_WORLD 1\n"
           "GRID 8 8\nTICK 1\nREVISION 2\nNEXT 2 1 1 1 2\n"
           "BUILDINGS 1\nB 1 0 2 2 1\nI 1 1 0\nP 1 1 0 0\n"
           "ROADS 0\nORDERS 0\nPENDING 0\nRESULTS 1\nC 1 2 0 1 0 0\nEND\n";
}

void AddRoadLine(World &world, int x0, int x1, int z) {
    for (int x = x0; x < x1; ++x)
        world.BuildRoad({x, z}, {x + 1, z});
}

bool LoadRejectedWithoutMutation(const std::string &text, const Catalog &catalog, const std::string &label) {
    World target(catalog, 8, 8);
    target.PlaceBuilding(0, {2, 2});
    target.Step();
    const auto before = SaveWorld(target);
    ParseError error;
    const bool rejected = !LoadWorld(text, catalog, target, error);
    Check(rejected, label + " is rejected");
    Check(SaveWorld(target) == before, label + " rejection is transactional");
    return rejected;
}
} // namespace

int main() {
    const auto catalog = MakeSyntheticCatalog();
    {
        World oldVersion(catalog, 8, 8);
        ParseError error;
        Check(LoadWorld(V1Fixture(), catalog, oldVersion, error), "v1 fixture loads");
        const auto legacyHash = oldVersion.Snapshot().stateHash;
        const auto migrated = SaveWorld(oldVersion);
        Check(migrated.find("OPENREPUBLIC_ECONOMY_WORLD 2\n") == 0,
              "uncompacted v1 still migrates to byte-compatible v2 header");
        World v2(catalog, 8, 8);
        Check(LoadWorld(migrated, catalog, v2, error), "v2 fixture loads");
        Check(v2.Snapshot().stateHash == legacyHash, "v1 and v2 floor-one state hash is unchanged");
        const auto before = SaveWorld(v2);
        const auto hash = v2.Snapshot().stateHash;
        const auto noOp = v2.CompactCommandHistory(10);
        Check(noOp.error == CommandHistoryError::None && noOp.removedResults == 0 &&
                  SaveWorld(v2) == before && v2.Snapshot().stateHash == hash,
              "no-op compaction preserves exact v2 bytes, revision and hash");
    }

    {
        World world(catalog, 8, 8);
        const auto place = world.PlaceBuilding(0, {2, 2});
        const auto rejected = world.PlaceBuilding(999, {5, 5});
        const auto toggle = world.SetBuildingEnabled(1, false);
        world.Step();
        Check(world.GetCommandResult(place).state == CommandState::Accepted &&
                  world.GetCommandResult(rejected).state == CommandState::Rejected &&
                  world.GetCommandResult(toggle).state == CommandState::Accepted,
              "fixture contains accepted and rejected terminal results");
        const auto history = world.CommandHistory();
        Check(history.firstRetainedCommandId == 1 && history.nextCommandId == 4 &&
                  history.retainedTerminalResults == 3 && history.pendingCommands == 0,
              "history snapshot reports the full terminal prefix");
        const auto beforeQuery = SaveWorld(world);
        const auto beforeSnapshot = world.Snapshot();
        (void)world.CommandHistory();
        (void)world.Snapshot();
        Check(SaveWorld(world) == beforeQuery && world.Snapshot().stateHash == beforeSnapshot.stateHash,
              "history and snapshot queries are immutable");

        const auto invalid = world.CompactCommandHistory(MaxCommandHistoryRetention + 1);
        Check(invalid.error == CommandHistoryError::InvalidRetention && invalid.removedResults == 0 &&
                  SaveWorld(world) == beforeQuery,
              "retention above the public bound is rejected transactionally");

        const auto compacted = world.CompactCommandHistory(1);
        Check(compacted.error == CommandHistoryError::None && compacted.removedResults == 2 &&
                  compacted.history.firstRetainedCommandId == 3 && compacted.history.nextCommandId == 4 &&
                  compacted.history.retainedTerminalResults == 1 && compacted.history.pendingCommands == 0,
              "compaction retains newest terminal result and increments the history floor");
        Check(world.Revision() == beforeSnapshot.revision + 1 && world.Tick() == beforeSnapshot.tick,
              "actual compaction advances revision once without advancing simulation tick");
        const auto expiredPlace = world.GetCommandResult(place);
        const auto expiredRejected = world.GetCommandResult(rejected);
        Check(expiredPlace.state == CommandState::Expired && expiredPlace.failure == CommandFailure::None &&
                  expiredPlace.message == "command result expired" && expiredRejected.state == CommandState::Expired,
              "pruned accepted and rejected results synthesize the explicit expired state");
        Check(world.GetCommandResult(0).state == CommandState::Unknown &&
                  world.GetCommandResult(0).failure == CommandFailure::InvalidCommand &&
                  world.GetCommandResult(4).state == CommandState::Unknown,
              "zero and never-issued future IDs remain unknown");
        Check(world.GetCommandResult(toggle).state == CommandState::Accepted,
              "newest retained terminal result remains queryable");
        const auto v3 = SaveWorld(world);
        Check(v3.find("OPENREPUBLIC_ECONOMY_WORLD 3\n") == 0 && v3.find("HISTORY 3\n") != std::string::npos,
              "compacted saves use v3 with HISTORY directly after NEXT");
        World loaded(catalog, 1, 1);
        ParseError error;
        Check(LoadWorld(v3, catalog, loaded, error) && SaveWorld(loaded) == v3 &&
                  loaded.Snapshot().stateHash == world.Snapshot().stateHash &&
                  loaded.GetCommandResult(place).state == CommandState::Expired,
              "v3 history floor round-trips with identical hash and expiry semantics");

        LoadRejectedWithoutMutation(ReplaceLine(v3, "HISTORY ", "HISTORY 0"), catalog,
                                   "v3 zero history floor");
        LoadRejectedWithoutMutation(ReplaceLine(v3, "HISTORY ", "HISTORY 5"), catalog,
                                   "v3 history floor above next ID");
        LoadRejectedWithoutMutation(ReplaceLine(v3, "HISTORY ", "HISTORY -1"), catalog,
                                   "v3 signed history floor");
        LoadRejectedWithoutMutation(ReplaceLine(v3, "HISTORY ", "HISTORY 18446744073709551616"), catalog,
                                   "v3 overflowing history floor");
        LoadRejectedWithoutMutation(ReplaceLine(v3, "C 3 ", "C 3 4 0 0 0 0"), catalog,
                                   "v3 persisted Expired result is invalid");
        LoadRejectedWithoutMutation(ReplaceLine(v3, "HISTORY ", "HISTORY 2"), catalog,
                                   "v3 result gap after history floor change");
        LoadRejectedWithoutMutation(ReplaceLine(v3, "NEXT ", "NEXT 2 1 1 1 2"), catalog,
                                   "v3 next counter below retained history floor");
        LoadRejectedWithoutMutation(ReplaceLine(v3, "REVISION ", "REVISION 2"), catalog,
                                   "v3 revision below lifetime command count");
    }

    {
        World world(catalog, 16, 8);
        AddRoadLine(world, 1, 11, 1);
        world.PlaceBuilding(0, {1, 2});
        world.PlaceBuilding(1, {4, 2});
        world.Step();
        ResourceId grain{};
        Check(catalog.FindResource("grain", grain), "synthetic catalog has grain");
        const auto orderCommand = world.CreateHaulOrder(1, 2, grain, 0);
        world.Step();
        Check(orderCommand != 0 && world.GetCommandResult(orderCommand).state == CommandState::Accepted,
              "loaded-cargo fixture order applies before the pending retarget");
        bool loadedCargo = false;
        for (std::uint32_t tick = 0; tick < 100 && !loadedCargo; ++tick) {
            world.Step();
            for (const auto &order : world.Snapshot().orders)
                loadedCargo = loadedCargo || order.cargo > 0;
        }
        Check(loadedCargo, "checkpoint fixture has a loaded vehicle");
        const auto retarget = world.RetargetHaulOrder(1, 2);
        Check(retarget != 0 && world.GetCommandResult(retarget).state == CommandState::Queued,
              "pending retarget follows loaded cargo");
        const auto compacted = world.CompactCommandHistory(4);
        Check(compacted.error == CommandHistoryError::None && compacted.history.pendingCommands == 1 &&
                  compacted.history.firstRetainedCommandId == retarget - 4,
              "compaction keeps recent terminal results and the entire pending suffix");
        const auto saved = SaveWorld(world);
        World restored(catalog, 1, 1);
        ParseError error;
        Check(LoadWorld(saved, catalog, restored, error) && SaveWorld(restored) == saved,
              "v3 loaded-cargo pending-retarget fixture round-trips exactly");
        Check(restored.GetCommandResult(retarget).state == CommandState::Queued &&
                  restored.GetCommandResult(1).state == CommandState::Expired,
              "pending suffix survives while older terminal results expire");
        Check(world.Step().accepted && restored.Step().accepted && SaveWorld(world) == SaveWorld(restored),
              "loaded cargo and pending retarget replay identically after v3 load");
    }

    {
        World world(catalog, 8, 8);
        const auto first = world.PlaceBuilding(0, {2, 2});
        world.Step();
        const auto second = world.SetBuildingEnabled(1, false);
        world.Step();
        const auto floorBefore = world.CommandHistory().firstRetainedCommandId;
        const auto compacted = world.CompactCommandHistory(0);
        Check(compacted.error == CommandHistoryError::None && compacted.removedResults == 2 &&
                  compacted.history.firstRetainedCommandId == second + 1 &&
                  compacted.history.firstRetainedCommandId == compacted.history.nextCommandId &&
                  compacted.history.retainedTerminalResults == 0 && compacted.history.pendingCommands == 0,
              "zero retention can expire every terminal result and move floor to next ID");
        Check(floorBefore == 1 && world.GetCommandResult(first).state == CommandState::Expired &&
                  world.GetCommandResult(second).state == CommandState::Expired,
              "fully compacted history reports old IDs as expired");
        World restored(catalog, 1, 1);
        ParseError error;
        const auto saved = SaveWorld(world);
        Check(LoadWorld(saved, catalog, restored, error) && SaveWorld(restored) == saved &&
                  restored.CommandHistory().firstRetainedCommandId == restored.CommandHistory().nextCommandId,
              "v3 all-expired history with no retained results reloads canonically");
    }

    {
        World world(catalog, 8, 8);
        const auto place = world.PlaceBuilding(0, {2, 2});
        world.Step();
        const auto pendingA = world.SetBuildingEnabled(1, false);
        const auto pendingB = world.SetBuildingEnabled(1, true);
        Check(pendingA != 0 && pendingB == pendingA + 1, "pending commands form a consecutive suffix");
        const auto before = SaveWorld(world);
        const auto result = world.CompactCommandHistory(0);
        Check(result.error == CommandHistoryError::None && result.removedResults == 1 &&
                  result.history.firstRetainedCommandId == pendingA &&
                  result.history.nextCommandId == pendingB + 1 && result.history.pendingCommands == 2,
              "zero retention prunes only terminal prefix and keeps every queued ID");
        Check(world.GetCommandResult(place).state == CommandState::Expired &&
                  world.GetCommandResult(pendingA).state == CommandState::Queued &&
                  world.GetCommandResult(pendingB).state == CommandState::Queued,
              "pending results are never expired");
        const auto next = world.SetBuildingEnabled(1, false);
        Check(next == pendingB + 1, "command IDs continue monotonically after compaction");
        (void)before;
        const auto savePending = SaveWorld(world);
        // A missing or pruned pending result is rejected without touching the destination.
        auto corruptFloor = ReplaceLine(savePending, "HISTORY ", "HISTORY " + std::to_string(next));
        LoadRejectedWithoutMutation(corruptFloor, catalog, "v3 floor that prunes queued suffix");
    }

    {
        World noOpWorld(catalog, 8, 8);
        noOpWorld.PlaceBuilding(0, {2, 2});
        noOpWorld.Step();
        const auto id = noOpWorld.SetBuildingEnabled(1, false);
        noOpWorld.Step();
        (void)id;
        auto exhaustedText = ReplaceLine(SaveWorld(noOpWorld), "REVISION ",
                                         "REVISION " + std::to_string(std::numeric_limits<std::uint64_t>::max()));
        World exhausted(catalog, 1, 1);
        ParseError error;
        Check(LoadWorld(exhaustedText, catalog, exhausted, error),
              "terminal history can load at maximum revision for read/no-op operations");
        const auto before = SaveWorld(exhausted);
        const auto noOp = exhausted.CompactCommandHistory(2);
        Check(noOp.error == CommandHistoryError::None && noOp.removedResults == 0 &&
                  SaveWorld(exhausted) == before,
              "no-op succeeds unchanged even when revision counter is exhausted");
        const auto rejected = exhausted.CompactCommandHistory(0);
        Check(rejected.error == CommandHistoryError::RevisionExhausted && rejected.removedResults == 0 &&
                  SaveWorld(exhausted) == before,
              "actual pruning fails transactionally when revision cannot advance");

        World reservationSource(catalog, 8, 8);
        reservationSource.PlaceBuilding(0, {2, 2});
        reservationSource.Step();
        reservationSource.SetBuildingEnabled(1, false);
        reservationSource.SetBuildingEnabled(1, true);
        auto reservationText = ReplaceLine(SaveWorld(reservationSource), "REVISION ",
                                           "REVISION " + std::to_string(
                                               std::numeric_limits<std::uint64_t>::max() - 3));
        World reserved(catalog, 1, 1);
        Check(LoadWorld(reservationText, catalog, reserved, error),
              "pending world loads with one more revision reserved than its queue requires");
        const auto reservationBefore = SaveWorld(reserved);
        const auto reservationResult = reserved.CompactCommandHistory(0);
        Check(reservationResult.error == CommandHistoryError::RevisionExhausted &&
                  SaveWorld(reserved) == reservationBefore,
              "compaction reserves one revision in addition to every pending command and final tick");
        reservationText = ReplaceLine(reservationText, "REVISION ",
                                     "REVISION " + std::to_string(
                                         std::numeric_limits<std::uint64_t>::max() - 4));
        Check(LoadWorld(reservationText, catalog, reserved, error),
              "exact pending-plus-compaction execution budget loads");
        Check(reserved.CompactCommandHistory(0).error == CommandHistoryError::None &&
                  reserved.Step().accepted && reserved.Revision() == std::numeric_limits<std::uint64_t>::max(),
              "exact compaction budget still permits every pending command and following tick");

        auto oneRevisionLeft = ReplaceLine(before, "REVISION ",
                                          "REVISION " + std::to_string(
                                              std::numeric_limits<std::uint64_t>::max() - 1));
        Check(LoadWorld(oneRevisionLeft, catalog, exhausted, error) &&
                  exhausted.CompactCommandHistory(0).error == CommandHistoryError::None &&
                  exhausted.Revision() == std::numeric_limits<std::uint64_t>::max(),
              "without pending work compaction can consume the final available revision");
    }

    {
        World pendingWorld(catalog, 8, 8);
        pendingWorld.PlaceBuilding(0, {2, 2});
        pendingWorld.Step();
        const auto firstPending = pendingWorld.SetBuildingEnabled(1, false);
        for (std::uint32_t i = 1; i < 75000; ++i)
            pendingWorld.SetBuildingEnabled(1, (i & 1u) != 0);
        Check(SaveWorld(pendingWorld).empty(), "large pending payload also exceeds save bound");
        const auto compacted = pendingWorld.CompactCommandHistory(0);
        Check(compacted.error == CommandHistoryError::None && compacted.removedResults == 1 &&
                  compacted.history.pendingCommands == 75000 && SaveWorld(pendingWorld).empty() &&
                  pendingWorld.GetCommandResult(firstPending).state == CommandState::Queued,
              "compaction cannot make an oversized pending queue saveable by discarding live work");
    }

    {
        constexpr std::uint32_t CommandsPerCycle = 75000;
        World world(catalog, 8, 8);
        world.PlaceBuilding(0, {2, 2});
        world.Step();
        for (int cycle = 0; cycle < 3; ++cycle) {
            for (std::uint32_t i = 0; i < CommandsPerCycle; ++i)
                world.SetBuildingEnabled(1, ((i + static_cast<std::uint32_t>(cycle)) & 1u) == 0);
            Check(world.Step().accepted, "large terminal-history cycle advances");
            Check(SaveWorld(world).empty(), "large retained history exceeds the bounded save size");
            const auto compacted = world.CompactCommandHistory(32);
            Check(compacted.error == CommandHistoryError::None && compacted.removedResults >= CommandsPerCycle - 32 &&
                      compacted.history.retainedTerminalResults <= 32,
                  "explicit compaction bounds retained results after an oversized history");
            const auto saved = SaveWorld(world);
            Check(!saved.empty() && saved.size() < MaxWorldSaveBytes,
                  "compacted save returns below the shared 1 MiB limit");
            World restored(catalog, 1, 1);
            ParseError error;
            Check(LoadWorld(saved, catalog, restored, error) && SaveWorld(restored) == saved,
                  "large compacted v3 history reloads exactly");
            Check(world.Step().accepted && restored.Step().accepted && SaveWorld(world) == SaveWorld(restored),
                  "large compacted history replays identically");
            world = std::move(restored);
        }
    }

    if (failures) {
        std::cerr << failures << " command-history checks failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "command history compaction checks passed\n";
    return EXIT_SUCCESS;
}
