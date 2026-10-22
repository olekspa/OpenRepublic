#include "openrepublic/economy/economy.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace openrepublic::economy;

namespace {
int failures = 0;

void Check(bool condition, const std::string &message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::vector<std::string> Lines(const std::string &text) {
    std::istringstream input(text);
    std::vector<std::string> lines;
    for (std::string line; std::getline(input, line);)
        lines.push_back(line);
    return lines;
}

std::vector<std::string> Fields(const std::string &line) {
    std::istringstream input(line);
    std::vector<std::string> fields;
    for (std::string field; input >> field;)
        fields.push_back(field);
    return fields;
}

std::string Join(const std::vector<std::string> &fields) {
    std::string line;
    for (const auto &field : fields) {
        if (!line.empty())
            line += ' ';
        line += field;
    }
    return line;
}

std::string Rebuild(const std::vector<std::string> &lines) {
    std::string text;
    for (const auto &line : lines)
        text += line + '\n';
    return text;
}

// Mutations address the canonical record tag and its token index, avoiding
// dependence on unrelated whitespace or the number of entities in a fixture.
std::string SetField(const std::string &text, const std::string &recordTag, std::size_t occurrence,
                     std::size_t fieldIndex, const std::string &value) {
    auto lines = Lines(text);
    std::size_t found = 0;
    for (auto &line : lines) {
        auto fields = Fields(line);
        if (fields.empty() || fields[0] != recordTag)
            continue;
        if (found++ != occurrence)
            continue;
        if (fieldIndex >= fields.size()) {
            Check(false, "fixture mutation field is outside " + recordTag + " record");
            return text;
        }
        fields[fieldIndex] = value;
        line = Join(fields);
        return Rebuild(lines);
    }
    Check(false, "fixture mutation cannot find " + recordTag + " record");
    return text;
}

std::string SetNext(const std::string &text, std::size_t counter, const std::string &value) {
    auto lines = Lines(text);
    for (auto &line : lines) {
        auto fields = Fields(line);
        if (!fields.empty() && fields[0] == "NEXT" && counter + 1 < fields.size()) {
            fields[counter + 1] = value;
            line = Join(fields);
            return Rebuild(lines);
        }
    }
    return text;
}

std::string RecordField(const std::string &text, const std::string &tag, std::size_t occurrence,
                        std::size_t fieldIndex) {
    std::size_t found = 0;
    for (const auto &line : Lines(text)) {
        const auto fields = Fields(line);
        if (!fields.empty() && fields[0] == tag && found++ == occurrence && fieldIndex < fields.size())
            return fields[fieldIndex];
    }
    return {};
}

std::size_t RecordOccurrenceForId(const std::string &text, const std::string &tag, std::size_t idIndex,
                                  const std::string &id) {
    std::size_t occurrence = 0;
    for (const auto &line : Lines(text)) {
        const auto fields = Fields(line);
        if (fields.empty() || fields[0] != tag)
            continue;
        if (idIndex < fields.size() && fields[idIndex] == id)
            return occurrence;
        ++occurrence;
    }
    return std::numeric_limits<std::size_t>::max();
}

std::string SetRecordById(const std::string &text, const std::string &tag, std::size_t idIndex,
                          const std::string &id, std::size_t targetIndex, const std::string &value) {
    const auto occurrence = RecordOccurrenceForId(text, tag, idIndex, id);
    return occurrence == std::numeric_limits<std::size_t>::max()
               ? text
               : SetField(text, tag, occurrence, targetIndex, value);
}

std::string SwapRows(const std::string &text, const std::string &tag, std::size_t first,
                     std::size_t second) {
    auto lines = Lines(text);
    std::vector<std::size_t> rowLines;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto fields = Fields(lines[i]);
        if (!fields.empty() && fields[0] == tag)
            rowLines.push_back(i);
    }
    if (first >= rowLines.size() || second >= rowLines.size())
        return text;
    std::swap(lines[rowLines[first]], lines[rowLines[second]]);
    return Rebuild(lines);
}

template <typename Mutator>
void RejectTransactionally(const std::string &canonical, const Catalog &catalog, Mutator mutate,
                           const std::string &label) {
    const std::string malformed = mutate(canonical);
    World target(catalog, 8, 8);
    target.PlaceBuilding(0, {2, 2});
    target.Step();
    const std::string before = SaveWorld(target);
    ParseError error;
    const bool loaded = LoadWorld(malformed, catalog, target, error);
    Check(!loaded, label + " is rejected");
    Check(error.code != ParseErrorCode::None, label + " reports a parse diagnostic");
    Check(SaveWorld(target) == before, label + " leaves target world unchanged");
}

void AddRoadLine(World &world, int x0, int x1, int z) {
    for (int x = x0; x < x1; ++x)
        world.BuildRoad({x, z}, {x + 1, z});
}

std::string RichCanonicalV2(const Catalog &catalog) {
    World world(catalog, 24, 12);
    AddRoadLine(world, 1, 10, 1);
    world.PlaceBuilding(0, {1, 2});
    world.PlaceBuilding(1, {4, 2});
    world.PlaceBuilding(2, {7, 2});
    world.PlaceBuilding(3, {10, 2});
    world.Step();
    world.CreateHaulOrder(1, 2, 0, 0);
    world.CreateHaulOrder(2, 3, 1, 0);
    world.Step();
    // Keep a real queued command/result pair in the canonical baseline.
    world.SetBuildingEnabled(4, false);
    world.SetBuildingEnabled(1, false);
    world.SetBuildingEnabled(2, false);
    return SaveWorld(world);
}

std::string LoadedCargoCanonical(const Catalog &catalog) {
    World world(catalog, 16, 8);
    AddRoadLine(world, 1, 6, 1);
    world.PlaceBuilding(0, {1, 2});
    world.PlaceBuilding(1, {5, 2});
    world.Step();
    world.CreateHaulOrder(1, 2, 0, 0);
    for (int tick = 0; tick < 80; ++tick) {
        world.Step();
        const auto text = SaveWorld(world);
        const auto lines = Lines(text);
        for (const auto &line : lines) {
            const auto fields = Fields(line);
            if (fields.size() > 9 && fields[0] == "O" && fields[9] != "0")
                return text;
        }
    }
    return SaveWorld(world);
}

std::string V1Fixture() {
    // Kept independently as a small historical positive fixture. Version 1
    // has no pending retarget command and omits the v2-only tail semantics.
    return "OPENREPUBLIC_ECONOMY_WORLD 1\n"
           "GRID 8 8\nTICK 1\nREVISION 2\nNEXT 2 1 1 1 2\n"
           "BUILDINGS 1\nB 1 0 2 2 1\nI 1 1 0\nP 1 1 0 0\n"
           "ROADS 0\nORDERS 0\nPENDING 0\nRESULTS 1\nC 1 2 0 1 0 0\nEND\n";
}

} // namespace

