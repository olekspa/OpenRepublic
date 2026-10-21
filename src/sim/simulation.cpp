#include "openrepublic/sim/simulation.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <limits>
#include <map>
#include <queue>
#include <sstream>
#include <utility>

namespace openrepublic::sim {
namespace {
constexpr std::uint32_t FarmGrainCapacity = 12;
constexpr std::uint32_t MillGrainCapacity = 4;
constexpr std::uint32_t MillFlourCapacity = 4;
constexpr std::uint32_t WarehouseFlourCapacity = 5;
constexpr std::uint32_t TruckCapacity = 2;
constexpr std::array<EntityId, 5> Nodes{1, 2, 3, 4, 5};
struct Edge { EntityId id; EntityId a; EntityId b; bool enabled; };

template <typename UInt>
bool ReadUnsigned(std::istream& in, UInt& value) {
    std::string token;
    if (!(in >> token) || token.empty()) return false;
    for (const unsigned char ch : token) if (!std::isdigit(ch)) return false;
    UInt parsed{};
    const auto result = std::from_chars(token.data(), token.data() + token.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) return false;
    value = parsed;
    return true;
}

template <typename Int>
bool ReadSigned(std::istream& in, Int& value) {
    std::string token;
    if (!(in >> token) || token.empty()) return false;
    std::size_t firstDigit = token[0] == '-' ? 1 : 0;
    if (firstDigit == token.size()) return false;
    for (std::size_t i = firstDigit; i < token.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(token[i]))) return false;
    Int parsed{};
    const auto result = std::from_chars(token.data(), token.data() + token.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) return false;
    value = parsed;
    return true;
}

bool HasNode(EntityId node) {
    return std::find(Nodes.begin(), Nodes.end(), node) != Nodes.end();
}
}

struct Simulation::State {
    Tick tick{};
    SimulationRules rules{};
    std::uint64_t deliveredFlourTotal{};
    std::uint32_t farmGrain{};
    std::uint32_t millGrain{};
    std::uint32_t millFlour{};
    std::uint32_t warehouseFlour{};
    EntityId truckNode{FarmNodeId};
    Resource cargo{Resource::None};
    std::uint32_t cargoAmount{};
    TruckStage stage{TruckStage::LoadGrainAtFarm};
    std::vector<EntityId> route;
    std::size_t routeCursor{};
    std::vector<PlacedStorageView> prototypeStorages;
    std::array<Edge, 4> edges{{
        {201, 1, 2, true}, {202, 2, 3, true},
        {203, 3, 4, true}, {204, 4, 5, true}
    }};

    std::vector<EntityId> FindRoute(EntityId from, EntityId to) const {
        if (!HasNode(from) || !HasNode(to)) return {};
        std::map<EntityId, EntityId> previous;
        std::queue<EntityId> pending;
        previous.emplace(from, from);
        pending.push(from);
        while (!pending.empty()) {
            const auto node = pending.front();
            pending.pop();
            if (node == to) break;
            // Edges are stored in stable ID order; this is the canonical tie-break.
            for (const auto& edge : edges) {
                if (!edge.enabled) continue;
                EntityId next = 0;
                if (edge.a == node) next = edge.b;
                else if (edge.b == node) next = edge.a;
                if (next && previous.emplace(next, node).second) pending.push(next);
            }
        }
        if (!previous.count(to)) return {};
        std::vector<EntityId> result;
        for (auto node = to;; node = previous.at(node)) {
            result.push_back(node);
            if (node == from) break;
        }
        std::reverse(result.begin(), result.end());
        return result;
    }

    bool RouteUses(EntityId edgeId) const {
        const auto edgeIt = std::find_if(edges.begin(), edges.end(), [edgeId](const Edge& e) { return e.id == edgeId; });
        if (edgeIt == edges.end() || route.size() < 2) return false;
        for (std::size_t i = routeCursor; i + 1 < route.size(); ++i) {
            const auto a = route[i], b = route[i + 1];
            if ((a == edgeIt->a && b == edgeIt->b) || (a == edgeIt->b && b == edgeIt->a)) return true;
        }
        return false;
    }

    void StartRoute(EntityId target) {
        auto path = FindRoute(truckNode, target);
        if (path.size() < 2) return;
        route = std::move(path);
        routeCursor = 0;
    }

