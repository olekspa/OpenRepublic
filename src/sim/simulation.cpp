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
constexpr std::uint64_t FirstFacilityCommandId = 1;
constexpr EntityId FirstDynamicRoadNodeId = 6;
constexpr EntityId FirstDynamicRoadId = 205;
struct Edge { EntityId id; EntityId a; EntityId b; bool enabled; };
struct RoadNode { EntityId id; std::int32_t x; std::int32_t z; };

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
    struct PendingCommand { std::uint64_t commandId; ConstructionCommandType type; FacilityType facilityType; std::int32_t ax, az, bx, bz; };
    std::vector<FacilityView> facilities;
    std::vector<PendingCommand> commandQueue;
    std::vector<ConstructionResult> commandResults;
    std::uint64_t nextCommandId{FirstFacilityCommandId};
    EntityId nextRoadNodeId{FirstDynamicRoadNodeId};
    EntityId nextRoadId{FirstDynamicRoadId};
    std::vector<RoadNode> nodes{{1,-2,0},{2,-1,0},{3,0,0},{4,1,0},{5,2,0}};
    std::vector<Edge> edges{{201,1,2,true},{202,2,3,true},{203,3,4,true},{204,4,5,true}};

    const RoadNode* NodeAt(std::int32_t x, std::int32_t z) const {
        const auto it = std::find_if(nodes.begin(), nodes.end(), [x,z](const RoadNode& n){ return n.x==x && n.z==z; });
        return it == nodes.end() ? nullptr : &*it;
    }
    bool HasNode(EntityId id) const {
        return std::any_of(nodes.begin(), nodes.end(), [id](const RoadNode& n){ return n.id==id; });
    }

    std::vector<EntityId> FindRoute(EntityId from, EntityId to, EntityId excludedEdgeId = 0) const {
        if (!HasNode(from) || !HasNode(to)) return {};
        std::map<EntityId, EntityId> previous;
        std::queue<EntityId> pending;
        previous.emplace(from, from);
        pending.push(from);
        while (!pending.empty()) {
            const auto node = pending.front();
            pending.pop();
            if (node == to) break;
            // Stable edge-ID order is the deterministic BFS tie-break.
            for (const auto& edge : edges) {
                if (!edge.enabled || edge.id == excludedEdgeId) continue;
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

    bool CellOccupied(std::int32_t x, std::int32_t z) const {
        return std::any_of(facilities.begin(), facilities.end(), [x, z](const FacilityView& f) {
            return f.gridX == x && f.gridZ == z;
        }) || std::any_of(prototypeStorages.begin(), prototypeStorages.end(), [x, z](const PlacedStorageView& s) {
            return s.gridX == x && s.gridZ == z;
        });
    }

    void ApplyCommands() {
        for (const auto& request : commandQueue) {
            bool accepted = false;
            EntityId entity = 0;
            if (request.type == ConstructionCommandType::FacilityPlacement) {
                const bool validType = request.facilityType == FacilityType::Farm || request.facilityType == FacilityType::Mill;
                const bool validCell = request.ax >= Simulation::PrototypeGridMin && request.ax <= Simulation::PrototypeGridMax &&
                    request.az >= Simulation::PrototypeGridMin && request.az <= Simulation::PrototypeGridMax && !CellOccupied(request.ax, request.az);
                accepted = validType && validCell && facilities.size() < Simulation::MaxFacilities;
                if (accepted) {
                    entity = Simulation::FirstFacilityId + static_cast<EntityId>(facilities.size());
                    facilities.push_back(FacilityView{entity, request.facilityType, request.ax, request.az});
                }
            } else {
                const auto dx = static_cast<std::int64_t>(request.bx) - request.ax;
                const auto dz = static_cast<std::int64_t>(request.bz) - request.az;
                const bool inBounds = request.ax >= Simulation::PrototypeGridMin && request.ax <= Simulation::PrototypeGridMax &&
                    request.az >= Simulation::PrototypeGridMin && request.az <= Simulation::PrototypeGridMax &&
                    request.bx >= Simulation::PrototypeGridMin && request.bx <= Simulation::PrototypeGridMax &&
                    request.bz >= Simulation::PrototypeGridMin && request.bz <= Simulation::PrototypeGridMax;
                const auto* a = NodeAt(request.ax, request.az);
                const auto* b = NodeAt(request.bx, request.bz);
                // Copy endpoint identity before appending any nodes: vector
                // growth may invalidate both lookup pointers.
                const bool hasA = a != nullptr;
                const bool hasB = b != nullptr;
                const EntityId existingA = hasA ? a->id : 0;
                const EntityId existingB = hasB ? b->id : 0;
                const bool adjacent = (dx == 1 || dx == -1) && dz == 0 || (dz == 1 || dz == -1) && dx == 0;
                const bool duplicate = hasA && hasB && std::any_of(edges.begin(), edges.end(), [existingA,existingB](const Edge& e){
                    return (e.a==existingA && e.b==existingB)||(e.a==existingB && e.b==existingA);
                });
                const auto missing = static_cast<std::size_t>(!hasA) + static_cast<std::size_t>(!hasB);
                accepted = inBounds && adjacent && !duplicate && edges.size() < Simulation::MaxRoadSegments &&
                    nodes.size() + missing <= Simulation::MaxRoadNodes && nextRoadId != std::numeric_limits<EntityId>::max() &&
                    nextRoadNodeId <= std::numeric_limits<EntityId>::max() - missing;
                if (accepted) {
                    // No mutation occurs until every geometric and capacity check passes.
                    EntityId aid = hasA ? existingA : nextRoadNodeId++;
                    EntityId bid = hasB ? existingB : nextRoadNodeId++;
                    if (!hasA) nodes.push_back(RoadNode{aid,request.ax,request.az});
                    if (!hasB) nodes.push_back(RoadNode{bid,request.bx,request.bz});
                    entity = nextRoadId++;
                    edges.push_back(Edge{entity,aid,bid,true});
                }
            }
            commandResults.push_back(ConstructionResult{request.commandId, request.type, accepted, entity});
        }
        commandQueue.clear();
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
    state_->ApplyCommands();
    const auto extraFarms = static_cast<std::uint32_t>(std::count_if(state_->facilities.begin(), state_->facilities.end(),
        [](const FacilityView& f) { return f.type == FacilityType::Farm; }));
    const auto extraMills = static_cast<std::uint32_t>(std::count_if(state_->facilities.begin(), state_->facilities.end(),
        [](const FacilityView& f) { return f.type == FacilityType::Mill; }));
    const auto farmRate = state_->rules.farmGrainPerHarvest * (1U + extraFarms);
    const auto millRate = state_->rules.millGrainPerTick * (1U + extraMills);
    if (state_->tick % state_->rules.farmHarvestIntervalTicks == 0 && state_->farmGrain < FarmGrainCapacity) {
        state_->farmGrain += std::min(farmRate, FarmGrainCapacity - state_->farmGrain);
    }
    if (state_->millGrain > 0 && state_->millFlour < MillFlourCapacity) {
        const auto processed = std::min({state_->millGrain, millRate,
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
    for (const auto& edge : state_->edges) {
        const auto a = std::find_if(state_->nodes.begin(), state_->nodes.end(), [&edge](const RoadNode& n){return n.id==edge.a;});
        const auto b = std::find_if(state_->nodes.begin(), state_->nodes.end(), [&edge](const RoadNode& n){return n.id==edge.b;});
        roads.push_back(RoadView{edge.id, edge.a, edge.b, edge.enabled, a->x, a->z, b->x, b->z});
    }
    std::vector<RoadNodeView> roadNodes;
    for (const auto& node : state_->nodes) roadNodes.push_back(RoadNodeView{node.id,node.x,node.z});
    std::vector<FacilityPlacementResult> legacyResults;
    for (const auto& result : state_->commandResults)
        if (result.type == ConstructionCommandType::FacilityPlacement)
            legacyResults.push_back(FacilityPlacementResult{result.commandId,result.accepted,result.entityId});
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
        state_->facilities,
        std::move(legacyResults),
        state_->commandResults,
        std::move(roadNodes),
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
        state_->prototypeStorages.size() >= MaxPrototypeStorages || state_->CellOccupied(gridX, gridZ)) return 0;
    const auto duplicate = std::find_if(state_->prototypeStorages.begin(), state_->prototypeStorages.end(),
        [gridX, gridZ](const PlacedStorageView& item) { return item.gridX == gridX && item.gridZ == gridZ; });
    if (duplicate != state_->prototypeStorages.end()) return 0;
    const auto id = FirstPrototypeStorageId + static_cast<EntityId>(state_->prototypeStorages.size());
    state_->prototypeStorages.push_back(PlacedStorageView{id, gridX, gridZ});
    return id;
}

std::uint64_t Simulation::QueueFacilityPlacement(FacilityType type, std::int32_t gridX, std::int32_t gridZ) {
    if (state_->tick == std::numeric_limits<Tick>::max() ||
        state_->commandQueue.size() >= MaxQueuedFacilityPlacements ||
        state_->commandQueue.size() + state_->commandResults.size() >= MaxUnacknowledgedFacilityResults ||
        state_->nextCommandId == std::numeric_limits<std::uint64_t>::max()) return 0;
    const auto id = state_->nextCommandId++;
    state_->commandQueue.push_back(State::PendingCommand{id, ConstructionCommandType::FacilityPlacement, type, gridX, gridZ, 0, 0});
    return id;
}

std::uint64_t Simulation::QueueRoadSegment(std::int32_t ax, std::int32_t az, std::int32_t bx, std::int32_t bz) {
    if (state_->tick == std::numeric_limits<Tick>::max() ||
        state_->commandQueue.size() >= MaxQueuedFacilityPlacements ||
        state_->commandQueue.size() + state_->commandResults.size() >= MaxUnacknowledgedFacilityResults ||
        state_->nextCommandId == std::numeric_limits<std::uint64_t>::max()) return 0;
    const auto id = state_->nextCommandId++;
    state_->commandQueue.push_back(State::PendingCommand{id, ConstructionCommandType::RoadSegment, FacilityType::Farm, ax, az, bx, bz});
    return id;
}

bool Simulation::AcknowledgeConstructionResult(std::uint64_t commandId) {
    const auto it = std::find_if(state_->commandResults.begin(), state_->commandResults.end(),
        [commandId](const ConstructionResult& result) { return result.commandId == commandId; });
    if (it == state_->commandResults.end()) return false;
    state_->commandResults.erase(it);
    return true;
}

bool Simulation::AcknowledgeFacilityPlacementResult(std::uint64_t commandId) {
    const auto it = std::find_if(state_->commandResults.begin(), state_->commandResults.end(),
        [commandId](const ConstructionResult& result) { return result.commandId == commandId && result.type == ConstructionCommandType::FacilityPlacement; });
    return it != state_->commandResults.end() && AcknowledgeConstructionResult(commandId);
}

bool Simulation::SetRoadEnabled(EntityId edgeId, bool enabled) {
    const auto it = std::find_if(state_->edges.begin(), state_->edges.end(), [edgeId](const Edge& edge) { return edge.id == edgeId; });
    if (it == state_->edges.end()) return false;
    if (!enabled && state_->RouteUses(edgeId)) {
        // Compute against the graph with this edge excluded before mutating
        // anything. A rejected closure must preserve both graph and truck state.
        auto alternate = state_->FindRoute(state_->truckNode, state_->route.back(), edgeId);
        if (alternate.size() < 2) return false;
        it->enabled = false;
        state_->route = std::move(alternate);
        state_->routeCursor = 0;
        return true;
    }
    it->enabled = enabled;
    return true;
}

std::string Simulation::Save() const {
    std::ostringstream out;
    out << "OPENREPUBLIC_SIM 6\n"
        << "TICK " << state_->tick << '\n'
        << "DELIVERED " << state_->deliveredFlourTotal << '\n'
        << "FARM " << state_->farmGrain << '\n'
        << "MILL " << state_->millGrain << ' ' << state_->millFlour << '\n'
        << "WAREHOUSE " << state_->warehouseFlour << '\n'
        << "TRUCK " << state_->truckNode << ' ' << static_cast<unsigned>(state_->stage) << ' '
        << static_cast<unsigned>(state_->cargo) << ' ' << state_->cargoAmount << ' '
        << state_->routeCursor << ' ' << state_->route.size();
    for (const auto node : state_->route) out << ' ' << node;
    out << '\n' << "ROAD_GRAPH " << state_->nextRoadNodeId << ' ' << state_->nextRoadId << ' ' << state_->nodes.size();
    for (const auto& node : state_->nodes) out << ' ' << node.id << ' ' << node.x << ' ' << node.z;
    out << ' ' << state_->edges.size();
    for (const auto& edge : state_->edges) out << ' ' << edge.id << ' ' << edge.a << ' ' << edge.b << ' ' << (edge.enabled ? 1 : 0);
    out << '\n' << "PROTOTYPE_STORAGES " << state_->prototypeStorages.size();
    for (const auto& storage : state_->prototypeStorages)
        out << ' ' << storage.id << ' ' << storage.gridX << ' ' << storage.gridZ;
    out << '\n' << "RULES " << state_->rules.farmGrainPerHarvest << ' '
        << state_->rules.farmHarvestIntervalTicks << ' ' << state_->rules.millGrainPerTick << '\n'
        << "FACILITIES " << state_->facilities.size();
    for (const auto& facility : state_->facilities)
        out << ' ' << facility.id << ' ' << static_cast<unsigned>(facility.type) << ' ' << facility.gridX << ' ' << facility.gridZ;
    out << '\n' << "CONSTRUCTION_QUEUE " << state_->nextCommandId << ' ' << state_->commandQueue.size();
    for (const auto& request : state_->commandQueue)
        out << ' ' << request.commandId << ' ' << static_cast<unsigned>(request.type) << ' '
            << static_cast<unsigned>(request.facilityType) << ' ' << request.ax << ' ' << request.az << ' ' << request.bx << ' ' << request.bz;
    out << '\n' << "CONSTRUCTION_RESULTS " << state_->commandResults.size();
    for (const auto& result : state_->commandResults)
        out << ' ' << result.commandId << ' ' << static_cast<unsigned>(result.type) << ' '
            << (result.accepted ? 1 : 0) << ' ' << result.entityId;
    out << '\n';
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
    if (version < 1 || version > 6) return fail("unsupported save version");

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
    if (routeSize > MaxRoadNodes) return fail("route record too large");
    candidate.stage = static_cast<TruckStage>(stage);
    candidate.cargo = static_cast<Resource>(cargo);
    candidate.route.reserve(static_cast<std::size_t>(routeSize));
    for (std::uint64_t i = 0; i < routeSize; ++i) {
        EntityId node = 0;
        if (!ReadUnsigned(in, node)) return fail("truncated or invalid route record");
        candidate.route.push_back(node);
    }
    if (version >= 6) {
        std::uint64_t nodeCount = 0, edgeCount = 0;
        if (!(in >> tag) || tag != "ROAD_GRAPH" || !ReadUnsigned(in, candidate.nextRoadNodeId) ||
            !ReadUnsigned(in, candidate.nextRoadId) || !ReadUnsigned(in, nodeCount) ||
            nodeCount < 5 || nodeCount > MaxRoadNodes) return fail("invalid road graph header");
        candidate.nodes.clear(); candidate.nodes.reserve(static_cast<std::size_t>(nodeCount));
        for (std::uint64_t i=0;i<nodeCount;++i) {
            RoadNode node{};
            if (!ReadUnsigned(in,node.id)||!ReadSigned(in,node.x)||!ReadSigned(in,node.z)) return fail("invalid road node");
            candidate.nodes.push_back(node);
        }
        if (!ReadUnsigned(in,edgeCount)||edgeCount<4||edgeCount>MaxRoadSegments) return fail("invalid road segment count");
        candidate.edges.clear(); candidate.edges.reserve(static_cast<std::size_t>(edgeCount));
        for (std::uint64_t i=0;i<edgeCount;++i) {
            Edge edge{}; unsigned enabled=0;
            if (!ReadUnsigned(in,edge.id)||!ReadUnsigned(in,edge.a)||!ReadUnsigned(in,edge.b)||!ReadUnsigned(in,enabled)||enabled>1)
                return fail("invalid road segment");
            edge.enabled=enabled!=0; candidate.edges.push_back(edge);
        }
    } else {
        if (!(in >> tag) || tag != "ROADS") return fail("invalid roads record");
        for (auto& edge : candidate.edges) {
            unsigned enabled = 0;
            if (!ReadUnsigned(in, enabled) || enabled > 1) return fail("invalid road state");
            edge.enabled = enabled != 0;
        }
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
    if (version >= 4) {
        std::uint64_t count = 0;
        if (!(in >> tag) || tag != "FACILITIES" || !ReadUnsigned(in, count)) return fail("invalid facilities record");
        if (count > MaxFacilities) return fail("too many facilities");
        candidate.facilities.reserve(static_cast<std::size_t>(count));
        for (std::uint64_t i = 0; i < count; ++i) {
            FacilityView facility;
            unsigned type = 0;
            if (!ReadUnsigned(in, facility.id) || !ReadUnsigned(in, type) ||
                !ReadSigned(in, facility.gridX) || !ReadSigned(in, facility.gridZ)) return fail("invalid facility entry");
            if (type != static_cast<unsigned>(FacilityType::Farm) && type != static_cast<unsigned>(FacilityType::Mill))
                return fail("unknown serialized facility type");
            facility.type = static_cast<FacilityType>(type);
            candidate.facilities.push_back(facility);
        }
        std::uint64_t queueCount = 0;
        if (version >= 6) {
            if (!(in >> tag) || tag != "CONSTRUCTION_QUEUE" || !ReadUnsigned(in,candidate.nextCommandId) || !ReadUnsigned(in,queueCount))
                return fail("invalid construction queue record");
            if (queueCount > MaxQueuedFacilityPlacements) return fail("too many queued construction commands");
            for (std::uint64_t i=0;i<queueCount;++i) {
                State::PendingCommand request{}; unsigned type=0, facilityType=0;
                if (!ReadUnsigned(in,request.commandId)||!ReadUnsigned(in,type)||!ReadUnsigned(in,facilityType)||
                    !ReadSigned(in,request.ax)||!ReadSigned(in,request.az)||!ReadSigned(in,request.bx)||!ReadSigned(in,request.bz))
                    return fail("invalid queued construction command");
                if (type > std::numeric_limits<std::uint8_t>::max() || facilityType > std::numeric_limits<std::uint8_t>::max())
                    return fail("queued construction enum exceeds representation");
                request.type=static_cast<ConstructionCommandType>(type); request.facilityType=static_cast<FacilityType>(facilityType);
                candidate.commandQueue.push_back(request);
            }
        } else if (!(in >> tag) || tag != "PLACEMENT_QUEUE" || !ReadUnsigned(in, candidate.nextCommandId) ||
            !ReadUnsigned(in, queueCount)) return fail("invalid placement queue record");
        if (version < 6) {
            if (queueCount > MaxQueuedFacilityPlacements) return fail("too many queued placements");
            for (std::uint64_t i = 0; i < queueCount; ++i) {
                std::uint64_t command=0; unsigned type=0; std::int32_t x=0,z=0;
                if (!ReadUnsigned(in,command) || !ReadUnsigned(in,type) || !ReadSigned(in,x) || !ReadSigned(in,z)) return fail("invalid queued placement entry");
                if (type > std::numeric_limits<std::uint8_t>::max()) return fail("queued placement type exceeds enum representation");
                candidate.commandQueue.push_back(State::PendingCommand{command,ConstructionCommandType::FacilityPlacement,static_cast<FacilityType>(type),x,z,0,0});
            }
        }
    }
    if (version >= 5) {
        std::uint64_t resultCount = 0;
        if (version >= 6) {
            if (!(in >> tag) || tag != "CONSTRUCTION_RESULTS" || !ReadUnsigned(in,resultCount)) return fail("invalid construction results record");
            if (resultCount > MaxUnacknowledgedFacilityResults) return fail("too many unacknowledged construction results");
            for (std::uint64_t i=0;i<resultCount;++i) {
                ConstructionResult result{}; unsigned type=0,accepted=0;
                if (!ReadUnsigned(in,result.commandId)||!ReadUnsigned(in,type)||!ReadUnsigned(in,accepted)||!ReadUnsigned(in,result.entityId)||accepted>1||
                    type>std::numeric_limits<std::uint8_t>::max()) return fail("invalid construction result entry");
                result.type=static_cast<ConstructionCommandType>(type); result.accepted=accepted!=0; candidate.commandResults.push_back(result);
            }
        } else if (!(in >> tag) || tag != "PLACEMENT_RESULTS" || !ReadUnsigned(in, resultCount))
            return fail("invalid placement results record");
        if (version < 6) {
            if (resultCount > MaxUnacknowledgedFacilityResults) return fail("too many unacknowledged placement results");
            for (std::uint64_t i=0;i<resultCount;++i) {
                ConstructionResult result{}; unsigned accepted=0;
                if (!ReadUnsigned(in,result.commandId)||!ReadUnsigned(in,accepted)||!ReadUnsigned(in,result.entityId)||accepted>1) return fail("invalid placement result entry");
                result.type=ConstructionCommandType::FacilityPlacement; result.accepted=accepted!=0; candidate.commandResults.push_back(result);
            }
        }
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
    for (std::size_t i = 0; i < candidate.facilities.size(); ++i) {
        const auto& facility = candidate.facilities[i];
        if (facility.id != FirstFacilityId + i) return fail("facility IDs are not canonical");
        if (facility.type != FacilityType::Farm && facility.type != FacilityType::Mill) return fail("unknown facility type");
        if (facility.gridX < PrototypeGridMin || facility.gridX > PrototypeGridMax ||
            facility.gridZ < PrototypeGridMin || facility.gridZ > PrototypeGridMax) return fail("facility cell is out of bounds");
        for (std::size_t j = 0; j < i; ++j)
            if (candidate.facilities[j].gridX == facility.gridX && candidate.facilities[j].gridZ == facility.gridZ)
                return fail("duplicate facility cell");
        for (const auto& storage : candidate.prototypeStorages)
            if (storage.gridX == facility.gridX && storage.gridZ == facility.gridZ) return fail("facility overlaps prototype storage");
    }
    if (candidate.nodes.size() < 5 || candidate.nodes.size() > MaxRoadNodes || candidate.edges.size() < 4 || candidate.edges.size() > MaxRoadSegments)
        return fail("road graph exceeds supported limits");
    for (std::size_t i=0;i<candidate.nodes.size();++i) {
        const auto& n=candidate.nodes[i];
        if (n.id != i+1 || n.x < PrototypeGridMin || n.x > PrototypeGridMax || n.z < PrototypeGridMin || n.z > PrototypeGridMax)
            return fail("road node IDs or coordinates are noncanonical");
        for (std::size_t j=0;j<i;++j) if (candidate.nodes[j].x==n.x && candidate.nodes[j].z==n.z) return fail("duplicate road node coordinate");
    }
    const std::int32_t seedX[5]={-2,-1,0,1,2};
    for (std::size_t i=0;i<5;++i) if (candidate.nodes[i].x!=seedX[i]||candidate.nodes[i].z!=0) return fail("legacy road anchors changed");
    if (candidate.nextRoadNodeId != candidate.nodes.size()+1 || candidate.nextRoadId != 201+candidate.edges.size())
        return fail("next road graph IDs are not canonical");
    for (std::size_t i=0;i<candidate.edges.size();++i) {
        const auto& e=candidate.edges[i];
        if (e.id != 201+i || !candidate.HasNode(e.a) || !candidate.HasNode(e.b) || e.a==e.b) return fail("road edge IDs or endpoints are invalid");
        const auto a=std::find_if(candidate.nodes.begin(),candidate.nodes.end(),[&e](const RoadNode& n){return n.id==e.a;});
        const auto b=std::find_if(candidate.nodes.begin(),candidate.nodes.end(),[&e](const RoadNode& n){return n.id==e.b;});
        const auto dx=static_cast<std::int64_t>(a->x)-b->x, dz=static_cast<std::int64_t>(a->z)-b->z;
        if (!((dx==1||dx==-1)&&dz==0 || (dz==1||dz==-1)&&dx==0)) return fail("road segment is not orthogonally adjacent");
        for (std::size_t j=0;j<i;++j) if ((candidate.edges[j].a==e.a&&candidate.edges[j].b==e.b)||(candidate.edges[j].a==e.b&&candidate.edges[j].b==e.a)) return fail("duplicate road edge");
    }
    const EntityId seedA[4]={1,2,3,4}, seedB[4]={2,3,4,5};
    for (std::size_t i=0;i<4;++i) {
        const auto& edge=candidate.edges[i];
        if (edge.id!=201+i || !((edge.a==seedA[i]&&edge.b==seedB[i])||(edge.a==seedB[i]&&edge.b==seedA[i])))
            return fail("legacy seed road IDs and endpoints are not canonical");
    }
    if (candidate.nodes[0].id!=FarmNodeId || candidate.nodes[2].id!=MillNodeId || candidate.nodes[4].id!=WarehouseNodeId)
        return fail("road service anchors are invalid");

    std::uint64_t previousCommand = 0;
    if (candidate.nextCommandId == 0) return fail("next construction command ID is invalid");
    for (const auto& request : candidate.commandQueue) {
        if (request.commandId <= previousCommand || request.commandId >= candidate.nextCommandId) return fail("queued command IDs are not canonical");
        if (previousCommand != 0 && request.commandId != previousCommand + 1) return fail("queued command IDs are not contiguous");
        if (request.type != ConstructionCommandType::FacilityPlacement && request.type != ConstructionCommandType::RoadSegment) return fail("unknown queued command type");
        previousCommand = request.commandId;
    }
    if (!candidate.commandQueue.empty() && candidate.commandQueue.back().commandId != candidate.nextCommandId-1) return fail("queued commands are not latest command suffix");
    if (candidate.tick==std::numeric_limits<Tick>::max()&&!candidate.commandQueue.empty()) return fail("terminal tick cannot contain queued commands");
    std::uint64_t previousResult=0; EntityId previousFacility=0, previousRoad=204;
    for (const auto& result : candidate.commandResults) {
        if (result.commandId<=previousResult||result.commandId>=candidate.nextCommandId) return fail("construction result IDs are not canonical");
        if (result.type!=ConstructionCommandType::FacilityPlacement&&result.type!=ConstructionCommandType::RoadSegment) return fail("unknown construction result type");
        if (result.accepted) {
            if (result.type==ConstructionCommandType::FacilityPlacement) {
                if (result.entityId<FirstFacilityId||result.entityId>=FirstFacilityId+candidate.facilities.size()||result.entityId<=previousFacility) return fail("facility result references invalid facility");
                previousFacility=result.entityId;
            } else {
                if (result.entityId<FirstDynamicRoadId||result.entityId>=candidate.nextRoadId||result.entityId<=previousRoad) return fail("road result references invalid edge");
                previousRoad=result.entityId;
            }
        } else if (result.entityId!=0) return fail("rejected construction result has entity ID");
        previousResult=result.commandId;
    }
    for (const auto& request : candidate.commandQueue) {
        if (request.commandId<=previousResult||request.commandId>=candidate.nextCommandId) return fail("outstanding command IDs are not globally ordered");
        previousResult=request.commandId;
    }
    if (candidate.commandQueue.size()+candidate.commandResults.size()>MaxUnacknowledgedFacilityResults) return fail("too many pending construction acknowledgments");
    if (!candidate.HasNode(candidate.truckNode)) return fail("truck references unknown node");
    if (stage > static_cast<unsigned>(TruckStage::ReturnToFarm) || cargo > static_cast<unsigned>(Resource::Flour)) return fail("unknown truck stage or cargo resource");
    if (candidate.cargoAmount > TruckCapacity || ((candidate.cargoAmount == 0) != (candidate.cargo == Resource::None))) return fail("invalid cargo amount/resource pair");
    if (candidate.route.empty()) {
        if (candidate.routeCursor != 0) return fail("empty route has nonzero cursor");
    } else {
        if (candidate.route.size() < 2 || candidate.routeCursor >= candidate.route.size()) return fail("invalid route cursor");
        if (candidate.route[candidate.routeCursor] != candidate.truckNode) return fail("truck node does not match route cursor");
        for (std::size_t i = 0; i < candidate.route.size(); ++i) {
            if (!candidate.HasNode(candidate.route[i])) return fail("route references unknown node");
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
            candidate.route.back() != MillNodeId)
            return fail("invalid grain delivery state");
        break;
    case TruckStage::LoadFlourAtMill:
        if (candidate.truckNode != MillNodeId || candidate.cargoAmount || !candidate.route.empty()) return fail("invalid mill loading state");
        break;
    case TruckStage::DeliverFlourToWarehouse:
        if (candidate.cargo != Resource::Flour || candidate.cargoAmount == 0 || candidate.route.empty() ||
            candidate.route.back() != WarehouseNodeId)
            return fail("invalid flour delivery state");
        break;
    case TruckStage::ReturnToFarm:
        if (candidate.cargoAmount || candidate.cargo != Resource::None) return fail("invalid return state cargo");
        if (!candidate.route.empty() && candidate.route.back() != FarmNodeId) return fail("invalid return route");
        if (candidate.route.empty() && candidate.truckNode != WarehouseNodeId && candidate.truckNode != FarmNodeId) return fail("invalid return origin");
        break;
    }
    *state_ = std::move(candidate);
    if (error) error->clear();
    return true;
}

} // namespace openrepublic::sim