int main() {
    const Catalog catalog = MakeSyntheticCatalog();
    const std::string canonical = RichCanonicalV2(catalog);

    {
        World restored(catalog, 1, 1);
        ParseError error;
        Check(LoadWorld(canonical, catalog, restored, error), "canonical v2 save loads");
        Check(SaveWorld(restored) == canonical, "valid load preserves canonical SaveWorld bytes");
        World duplicateLoad(catalog, 1, 1);
        Check(LoadWorld(canonical, catalog, duplicateLoad, error) &&
                  duplicateLoad.Snapshot().stateHash == restored.Snapshot().stateHash,
              "valid load preserves deterministic world hash");

        World v1(catalog, 1, 1);
        Check(LoadWorld(V1Fixture(), catalog, v1, error), "historical v1 fixture still loads");
        Check(SaveWorld(v1).find("OPENREPUBLIC_ECONOMY_WORLD 2\n") == 0,
              "v1 fixture migrates to canonical v2 output");
    }

    // Command history is intentionally unbounded by the catalog definition
    // limit. More than 1,024 terminal results must survive save/load/replay.
    {
        World longHistory(catalog, 8, 8);
        longHistory.PlaceBuilding(0, {2, 2});
        longHistory.Step();
        for (std::uint32_t i = 0; i < 1100; ++i) {
            longHistory.SetBuildingEnabled(1, (i & 1u) == 0);
            Check(longHistory.Step().accepted, "history command tick advances");
        }
        const auto saved = SaveWorld(longHistory);
        World restored(catalog, 1, 1);
        ParseError error;
        Check(LoadWorld(saved, catalog, restored, error),
              "save with more than 1,024 command results loads");
        Check(SaveWorld(restored) == saved, "large command history remains canonical after load");
        for (int i = 0; i < 8; ++i) {
            longHistory.SetBuildingEnabled(1, (i & 1) == 0);
            restored.SetBuildingEnabled(1, (i & 1) == 0);
            longHistory.Step();
            restored.Step();
            Check(SaveWorld(longHistory) == SaveWorld(restored),
                  "large command history replay stays canonical");
        }
    }

    // Save output has a fixed byte limit. A large still-pending batch should
    // fail explicitly rather than produce text the loader cannot accept.
    {
        World oversized(catalog, 8, 8);
        for (std::uint32_t i = 0; i < 24000; ++i)
            oversized.SetBuildingEnabled(1, (i & 1u) == 0);
        Check(SaveWorld(oversized).empty(), "oversized world save fails explicitly");
    }

    // A loaded world may retain terminal counter values at the integer limit;
    // stepping must reject before mutating any part of its state.
    {
        World empty(catalog, 8, 8);
        const auto max = std::string("18446744073709551615");
        auto exhaustedText = SetField(SaveWorld(empty), "TICK", 0, 1, max);
        exhaustedText = SetField(exhaustedText, "REVISION", 0, 1, max);
        World exhausted(catalog, 1, 1);
        ParseError error;
        Check(LoadWorld(exhaustedText, catalog, exhausted, error),
              "tick and revision maximum fixture loads transactionally");
        const auto before = SaveWorld(exhausted);
        const auto stepped = exhausted.Step();
        Check(!stepped.accepted && SaveWorld(exhausted) == before,
              "tick overflow step rejects without mutation");
        const auto beforeQueue = SaveWorld(exhausted);
        Check(exhausted.SetBuildingEnabled(1, false) == 0 && SaveWorld(exhausted) == beforeQueue,
              "command queue rejects at exhausted tick without mutation");
    }
    {
        World empty(catalog, 8, 8);
        const auto atMaximum =
            SetField(SaveWorld(empty), "REVISION", 0, 1, "18446744073709551615");
        World exhausted(catalog, 1, 1);
        ParseError error;
        Check(LoadWorld(atMaximum, catalog, exhausted, error),
              "revision maximum fixture loads transactionally");
        const auto before = SaveWorld(exhausted);
        const auto stepped = exhausted.Step();
        Check(!stepped.accepted && SaveWorld(exhausted) == before,
              "revision overflow step rejects without mutation");
        Check(exhausted.BuildRoad({0, 0}, {1, 0}) == 0 && SaveWorld(exhausted) == before,
              "command queue rejects at exhausted revision without mutation");
    }
    {
        World empty(catalog, 8, 8);
        const auto atMaxMinusOne =
            SetField(SaveWorld(empty), "REVISION", 0, 1, "18446744073709551614");
        World exhausted(catalog, 1, 1);
        ParseError error;
        Check(LoadWorld(atMaxMinusOne, catalog, exhausted, error),
              "revision max-minus-one fixture loads");
        const auto before = SaveWorld(exhausted);
        Check(exhausted.SetRoadEnabled(1, false) == 0 && SaveWorld(exhausted) == before,
              "command queue rejects with only one revision remaining");
    }
    {
        World nearLimit(catalog, 8, 8);
        ParseError error;
        Check(LoadWorld(SetField(SaveWorld(nearLimit), "REVISION", 0, 1,
                                 "18446744073709551613"),
                        catalog, nearLimit, error),
              "revision max minus two fixture loads");
        const auto first = nearLimit.SetBuildingEnabled(1, false);
        const auto beforeSecond = SaveWorld(nearLimit);
        Check(first != 0 && nearLimit.SetBuildingEnabled(2, false) == 0 &&
                  SaveWorld(nearLimit) == beforeSecond,
              "remaining revision budget permits one queue and rejects a second");
        const auto stepped = nearLimit.Step();
        Check(stepped.accepted && nearLimit.Revision() == UINT64_MAX,
              "reserved revision budget permits one tick to reach UINT64_MAX");
    }
    {
        const auto max = std::string("18446744073709551615");
        World queued(catalog, 8, 8);
        queued.SetBuildingEnabled(1, false);
        const auto pendingSave = SaveWorld(queued);

        auto tickExhausted = SetField(pendingSave, "TICK", 0, 1, max);
        tickExhausted = SetField(tickExhausted, "REVISION", 0, 1, max);
        RejectTransactionally(tickExhausted, catalog, [](const std::string &text) { return text; },
                              "pending command at maximum tick");

        auto insufficientRevision =
            SetField(pendingSave, "REVISION", 0, 1, "18446744073709551614");
        RejectTransactionally(insufficientRevision, catalog,
                              [](const std::string &text) { return text; },
                              "pending commands without revision budget");
    }
    {
        World queued(catalog, 8, 8);
        ParseError error;
        Check(LoadWorld(SetField(SaveWorld(queued), "REVISION", 0, 1,
                                 "18446744073709551613"),
                        catalog, queued, error),
              "pending catalog replacement budget fixture loads");
        Check(queued.SetBuildingEnabled(1, false) != 0, "queue pending command before catalog reload");
        const auto before = SaveWorld(queued);
        std::string reloadError;
        Check(!queued.ReplaceCatalog(catalog, reloadError) && !reloadError.empty() &&
                  SaveWorld(queued) == before,
              "catalog replacement cannot consume pending tick revision reservation");
    }

    // Constructor dimensions are bounded so coordinate addition/subtraction
    // stays representable. The last in-grid cells remain usable at the bound.
    {
        bool rejectedZero = false;
        bool rejectedTooLarge = false;
        try {
            World invalid(catalog, 0, 1);
        } catch (const std::invalid_argument &) {
            rejectedZero = true;
        }
        try {
            World invalid(catalog, MaxWorldGridSize + std::uint32_t{1}, 1);
        } catch (const std::invalid_argument &) {
            rejectedTooLarge = true;
        }
        Check(rejectedZero && rejectedTooLarge, "invalid world dimensions throw invalid_argument");

        World edge(catalog, MaxWorldGridSize, 1);
        const auto road = edge.BuildRoad({INT32_MAX - 2, 0}, {INT32_MAX - 1, 0});
        Check(road != 0 && edge.Step().accepted && edge.Snapshot().roads.size() == 1,
              "road adjacent to maximum grid coordinate stays in bounds");
    }

    // All five counters must be nonzero, in range for their ID type, and
    // strictly beyond every live or historical ID that can be reused.
    for (std::size_t index = 0; index < 5; ++index) {
        RejectTransactionally(canonical, catalog,
                              [=](const std::string &text) { return SetNext(text, index, "0"); },
                              "NEXT counter " + std::to_string(index) + " zero");
        RejectTransactionally(canonical, catalog,
                              [=](const std::string &text) { return SetNext(text, index, "-1"); },
                              "NEXT counter " + std::to_string(index) + " signed token");
        RejectTransactionally(canonical, catalog,
                              [=](const std::string &text) {
                                  return SetNext(text, index, "18446744073709551616");
                              },
                              "NEXT counter " + std::to_string(index) + " uint64 overflow");
    }
    for (std::size_t index = 0; index < 4; ++index)
        RejectTransactionally(canonical, catalog,
                              [=](const std::string &text) {
                                  return SetNext(text, index, "4294967296");
                              },
                              "typed NEXT counter out of range " + std::to_string(index));
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SetNext(text, 4, "18446744073709551615"); },
                          "command NEXT counter cannot safely advance");
    for (const auto &value : {"1", "4"})
        RejectTransactionally(canonical, catalog,
                              [value](const std::string &text) { return SetNext(text, 0, value); },
                              "building NEXT reused or not above maximum");
    for (const auto &value : {"1", "9"})
        RejectTransactionally(canonical, catalog,
                              [value](const std::string &text) { return SetNext(text, 1, value); },
                              "road NEXT reused or not above maximum");
    for (const auto &value : {"1", "2"})
        RejectTransactionally(canonical, catalog,
                              [value](const std::string &text) { return SetNext(text, 2, value); },
                              "order NEXT reused or not above maximum");
    for (const auto &value : {"1", "2"})
        RejectTransactionally(canonical, catalog,
                              [value](const std::string &text) { return SetNext(text, 3, value); },
                              "vehicle NEXT reused or not above maximum");

    // Writer ordering is canonical by ID; duplicate IDs are separately
    // invalid even if the rows otherwise describe valid entities.
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SwapRows(text, "B", 0, 1); },
                          "unsorted buildings");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SetField(text, "B", 1, 1, "1"); },
                          "duplicate building IDs");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SwapRows(text, "R", 0, 1); },
                          "unsorted roads");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SetField(text, "R", 1, 1, "1"); },
                          "duplicate road IDs");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SwapRows(text, "O", 0, 1); },
                          "unsorted orders");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SetField(text, "O", 1, 1, "1"); },
                          "duplicate order IDs");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SetField(text, "O", 1, 6, "1"); },
                          "duplicate vehicle IDs");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) {
                              auto reordered = SetField(text, "O", 0, 6, "2");
                              return SetField(reordered, "O", 1, 6, "1");
                          },
                          "unsorted vehicle IDs");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SwapRows(text, "C", 0, 1); },
                          "unsorted command result IDs");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SetField(text, "C", 0, 1, "0"); },
                          "zero command result ID");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SetField(text, "C", 1, 1, "1"); },
                          "duplicate command result IDs");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SetField(text, "C", 1, 1, "99"); },
                          "command result ID gap or at/above next");

    // Strict booleans are required on every persisted boolean record field.
    for (const auto &bad : {"2", "-1"}) {
        RejectTransactionally(canonical, catalog,
                              [bad](const std::string &text) { return SetField(text, "B", 0, 5, bad); },
                              "B enabled boolean " + std::string(bad));
        RejectTransactionally(canonical, catalog,
                              [bad](const std::string &text) { return SetField(text, "R", 0, 6, bad); },
                              "R enabled boolean " + std::string(bad));
        RejectTransactionally(canonical, catalog,
                              [bad](const std::string &text) { return SetField(text, "O", 0, 12, bad); },
                              "O direction boolean " + std::string(bad));
        RejectTransactionally(canonical, catalog,
                              [bad](const std::string &text) { return SetField(text, "Q", 0, 10, bad); },
                              "Q enabled boolean " + std::string(bad));
    }

    const std::string queuedId0 = RecordField(canonical, "Q", 0, 1);
    const std::string queuedId2 = RecordField(canonical, "Q", 2, 1);
    const std::string nextCommand = RecordField(canonical, "NEXT", 0, 5);
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SetField(text, "Q", 0, 1, "0"); },
                          "pending command ID zero");
    RejectTransactionally(canonical, catalog,
                          [queuedId0](const std::string &text) {
                              return SetField(text, "Q", 1, 1, queuedId0);
                          },
                          "pending command ID duplicate");
    RejectTransactionally(canonical, catalog,
                          [nextCommand](const std::string &text) {
                              return SetField(text, "Q", 0, 1, nextCommand);
                          },
                          "pending command ID at or above next");
    RejectTransactionally(canonical, catalog,
                          [queuedId0, queuedId2](const std::string &text) {
                              auto swapped = SetField(text, "Q", 0, 1, queuedId2);
                              return SetField(swapped, "Q", 2, 1, queuedId0);
                          },
                          "pending command IDs out of order");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SetField(text, "Q", 0, 1, "-1"); },
                          "pending command ID signed token");

    // Pair each pending command with its Queued result, and reject either
    // direction of a missing/orphan association.
    RejectTransactionally(canonical, catalog,
                          [queuedId0](const std::string &text) {
                              return SetRecordById(text, "C", 1, queuedId0, 2, "2");
                          },
                          "pending command has terminal result");
    RejectTransactionally(canonical, catalog,
                          [queuedId0](const std::string &text) {
                              return SetRecordById(text, "C", 1, queuedId0, 1, "99");
                          },
                          "pending command has no matching result");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) {
                              return SetField(text, "Q", 0, 1, "99");
                          },
                          "orphan queued result");
    RejectTransactionally(canonical, catalog,
                          [queuedId0](const std::string &text) {
                              return SetRecordById(text, "C", 1, queuedId0, 2, "0");
                          },
                          "unknown command result state");
    RejectTransactionally(canonical, catalog,
                          [queuedId0](const std::string &text) {
                              return SetRecordById(text, "C", 1, queuedId0, 3, "1");
                          },
                          "queued result has failure");
    for (std::size_t field : {4u, 5u, 6u})
        RejectTransactionally(canonical, catalog,
                              [queuedId0, field](const std::string &text) {
                                  return SetRecordById(text, "C", 1, queuedId0, field, "1");
                              },
                              "queued result has entity reference field " + std::to_string(field));
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SetField(text, "C", 0, 3, "4294967295"); },
                          "command result failure out of range");
    RejectTransactionally(canonical, catalog,
                          [queuedId0](const std::string &text) {
                              return SetRecordById(text, "C", 1, queuedId0, 2, "2");
                          },
                          "accepted result for pending command");

    {
        World rejected(catalog, 8, 8);
        const auto bad = rejected.PlaceBuilding(99, {2, 2});
        rejected.Step();
        Check(rejected.GetCommandResult(bad).state == CommandState::Rejected &&
                  rejected.GetCommandResult(bad).failure != CommandFailure::None,
              "rejected result fixture records a failure");
        const auto rejectedSave = SaveWorld(rejected);
        RejectTransactionally(rejectedSave, catalog,
                              [](const std::string &text) { return SetField(text, "C", 0, 3, "0"); },
                              "rejected result with no failure");
        RejectTransactionally(rejectedSave, catalog,
                              [](const std::string &text) { return SetField(text, "C", 0, 2, "2"); },
                              "accepted result with failure");
        World restored(catalog, 1, 1);
        ParseError error;
        Check(LoadWorld(rejectedSave, catalog, restored, error),
              "valid rejected result with failure loads");
    }

    // Numeric extraction must reject signed/overflow values instead of
    // wrapping into valid looking IDs or counters.
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SetField(text, "B", 0, 1, "-1"); },
                          "signed building ID");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) {
                              return SetField(text, "B", 0, 1, "18446744073709551616");
                          },
                          "overflow building ID");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) { return SetField(text, "R", 0, 1, "-1"); },
                          "signed road ID");
    RejectTransactionally(canonical, catalog,
                          [](const std::string &text) {
                              return SetField(text, "C", 0, 1, "18446744073709551616");
                          },
                          "overflow command ID");

    // The source and destination below are produced earlier in the same tick
    // than the haul command. Loading must not require endpoints to exist yet;
    // stepping proves command application preserves the intended order.
    {
        World queued(catalog, 12, 8);
        queued.PlaceBuilding(0, {1, 2});
        queued.PlaceBuilding(1, {6, 2});
        queued.CreateHaulOrder(1, 2, 0, 0);
        const std::string forwardReferences = SaveWorld(queued);
        World restored(catalog, 1, 1);
        ParseError error;
        Check(LoadWorld(forwardReferences, catalog, restored, error),
              "pending commands may reference entities created earlier in the same tick");
        Check(SaveWorld(restored) == forwardReferences,
              "forward-reference pending save stays canonical after load");
        Check(restored.Step().accepted && restored.Snapshot().buildings.size() == 2 &&
                  restored.Snapshot().orders.size() == 1,
              "forward-reference commands execute in queue order after load");
        World uninterrupted(catalog, 12, 8);
        uninterrupted.PlaceBuilding(0, {1, 2});
        uninterrupted.PlaceBuilding(1, {6, 2});
        uninterrupted.CreateHaulOrder(1, 2, 0, 0);
        uninterrupted.Step();
        Check(uninterrupted.Snapshot().stateHash == restored.Snapshot().stateHash,
              "loaded forward-reference command replay matches uninterrupted execution");
    }
    {
        World queued(catalog, 8, 8);
        const auto pending = queued.SetBuildingEnabled(999, false);
        const auto text = SaveWorld(queued);
        World restored(catalog, 1, 1);
        ParseError error;
        Check(LoadWorld(text, catalog, restored, error),
              "pending command with a runtime-invalid target remains serializable");
        Check(restored.Step().accepted &&
                  restored.GetCommandResult(pending).state == CommandState::Rejected,
              "runtime-invalid pending target rejects at tick boundary after load");
    }

    // A removed entity remains represented by its terminal creation result;
    // its ID may not become available for reuse after loading.
    {
        World removed(catalog, 8, 8);
        auto create = removed.PlaceBuilding(0, {2, 2});
        removed.Step();
        const auto created = removed.GetCommandResult(create);
        auto remove = removed.RemoveBuilding(created.building);
        removed.Step();
        const auto terminal = removed.GetCommandResult(remove);
        Check(terminal.state == CommandState::Accepted && terminal.building == created.building,
              "removed building has a terminal result retaining its ID");
        auto save = SaveWorld(removed);
        save = SetNext(save, 0, std::to_string(created.building));
        RejectTransactionally(save, catalog, [](const std::string &text) { return text; },
                              "NEXT reuses removed maximum building ID from terminal result");
        World restored(catalog, 1, 1);
        ParseError error;
        Check(LoadWorld(SaveWorld(removed), catalog, restored, error),
              "world with removed building loads with terminal ID history");
        const auto recreate = restored.PlaceBuilding(0, {2, 2});
        restored.Step();
        Check(restored.GetCommandResult(recreate).building > created.building,
              "new building ID advances beyond removed historical ID after load");
    }
    {
        World cancelled(catalog, 16, 8);
        AddRoadLine(cancelled, 1, 6, 1);
        cancelled.PlaceBuilding(0, {1, 2});
        cancelled.PlaceBuilding(1, {5, 2});
        cancelled.Step();
        auto create = cancelled.CreateHaulOrder(1, 2, 0, 0);
        const OrderId order = 1;
        auto cancel = cancelled.CancelHaulOrder(order);
        cancelled.Step();
        const auto terminal = cancelled.GetCommandResult(create);
        Check(terminal.state == CommandState::Accepted && terminal.order == order &&
                  cancelled.GetCommandResult(cancel).state == CommandState::Accepted &&
                  cancelled.Snapshot().orders.empty(),
              "same-tick created then cancelled haul retains terminal order history");
        auto save = SaveWorld(cancelled);
        save = SetNext(save, 2, std::to_string(order));
        RejectTransactionally(save, catalog, [](const std::string &text) { return text; },
                              "NEXT reuses cancelled maximum order ID from terminal result");
        World restored(catalog, 1, 1);
        ParseError error;
        Check(LoadWorld(SaveWorld(cancelled), catalog, restored, error),
              "world with cancelled haul loads with terminal ID history");
        const auto nextOrderCommand = restored.CreateHaulOrder(1, 2, 0, 0);
        restored.Step();
        Check(restored.GetCommandResult(nextOrderCommand).order > order,
              "new haul order ID advances beyond cancelled historical ID after load");
    }

    const std::string loaded = LoadedCargoCanonical(catalog);
    bool hasLoadedCargo = false;
    for (const auto &line : Lines(loaded)) {
        const auto fields = Fields(line);
        hasLoadedCargo = hasLoadedCargo ||
                         (fields.size() > 9 && fields[0] == "O" && fields[9] != "0");
    }
    Check(hasLoadedCargo, "cargo validation fixture contains a loaded vehicle");
    RejectTransactionally(loaded, catalog,
                          [](const std::string &text) { return SetField(text, "O", 0, 12, "0"); },
                          "loaded cargo points toward source");
    RejectTransactionally(loaded, catalog,
                          [](const std::string &text) { return SetField(text, "O", 0, 10, "-1"); },
                          "vehicle cell outside grid");
    RejectTransactionally(loaded, catalog,
                          [](const std::string &text) { return SetField(text, "O", 0, 9, "5"); },
                          "cargo exceeds vehicle capacity");

    // A loaded valid snapshot round-trips and continues deterministically.
    {
        World original(catalog, 16, 8);
        ParseError error;
        Check(LoadWorld(loaded, catalog, original, error), "valid loaded-cargo snapshot loads");
        const auto savedAgain = SaveWorld(original);
        Check(savedAgain == loaded, "valid loaded-cargo snapshot remains canonical");
        World replay(catalog, 1, 1);
        Check(LoadWorld(savedAgain, catalog, replay, error), "canonical cargo snapshot reloads");
        for (int tick = 0; tick < 32; ++tick) {
            original.Step();
            replay.Step();
            Check(original.Snapshot().stateHash == replay.Snapshot().stateHash,
                  "loaded cargo replay has deterministic continuation");
        }
    }

    if (failures != 0) {
        std::cerr << failures << " save-integrity check(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "Economy save-integrity checks passed\n";
    return EXIT_SUCCESS;
}