    void MoveOneEdge() {
        if (route.empty() || routeCursor + 1 >= route.size()) return;
        const auto next = route[routeCursor + 1];
        const auto edge = std::find_if(edges.begin(), edges.end(), [this, next](const Edge& e) {
            return e.enabled && ((e.a == truckNode && e.b == next) || (e.b == truckNode && e.a == next));
        });
        if (edge == edges.end()) {
            route.clear();
            routeCursor = 0;
            return;
        }
        truckNode = next;
        ++routeCursor;
    }

    void ServiceTruck() {
        switch (stage) {
        case TruckStage::LoadGrainAtFarm: {
            if (truckNode != FarmNodeId || cargoAmount != 0) return;
            const auto path = FindRoute(truckNode, MillNodeId);
            if (path.size() < 2) return;
            const auto amount = std::min(TruckCapacity, farmGrain);
            if (!amount) return;
            farmGrain -= amount;
            cargo = Resource::Grain;
            cargoAmount = amount;
            stage = TruckStage::DeliverGrainToMill;
            route = path;
            routeCursor = 0;
            break;
        }
        case TruckStage::DeliverGrainToMill:
            if (truckNode != MillNodeId || route.empty() || routeCursor + 1 != route.size()) return;
            {
                const auto room = MillGrainCapacity - millGrain;
                const auto moved = std::min(room, cargoAmount);
                millGrain += moved;
                cargoAmount -= moved;
                if (cargoAmount == 0) {
                    cargo = Resource::None;
                    stage = TruckStage::LoadFlourAtMill;
                    route.clear();
                    routeCursor = 0;
                }
            }
            break;
        case TruckStage::LoadFlourAtMill: {
            if (truckNode != MillNodeId || cargoAmount != 0) return;
            const auto path = FindRoute(truckNode, WarehouseNodeId);
            if (path.size() < 2) return;
            const auto amount = std::min(TruckCapacity, millFlour);
            if (!amount) return;
            millFlour -= amount;
            cargo = Resource::Flour;
            cargoAmount = amount;
            stage = TruckStage::DeliverFlourToWarehouse;
            route = path;
            routeCursor = 0;
            break;
        }
        case TruckStage::DeliverFlourToWarehouse:
            if (truckNode != WarehouseNodeId || route.empty() || routeCursor + 1 != route.size()) return;
            {
                const auto room = WarehouseFlourCapacity - warehouseFlour;
                const auto moved = std::min(room, cargoAmount);
                warehouseFlour += moved;
                if (std::numeric_limits<std::uint64_t>::max() - deliveredFlourTotal < moved)
                    deliveredFlourTotal = std::numeric_limits<std::uint64_t>::max();
                else
                    deliveredFlourTotal += moved;
                cargoAmount -= moved;
                if (cargoAmount == 0) {
                    cargo = Resource::None;
                    stage = TruckStage::ReturnToFarm;
                    route.clear();
                    routeCursor = 0;
                }
            }
            break;
        case TruckStage::ReturnToFarm:
            if (truckNode == FarmNodeId) {
                stage = TruckStage::LoadGrainAtFarm;
                route.clear();
                routeCursor = 0;
            } else if (route.empty()) {
                StartRoute(FarmNodeId);
            }
            break;
        }
    }
};

Simulation::Simulation() : state_(std::make_unique<State>()) {}
Simulation::~Simulation() = default;
Simulation Simulation::CreateSyntheticDemo() { return Simulation{}; }
Simulation::Simulation(Simulation&&) noexcept = default;
Simulation& Simulation::operator=(Simulation&&) noexcept = default;

void Simulation::Step() {
    if (state_->tick == std::numeric_limits<Tick>::max()) return;
    // Synthetic rules are data-driven; the mill conversion remains 1:1 and
    // process/output limits are explicit OpenRepublic prototype constraints.
    ++state_->tick;
    if (state_->tick % state_->rules.farmHarvestIntervalTicks == 0 && state_->farmGrain < FarmGrainCapacity) {
        state_->farmGrain += std::min(state_->rules.farmGrainPerHarvest, FarmGrainCapacity - state_->farmGrain);
    }
    if (state_->millGrain > 0 && state_->millFlour < MillFlourCapacity) {
        const auto processed = std::min({state_->millGrain, state_->rules.millGrainPerTick,
            MillFlourCapacity - state_->millFlour});
        state_->millGrain -= processed;
        state_->millFlour += processed;
    }
    if (!state_->route.empty() && state_->routeCursor + 1 < state_->route.size()) {
        state_->MoveOneEdge();
        return;
    }
    state_->ServiceTruck();
}

