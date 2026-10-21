#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::sim {

using EntityId = std::uint32_t;
using Tick = std::uint64_t;

enum class Resource : std::uint8_t { None = 0, Grain = 1, Flour = 2 };
enum class TruckStage : std::uint8_t {
    LoadGrainAtFarm = 0,
    DeliverGrainToMill = 1,
    LoadFlourAtMill = 2,
    DeliverFlourToWarehouse = 3,
    ReturnToFarm = 4
};

struct InventoryView {
    std::uint32_t grain{};
    std::uint32_t flour{};
};

struct PlacedStorageView {
    EntityId id{};
    std::int32_t gridX{};
    std::int32_t gridZ{};
};

struct RoadView {
    EntityId id{};
    EntityId a{};
    EntityId b{};
    bool enabled{};
};

struct SimulationRules {
    std::uint32_t farmGrainPerHarvest{2};
    std::uint32_t farmHarvestIntervalTicks{2};
    std::uint32_t millGrainPerTick{1};
};

struct Snapshot {
    Tick tick{};
    InventoryView farm;
    InventoryView mill;
    InventoryView warehouse;
    EntityId truckNode{};
    Resource truckCargo{Resource::None};
    std::uint32_t truckCargoAmount{};
    std::uint64_t deliveredFlourTotal{};
    TruckStage truckStage{TruckStage::LoadGrainAtFarm};
    std::vector<EntityId> activeRoute;
    std::size_t routeCursor{};
    std::vector<PlacedStorageView> prototypeStorages;
    std::vector<RoadView> roads;
    SimulationRules rules;
};

// OpenRepublic-authored demonstration scenario. Values are synthetic and do not
// claim to reproduce WRSR's undocumented economy or transport formulas.
class Simulation final {
public:
    static constexpr EntityId FarmId = 1001;
    static constexpr EntityId MillId = 1002;
    static constexpr EntityId WarehouseId = 1003;
    static constexpr EntityId TruckId = 2001;
    static constexpr EntityId FarmNodeId = 1;
    static constexpr EntityId MillNodeId = 3;
    static constexpr EntityId WarehouseNodeId = 5;
    static constexpr EntityId FirstPrototypeStorageId = 3001;
    static constexpr std::size_t MaxPrototypeStorages = 8;
    static constexpr std::int32_t PrototypeGridMin = -16;
    static constexpr std::int32_t PrototypeGridMax = 16;

    Simulation();
    static Simulation CreateSyntheticDemo();
    ~Simulation();
    Simulation(Simulation&&) noexcept;
    Simulation& operator=(Simulation&&) noexcept;
    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    void Step();
    void Step(std::uint64_t count);
    Snapshot GetSnapshot() const;
    SimulationRules GetRules() const;
    bool SetRules(const SimulationRules& rules, std::string* error = nullptr);

    // Non-WRSR synthetic placement prototype. Returns zero on invalid or duplicate cells.
    EntityId PlacePrototypeStorage(std::int32_t gridX, std::int32_t gridZ);

    // Toggle a synthetic road edge for scenario construction and connectivity checks.
    // Edge IDs in the default scenario are 201..204.
    bool SetRoadEnabled(EntityId edgeId, bool enabled);

    // Canonical text save format, versioned independently from WRSR saves.
    std::string Save() const;
    // Parsing and invariant checks occur in temporary state. On failure this
    // simulation remains unchanged at the public state level.
    bool Load(std::string_view data, std::string* error = nullptr);

private:
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace openrepublic::sim