void Simulation::Step(std::uint64_t count) {
    for (std::uint64_t i = 0; i < count; ++i) Step();
}

Snapshot Simulation::GetSnapshot() const {
    std::vector<RoadView> roads;
    roads.reserve(state_->edges.size());
    for (const auto& edge : state_->edges) roads.push_back(RoadView{edge.id, edge.a, edge.b, edge.enabled});
    return Snapshot{
        state_->tick,
        InventoryView{state_->farmGrain, 0},
        InventoryView{state_->millGrain, state_->millFlour},
        InventoryView{0, state_->warehouseFlour},
        state_->truckNode,
        state_->cargo,
        state_->cargoAmount,
        state_->deliveredFlourTotal,
        state_->stage,
        state_->route,
        state_->routeCursor,
        state_->prototypeStorages,
        std::move(roads),
        state_->rules
    };
}

SimulationRules Simulation::GetRules() const {
    return state_->rules;
}

bool Simulation::SetRules(const SimulationRules& rules, std::string* error) {
    const char* problem = nullptr;
    if (rules.farmGrainPerHarvest < 1 || rules.farmGrainPerHarvest > FarmGrainCapacity)
        problem = "farmGrainPerHarvest must be between 1 and 12";
    else if (rules.farmHarvestIntervalTicks < 1 || rules.farmHarvestIntervalTicks > 10000)
        problem = "farmHarvestIntervalTicks must be between 1 and 10000";
    else if (rules.millGrainPerTick < 1 || rules.millGrainPerTick > MillGrainCapacity)
        problem = "millGrainPerTick must be between 1 and 4";
    if (problem) {
        if (error) *error = problem;
        return false;
    }
    state_->rules = rules;
    if (error) error->clear();
    return true;
}

EntityId Simulation::PlacePrototypeStorage(std::int32_t gridX, std::int32_t gridZ) {
    if (gridX < PrototypeGridMin || gridX > PrototypeGridMax ||
        gridZ < PrototypeGridMin || gridZ > PrototypeGridMax ||
        state_->prototypeStorages.size() >= MaxPrototypeStorages) return 0;
    const auto duplicate = std::find_if(state_->prototypeStorages.begin(), state_->prototypeStorages.end(),
        [gridX, gridZ](const PlacedStorageView& item) { return item.gridX == gridX && item.gridZ == gridZ; });
    if (duplicate != state_->prototypeStorages.end()) return 0;
    const auto id = FirstPrototypeStorageId + static_cast<EntityId>(state_->prototypeStorages.size());
    state_->prototypeStorages.push_back(PlacedStorageView{id, gridX, gridZ});
    return id;
}

bool Simulation::SetRoadEnabled(EntityId edgeId, bool enabled) {
    const auto it = std::find_if(state_->edges.begin(), state_->edges.end(), [edgeId](const Edge& edge) { return edge.id == edgeId; });
    if (it == state_->edges.end()) return false;
    if (!enabled && state_->RouteUses(edgeId)) return false;
    it->enabled = enabled;
    return true;
}

std::string Simulation::Save() const {
    std::ostringstream out;
    out << "OPENREPUBLIC_SIM 3\n"
        << "TICK " << state_->tick << '\n'
        << "DELIVERED " << state_->deliveredFlourTotal << '\n'
        << "FARM " << state_->farmGrain << '\n'
        << "MILL " << state_->millGrain << ' ' << state_->millFlour << '\n'
        << "WAREHOUSE " << state_->warehouseFlour << '\n'
        << "TRUCK " << state_->truckNode << ' ' << static_cast<unsigned>(state_->stage) << ' '
        << static_cast<unsigned>(state_->cargo) << ' ' << state_->cargoAmount << ' '
        << state_->routeCursor << ' ' << state_->route.size();
    for (const auto node : state_->route) out << ' ' << node;
    out << '\n' << "ROADS";
    for (const auto& edge : state_->edges) out << ' ' << (edge.enabled ? 1 : 0);
    out << '\n' << "PROTOTYPE_STORAGES " << state_->prototypeStorages.size();
    for (const auto& storage : state_->prototypeStorages)
        out << ' ' << storage.id << ' ' << storage.gridX << ' ' << storage.gridZ;
    out << '\n' << "RULES " << state_->rules.farmGrainPerHarvest << ' '
        << state_->rules.farmHarvestIntervalTicks << ' ' << state_->rules.millGrainPerTick << '\n';
    return out.str();
}

bool Simulation::Load(std::string_view data, std::string* error) {
    auto fail = [error](const char* why) {
        if (error) *error = why;
        return false;
    };
    std::istringstream in{std::string(data)};
    std::string tag;
    unsigned version = 0;
    if (!(in >> tag) || tag != "OPENREPUBLIC_SIM" || !ReadUnsigned(in, version)) return fail("invalid save header");
    if (version < 1 || version > 3) return fail("unsupported save version");

    State candidate;
    std::uint64_t routeSize = 0;
    unsigned stage = 0, cargo = 0;
    if (!(in >> tag) || tag != "TICK" || !ReadUnsigned(in, candidate.tick)) return fail("invalid tick record");
    if (!(in >> tag) || tag != "DELIVERED" || !ReadUnsigned(in, candidate.deliveredFlourTotal)) return fail("invalid delivered-total record");
    if (!(in >> tag) || tag != "FARM" || !ReadUnsigned(in, candidate.farmGrain)) return fail("invalid farm record");
    if (!(in >> tag) || tag != "MILL" || !ReadUnsigned(in, candidate.millGrain) || !ReadUnsigned(in, candidate.millFlour)) return fail("invalid mill record");
    if (!(in >> tag) || tag != "WAREHOUSE" || !ReadUnsigned(in, candidate.warehouseFlour)) return fail("invalid warehouse record");
    if (!(in >> tag) || tag != "TRUCK" || !ReadUnsigned(in, candidate.truckNode) || !ReadUnsigned(in, stage) ||
        !ReadUnsigned(in, cargo) || !ReadUnsigned(in, candidate.cargoAmount) ||
        !ReadUnsigned(in, candidate.routeCursor) || !ReadUnsigned(in, routeSize)) return fail("invalid truck record");
    if (routeSize > Nodes.size()) return fail("route record too large");
    candidate.stage = static_cast<TruckStage>(stage);
    candidate.cargo = static_cast<Resource>(cargo);
    candidate.route.reserve(static_cast<std::size_t>(routeSize));
    for (std::uint64_t i = 0; i < routeSize; ++i) {
        EntityId node = 0;
        if (!ReadUnsigned(in, node)) return fail("truncated or invalid route record");
        candidate.route.push_back(node);
    }
    if (!(in >> tag) || tag != "ROADS") return fail("invalid roads record");
    for (auto& edge : candidate.edges) {
        unsigned enabled = 0;
        if (!ReadUnsigned(in, enabled) || enabled > 1) return fail("invalid road state");
        edge.enabled = enabled != 0;
    }
    if (version >= 2) {
        std::uint64_t storageCount = 0;
        if (!(in >> tag) || tag != "PROTOTYPE_STORAGES" || !ReadUnsigned(in, storageCount))
            return fail("invalid prototype storage record");
        if (storageCount > MaxPrototypeStorages) return fail("too many prototype storages");
        candidate.prototypeStorages.reserve(static_cast<std::size_t>(storageCount));
        for (std::uint64_t i = 0; i < storageCount; ++i) {
            PlacedStorageView storage;
            if (!ReadUnsigned(in, storage.id) || !ReadSigned(in, storage.gridX) || !ReadSigned(in, storage.gridZ))
                return fail("invalid prototype storage entry");
            candidate.prototypeStorages.push_back(storage);
        }
    }
    if (version >= 3) {
        if (!(in >> tag) || tag != "RULES" ||
            !ReadUnsigned(in, candidate.rules.farmGrainPerHarvest) ||
            !ReadUnsigned(in, candidate.rules.farmHarvestIntervalTicks) ||
            !ReadUnsigned(in, candidate.rules.millGrainPerTick)) return fail("invalid simulation rules record");
    }
    std::string extra;
    if (in >> extra) return fail("unexpected trailing save data");

    if (candidate.rules.farmGrainPerHarvest < 1 || candidate.rules.farmGrainPerHarvest > FarmGrainCapacity ||
        candidate.rules.farmHarvestIntervalTicks < 1 || candidate.rules.farmHarvestIntervalTicks > 10000 ||
        candidate.rules.millGrainPerTick < 1 || candidate.rules.millGrainPerTick > MillGrainCapacity)
        return fail("simulation rules are outside supported bounds");
    if (candidate.farmGrain > FarmGrainCapacity || candidate.millGrain > MillGrainCapacity ||
        candidate.millFlour > MillFlourCapacity || candidate.warehouseFlour > WarehouseFlourCapacity) return fail("inventory exceeds capacity");
    if (candidate.deliveredFlourTotal < candidate.warehouseFlour) return fail("delivered total is below current warehouse stock");
    for (std::size_t i = 0; i < candidate.prototypeStorages.size(); ++i) {
        const auto& storage = candidate.prototypeStorages[i];
        if (storage.id != FirstPrototypeStorageId + i) return fail("prototype storage IDs are not canonical");
        if (storage.gridX < PrototypeGridMin || storage.gridX > PrototypeGridMax ||
            storage.gridZ < PrototypeGridMin || storage.gridZ > PrototypeGridMax) return fail("prototype storage cell is out of bounds");
        for (std::size_t j = 0; j < i; ++j) {
            if (candidate.prototypeStorages[j].gridX == storage.gridX &&
                candidate.prototypeStorages[j].gridZ == storage.gridZ) return fail("duplicate prototype storage cell");
        }
    }
    if (!HasNode(candidate.truckNode)) return fail("truck references unknown node");
    if (stage > static_cast<unsigned>(TruckStage::ReturnToFarm) || cargo > static_cast<unsigned>(Resource::Flour)) return fail("unknown truck stage or cargo resource");
    if (candidate.cargoAmount > TruckCapacity || ((candidate.cargoAmount == 0) != (candidate.cargo == Resource::None))) return fail("invalid cargo amount/resource pair");
    if (candidate.route.empty()) {
        if (candidate.routeCursor != 0) return fail("empty route has nonzero cursor");
    } else {
        if (candidate.route.size() < 2 || candidate.routeCursor >= candidate.route.size()) return fail("invalid route cursor");
        if (candidate.route[candidate.routeCursor] != candidate.truckNode) return fail("truck node does not match route cursor");
        for (std::size_t i = 0; i < candidate.route.size(); ++i) {
            if (!HasNode(candidate.route[i])) return fail("route references unknown node");
            if (std::find(candidate.route.begin(), candidate.route.begin() + static_cast<std::ptrdiff_t>(i), candidate.route[i]) !=
                candidate.route.begin() + static_cast<std::ptrdiff_t>(i)) return fail("route repeats a node");
            if (i) {
                const auto a = candidate.route[i - 1], b = candidate.route[i];
                const auto edge = std::find_if(candidate.edges.begin(), candidate.edges.end(), [a, b](const Edge& e) {
                    return (e.a == a && e.b == b) || (e.a == b && e.b == a);
                });
                if (edge == candidate.edges.end()) return fail("route crosses missing road");
                // Passed segments are historical route data. A road may be closed
                // behind the truck; only the remaining route must stay traversable.
                if (i - 1 >= candidate.routeCursor && !edge->enabled) return fail("active route crosses disabled road");
            }
        }
    }
    switch (candidate.stage) {
    case TruckStage::LoadGrainAtFarm:
        if (candidate.truckNode != FarmNodeId || candidate.cargoAmount || !candidate.route.empty()) return fail("invalid farm loading state");
        break;
    case TruckStage::DeliverGrainToMill:
        if (candidate.cargo != Resource::Grain || candidate.cargoAmount == 0 || candidate.route.empty() ||
            candidate.route.front() != FarmNodeId || candidate.route.back() != MillNodeId) return fail("invalid grain delivery state");
        break;
    case TruckStage::LoadFlourAtMill:
        if (candidate.truckNode != MillNodeId || candidate.cargoAmount || !candidate.route.empty()) return fail("invalid mill loading state");
        break;
    case TruckStage::DeliverFlourToWarehouse:
        if (candidate.cargo != Resource::Flour || candidate.cargoAmount == 0 || candidate.route.empty() ||
            candidate.route.front() != MillNodeId || candidate.route.back() != WarehouseNodeId) return fail("invalid flour delivery state");
        break;
    case TruckStage::ReturnToFarm:
        if (candidate.cargoAmount || candidate.cargo != Resource::None) return fail("invalid return state cargo");
        if (!candidate.route.empty() && (candidate.route.front() != WarehouseNodeId || candidate.route.back() != FarmNodeId)) return fail("invalid return route");
        if (candidate.route.empty() && candidate.truckNode != WarehouseNodeId && candidate.truckNode != FarmNodeId) return fail("invalid return origin");
        break;
    }
    *state_ = std::move(candidate);
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::sim
