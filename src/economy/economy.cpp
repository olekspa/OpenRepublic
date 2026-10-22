#include "openrepublic/economy/economy.h"

#include <algorithm>
#include <atomic>
#include <thread>
#include <charconv>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <tuple>
#include <utility>

namespace openrepublic::economy {
namespace {
constexpr std::size_t MaxName = 96;
constexpr std::size_t MaxDefinitions = 1024;
constexpr std::size_t MaxText = 1024 * 1024;
constexpr std::size_t MaxLine = 4096;
constexpr std::uint32_t MaxStep = 100000;

// Unlike formatted stream extraction, unsigned from_chars never accepts a minus
// sign or silently narrows counters. Bound every field before it reaches state.
class WorldSaveInput final {
public:
    explicit WorldSaveInput(std::string_view text) : remaining_(text) {}
    explicit operator bool() const noexcept { return !failed_; }
    WorldSaveInput &operator>>(std::string &out) {
        const auto token = Next();
        if (!failed_)
            out.assign(token);
        return *this;
    }
    template <class T> WorldSaveInput &operator>>(T &out) {
        static_assert(std::is_integral_v<T>);
        const auto token = Next();
        if (failed_)
            return *this;
        if constexpr (std::is_same_v<T, bool>) {
            failed_ = token != "0" && token != "1";
            if (!failed_)
                out = token == "1";
        } else {
            T value{};
            const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
            failed_ = result.ec != std::errc{} || result.ptr != token.data() + token.size();
            if (!failed_)
                out = value;
        }
        return *this;
    }
private:
    static bool Space(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
    }
    std::string_view Next() {
        if (failed_)
            return {};
        while (!remaining_.empty() && Space(remaining_.front()))
            remaining_.remove_prefix(1);
        if (remaining_.empty()) {
            failed_ = true;
            return {};
        }
        std::size_t size = 0;
        while (size < remaining_.size() && !Space(remaining_[size]))
            ++size;
        const auto token = remaining_.substr(0, size);
        remaining_.remove_prefix(size);
        return token;
    }
    std::string_view remaining_;
    bool failed_{};
};

void CatError(CatalogErrorInfo &e, CatalogError c, const std::string &m) {
    e.code = c;
    e.message = m;
}
bool NameOk(std::string_view s) {
    if (s.empty() || s.size() > MaxName)
        return false;
    for (char c : s)
        if (c == '#' || c == ' ' || c == '\t' || c == '\r' || c == '\n')
            return false;
    return true;
}
bool Same(std::string_view a, const std::string &b) {
    return a.size() == b.size() && a == b;
}
template <class T> bool Unsigned(std::string_view s, T &out) {
    if (s.empty() || s.front() == '-')
        return false;
    T v{};
    const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
    if (r.ec != std::errc{} || r.ptr != s.data() + s.size())
        return false;
    out = v;
    return true;
}
bool Signed(std::string_view s, std::int32_t &out) {
    std::int64_t v{};
    if (s.empty())
        return false;
    const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
    if (r.ec != std::errc{} || r.ptr != s.data() + s.size() || v < std::numeric_limits<std::int32_t>::min() ||
        v > std::numeric_limits<std::int32_t>::max())
        return false;
    out = static_cast<std::int32_t>(v);
    return true;
}
std::vector<std::string> Words(std::string_view s) {
    std::vector<std::string> v;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r'))
            ++i;
        if (i == s.size())
            break;
        std::size_t b = i;
        while (i < s.size() && s[i] != ' ' && s[i] != '\t' && s[i] != '\r')
            ++i;
        v.emplace_back(s.substr(b, i - b));
    }
    return v;
}
void ParseErr(ParseError &e, ParseErrorCode c, std::uint32_t line, const std::string &m) {
    e.code = c;
    e.line = line;
    e.message = m;
}
std::string Num(std::uint64_t n) {
    return std::to_string(n);
}
bool InGrid(Cell c, std::uint32_t w, std::uint32_t d) {
    return c.x >= 0 && c.z >= 0 && static_cast<std::uint32_t>(c.x) < w && static_cast<std::uint32_t>(c.z) < d;
}
bool Less(Cell a, Cell b) {
    return a.x < b.x || (a.x == b.x && a.z < b.z);
}
bool Adjacent(Cell a, Cell b) {
    return (a.x == b.x && (a.z + 1 == b.z || b.z + 1 == a.z)) ||
           (a.z == b.z && (a.x + 1 == b.x || b.x + 1 == a.x));
}

struct Building {
    BuildingId id{};
    BuildingTypeId type{};
    Cell origin{};
    Cell access{};
    bool enabled{true};
    std::vector<std::uint64_t> inventory;
    std::vector<std::uint32_t> remaining;
    std::vector<std::uint64_t> completed;
    StallRecord stall;
};
struct Road {
    RoadId id{};
    Cell a{};
    Cell b{};
    bool enabled{true};
};
struct Vehicle {
    VehicleId id{};
    OrderId order{};
    VehicleTypeId type{};
    Cell cell{};
    std::uint64_t cargo{};
    HaulState state{HaulState::Idle};
    HaulFailure failure{HaulFailure::None};
    bool toDestination{true};
    std::vector<Cell> route;
    std::size_t cursor{};
};
struct Order {
    OrderId id{};
    BuildingId source{};
    BuildingId destination{};
    ResourceId resource{};
    VehicleTypeId vehicleType{};
    VehicleId vehicle{};
};
enum class Kind : std::uint8_t {
    Place = 1,
    Remove,
    Road,
    RoadEnabled,
    BuildingEnabled,
    Haul,
    Cancel,
    Retarget,
    ConnectUtility,
    DisconnectUtility
};
struct Pending {
    CommandId id{};
    Kind kind{Kind::Place};
    BuildingTypeId type{};
    BuildingId building{};
    Cell a{}, b{};
    RoadId road{};
    bool enabled{};
    BuildingId source{}, destination{};
    ResourceId resource{};
    VehicleTypeId vehicleType{};
    OrderId order{};
    // Utility endpoints are stored in source/destination so old command-record
    // layout stays canonical and the two new command kinds need no new fields.
};
struct State {
    const Catalog *catalog{};
    std::uint32_t width{}, depth{};
    std::uint64_t tick{}, revision{};
    BuildingId nextBuilding{1};
    RoadId nextRoad{1};
    OrderId nextOrder{1};
    VehicleId nextVehicle{1};
    CommandId nextCommand{1};
    CommandId firstRetainedCommand{1};
    std::vector<Building> buildings;
    std::vector<Road> roads;
    std::vector<Vehicle> vehicles;
    std::vector<Order> orders;
    std::vector<UtilityConnectionSnapshot> utilityConnections;
    std::set<CommandId> utilityCommands;
    std::vector<Pending> pending;
    std::map<CommandId, CommandResult> results;
};
std::size_t Slot(const BuildingTypeDef &t, ResourceId r) {
    for (std::size_t i = 0; i < t.storage.size(); ++i)
        if (t.storage[i].resource == r)
            return i;
    return t.storage.size();
}
// Buildings stay sorted by id: ids are issued in increasing order, removal keeps order, and each tick re-sorts.
Building *Find(State &s, BuildingId id) {
    const auto it = std::lower_bound(s.buildings.begin(), s.buildings.end(), id,
                                     [](const Building &b, BuildingId value) { return b.id < value; });
    return it != s.buildings.end() && it->id == id ? &*it : nullptr;
}
const Building *Find(const State &s, BuildingId id) {
    const auto it = std::lower_bound(s.buildings.begin(), s.buildings.end(), id,
                                     [](const Building &b, BuildingId value) { return b.id < value; });
    return it != s.buildings.end() && it->id == id ? &*it : nullptr;
}
Road *FindRoad(State &s, RoadId id) {
    for (auto &x : s.roads)
        if (x.id == id)
            return &x;
    return nullptr;
}
Order *FindOrder(State &s, OrderId id) {
    for (auto &x : s.orders)
        if (x.id == id)
            return &x;
    return nullptr;
}
Vehicle *FindVehicle(State &s, VehicleId id) {
    for (auto &x : s.vehicles)
        if (x.id == id)
            return &x;
    return nullptr;
}
const Vehicle *FindVehicle(const State &s, VehicleId id) {
    for (const auto &x : s.vehicles)
        if (x.id == id)
            return &x;
    return nullptr;
}
bool InFootprint(const BuildingTypeDef &t, Cell origin, Cell c) {
    return c.x >= origin.x && c.z >= origin.z &&
           static_cast<std::int64_t>(c.x) < static_cast<std::int64_t>(origin.x) + t.width &&
           static_cast<std::int64_t>(c.z) < static_cast<std::int64_t>(origin.z) + t.depth;
}
bool FootprintsOverlap(const BuildingTypeDef &a, Cell ao, const BuildingTypeDef &b, Cell bo) {
    return ao.x < static_cast<std::int64_t>(bo.x) + b.width &&
           bo.x < static_cast<std::int64_t>(ao.x) + a.width &&
           ao.z < static_cast<std::int64_t>(bo.z) + b.depth &&
           bo.z < static_cast<std::int64_t>(ao.z) + a.depth;
}
bool Occupied(const State &s, Cell c) {
    for (const auto &b : s.buildings)
        if (InFootprint(*s.catalog->BuildingType(b.type), b.origin, c))
            return true;
    return false;
}
bool RoadBetween(const State &s, Cell a, Cell b, bool open) {
    for (const auto &r : s.roads)
        if (((r.a == a && r.b == b) || (r.a == b && r.b == a)) && (!open || r.enabled))
            return true;
    return false;
}
bool RoadAt(const State &s, Cell c) {
    for (const auto &r : s.roads)
        if (r.enabled && (r.a == c || r.b == c))
            return true;
    return false;
}
CommandFailure PlacementFailure(const State &s, const BuildingTypeDef &t, Cell o) {
    if (o.x < 0 || o.z < 0 || static_cast<std::uint64_t>(o.x) + t.width > s.width ||
        static_cast<std::uint64_t>(o.z) + t.depth > s.depth)
        return CommandFailure::OutOfBounds;
    // Check overlap before access/road reservations so the diagnosis is stable
    // when several existing buildings conflict with the proposed placement.
    for (const auto &b : s.buildings)
        if (FootprintsOverlap(t, o, *s.catalog->BuildingType(b.type), b.origin))
            return CommandFailure::BuildingCollision;
    const Cell access{o.x + static_cast<std::int32_t>((t.width - 1) / 2), o.z - 1};
    if (!InGrid(access, s.width, s.depth) || Occupied(s, access))
        return CommandFailure::AccessCellUnavailable;
    for (const auto &b : s.buildings)
        if (InFootprint(t, o, b.access))
            return CommandFailure::AccessCellUnavailable;
    // Disabled roads retain their cells too: reopening must not put a road
    // through a building that was placed while it was closed.
    for (const auto &r : s.roads)
        if (InFootprint(t, o, r.a) || InFootprint(t, o, r.b))
            return CommandFailure::RoadOnBuilding;
    return CommandFailure::None;
}
const char *PlacementMessage(CommandFailure failure) {
    switch (failure) {
    case CommandFailure::OutOfBounds:
        return "building footprint is outside the grid";
    case CommandFailure::BuildingCollision:
        return "building footprint overlaps another building";
    case CommandFailure::AccessCellUnavailable:
        return "building access cell is outside the grid or would be blocked by a footprint";
    case CommandFailure::RoadOnBuilding:
        return "building footprint would cover an existing road";
    default:
        return "invalid building placement";
    }
}
std::vector<Cell> Path(const State &s, Cell start, Cell goal) {
    if (start == goal)
        return {start};
    if (!RoadAt(s, start) || !RoadAt(s, goal))
        return {};
    std::vector<Cell> seen{start}, prev{start}, queue{start};
    std::size_t head = 0;
    const Cell dirs[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    while (head < queue.size()) {
        Cell at = queue[head++];
        for (Cell d : dirs) {
            Cell n{at.x + d.x, at.z + d.z};
            if (!InGrid(n, s.width, s.depth) || !RoadBetween(s, at, n, true))
                continue;
            bool known = false;
            for (Cell x : seen)
                if (x == n)
                    known = true;
            if (known)
                continue;
            seen.push_back(n);
            prev.push_back(at);
            queue.push_back(n);
            if (n == goal) {
                std::vector<Cell> out{goal};
                Cell cur = goal;
                while (cur != start) {
                    std::size_t i = 0;
                    while (i < seen.size() && seen[i] != cur)
                        ++i;
                    if (i == seen.size())
                        return {};
                    cur = prev[i];
                    out.push_back(cur);
                }
                std::reverse(out.begin(), out.end());
                return out;
            }
        }
    }
    return {};
}
bool Inputs(const Building &b, const BuildingTypeDef &t, const RecipeDef &r, ResourceId &missing) {
    for (auto a : r.inputs) {
        auto i = Slot(t, a.resource);
        if (i >= b.inventory.size() || b.inventory[i] < a.amount) {
            missing = a.resource;
            return false;
        }
    }
    return true;
}
bool Room(const Building &b, const BuildingTypeDef &t, const RecipeDef &r, ResourceId &full) {
    for (auto a : r.outputs) {
        auto i = Slot(t, a.resource);
        if (i >= b.inventory.size() || a.amount > t.storage[i].capacity - b.inventory[i]) {
            full = a.resource;
            return false;
        }
    }
    return true;
}
void Consume(Building &b, const BuildingTypeDef &t, const RecipeDef &r) {
    for (auto a : r.inputs)
        b.inventory[Slot(t, a.resource)] -= a.amount;
}
void Produce(Building &b, const BuildingTypeDef &t, const RecipeDef &r) {
    for (auto a : r.outputs)
        b.inventory[Slot(t, a.resource)] += a.amount;
}
void Production(State &s, Building &b) {
    const auto *t = s.catalog->BuildingType(b.type);
    b.stall = {b.id, StallReason::MissingInput, InvalidResourceId, 0};
    if (!b.enabled) {
        b.stall.reason = StallReason::Disabled;
        return;
    }
    bool running = false, missing = false, full = false;
    ResourceId mr = InvalidResourceId, fr = InvalidResourceId;
    for (std::size_t i = 0; i < t->recipes.size(); ++i) {
        const auto &r = t->recipes[i];
        if (b.remaining[i]) {
            running = true;
            if (b.remaining[i] < r.intervalTicks)
                ++b.remaining[i];
            if (b.remaining[i] == r.intervalTicks) {
                ResourceId x{};
                if (Room(b, *t, r, x)) {
                    Produce(b, *t, r);
                    b.remaining[i] = 0;
                    ++b.completed[i];
                } else {
                    full = true;
                    if (fr == InvalidResourceId)
                        fr = x;
                }
            }
            continue;
        }
        ResourceId x{};
        if (!Inputs(b, *t, r, x)) {
            missing = true;
            if (mr == InvalidResourceId)
                mr = x;
            continue;
        }
        if (!Room(b, *t, r, x)) {
            full = true;
            if (fr == InvalidResourceId)
                fr = x;
            continue;
        }
        Consume(b, *t, r);
        b.remaining[i] = 1;
        running = true;
        if (r.intervalTicks == 1) {
            Produce(b, *t, r);
            b.remaining[i] = 0;
            ++b.completed[i];
        }
    }
    if (full) {
        b.stall.reason = StallReason::OutputFull;
        b.stall.resource = fr;
    } else if (running)
        b.stall.reason = StallReason::Running;
    else if (missing) {
        b.stall.reason = StallReason::MissingInput;
        b.stall.resource = mr;
    } else
        b.stall.reason = StallReason::Running;
}
void Result(State &s, const Pending &p, bool ok, CommandFailure f, const std::string &m, BuildingId b = 0,
            RoadId r = 0, OrderId o = 0) {
    auto &x = s.results[p.id];
    x.commandId = p.id;
    x.state = ok ? CommandState::Accepted : CommandState::Rejected;
    x.failure = ok ? CommandFailure::None : f;
    x.message = m;
    x.building = b;
    x.road = r;
    x.order = o;
}

void Apply(State &s, const Pending &p) {
    auto reject = [&](CommandFailure f, const std::string &m) { Result(s, p, false, f, m); };
    if (p.kind == Kind::Place) {
        const auto *t = s.catalog->BuildingType(p.type);
        if (!t) {
            reject(CommandFailure::UnknownBuildingType, "unknown building type");
            return;
        }
        const auto placementFailure = PlacementFailure(s, *t, p.a);
        if (placementFailure != CommandFailure::None) {
            reject(placementFailure, PlacementMessage(placementFailure));
            return;
        }
        if (s.nextBuilding == UINT32_MAX) {
            reject(CommandFailure::InvalidCommand, "building ID space exhausted");
            return;
        }
        Building b;
        b.id = s.nextBuilding++;
        b.type = p.type;
        b.origin = p.a;
        b.access = {p.a.x + static_cast<std::int32_t>((t->width - 1) / 2), p.a.z - 1};
        b.inventory.assign(t->storage.size(), 0);
        b.remaining.assign(t->recipes.size(), 0);
        b.completed.assign(t->recipes.size(), 0);
        b.stall = {b.id, StallReason::Running, InvalidResourceId, 0};
        s.buildings.push_back(std::move(b));
        Result(s, p, true, CommandFailure::None, "building placed", s.buildings.back().id);
        return;
    }
    if (p.kind == Kind::Remove) {
        if (!Find(s, p.building)) {
            reject(CommandFailure::UnknownBuilding, "unknown building");
            return;
        }
        for (const auto &o : s.orders)
            if (o.source == p.building || o.destination == p.building) {
                reject(CommandFailure::ReferencedByOrder, "building is referenced by an order");
                return;
            }
        for (const auto &connection : s.utilityConnections)
            if (connection.a == p.building || connection.b == p.building) {
                reject(CommandFailure::ReferencedByUtilityConnection,
                       "building is referenced by a utility connection");
                return;
            }
        s.buildings.erase(std::remove_if(s.buildings.begin(), s.buildings.end(),
                                         [&](const Building &b) { return b.id == p.building; }),
                          s.buildings.end());
        Result(s, p, true, CommandFailure::None, "building removed", p.building);
        return;
    }
    if (p.kind == Kind::Road) {
        if (!InGrid(p.a, s.width, s.depth) || !InGrid(p.b, s.width, s.depth) || !Adjacent(p.a, p.b)) {
            reject(CommandFailure::InvalidRoadGeometry,
                   "road must be an in-bounds axis-aligned unit segment");
            return;
        }
        if (Occupied(s, p.a) || Occupied(s, p.b)) {
            reject(CommandFailure::RoadOnBuilding, "road intersects a building");
            return;
        }
        Cell a = p.a, b = p.b;
        if (Less(b, a))
            std::swap(a, b);
        if (RoadBetween(s, a, b, false)) {
            reject(CommandFailure::DuplicateRoad, "road already exists");
            return;
        }
        if (s.nextRoad == UINT32_MAX) {
            reject(CommandFailure::InvalidCommand, "road ID space exhausted");
            return;
        }
        s.roads.push_back({s.nextRoad++, a, b, true});
        Result(s, p, true, CommandFailure::None, "road built", 0, s.roads.back().id);
        return;
    }
    if (p.kind == Kind::RoadEnabled) {
        auto *r = FindRoad(s, p.road);
        if (!r) {
            reject(CommandFailure::UnknownRoad, "unknown road");
            return;
        }
        r->enabled = p.enabled;
        Result(s, p, true, CommandFailure::None, p.enabled ? "road enabled" : "road disabled", 0, r->id);
        return;
    }
    if (p.kind == Kind::BuildingEnabled) {
        auto *b = Find(s, p.building);
        if (!b) {
            reject(CommandFailure::UnknownBuilding, "unknown building");
            return;
        }
        b->enabled = p.enabled;
        Result(s, p, true, CommandFailure::None, p.enabled ? "building enabled" : "building disabled", b->id);
        return;
    }
    if (p.kind == Kind::Haul) {
        const auto *src = Find(s, p.source);
        const auto *dst = Find(s, p.destination);
        const auto *vt = s.catalog->VehicleType(p.vehicleType);
        if (!src || !dst) {
            reject(CommandFailure::InvalidOrderEndpoint, "haul endpoint does not exist");
            return;
        }
        if (!s.catalog->Resource(p.resource)) {
            reject(CommandFailure::UnknownResource, "unknown resource");
            return;
        }
        if (s.catalog->Resource(p.resource)->utility) {
            reject(CommandFailure::InvalidOrderEndpoint, "utility resources cannot be hauled");
            return;
        }
        if (!vt) {
            reject(CommandFailure::UnknownVehicleType, "unknown vehicle type");
            return;
        }
        const auto *st = s.catalog->BuildingType(src->type);
        const auto *dt = s.catalog->BuildingType(dst->type);
        if (Slot(*st, p.resource) >= st->storage.size() || Slot(*dt, p.resource) >= dt->storage.size()) {
            reject(CommandFailure::InvalidOrderEndpoint, "resource is not stored at both endpoints");
            return;
        }
        for (const auto &o : s.orders)
            if (o.source == p.source && o.destination == p.destination && o.resource == p.resource) {
                reject(CommandFailure::DuplicateOrder, "duplicate haul order");
                return;
            }
        if (s.nextOrder == UINT32_MAX || s.nextVehicle == UINT32_MAX) {
            reject(CommandFailure::InvalidCommand, "order or vehicle ID space exhausted");
            return;
        }
        Order o{o.id = s.nextOrder++, p.source, p.destination, p.resource, p.vehicleType, 0};
        o.vehicle = s.nextVehicle++;
        s.orders.push_back(o);
        s.vehicles.push_back({o.vehicle,
                              o.id,
                              o.vehicleType,
                              src->access,
                              0,
                              HaulState::Idle,
                              HaulFailure::None,
                              true,
                              {},
                              0});
        Result(s, p, true, CommandFailure::None, "haul order created", 0, 0, o.id);
        return;
    }
    if (p.kind == Kind::Retarget) {
        auto *o = FindOrder(s, p.order);
        if (!o) {
            reject(CommandFailure::UnknownOrder, "unknown haul order");
            return;
        }
        const auto *dst = Find(s, p.destination);
        if (!dst || p.destination == o->source) {
            reject(CommandFailure::InvalidOrderEndpoint, "destination must exist and differ from source");
            return;
        }
        const auto *dt = s.catalog->BuildingType(dst->type);
        if (Slot(*dt, o->resource) >= dt->storage.size()) {
            reject(CommandFailure::InvalidOrderEndpoint, "resource is not stored at destination");
            return;
        }
        for (const auto &other : s.orders)
            if (other.id != o->id && other.source == o->source &&
                other.destination == p.destination && other.resource == o->resource) {
                reject(CommandFailure::DuplicateOrder, "duplicate haul order");
                return;
            }
        if (o->destination != p.destination) {
            auto *v = FindVehicle(s, o->vehicle);
            o->destination = p.destination;
            v->toDestination = v->cargo > 0;
            v->state = v->toDestination ? HaulState::ToDestination : HaulState::ToSource;
            v->failure = HaulFailure::None;
            v->route.clear();
            v->cursor = 0;
        }
        Result(s, p, true, CommandFailure::None, "haul destination updated", 0, 0, o->id);
        return;
    }
    if (p.kind == Kind::Cancel) {
        auto *o = FindOrder(s, p.order);
        if (!o) {
            reject(CommandFailure::UnknownOrder, "unknown haul order");
            return;
        }
        auto v = o->vehicle;
        if (FindVehicle(s, v)->cargo > 0) {
            reject(CommandFailure::CargoInTransit,
                   "loaded haul cannot be cancelled; deliver cargo or retarget the destination first");
            return;
        }
        s.orders.erase(
            std::remove_if(s.orders.begin(), s.orders.end(), [&](const Order &x) { return x.id == p.order; }),
            s.orders.end());
        s.vehicles.erase(
            std::remove_if(s.vehicles.begin(), s.vehicles.end(), [&](const Vehicle &x) { return x.id == v; }),
            s.vehicles.end());
        Result(s, p, true, CommandFailure::None, "haul order cancelled", 0, 0, p.order);
        return;
    }
    if (p.kind == Kind::ConnectUtility || p.kind == Kind::DisconnectUtility) {
        const auto *resource = s.catalog->Resource(p.resource);
        if (!resource || !resource->utility || p.source == p.destination || !Find(s, p.source) ||
            !Find(s, p.destination)) {
            reject(CommandFailure::InvalidUtilityConnection,
                   "utility connection requires a utility resource and two distinct live buildings");
            return;
        }
        const auto a = std::min(p.source, p.destination), b = std::max(p.source, p.destination);
        const auto existing = std::find_if(s.utilityConnections.begin(), s.utilityConnections.end(),
                                           [&](const UtilityConnectionSnapshot &edge) {
                                               return edge.resource == p.resource && edge.a == a && edge.b == b;
                                           });
        if (p.kind == Kind::ConnectUtility) {
            if (existing != s.utilityConnections.end()) {
                reject(CommandFailure::DuplicateUtilityConnection, "utility connection already exists");
                return;
            }
            s.utilityConnections.push_back({p.resource, a, b});
            std::sort(s.utilityConnections.begin(), s.utilityConnections.end(),
                      [](const auto &left, const auto &right) {
                          if (left.resource != right.resource)
                              return left.resource < right.resource;
                          if (left.a != right.a)
                              return left.a < right.a;
                          return left.b < right.b;
                      });
            Result(s, p, true, CommandFailure::None, "utility connection created");
        } else {
            if (existing == s.utilityConnections.end()) {
                reject(CommandFailure::MissingUtilityConnection, "utility connection does not exist");
                return;
            }
            s.utilityConnections.erase(existing);
            Result(s, p, true, CommandFailure::None, "utility connection removed");
        }
    }
}
void Repath(const State &s, Vehicle &v, Cell target) {
    v.route = Path(s, v.cell, target);
    v.cursor = 0;
}
bool Move(State &s, Vehicle &v, Cell target) {
    if (v.cell == target)
        return true;
    if (v.route.empty() || v.cursor >= v.route.size() || v.route[v.cursor] != v.cell ||
        (v.cursor + 1 < v.route.size() && !RoadBetween(s, v.route[v.cursor], v.route[v.cursor + 1], true))) {
        Repath(s, v, target);
        if (v.route.empty()) {
            v.state = HaulState::NoRoute;
            v.failure = HaulFailure::NoRoute;
            return false;
        }
    }
    const auto *vt = s.catalog->VehicleType(v.type);
    std::uint32_t moved = 0;
    while (moved < vt->cellsPerTick && v.cursor + 1 < v.route.size()) {
        if (!RoadBetween(s, v.route[v.cursor], v.route[v.cursor + 1], true)) {
            Repath(s, v, target);
            if (v.route.empty()) {
                v.state = HaulState::NoRoute;
                v.failure = HaulFailure::RoadDisabled;
                return false;
            }
            break;
        }
        v.cell = v.route[++v.cursor];
        ++moved;
    }
    return v.cell == target;
}
void Haul(State &s, Order &o) {
    auto *v = FindVehicle(s, o.vehicle);
    auto *src = Find(s, o.source);
    auto *dst = Find(s, o.destination);
    if (!v || !src || !dst)
        return;
    const auto *st = s.catalog->BuildingType(src->type);
    const auto *dt = s.catalog->BuildingType(dst->type);
    auto si = Slot(*st, o.resource);
    auto di = Slot(*dt, o.resource);
    if (v->state == HaulState::Idle || v->state == HaulState::Waiting || v->state == HaulState::NoRoute) {
        if (v->cargo == 0 && v->cell == src->access) {
            if (src->inventory[si] == 0) {
                v->state = HaulState::Waiting;
                v->failure = HaulFailure::SourceEmpty;
                return;
            }
            auto room = dt->storage[di].capacity - dst->inventory[di];
            if (!room) {
                v->state = HaulState::Waiting;
                v->failure = HaulFailure::DestinationFull;
                return;
            }
            auto cap = s.catalog->VehicleType(v->type)->capacity;
            auto amount = std::min(cap, std::min(src->inventory[si], room));
            src->inventory[si] -= amount;
            v->cargo = amount;
            v->toDestination = true;
            v->state = HaulState::ToDestination;
            v->failure = HaulFailure::None;
            Repath(s, *v, dst->access);
            if (v->route.empty()) {
                v->state = HaulState::NoRoute;
                v->failure = HaulFailure::NoRoute;
                return;
            }
        } else {
            // An empty truck must reach its source before it can take inventory.
            // In particular, recovery from a blocked return leg must not load remotely.
            v->toDestination = v->cargo > 0;
            v->state = v->toDestination ? HaulState::ToDestination : HaulState::ToSource;
        }
    }
    if (v->state == HaulState::ToDestination && Move(s, *v, dst->access)) {
        auto room = dt->storage[di].capacity - dst->inventory[di];
        auto amount = std::min(v->cargo, room);
        dst->inventory[di] += amount;
        v->cargo -= amount;
        if (v->cargo) {
            v->state = HaulState::Waiting;
            v->failure = HaulFailure::DestinationFull;
            return;
        }
        v->toDestination = false;
        v->state = HaulState::ToSource;
        v->failure = HaulFailure::None;
        Repath(s, *v, src->access);
        if (v->route.empty() && v->cell != src->access) {
            v->state = HaulState::NoRoute;
            v->failure = HaulFailure::NoRoute;
            return;
        }
    }
    if (v->state == HaulState::ToSource && Move(s, *v, src->access)) {
        v->state = HaulState::Idle;
        v->failure = HaulFailure::None;
        v->route.clear();
        v->cursor = 0;
    }
}
std::atomic<unsigned> g_productionThreads{0};
constexpr std::size_t kParallelBuildingThreshold = 1024;
// Production only touches its own building (catalog is read-only), so chunks can run concurrently
// and the result does not depend on the chunking. Haul movement stays serial: it couples buildings.
void ProductionPhase(State &s) {
    unsigned threads = g_productionThreads.load();
    if (threads == 0)
        threads = std::max(1u, std::thread::hardware_concurrency());
    const std::size_t n = s.buildings.size();
    if (threads <= 1 || n < kParallelBuildingThreshold) {
        for (auto &b : s.buildings)
            Production(s, b);
        return;
    }
    threads = static_cast<unsigned>(std::min<std::size_t>(threads, n / 256));
    std::vector<std::thread> pool;
    const std::size_t chunk = (n + threads - 1) / threads;
    for (unsigned t = 1; t < threads; ++t) {
        const std::size_t begin = t * chunk, end = std::min(n, begin + chunk);
        pool.emplace_back([&s, begin, end] {
            for (std::size_t i = begin; i < end; ++i)
                Production(s, s.buildings[i]);
        });
    }
    for (std::size_t i = 0; i < std::min(n, chunk); ++i)
        Production(s, s.buildings[i]);
    for (auto &th : pool)
        th.join();
}
bool HasUtility(const Catalog &catalog, const std::vector<RecipeAmount> &amounts) {
    for (const auto amount : amounts) {
        const auto *resource = catalog.Resource(amount.resource);
        if (resource && resource->utility)
            return true;
    }
    return false;
}
bool StockInputs(const State &s, const Building &b, const BuildingTypeDef &type, const RecipeDef &recipe,
                 ResourceId &missing) {
    for (const auto amount : recipe.inputs) {
        const auto *resource = s.catalog->Resource(amount.resource);
        if (resource && resource->utility)
            continue;
        const auto slot = Slot(type, amount.resource);
        if (slot >= b.inventory.size() || b.inventory[slot] < amount.amount) {
            missing = amount.resource;
            return false;
        }
    }
    return true;
}
bool StockRoom(const State &s, const Building &b, const BuildingTypeDef &type, const RecipeDef &recipe,
               ResourceId &full) {
    for (const auto amount : recipe.outputs) {
        const auto *resource = s.catalog->Resource(amount.resource);
        if (resource && resource->utility)
            continue;
        const auto slot = Slot(type, amount.resource);
        if (slot >= b.inventory.size() || amount.amount > type.storage[slot].capacity - b.inventory[slot]) {
            full = amount.resource;
            return false;
        }
    }
    return true;
}
void ConsumeStock(const State &s, Building &b, const BuildingTypeDef &type, const RecipeDef &recipe) {
    for (const auto amount : recipe.inputs) {
        const auto *resource = s.catalog->Resource(amount.resource);
        if (!(resource && resource->utility))
            b.inventory[Slot(type, amount.resource)] -= amount.amount;
    }
}
void ProduceStock(const State &s, Building &b, const BuildingTypeDef &type, const RecipeDef &recipe) {
    for (const auto amount : recipe.outputs) {
        const auto *resource = s.catalog->Resource(amount.resource);
        if (!(resource && resource->utility))
            b.inventory[Slot(type, amount.resource)] += amount.amount;
    }
}
using UtilityBudgetKey = std::pair<ResourceId, BuildingId>;
struct UtilityBudget {
    std::uint64_t low{};
    std::uint64_t high{};
    void Add(std::uint64_t amount) noexcept {
        const auto old = low;
        low += amount;
        if (low < old)
            ++high;
    }
    bool Covers(std::uint64_t amount) const noexcept { return high != 0 || low >= amount; }
    void Consume(std::uint64_t amount) noexcept {
        if (low < amount) {
            --high;
            low -= amount;
        } else {
            low -= amount;
        }
    }
};
void AddUtilityAmount(UtilityAmount &total, std::uint64_t amount) noexcept {
    const auto old = total.low;
    total.low += amount;
    if (total.low < old)
        ++total.high;
}
UtilityAmount AddUtilityAmount(UtilityAmount left, UtilityAmount right) noexcept {
    const auto low = left.low + right.low;
    const auto carry = low < left.low ? std::uint64_t{1} : std::uint64_t{0};
    return {low, left.high + right.high + carry};
}
std::map<ResourceId, std::map<BuildingId, BuildingId>> UtilityComponents(const State &s);
UtilityDiagnostics MakeUtilityDiagnostics(const State &s,
                                           const std::map<UtilityBudgetKey, UtilityBudget> &budget,
                                           const std::map<UtilityBudgetKey, UtilityAmount> &requested,
                                           const std::map<UtilityBudgetKey, UtilityAmount> &consumed,
                                           const std::map<UtilityBudgetKey, std::uint64_t> &starved) {
    UtilityDiagnostics receipt;
    receipt.available = true;
    receipt.tick = s.tick + 1;

    const auto components = UtilityComponents(s);
    std::map<UtilityBudgetKey, std::vector<BuildingId>> membersByComponent;
    for (const auto &resource : components)
        for (const auto &entry : resource.second)
            membersByComponent[{resource.first, entry.second}].push_back(entry.first);

    std::set<UtilityBudgetKey> used;
    for (const auto &edge : s.utilityConnections) {
        const auto resource = components.find(edge.resource);
        if (resource != components.end())
            used.emplace(edge.resource, resource->second.at(edge.a));
    }
    for (const auto &building : s.buildings) {
        const auto *type = s.catalog->BuildingType(building.type);
        for (const auto &recipe : type->recipes) {
            for (const auto amount : recipe.inputs) {
                const auto *resource = s.catalog->Resource(amount.resource);
                if (resource && resource->utility)
                    used.emplace(amount.resource, components.at(amount.resource).at(building.id));
            }
            for (const auto amount : recipe.outputs) {
                const auto *resource = s.catalog->Resource(amount.resource);
                if (resource && resource->utility)
                    used.emplace(amount.resource, components.at(amount.resource).at(building.id));
            }
        }
    }
    std::map<UtilityBudgetKey, std::size_t> indexByKey;
    for (const auto &key : used) {
        UtilityNetworkDiagnostic network;
        network.resource = key.first;
        network.component = key.second;
        network.members = membersByComponent[key];
        const auto demand = requested.find(key);
        if (demand != requested.end())
            network.requested = demand->second;
        const auto shortage = starved.find(key);
        if (shortage != starved.end())
            network.starvedRecipes = shortage->second;
        indexByKey.emplace(key, receipt.networks.size());
        receipt.networks.push_back(std::move(network));
    }
    for (const auto &key : used) {
        const auto found = budget.find(key);
        if (found == budget.end())
            continue;
        auto &network = receipt.networks[indexByKey.at(key)];
        network.unused = {found->second.low, found->second.high};
        const auto debit = consumed.find(key);
        if (debit != consumed.end())
            network.consumed = debit->second;
        network.generated = AddUtilityAmount(network.unused, network.consumed);
    }
    return receipt;
}
std::map<ResourceId, std::map<BuildingId, BuildingId>> UtilityComponents(const State &s) {
    std::map<ResourceId, std::map<BuildingId, BuildingId>> result;
    for (const auto &resource : s.catalog->Resources()) {
        if (!resource.utility)
            continue;
        auto &parent = result[resource.id];
        for (const auto &building : s.buildings)
            parent.emplace(building.id, building.id);
        auto root = [&](BuildingId id) {
            BuildingId at = id;
            while (parent.at(at) != at)
                at = parent.at(at);
            while (parent.at(id) != id) {
                auto next = parent.at(id);
                parent[id] = at;
                id = next;
            }
            return at;
        };
        for (const auto &edge : s.utilityConnections) {
            if (edge.resource != resource.id)
                continue;
            const auto a = root(edge.a), b = root(edge.b);
            if (a != b) {
                const auto low = std::min(a, b), high = std::max(a, b);
                parent[high] = low;
            }
        }
        for (auto &entry : parent)
            entry.second = root(entry.first);
    }
    return result;
}
bool IsGenerator(const State &s, const RecipeDef &recipe) {
    return HasUtility(*s.catalog, recipe.outputs);
}
UtilityDiagnostics UtilityProductionPhase(State &s) {
    const auto components = UtilityComponents(s);
    std::map<UtilityBudgetKey, UtilityBudget> budget;
    std::map<UtilityBudgetKey, UtilityAmount> requested, consumed;
    std::map<UtilityBudgetKey, std::uint64_t> starved;
    std::vector<bool> running(s.buildings.size()), missing(s.buildings.size()), utilityMissing(s.buildings.size()),
        full(s.buildings.size());
    std::vector<ResourceId> missingResource(s.buildings.size(), InvalidResourceId), fullResource(s.buildings.size(), InvalidResourceId);

    // Generators run first in stable building and recipe order. Fuel, stock
    // output room, and all stock changes are committed before utility is offered.
    for (std::size_t bi = 0; bi < s.buildings.size(); ++bi) {
        auto &building = s.buildings[bi];
        const auto *type = s.catalog->BuildingType(building.type);
        if (!building.enabled)
            continue;
        for (std::size_t ri = 0; ri < type->recipes.size(); ++ri) {
            const auto &recipe = type->recipes[ri];
            if (!IsGenerator(s, recipe))
                continue;
            ResourceId blocked{};
            if (!StockInputs(s, building, *type, recipe, blocked)) {
                missing[bi] = true;
                if (missingResource[bi] == InvalidResourceId)
                    missingResource[bi] = blocked;
                continue;
            }
            if (!StockRoom(s, building, *type, recipe, blocked)) {
                full[bi] = true;
                if (fullResource[bi] == InvalidResourceId)
                    fullResource[bi] = blocked;
                continue;
            }
            ConsumeStock(s, building, *type, recipe);
            ProduceStock(s, building, *type, recipe);
            running[bi] = true;
            for (const auto output : recipe.outputs) {
                const auto *resource = s.catalog->Resource(output.resource);
                if (!(resource && resource->utility))
                    continue;
                const auto &byBuilding = components.at(output.resource);
                const auto component = byBuilding.at(building.id);
                budget[{output.resource, component}].Add(output.amount);
            }
            ++building.completed[ri];
        }
    }

    // Consumers are visited in stable building/recipe order. A recipe first
    // preflights every utility, stock input, and stock output before committing.
    for (std::size_t bi = 0; bi < s.buildings.size(); ++bi) {
        auto &building = s.buildings[bi];
        const auto *type = s.catalog->BuildingType(building.type);
        if (!building.enabled)
            continue;
        for (std::size_t ri = 0; ri < type->recipes.size(); ++ri) {
            const auto &recipe = type->recipes[ri];
            if (IsGenerator(s, recipe))
                continue;
            if (HasUtility(*s.catalog, recipe.inputs)) {
                ResourceId stockBlocked{};
                const bool stockEligible = StockInputs(s, building, *type, recipe, stockBlocked) &&
                                           StockRoom(s, building, *type, recipe, stockBlocked);
                if (stockEligible) {
                    for (const auto input : recipe.inputs) {
                        const auto *resource = s.catalog->Resource(input.resource);
                        if (!(resource && resource->utility))
                            continue;
                        const auto component = components.at(input.resource).at(building.id);
                        AddUtilityAmount(requested[{input.resource, component}], input.amount);
                    }
                }
                ResourceId unavailable = InvalidResourceId;
                std::vector<UtilityBudgetKey> shortNetworks;
                const auto &byBuilding = components;
                for (const auto input : recipe.inputs) {
                    const auto *resource = s.catalog->Resource(input.resource);
                    if (!(resource && resource->utility))
                        continue;
                    const auto component = byBuilding.at(input.resource).at(building.id);
                    const auto found = budget.find({input.resource, component});
                    if (found == budget.end() || !found->second.Covers(input.amount)) {
                        shortNetworks.emplace_back(input.resource, component);
                        if (unavailable == InvalidResourceId)
                            unavailable = input.resource;
                    }
                }
                if (unavailable != InvalidResourceId) {
                    if (stockEligible)
                        for (const auto &key : shortNetworks)
                            ++starved[key];
                    missing[bi] = true;
                    utilityMissing[bi] = true;
                    if (missingResource[bi] == InvalidResourceId)
                        missingResource[bi] = unavailable;
                    continue;
                }
                ResourceId blocked{};
                if (!StockInputs(s, building, *type, recipe, blocked)) {
                    missing[bi] = true;
                    if (missingResource[bi] == InvalidResourceId)
                        missingResource[bi] = blocked;
                    continue;
                }
                if (!StockRoom(s, building, *type, recipe, blocked)) {
                    full[bi] = true;
                    if (fullResource[bi] == InvalidResourceId)
                        fullResource[bi] = blocked;
                    continue;
                }
                for (const auto input : recipe.inputs) {
                    const auto *resource = s.catalog->Resource(input.resource);
                    if (resource && resource->utility) {
                        const auto component = components.at(input.resource).at(building.id);
                        budget[{input.resource, component}].Consume(input.amount);
                        AddUtilityAmount(consumed[{input.resource, component}], input.amount);
                    }
                }
                ConsumeStock(s, building, *type, recipe);
                ProduceStock(s, building, *type, recipe);
                ++building.completed[ri];
                running[bi] = true;
                continue;
            }
            // Preserve the established interval behavior for unrelated recipes.
            if (building.remaining[ri]) {
                running[bi] = true;
                if (building.remaining[ri] < recipe.intervalTicks)
                    ++building.remaining[ri];
                if (building.remaining[ri] == recipe.intervalTicks) {
                    ResourceId blocked{};
                    if (StockRoom(s, building, *type, recipe, blocked)) {
                        ProduceStock(s, building, *type, recipe);
                        building.remaining[ri] = 0;
                        ++building.completed[ri];
                    } else {
                        full[bi] = true;
                        if (fullResource[bi] == InvalidResourceId)
                            fullResource[bi] = blocked;
                    }
                }
                continue;
            }
            ResourceId blocked{};
            if (!StockInputs(s, building, *type, recipe, blocked)) {
                missing[bi] = true;
                if (missingResource[bi] == InvalidResourceId)
                    missingResource[bi] = blocked;
                continue;
            }
            if (!StockRoom(s, building, *type, recipe, blocked)) {
                full[bi] = true;
                if (fullResource[bi] == InvalidResourceId)
                    fullResource[bi] = blocked;
                continue;
            }
            ConsumeStock(s, building, *type, recipe);
            building.remaining[ri] = 1;
            running[bi] = true;
            if (recipe.intervalTicks == 1) {
                ProduceStock(s, building, *type, recipe);
                building.remaining[ri] = 0;
                ++building.completed[ri];
            }
        }
    }
    for (std::size_t bi = 0; bi < s.buildings.size(); ++bi) {
        auto &building = s.buildings[bi];
        if (!building.enabled)
            building.stall = {building.id, StallReason::Disabled, InvalidResourceId, 0};
        else if (full[bi])
            building.stall = {building.id, StallReason::OutputFull, fullResource[bi], 0};
        else if (utilityMissing[bi])
            building.stall = {building.id, StallReason::MissingInput, missingResource[bi], 0};
        else if (running[bi])
            building.stall = {building.id, StallReason::Running, InvalidResourceId, 0};
        else if (missing[bi])
            building.stall = {building.id, StallReason::MissingInput, missingResource[bi], 0};
        else
            building.stall = {building.id, StallReason::Running, InvalidResourceId, 0};
    }
    return MakeUtilityDiagnostics(s, budget, requested, consumed, starved);
}
UtilityDiagnostics AdvanceTick(State &s) {
    auto pending = std::move(s.pending);
    s.pending.clear();
    for (const auto &p : pending) {
        Apply(s, p);
        ++s.revision;
    }
    std::sort(s.buildings.begin(), s.buildings.end(),
              [](const Building &a, const Building &b) { return a.id < b.id; });
    bool hasUtilityRecipes = false;
    for (const auto &type : s.catalog->BuildingTypes())
        for (const auto &recipe : type.recipes)
            hasUtilityRecipes = hasUtilityRecipes || HasUtility(*s.catalog, recipe.inputs) ||
                                HasUtility(*s.catalog, recipe.outputs);
    UtilityDiagnostics receipt;
    if (hasUtilityRecipes)
        receipt = UtilityProductionPhase(s);
    else
        ProductionPhase(s);
    std::sort(s.orders.begin(), s.orders.end(), [](const Order &a, const Order &b) { return a.id < b.id; });
    for (auto &o : s.orders)
        Haul(s, o);
    ++s.tick;
    ++s.revision;
    return receipt;
}
void HashU(std::uint64_t &h, std::uint64_t x) {
    for (int i = 0; i < 8; ++i) {
        h ^= static_cast<std::uint8_t>(x);
        h *= 1099511628211ULL;
        x >>= 8;
    }
}
void HashS(std::uint64_t &h, std::string_view s) {
    HashU(h, s.size());
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
}
std::uint64_t Hash(const State &s) {
    std::uint64_t h = 1469598103934665603ULL;
    HashU(h, s.width);
    HashU(h, s.depth);
    HashU(h, s.tick);
    HashU(h, s.revision);
    HashU(h, s.nextBuilding);
    HashU(h, s.nextRoad);
    HashU(h, s.nextOrder);
    HashU(h, s.nextVehicle);
    HashU(h, s.nextCommand);
    if (s.firstRetainedCommand > 1) {
        HashS(h, "openrepublic.command-history.first-retained.v1");
        HashU(h, s.firstRetainedCommand);
    }
    for (const auto &b : s.buildings) {
        HashU(h, b.id);
        HashU(h, b.type);
        HashU(h, b.origin.x);
        HashU(h, b.origin.z);
        HashU(h, b.enabled);
        HashU(h, b.access.x);
        HashU(h, b.access.z);
        for (auto x : b.inventory)
            HashU(h, x);
        for (auto x : b.remaining)
            HashU(h, x);
        for (auto x : b.completed)
            HashU(h, x);
    }
    for (const auto &r : s.roads) {
        HashU(h, r.id);
        HashU(h, r.a.x);
        HashU(h, r.a.z);
        HashU(h, r.b.x);
        HashU(h, r.b.z);
        HashU(h, r.enabled);
    }
    if (!s.utilityConnections.empty()) {
        HashS(h, "openrepublic.utility-connections.v1");
        for (const auto &edge : s.utilityConnections) {
            HashU(h, edge.resource);
            HashU(h, edge.a);
            HashU(h, edge.b);
        }
    }
    if (!s.utilityCommands.empty()) {
        HashS(h, "openrepublic.utility-command-history.v1");
        for (const auto id : s.utilityCommands)
            HashU(h, id);
    }
    for (const auto &o : s.orders) {
        HashU(h, o.id);
        HashU(h, o.source);
        HashU(h, o.destination);
        HashU(h, o.resource);
        HashU(h, o.vehicleType);
        HashU(h, o.vehicle);
    }
    for (const auto &v : s.vehicles) {
        HashU(h, v.id);
        HashU(h, v.order);
        HashU(h, v.type);
        HashU(h, v.cell.x);
        HashU(h, v.cell.z);
        HashU(h, v.cargo);
        HashU(h, static_cast<std::uint8_t>(v.state));
        HashU(h, static_cast<std::uint8_t>(v.failure));
        HashU(h, v.toDestination);
    }
    for (const auto &p : s.pending) {
        HashU(h, p.id);
        HashU(h, static_cast<std::uint8_t>(p.kind));
        HashU(h, p.type);
        HashU(h, p.building);
        HashU(h, p.a.x);
        HashU(h, p.a.z);
        HashU(h, p.b.x);
        HashU(h, p.b.z);
        HashU(h, p.road);
        HashU(h, p.enabled);
        HashU(h, p.source);
        HashU(h, p.destination);
        HashU(h, p.resource);
        HashU(h, p.vehicleType);
        HashU(h, p.order);
    }
    for (const auto &r : s.results) {
        HashU(h, r.first);
        HashU(h, static_cast<std::uint8_t>(r.second.state));
        HashU(h, static_cast<std::uint8_t>(r.second.failure));
        HashU(h, r.second.building);
        HashU(h, r.second.road);
        HashU(h, r.second.order);
    }
    return h;
}
bool AmountToken(std::string_view token, std::string &name, std::uint64_t &amount) {
    auto p = token.find(':');
    if (p == std::string_view::npos || p == 0 || p + 1 == token.size())
        return false;
    name = std::string(token.substr(0, p));
    return Unsigned(token.substr(p + 1), amount) && amount != 0;
}

} // namespace

std::string FormatUtilityAmount(UtilityAmount amount) {
    if (amount.high == 0)
        return std::to_string(amount.low);
    std::string digits;
    do {
        std::uint64_t quotientLow = 0, quotientHigh = 0;
        unsigned remainder = 0;
        for (int bit = 127; bit >= 0; --bit) {
            const bool set = bit >= 64 ? ((amount.high >> (bit - 64)) & 1U) != 0
                                       : ((amount.low >> bit) & 1U) != 0;
            remainder = remainder * 2U + (set ? 1U : 0U);
            if (remainder >= 10U) {
                remainder -= 10U;
                if (bit >= 64)
                    quotientHigh |= std::uint64_t{1} << (bit - 64);
                else
                    quotientLow |= std::uint64_t{1} << bit;
            }
        }
        digits.push_back(static_cast<char>('0' + remainder));
        amount = {quotientLow, quotientHigh};
    } while (amount.low != 0 || amount.high != 0);
    std::reverse(digits.begin(), digits.end());
    return digits;
}

bool operator==(Cell a, Cell b) noexcept {
    return a.x == b.x && a.z == b.z;
}
bool operator!=(Cell a, Cell b) noexcept {
    return !(a == b);
}
Catalog::Catalog() = default;
const std::vector<ResourceDef> &Catalog::Resources() const noexcept {
    return resources_;
}
const std::vector<BuildingTypeDef> &Catalog::BuildingTypes() const noexcept {
    return buildingTypes_;
}
const std::vector<VehicleTypeDef> &Catalog::VehicleTypes() const noexcept {
    return vehicleTypes_;
}
bool Catalog::FindResource(std::string_view n, ResourceId &i) const noexcept {
    for (const auto &x : resources_)
        if (Same(n, x.name)) {
            i = x.id;
            return true;
        }
    return false;
}
bool Catalog::FindBuildingType(std::string_view n, BuildingTypeId &i) const noexcept {
    for (const auto &x : buildingTypes_)
        if (Same(n, x.name)) {
            i = x.id;
            return true;
        }
    return false;
}
bool Catalog::FindVehicleType(std::string_view n, VehicleTypeId &i) const noexcept {
    for (const auto &x : vehicleTypes_)
        if (Same(n, x.name)) {
            i = x.id;
            return true;
        }
    return false;
}
const ResourceDef *Catalog::Resource(ResourceId i) const noexcept {
    return i < resources_.size() ? &resources_[i] : nullptr;
}
const BuildingTypeDef *Catalog::BuildingType(BuildingTypeId i) const noexcept {
    return i < buildingTypes_.size() ? &buildingTypes_[i] : nullptr;
}
const VehicleTypeDef *Catalog::VehicleType(VehicleTypeId i) const noexcept {
    return i < vehicleTypes_.size() ? &vehicleTypes_[i] : nullptr;
}

bool CatalogBuilder::AddResource(std::string_view n, ResourceId *out, CatalogErrorInfo &e) {
    if (!NameOk(n)) {
        CatError(e, n.empty() ? CatalogError::EmptyName : CatalogError::NameTooLong, "invalid resource name");
        return false;
    }
    for (const auto &x : draft_.resources_)
        if (Same(n, x.name)) {
            CatError(e, CatalogError::DuplicateName, "duplicate resource name");
            return false;
        }
    if (draft_.resources_.size() >= MaxDefinitions) {
        CatError(e, CatalogError::TooManyDefinitions, "resource limit exceeded");
        return false;
    }
    ResourceDef x{static_cast<ResourceId>(draft_.resources_.size()), std::string(n), false};
    draft_.resources_.push_back(x);
    if (out)
        *out = x.id;
    e = {};
    return true;
}
bool CatalogBuilder::AddUtilityResource(std::string_view n, ResourceId *out, CatalogErrorInfo &e) {
    ResourceId id{};
    if (!AddResource(n, &id, e))
        return false;
    draft_.resources_[id].utility = true;
    if (out)
        *out = id;
    return true;
}
bool CatalogBuilder::AddBuildingType(std::string_view n, std::uint32_t w, std::uint32_t d,
                                     BuildingTypeId *out, CatalogErrorInfo &e) {
    if (!NameOk(n)) {
        CatError(e, n.empty() ? CatalogError::EmptyName : CatalogError::NameTooLong, "invalid building name");
        return false;
    }
    if (!w || !d) {
        CatError(e, CatalogError::InvalidDimension, "building dimensions must be positive");
        return false;
    }
    for (const auto &x : draft_.buildingTypes_)
        if (Same(n, x.name)) {
            CatError(e, CatalogError::DuplicateName, "duplicate building name");
            return false;
        }
    if (draft_.buildingTypes_.size() >= MaxDefinitions) {
        CatError(e, CatalogError::TooManyDefinitions, "building limit exceeded");
        return false;
    }
    BuildingTypeDef x;
    x.id = static_cast<BuildingTypeId>(draft_.buildingTypes_.size());
    x.name = std::string(n);
    x.width = w;
    x.depth = d;
    draft_.buildingTypes_.push_back(std::move(x));
    if (out)
        *out = draft_.buildingTypes_.back().id;
    e = {};
    return true;
}
bool CatalogBuilder::AddStorage(BuildingTypeId b, ResourceId r, std::uint64_t cap, CatalogErrorInfo &e) {
    auto *t = b < draft_.buildingTypes_.size() ? &draft_.buildingTypes_[b] : nullptr;
    if (!t) {
        CatError(e, CatalogError::UnknownBuildingType, "unknown building type");
        return false;
    }
    if (r >= draft_.resources_.size()) {
        CatError(e, CatalogError::UnknownResource, "unknown resource");
        return false;
    }
    if (draft_.resources_[r].utility) {
        CatError(e, CatalogError::UtilityHasStorage, "utility resources cannot be stored");
        return false;
    }
    if (!cap) {
        CatError(e, CatalogError::InvalidCapacity, "storage capacity must be positive");
        return false;
    }
    for (auto x : t->storage)
        if (x.resource == r) {
            CatError(e, CatalogError::DuplicateStorage, "duplicate storage slot");
            return false;
        }
    t->storage.push_back({r, cap});
    e = {};
    return true;
}
bool CatalogBuilder::AddStorage(BuildingTypeId b, std::string_view n, std::uint64_t cap,
                                CatalogErrorInfo &e) {
    ResourceId r{};
    for (const auto &x : draft_.resources_)
        if (Same(n, x.name)) {
            r = x.id;
            return AddStorage(b, r, cap, e);
        }
    CatError(e, CatalogError::UnknownResource, "unknown resource");
    return false;
}
bool CatalogBuilder::AddRecipe(BuildingTypeId b, const RecipeDef &r, CatalogErrorInfo &e) {
    auto *t = b < draft_.buildingTypes_.size() ? &draft_.buildingTypes_[b] : nullptr;
    if (!t) {
        CatError(e, CatalogError::UnknownBuildingType, "unknown building type");
        return false;
    }
    if (!NameOk(r.name)) {
        CatError(e, CatalogError::InvalidRecipeName, "invalid recipe name");
        return false;
    }
    if (!r.intervalTicks) {
        CatError(e, CatalogError::InvalidInterval, "recipe interval must be positive");
        return false;
    }
    for (const auto &q : t->recipes)
        if (q.name == r.name) {
            CatError(e, CatalogError::DuplicateRecipe, "duplicate recipe name");
            return false;
        }
    auto check = [&](const std::vector<RecipeAmount> &v) {
        for (std::size_t i = 0; i < v.size(); ++i) {
            auto a = v[i];
            if (a.resource >= draft_.resources_.size()) {
                CatError(e, CatalogError::UnknownResource, "unknown recipe resource");
                return false;
            }
            if (!a.amount) {
                CatError(e, CatalogError::InvalidAmount, "recipe amount must be positive");
                return false;
            }
            if (!draft_.resources_[a.resource].utility && Slot(*t, a.resource) >= t->storage.size()) {
                CatError(e, CatalogError::MissingStorage, "recipe resource has no storage slot");
                return false;
            }
            for (std::size_t j = 0; j < i; ++j)
                if (v[j].resource == a.resource) {
                    CatError(e, CatalogError::InvalidAmount, "duplicate recipe resource");
                    return false;
                }
        }
        return true;
    };
    if (!check(r.inputs) || !check(r.outputs))
        return false;
    bool hasUtility = false, producesUtility = false, consumesUtility = false;
    for (auto a : r.inputs)
        if (draft_.resources_[a.resource].utility) {
            hasUtility = true;
            consumesUtility = true;
        }
    for (auto a : r.outputs)
        if (draft_.resources_[a.resource].utility) {
            hasUtility = true;
            producesUtility = true;
        }
    if (hasUtility && r.intervalTicks != 1) {
        CatError(e, CatalogError::InvalidUtilityRecipe, "utility recipes must run every tick");
        return false;
    }
    if (producesUtility && consumesUtility) {
        CatError(e, CatalogError::InvalidUtilityRecipe,
                 "utility-producing recipes cannot consume utility resources");
        return false;
    }
    t->recipes.push_back(r);
    e = {};
    return true;
}
bool CatalogBuilder::AddRecipe(BuildingTypeId b, const RecipeDef &r, const Catalog &names,
                               CatalogErrorInfo &e) {
    for (auto a : r.inputs)
        if (!names.Resource(a.resource)) {
            CatError(e, CatalogError::UnknownResource, "unknown resource ID");
            return false;
        }
    for (auto a : r.outputs)
        if (!names.Resource(a.resource)) {
            CatError(e, CatalogError::UnknownResource, "unknown resource ID");
            return false;
        }
    return AddRecipe(b, r, e);
}
bool CatalogBuilder::AddVehicleType(std::string_view n, std::uint64_t cap, std::uint32_t speed,
                                    VehicleTypeId *out, CatalogErrorInfo &e) {
    if (!NameOk(n)) {
        CatError(e, n.empty() ? CatalogError::EmptyName : CatalogError::NameTooLong, "invalid vehicle name");
        return false;
    }
    if (!cap) {
        CatError(e, CatalogError::InvalidCapacity, "vehicle capacity must be positive");
        return false;
    }
    if (!speed) {
        CatError(e, CatalogError::InvalidAmount, "vehicle speed must be positive");
        return false;
    }
    for (const auto &x : draft_.vehicleTypes_)
        if (Same(n, x.name)) {
            CatError(e, CatalogError::DuplicateVehicleType, "duplicate vehicle name");
            return false;
        }
    VehicleTypeDef x{static_cast<VehicleTypeId>(draft_.vehicleTypes_.size()), std::string(n), cap, speed};
    draft_.vehicleTypes_.push_back(std::move(x));
    if (out)
        *out = draft_.vehicleTypes_.back().id;
    e = {};
    return true;
}
bool CatalogBuilder::Build(Catalog &out, CatalogErrorInfo &e) const {
    out = draft_;
    e = {};
    return true;
}

bool ParseCatalogText(std::string_view text, Catalog &out, ParseError &e) {
    e = {};
    if (text.empty()) {
        ParseErr(e, ParseErrorCode::EmptyInput, 0, "empty catalog");
        return false;
    }
    if (text.size() > MaxText) {
        ParseErr(e, ParseErrorCode::LimitExceeded, 0, "catalog too large");
        return false;
    }
    CatalogBuilder b;
    CatalogErrorInfo ce;
    std::vector<std::string> rn;
    BuildingTypeId current = InvalidBuildingTypeId;
    std::uint32_t line = 0;
    bool header = false, closed = true;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        auto end = text.find('\n', pos);
        std::string l(text.substr(pos, end == std::string_view::npos ? text.size() - pos : end - pos));
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        pos = end == std::string_view::npos ? text.size() + 1 : end + 1;
        ++line;
        auto h = l.find('#');
        if (h != std::string::npos)
            l.resize(h);
        if (l.size() > MaxLine) {
            ParseErr(e, ParseErrorCode::LimitExceeded, line, "line too long");
            return false;
        }
        auto w = Words(l);
        if (w.empty())
            continue;
        if (!header) {
            if (w.size() != 2 || w[0] != "openrepublic-economy" || w[1] != "1") {
                ParseErr(e, ParseErrorCode::InvalidHeader, line, "expected openrepublic-economy 1");
                return false;
            }
            header = true;
            continue;
        }
        if (current != InvalidBuildingTypeId) {
            if (w[0] == "end" && w.size() == 1) {
                current = InvalidBuildingTypeId;
                closed = true;
                continue;
            }
            if (w[0] == "store" && w.size() == 3) {
                std::uint64_t cap{};
                if (!Unsigned(w[2], cap) || !b.AddStorage(current, w[1], cap, ce)) {
                    ParseErr(e, ParseErrorCode::CatalogInvalid, line, ce.message);
                    return false;
                }
                continue;
            }
            if (w[0] == "recipe" && w.size() >= 4 && w[2] == "every") {
                std::uint32_t interval{};
                if (!Unsigned(w[3], interval)) {
                    ParseErr(e, ParseErrorCode::InvalidNumber, line, "bad interval");
                    return false;
                }
                RecipeDef r;
                r.name = w[1];
                r.intervalTicks = interval;
                enum class Section {
                    None,
                    In,
                    Out
                };
                Section sec = Section::None;
                for (std::size_t i = 4; i < w.size(); ++i) {
                    if (w[i] == "in" && sec == Section::None) {
                        sec = Section::In;
                        continue;
                    }
                    if (w[i] == "out" && sec != Section::Out) {
                        sec = Section::Out;
                        continue;
                    }
                    if (sec == Section::None) {
                        ParseErr(e, ParseErrorCode::UnexpectedToken, line, "recipe section missing");
                        return false;
                    }
                    std::string name;
                    std::uint64_t amount{};
                    if (!AmountToken(w[i], name, amount)) {
                        ParseErr(e, ParseErrorCode::InvalidNumber, line, "bad recipe amount");
                        return false;
                    }
                    ResourceId id{};
                    bool found = false;
                    for (std::size_t k = 0; k < rn.size(); ++k)
                        if (rn[k] == name) {
                            id = static_cast<ResourceId>(k);
                            found = true;
                            break;
                        }
                    if (!found) {
                        ParseErr(e, ParseErrorCode::UnknownName, line, "unknown recipe resource");
                        return false;
                    }
                    if (sec == Section::In)
                        r.inputs.push_back({id, amount});
                    else
                        r.outputs.push_back({id, amount});
                }
                if (sec == Section::None || !b.AddRecipe(current, r, ce)) {
                    ParseErr(e, ParseErrorCode::CatalogInvalid, line, ce.message);
                    return false;
                }
                continue;
            }
            ParseErr(e, ParseErrorCode::UnexpectedToken, line, "unexpected building member");
            return false;
        }
        if ((w[0] == "resource" || w[0] == "utility") && w.size() == 2) {
            const bool added = w[0] == "utility" ? b.AddUtilityResource(w[1], nullptr, ce)
                                                  : b.AddResource(w[1], nullptr, ce);
            if (!added) {
                ParseErr(e, ParseErrorCode::DuplicateDefinition, line, ce.message);
                return false;
            }
            rn.push_back(w[1]);
            continue;
        }
        if (w[0] == "building" && w.size() == 4) {
            std::uint32_t wi{}, de{};
            if (!Unsigned(w[2], wi) || !Unsigned(w[3], de) ||
                !b.AddBuildingType(w[1], wi, de, &current, ce)) {
                ParseErr(e, ParseErrorCode::CatalogInvalid, line, ce.message);
                return false;
            }
            closed = false;
            continue;
        }
        if (w[0] == "vehicle" && w.size() == 6 && w[2] == "capacity" && w[4] == "speed") {
            std::uint64_t cap{};
            std::uint32_t sp{};
            if (!Unsigned(w[3], cap) || !Unsigned(w[5], sp) ||
                !b.AddVehicleType(w[1], cap, sp, nullptr, ce)) {
                ParseErr(e, ParseErrorCode::CatalogInvalid, line, ce.message);
                return false;
            }
            continue;
        }
        ParseErr(e, ParseErrorCode::UnexpectedToken, line, "unexpected catalog token");
        return false;
    }
    if (!header || !closed) {
        ParseErr(e, ParseErrorCode::MissingEnd, line, "missing catalog header or building end");
        return false;
    }
    Catalog candidate;
    if (!b.Build(candidate, ce)) {
        ParseErr(e, ParseErrorCode::CatalogInvalid, line, ce.message);
        return false;
    }
    out = std::move(candidate);
    return true;
}
std::string WriteCatalogText(const Catalog &c) {
    std::string o = "openrepublic-economy 1\n";
    for (const auto &r : c.Resources())
        o += std::string(r.utility ? "utility " : "resource ") + r.name + "\n";
    for (const auto &b : c.BuildingTypes()) {
        o += "building " + b.name + " " + Num(b.width) + " " + Num(b.depth) + "\n";
        for (auto s : b.storage)
            o += "  store " + c.Resource(s.resource)->name + " " + Num(s.capacity) + "\n";
        for (const auto &r : b.recipes) {
            o += "  recipe " + r.name + " every " + Num(r.intervalTicks);
            if (!r.inputs.empty()) {
                o += " in";
                for (auto a : r.inputs)
                    o += " " + c.Resource(a.resource)->name + ":" + Num(a.amount);
            }
            if (!r.outputs.empty()) {
                o += " out";
                for (auto a : r.outputs)
                    o += " " + c.Resource(a.resource)->name + ":" + Num(a.amount);
            }
            o += "\n";
        }
        o += "end\n";
    }
    for (const auto &v : c.VehicleTypes())
        o += "vehicle " + v.name + " capacity " + Num(v.capacity) + " speed " + Num(v.cellsPerTick) + "\n";
    return o;
}
Catalog MakeSyntheticCatalog() {
    CatalogBuilder b;
    CatalogErrorInfo e;
    ResourceId g{}, f{}, d{}, br{};
    b.AddResource("grain", &g, e);
    b.AddResource("flour", &f, e);
    b.AddResource("dough", &d, e);
    b.AddResource("bread", &br, e);
    BuildingTypeId farm{}, mill{}, bakery{}, market{};
    b.AddBuildingType("farm", 2, 2, &farm, e);
    b.AddBuildingType("mill", 2, 2, &mill, e);
    b.AddBuildingType("bakery", 2, 2, &bakery, e);
    b.AddBuildingType("market", 2, 2, &market, e);
    b.AddStorage(farm, g, 20, e);
    b.AddRecipe(farm, {"harvest", 2, {}, {{g, 2}}}, e);
    b.AddStorage(mill, g, 8, e);
    b.AddStorage(mill, f, 8, e);
    b.AddRecipe(mill, {"grind", 1, {{g, 1}}, {{f, 1}}}, e);
    b.AddStorage(bakery, f, 8, e);
    b.AddStorage(bakery, d, 8, e);
    b.AddStorage(bakery, br, 8, e);
    b.AddRecipe(bakery, {"bake", 2, {{f, 1}}, {{br, 1}}}, e);
    b.AddStorage(market, br, 16, e);
    b.AddRecipe(market, {"consume", 1, {{br, 1}}, {}}, e);
    b.AddVehicleType("truck", 4, 1, nullptr, e);
    Catalog out;
    b.Build(out, e);
    return out;
}

struct World::Impl {
    State state;
    UtilityDiagnostics utilityDiagnostics;
};
World::World(const Catalog &c, std::uint32_t w, std::uint32_t d) {
    if (!w || !d || w > MaxWorldGridSize || d > MaxWorldGridSize)
        throw std::invalid_argument("world dimensions must be 1..MaxWorldGridSize");
    impl_ = new Impl;
    impl_->state.catalog = &c;
    impl_->state.width = w;
    impl_->state.depth = d;
}
World::~World() {
    delete impl_;
}
World::World(World &&other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}
World &World::operator=(World &&other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}
const Catalog &World::GetCatalog() const noexcept {
    return *impl_->state.catalog;
}
bool World::ReplaceCatalog(const Catalog &next, std::string &error) {
    const auto &state = impl_->state;
    const auto remainingRevision = UINT64_MAX - state.revision;
    if (!remainingRevision || (!state.pending.empty() &&
        (state.tick == UINT64_MAX || state.pending.size() >= remainingRevision - 1))) {
        error = "world revision exhausted or reserved for queued commands";
        return false;
    }
    const Catalog &old = *impl_->state.catalog;
    const auto &or_ = old.Resources(), &nr = next.Resources();
    const auto &ob = old.BuildingTypes(), &nb = next.BuildingTypes();
    const auto &ov = old.VehicleTypes(), &nv = next.VehicleTypes();
    bool same = or_.size() == nr.size() && ob.size() == nb.size() && ov.size() == nv.size();
    for (std::size_t i = 0; same && i < or_.size(); ++i)
        same = or_[i].name == nr[i].name && or_[i].utility == nr[i].utility;
    for (std::size_t i = 0; same && i < ov.size(); ++i)
        same = ov[i].name == nv[i].name;
    for (std::size_t i = 0; same && i < ob.size(); ++i) {
        same = ob[i].name == nb[i].name && ob[i].width == nb[i].width && ob[i].depth == nb[i].depth &&
               ob[i].storage.size() == nb[i].storage.size() && ob[i].recipes.size() == nb[i].recipes.size();
        for (std::size_t j = 0; same && j < ob[i].storage.size(); ++j)
            same = ob[i].storage[j].resource == nb[i].storage[j].resource;
        for (std::size_t j = 0; same && j < ob[i].recipes.size(); ++j) {
            const auto &a = ob[i].recipes[j], &b = nb[i].recipes[j];
            same = a.name == b.name && a.inputs.size() == b.inputs.size() && a.outputs.size() == b.outputs.size();
            for (std::size_t k = 0; same && k < a.inputs.size(); ++k)
                same = a.inputs[k].resource == b.inputs[k].resource;
            for (std::size_t k = 0; same && k < a.outputs.size(); ++k)
                same = a.outputs[k].resource == b.outputs[k].resource;
        }
    }
    if (!same) {
        error = "the new catalog does not have the same structure as the running one";
        return false;
    }
    for (const auto &b : impl_->state.buildings)
        for (std::size_t i = 0; i < b.inventory.size(); ++i)
            if (b.inventory[i] > nb[b.type].storage[i].capacity) {
                error = "a stored amount exceeds the reduced capacity of its building type";
                return false;
            }
    for (const auto &v : impl_->state.vehicles)
        if (v.cargo > nv[v.type].capacity) {
            error = "onboard cargo exceeds the reduced capacity of its vehicle type";
            return false;
        }
    impl_->state.catalog = &next;
    for (auto &b : impl_->state.buildings)
        for (std::size_t i = 0; i < b.remaining.size(); ++i)
            b.remaining[i] = std::min(b.remaining[i], nb[b.type].recipes[i].intervalTicks);
    ++impl_->state.revision;
    impl_->utilityDiagnostics = {};
    return true;
}
void SetProductionThreads(unsigned threads) noexcept {
    g_productionThreads = threads;
}
std::uint64_t World::Tick() const noexcept {
    return impl_->state.tick;
}
std::uint64_t World::Revision() const noexcept {
    return impl_->state.revision;
}
CommandId Queue(State &s, Pending p) {
    const auto remainingRevision = UINT64_MAX - s.revision;
    if (s.nextCommand == UINT64_MAX || s.tick == UINT64_MAX || remainingRevision < 2 ||
        s.pending.size() > remainingRevision - 2)
        return 0;
    p.id = s.nextCommand++;
    s.pending.push_back(p);
    CommandResult r;
    r.commandId = p.id;
    r.state = CommandState::Queued;
    s.results[p.id] = r;
    return p.id;
}

void RefreshStall(State &s, Building &b) {
    const auto *t = s.catalog->BuildingType(b.type);
    b.stall = {b.id, b.enabled ? StallReason::Running : StallReason::Disabled, InvalidResourceId, 0};
    if (!b.enabled)
        return;
    for (std::size_t i = 0; i < t->recipes.size(); ++i) {
        const auto &r = t->recipes[i];
        if (b.remaining[i]) {
            b.stall.reason = StallReason::Running;
            return;
        }
        ResourceId x{};
        if (!Inputs(b, *t, r, x)) {
            b.stall.reason = StallReason::MissingInput;
            b.stall.resource = x;
            b.stall.recipeIndex = static_cast<std::uint32_t>(i);
            return;
        }
        if (!Room(b, *t, r, x)) {
            b.stall.reason = StallReason::OutputFull;
            b.stall.resource = x;
            b.stall.recipeIndex = static_cast<std::uint32_t>(i);
            return;
        }
    }
}
CommandId World::PlaceBuilding(BuildingTypeId t, Cell c) {
    Pending p;
    p.kind = Kind::Place;
    p.type = t;
    p.a = c;
    return Queue(impl_->state, p);
}
CommandId World::RemoveBuilding(BuildingId b) {
    Pending p;
    p.kind = Kind::Remove;
    p.building = b;
    return Queue(impl_->state, p);
}
CommandId World::BuildRoad(Cell a, Cell b) {
    Pending p;
    p.kind = Kind::Road;
    p.a = a;
    p.b = b;
    return Queue(impl_->state, p);
}
CommandId World::SetRoadEnabled(RoadId r, bool e) {
    Pending p;
    p.kind = Kind::RoadEnabled;
    p.road = r;
    p.enabled = e;
    return Queue(impl_->state, p);
}
CommandId World::SetBuildingEnabled(BuildingId b, bool e) {
    Pending p;
    p.kind = Kind::BuildingEnabled;
    p.building = b;
    p.enabled = e;
    return Queue(impl_->state, p);
}
CommandId World::CreateHaulOrder(BuildingId a, BuildingId b, ResourceId r, VehicleTypeId v) {
    Pending p;
    p.kind = Kind::Haul;
    p.source = a;
    p.destination = b;
    p.resource = r;
    p.vehicleType = v;
    return Queue(impl_->state, p);
}
CommandId World::CancelHaulOrder(OrderId o) {
    Pending p;
    p.kind = Kind::Cancel;
    p.order = o;
    return Queue(impl_->state, p);
}
CommandId World::RetargetHaulOrder(OrderId o, BuildingId destination) {
    Pending p;
    p.kind = Kind::Retarget;
    p.order = o;
    p.destination = destination;
    return Queue(impl_->state, p);
}
CommandId World::ConnectUtility(ResourceId resource, BuildingId a, BuildingId b) {
    Pending p;
    p.kind = Kind::ConnectUtility;
    p.resource = resource;
    p.source = std::min(a, b);
    p.destination = std::max(a, b);
    const auto id = Queue(impl_->state, p);
    if (id)
        impl_->state.utilityCommands.insert(id);
    return id;
}
CommandId World::DisconnectUtility(ResourceId resource, BuildingId a, BuildingId b) {
    Pending p;
    p.kind = Kind::DisconnectUtility;
    p.resource = resource;
    p.source = std::min(a, b);
    p.destination = std::max(a, b);
    const auto id = Queue(impl_->state, p);
    if (id)
        impl_->state.utilityCommands.insert(id);
    return id;
}
CommandResult World::GetCommandResult(CommandId id) const {
    auto i = impl_->state.results.find(id);
    if (i != impl_->state.results.end())
        return i->second;
    if (id != 0 && id < impl_->state.firstRetainedCommand)
        return {id, CommandState::Expired, CommandFailure::None, "command result expired", 0, 0, 0};
    return {id, CommandState::Unknown, CommandFailure::InvalidCommand, "unknown command", 0, 0, 0};
}
CommandHistorySnapshot World::CommandHistory() const noexcept {
    const auto &s = impl_->state;
    return {s.firstRetainedCommand, s.nextCommand,
            static_cast<std::uint64_t>(s.results.size() - s.pending.size()),
            static_cast<std::uint64_t>(s.pending.size())};
}
CommandHistoryCompactionResult World::CompactCommandHistory(std::uint32_t keepRecent) {
    auto &s = impl_->state;
    if (keepRecent > MaxCommandHistoryRetention)
        return {CommandHistoryError::InvalidRetention, 0, CommandHistory()};

    const auto terminalCount = static_cast<std::uint64_t>(s.results.size() - s.pending.size());
    const auto retainedTerminal = std::min<std::uint64_t>(keepRecent, terminalCount);
    const auto firstPending = s.nextCommand - static_cast<CommandId>(s.pending.size());
    const auto newFloor = firstPending - retainedTerminal;
    const auto removed = terminalCount - retainedTerminal;
    if (!removed)
        return {CommandHistoryError::None, 0, CommandHistory()};

    const auto remainingRevision = UINT64_MAX - s.revision;
    const auto requiredRevision = s.pending.empty() ? std::uint64_t{1}
                                                    : static_cast<std::uint64_t>(s.pending.size()) + 2;
    if (remainingRevision < requiredRevision)
        return {CommandHistoryError::RevisionExhausted, 0, CommandHistory()};

    auto result = s.results.begin();
    while (result != s.results.end() && result->first < newFloor)
        result = s.results.erase(result);
    auto utility = s.utilityCommands.begin();
    while (utility != s.utilityCommands.end() && *utility < newFloor)
        utility = s.utilityCommands.erase(utility);
    s.firstRetainedCommand = newFloor;
    ++s.revision;
    return {CommandHistoryError::None, removed, CommandHistory()};
}
StepResult World::Step(std::uint32_t n) {
    if (n > MaxStep)
        return {false, CommandFailure::StepLimitExceeded, "step exceeds 100000 ticks"};
    const auto &s = impl_->state;
    if (n && (n > UINT64_MAX - s.tick || n > UINT64_MAX - s.revision ||
              s.pending.size() > UINT64_MAX - s.revision - n))
        return {false, CommandFailure::StepLimitExceeded, "tick or revision counter exhausted"};
    for (std::uint32_t i = 0; i < n; ++i)
        impl_->utilityDiagnostics = AdvanceTick(impl_->state);
    return {true, CommandFailure::None, {}};
}
StallRecord World::Explain(BuildingId id) const {
    auto *b = Find(impl_->state, id);
    if (!b)
        return {id, StallReason::MissingInput, InvalidResourceId, 0};
    return b->stall;
}
UtilityDiagnostics World::QueryUtilityDiagnostics() const {
    return impl_->utilityDiagnostics;
}
WorldSnapshot World::Snapshot() const {
    const auto &s = impl_->state;
    WorldSnapshot o;
    o.gridWidth = s.width;
    o.gridDepth = s.depth;
    o.tick = s.tick;
    o.revision = s.revision;
    o.stateHash = Hash(s);
    o.commandHistory = CommandHistory();
    for (const auto &b : s.buildings) {
        BuildingSnapshot x;
        x.id = b.id;
        x.type = b.type;
        x.origin = b.origin;
        x.access = b.access;
        x.enabled = b.enabled;
        x.stall = b.stall;
        auto *t = s.catalog->BuildingType(b.type);
        for (std::size_t i = 0; i < t->storage.size(); ++i)
            x.inventory.push_back({t->storage[i].resource, b.inventory[i], t->storage[i].capacity});
        for (std::size_t i = 0; i < t->recipes.size(); ++i)
            x.recipes.push_back({b.remaining[i], b.completed[i]});
        o.buildings.push_back(std::move(x));
    }
    for (const auto &r : s.roads)
        o.roads.push_back({r.id, r.a, r.b, r.enabled});
    for (const auto &v : s.vehicles)
        o.vehicles.push_back({v.id, v.order, v.type, v.cell, v.cargo, v.state});
    for (const auto &q : s.orders) {
        const auto *v = FindVehicle(s, q.vehicle);
        o.orders.push_back({q.id, q.source, q.destination, q.resource, q.vehicleType, q.vehicle,
                            v ? v->state : HaulState::NoRoute, v ? v->failure : HaulFailure::NoRoute,
                            v ? v->cargo : 0, v ? v->cell : Cell{}});
    }
    o.utilityConnections = s.utilityConnections;
    return o;
}

std::string SaveWorld(const World &w) {
    const auto &s = w.impl_->state;
    const bool hasPendingUtility = std::any_of(s.pending.begin(), s.pending.end(), [](const Pending &p) {
        return p.kind == Kind::ConnectUtility || p.kind == Kind::DisconnectUtility;
    });
    const bool hasUtilityFailure = std::any_of(s.results.begin(), s.results.end(), [](const auto &entry) {
        return entry.second.failure >= CommandFailure::DuplicateUtilityConnection;
    });
    bool hasUtilityRecipes = false;
    for (const auto &type : s.catalog->BuildingTypes())
        for (const auto &recipe : type.recipes)
            hasUtilityRecipes = hasUtilityRecipes || HasUtility(*s.catalog, recipe.inputs) ||
                                HasUtility(*s.catalog, recipe.outputs);
    const bool version4 = !s.utilityConnections.empty() || hasPendingUtility || !s.utilityCommands.empty() ||
                          hasUtilityFailure || hasUtilityRecipes;
    std::string o = std::string("OPENREPUBLIC_ECONOMY_WORLD ") +
                    (version4 ? "4\n" : (s.firstRetainedCommand == 1 ? "2\n" : "3\n"));
    o += "GRID " + Num(s.width) + " " + Num(s.depth) + "\nTICK " + Num(s.tick) + "\nREVISION " +
         Num(s.revision) + "\nNEXT " + Num(s.nextBuilding) + " " + Num(s.nextRoad) + " " + Num(s.nextOrder) +
         " " + Num(s.nextVehicle) + " " + Num(s.nextCommand) + "\n";
    if (version4 || s.firstRetainedCommand > 1) {
        // v3 records only the explicit retained-history boundary. v1/v2 bytes
        // remain unchanged while the entire command history is retained.
        o += "HISTORY " + Num(s.firstRetainedCommand) + "\n";
    }
    o += "BUILDINGS " + Num(s.buildings.size()) + "\n";
    for (const auto &b : s.buildings) {
        o += "B " + Num(b.id) + " " + Num(b.type) + " " + std::to_string(b.origin.x) + " " +
             std::to_string(b.origin.z) + " " + Num(b.enabled) + "\nI " + Num(b.id) + " " +
             Num(b.inventory.size());
        for (auto x : b.inventory)
            o += " " + Num(x);
        o += "\nP " + Num(b.id) + " " + Num(b.remaining.size());
        for (auto x : b.remaining)
            o += " " + Num(x);
        for (auto x : b.completed)
            o += " " + Num(x);
        o += "\n";
        if (o.size() > MaxWorldSaveBytes)
            return {};
    }
    o += "ROADS " + Num(s.roads.size()) + "\n";
    for (const auto &r : s.roads) {
        o += "R " + Num(r.id) + " " + std::to_string(r.a.x) + " " + std::to_string(r.a.z) + " " +
             std::to_string(r.b.x) + " " + std::to_string(r.b.z) + " " + Num(r.enabled) + "\n";
        if (o.size() > MaxWorldSaveBytes)
            return {};
    }
    if (version4) {
        o += "UTILITY_CONNECTIONS " + Num(s.utilityConnections.size()) + "\n";
        for (const auto &edge : s.utilityConnections) {
            o += "U " + Num(edge.resource) + " " + Num(edge.a) + " " + Num(edge.b) + "\n";
            if (o.size() > MaxWorldSaveBytes)
                return {};
        }
        o += "UTILITY_STALLS " + Num(s.buildings.size()) + "\n";
        for (const auto &building : s.buildings) {
            o += "S " + Num(building.id) + " " + Num(static_cast<std::uint8_t>(building.stall.reason)) + " " +
                 Num(building.stall.resource) + " " + Num(building.stall.recipeIndex) + "\n";
            if (o.size() > MaxWorldSaveBytes)
                return {};
        }
        o += "UTILITY_COMMANDS " + Num(s.utilityCommands.size());
        for (const auto id : s.utilityCommands)
            o += " " + Num(id);
        o += "\n";
    }
    o += "ORDERS " + Num(s.orders.size()) + "\n";
    for (const auto &q : s.orders) {
        auto *v = FindVehicle(s, q.vehicle);
        o +=
            "O " + Num(q.id) + " " + Num(q.source) + " " + Num(q.destination) + " " + Num(q.resource) + " " +
            Num(q.vehicleType) + " " + Num(q.vehicle) + " " +
            Num(v ? static_cast<std::uint8_t>(v->state) : static_cast<std::uint8_t>(HaulState::NoRoute)) +
            " " +
            Num(v ? static_cast<std::uint8_t>(v->failure) : static_cast<std::uint8_t>(HaulFailure::NoRoute)) +
            " " + Num(v ? v->cargo : 0) + " " + std::to_string(v ? v->cell.x : 0) + " " +
            std::to_string(v ? v->cell.z : 0) + " " + Num(v ? v->toDestination : 1) + "\n";
        if (o.size() > MaxWorldSaveBytes)
            return {};
    }
    o += "PENDING " + Num(s.pending.size()) + "\n";
    for (const auto &p : s.pending) {
        o += "Q " + Num(p.id) + " " + Num(static_cast<std::uint8_t>(p.kind)) + " " + Num(p.type) + " " +
             Num(p.building) + " " + std::to_string(p.a.x) + " " + std::to_string(p.a.z) + " " +
             std::to_string(p.b.x) + " " + std::to_string(p.b.z) + " " + Num(p.road) + " " + Num(p.enabled) +
             " " + Num(p.source) + " " + Num(p.destination) + " " + Num(p.resource) + " " +
             Num(p.vehicleType) + " " + Num(p.order) + "\n";
        if (o.size() > MaxWorldSaveBytes)
            return {};
    }
    o += "RESULTS " + Num(s.results.size()) + "\n";
    for (const auto &r : s.results) {
        o += "C " + Num(r.first) + " " + Num(static_cast<std::uint8_t>(r.second.state)) + " " +
             Num(static_cast<std::uint8_t>(r.second.failure)) + " " + Num(r.second.building) + " " +
             Num(r.second.road) + " " + Num(r.second.order) + "\n";
        if (o.size() > MaxWorldSaveBytes)
            return {};
    }
    o += "END\n";
    if (o.size() > MaxWorldSaveBytes)
        return {};
    return o;
}

bool LoadWorld(std::string_view text, const Catalog &c, World &target, ParseError &e) {
    e = {};
    if (text.empty() || text.size() > MaxWorldSaveBytes) {
        ParseErr(e, ParseErrorCode::LimitExceeded, 0, "invalid world save size");
        return false;
    }
    WorldSaveInput in(text);
    std::string tag, version;
    if (!(in >> tag >> version) || tag != "OPENREPUBLIC_ECONOMY_WORLD" ||
        (version != "1" && version != "2" && version != "3" && version != "4")) {
        ParseErr(e, ParseErrorCode::InvalidHeader, 1, "invalid world header");
        return false;
    }
    std::uint32_t w{}, d{};
    BuildingId nb{};
    RoadId nr{};
    OrderId no{};
    VehicleId nv{};
    std::uint64_t tick{}, rev{}, nc{}, count{};
    if (!(in >> tag >> w >> d) || tag != "GRID" || !(in >> tag >> tick) || tag != "TICK" ||
        !(in >> tag >> rev) || tag != "REVISION" || !(in >> tag >> nb >> nr >> no >> nv >> nc) ||
        tag != "NEXT" || !w || !d || w > MaxWorldGridSize || d > MaxWorldGridSize ||
        !nb || !nr || !no || !nv || !nc || rev < tick) {
        ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "invalid world preamble");
        return false;
    }
    World temp(c, w, d);
    State &s = temp.impl_->state;
    std::map<BuildingId, StallRecord> utilityStalls;
    std::set<CommandId> recordedUtilityCommands;
    s.tick = tick;
    s.revision = rev;
    s.nextBuilding = nb;
    s.nextRoad = nr;
    s.nextOrder = no;
    s.nextVehicle = nv;
    s.nextCommand = nc;
    if (version == "3" || version == "4") {
        if (!(in >> tag >> s.firstRetainedCommand) || tag != "HISTORY" ||
            !s.firstRetainedCommand || s.firstRetainedCommand > nc) {
            ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "invalid command history boundary");
            return false;
        }
    }
    if (!(in >> tag >> count) || tag != "BUILDINGS" || count > text.size()) {
        ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "invalid building count");
        return false;
    }
    CommandId expectedResultId = s.firstRetainedCommand;
    for (std::uint64_t n = 0; n < count; ++n) {
        Building b;
        std::uint32_t enabled{};
        if (!(in >> tag >> b.id >> b.type >> b.origin.x >> b.origin.z >> enabled) || tag != "B" ||
            !s.catalog->BuildingType(b.type) || enabled > 1 || !b.id || b.id >= nb ||
            (!s.buildings.empty() && b.id <= s.buildings.back().id)) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid building");
            return false;
        }
        b.enabled = enabled != 0;
        auto *t = s.catalog->BuildingType(b.type);
        const auto placementFailure = PlacementFailure(s, *t, b.origin);
        if (placementFailure != CommandFailure::None) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, PlacementMessage(placementFailure));
            return false;
        }
        b.access = {b.origin.x + static_cast<std::int32_t>((t->width - 1) / 2), b.origin.z - 1};
        std::uint64_t recordId{}, size{};
        if (!(in >> tag >> recordId >> size) || tag != "I" || recordId != b.id || size != t->storage.size()) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid inventory count");
            return false;
        }
        b.inventory.resize(static_cast<std::size_t>(size));
        for (auto &x : b.inventory)
            if (!(in >> x)) {
                ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid inventory");
                return false;
            }
        if (!(in >> tag >> recordId >> size) || tag != "P" || recordId != b.id || size != t->recipes.size()) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid progress count");
            return false;
        }
        b.remaining.resize(static_cast<std::size_t>(size));
        for (auto &x : b.remaining)
            if (!(in >> x)) {
                ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid progress");
                return false;
            }
        b.completed.resize(static_cast<std::size_t>(size));
        for (auto &x : b.completed)
            if (!(in >> x) || x > tick) {
                ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid completion");
                return false;
            }
        for (std::size_t i = 0; i < b.inventory.size(); ++i)
            if (b.inventory[i] > t->storage[i].capacity) {
                ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "inventory exceeds definition");
                return false;
            }
        for (std::size_t i = 0; i < b.remaining.size(); ++i)
            if (b.remaining[i] > t->recipes[i].intervalTicks ||
                ((HasUtility(c, t->recipes[i].inputs) || HasUtility(c, t->recipes[i].outputs)) &&
                 b.remaining[i] != 0)) {
                ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "progress exceeds definition");
                return false;
            }
        if (!b.id || Find(s, b.id)) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "duplicate or zero building ID");
            return false;
        }
        b.stall = {b.id, b.enabled ? StallReason::Running : StallReason::Disabled, InvalidResourceId, 0};
        s.buildings.push_back(std::move(b));
    }
    if (!(in >> tag >> count) || tag != "ROADS" || count > text.size()) {
        ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "invalid road count");
        return false;
    }
    for (std::uint64_t n = 0; n < count; ++n) {
        Road r;
        std::uint32_t en{};
        if (!(in >> tag >> r.id >> r.a.x >> r.a.z >> r.b.x >> r.b.z >> en) || tag != "R" ||
            !InGrid(r.a, w, d) || !InGrid(r.b, w, d) || !Adjacent(r.a, r.b) ||
            RoadBetween(s, r.a, r.b, false) || en > 1 || r.id >= nr ||
            (!s.roads.empty() && r.id <= s.roads.back().id)) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid road");
            return false;
        }
        r.enabled = en != 0;
        if (Occupied(s, r.a) || Occupied(s, r.b)) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "road endpoint overlaps a building footprint");
            return false;
        }
        if (!r.id || Less(r.b, r.a) || FindRoad(s, r.id)) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid or duplicate road ID");
            return false;
        }
        s.roads.push_back(r);
    }
    if (version == "4") {
        if (!(in >> tag >> count) || tag != "UTILITY_CONNECTIONS" || count > text.size()) {
            ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "invalid utility connection count");
            return false;
        }
        for (std::uint64_t n = 0; n < count; ++n) {
            UtilityConnectionSnapshot edge;
            if (!(in >> tag >> edge.resource >> edge.a >> edge.b) || tag != "U" || edge.a >= edge.b ||
                !c.Resource(edge.resource) || !c.Resource(edge.resource)->utility || !Find(s, edge.a) ||
                !Find(s, edge.b) || (!s.utilityConnections.empty() &&
                    std::tie(edge.resource, edge.a, edge.b) <=
                        std::tie(s.utilityConnections.back().resource, s.utilityConnections.back().a,
                                 s.utilityConnections.back().b))) {
                ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid or duplicate utility connection");
                return false;
            }
            s.utilityConnections.push_back(edge);
        }
        if (!(in >> tag >> count) || tag != "UTILITY_STALLS" || count != s.buildings.size()) {
            ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "invalid utility stall count");
            return false;
        }
        for (const auto &building : s.buildings) {
            BuildingId id{};
            std::uint32_t reason{}, resource{}, recipe{};
            if (!(in >> tag >> id >> reason >> resource >> recipe) || tag != "S" || id != building.id ||
                reason > static_cast<std::uint32_t>(StallReason::Disabled) ||
                (resource != InvalidResourceId && !c.Resource(resource)) ||
                (c.BuildingType(building.type)->recipes.empty() ? recipe != 0
                                                                : recipe >= c.BuildingType(building.type)->recipes.size())) {
                ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid utility stall state");
                return false;
            }
            utilityStalls.emplace(id, StallRecord{id, static_cast<StallReason>(reason), resource, recipe});
        }
        if (!(in >> tag >> count) || tag != "UTILITY_COMMANDS" || count > text.size()) {
            ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "invalid utility command history count");
            return false;
        }
        CommandId lastUtilityCommand = 0;
        for (std::uint64_t n = 0; n < count; ++n) {
            CommandId id{};
            if (!(in >> id) || !id || id <= lastUtilityCommand || id < s.firstRetainedCommand || id >= nc) {
                ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid utility command history ID");
                return false;
            }
            recordedUtilityCommands.insert(id);
            lastUtilityCommand = id;
        }
    }
    if (!(in >> tag >> count) || tag != "ORDERS" || count > text.size()) {
        ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "invalid order count");
        return false;
    }
    for (std::uint64_t n = 0; n < count; ++n) {
        Order q;
        std::uint32_t st{}, fail{}, to{};
        std::uint64_t cargo{};
        Cell cell{};
        VehicleId vehicle{};
        if (!(in >> tag >> q.id >> q.source >> q.destination >> q.resource >> q.vehicleType >> vehicle >>
              st >> fail >> cargo >> cell.x >> cell.z >> to) ||
            tag != "O" || !Find(s, q.source) || !Find(s, q.destination) || !s.catalog->Resource(q.resource) ||
            s.catalog->Resource(q.resource)->utility ||
            !s.catalog->VehicleType(q.vehicleType) || st > static_cast<std::uint32_t>(HaulState::NoRoute) ||
            fail > static_cast<std::uint32_t>(HaulFailure::RoadDisabled) || to > 1 ||
            !InGrid(cell, w, d) || cargo > s.catalog->VehicleType(q.vehicleType)->capacity ||
            (cargo > 0 && (to == 0 || st == static_cast<std::uint32_t>(HaulState::Idle) ||
                          st == static_cast<std::uint32_t>(HaulState::ToSource)))) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid order");
            return false;
        }
        q.vehicle = vehicle;
        if (!q.id || q.id >= no || !vehicle || vehicle >= nv ||
            (!s.orders.empty() && q.id <= s.orders.back().id) ||
            (!s.vehicles.empty() && vehicle <= s.vehicles.back().id)) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "duplicate or zero order or vehicle ID");
            return false;
        }
        const auto *sourceType = s.catalog->BuildingType(Find(s, q.source)->type);
        const auto *destinationType = s.catalog->BuildingType(Find(s, q.destination)->type);
        if (Slot(*sourceType, q.resource) >= sourceType->storage.size() ||
            Slot(*destinationType, q.resource) >= destinationType->storage.size()) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "order resource is not stored at an endpoint");
            return false;
        }
        for (const auto &o : s.orders)
            if (o.source == q.source && o.destination == q.destination && o.resource == q.resource) {
                ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "duplicate haul endpoints and resource");
                return false;
            }
        s.orders.push_back(q);
        s.vehicles.push_back({vehicle,
                              q.id,
                              q.vehicleType,
                              cell,
                              cargo,
                              static_cast<HaulState>(st),
                              static_cast<HaulFailure>(fail),
                              to != 0,
                              {},
                              0});
    }
    if (!(in >> tag >> count) || tag != "PENDING" || count > text.size()) {
        ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "invalid pending count");
        return false;
    }
    for (std::uint64_t n = 0; n < count; ++n) {
        Pending p;
        std::uint32_t kind{};
        if (!(in >> tag >> p.id >> kind >> p.type >> p.building >> p.a.x >> p.a.z >> p.b.x >> p.b.z >>
              p.road >> p.enabled >> p.source >> p.destination >> p.resource >> p.vehicleType >> p.order) ||
            tag != "Q" || kind < 1 || kind > (version == "1" ? 7u : (version == "4" ? 10u : 8u)) ||
            !p.id || p.id >= nc ||
            (!s.pending.empty() && p.id <= s.pending.back().id)) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid pending command");
            return false;
        }
        p.kind = static_cast<Kind>(kind);
        if (p.kind == Kind::ConnectUtility || p.kind == Kind::DisconnectUtility) {
            if (version != "4" || p.source > p.destination || p.type || p.building || p.a != Cell{} ||
                p.b != Cell{} || p.road || p.enabled || p.vehicleType || p.order) {
                ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid pending utility command");
                return false;
            }
            if (!recordedUtilityCommands.count(p.id)) {
                ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "pending utility command is absent from its history");
                return false;
            }
        }
        s.pending.push_back(p);
    }
    if (!(in >> tag >> count) || tag != "RESULTS" || count > text.size()) {
        ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "invalid result count");
        return false;
    }
    for (std::uint64_t n = 0; n < count; ++n) {
        CommandResult r;
        std::uint32_t st{}, fail{};
        if (!(in >> tag >> r.commandId >> st >> fail >> r.building >> r.road >> r.order) || tag != "C" ||
            st < 1 || st > 3 || fail > static_cast<std::uint32_t>(version == "1" ? CommandFailure::ParseInvalid
                                                              : (version == "4"
                                                                     ? CommandFailure::ReferencedByUtilityConnection
                                                                     : CommandFailure::CargoInTransit))) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid command result");
            return false;
        }
        r.state = static_cast<CommandState>(st);
        r.failure = static_cast<CommandFailure>(fail);
        const auto outputIds = (r.building != 0) + (r.road != 0) + (r.order != 0);
        if (r.commandId != expectedResultId || r.commandId >= nc || r.building >= nb ||
            r.road >= nr || r.order >= no ||
            (r.state == CommandState::Accepted ?
                 (fail != 0 || (version == "4" && recordedUtilityCommands.count(r.commandId)
                                    ? outputIds != 0
                                    : outputIds != 1))
                                               : (outputIds != 0 ||
                                                  (r.state == CommandState::Queued ? fail != 0 : fail == 0)))) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "inconsistent command result or ID history");
            return false;
        }
        r.message = r.state == CommandState::Queued ? "queued" : "restored result";
        s.results[r.commandId] = r;
        ++expectedResultId;
    }
    for (const auto id : recordedUtilityCommands) {
        const auto result = s.results.find(id);
        if (result == s.results.end()) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "utility command history has no retained result");
            return false;
        }
        const bool pendingUtility = std::any_of(s.pending.begin(), s.pending.end(),
                                                [&](const Pending &pending) {
                                                    return pending.id == id &&
                                                           (pending.kind == Kind::ConnectUtility ||
                                                            pending.kind == Kind::DisconnectUtility);
                                                });
        const auto failure = result->second.failure;
        const bool utilityFailure = failure == CommandFailure::InvalidUtilityConnection ||
                                    failure == CommandFailure::DuplicateUtilityConnection ||
                                    failure == CommandFailure::MissingUtilityConnection;
        if ((pendingUtility && result->second.state != CommandState::Queued) ||
            (!pendingUtility && (result->second.state == CommandState::Queued ||
                                 (result->second.state == CommandState::Rejected && !utilityFailure)))) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "utility command history result is inconsistent");
            return false;
        }
        s.utilityCommands.insert(id);
    }
    if (!(in >> tag) || tag != "END") {
        ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "missing END");
        return false;
    }
    if (in >> tag) {
        ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "trailing world data");
        return false;
    }
    if (s.firstRetainedCommand > nc || s.results.size() != nc - s.firstRetainedCommand ||
        s.pending.size() > s.results.size()) {
        ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "incomplete command history");
        return false;
    }
    CommandId queuedId = nc - s.pending.size();
    for (const auto &p : s.pending)
        if (p.id != queuedId++ || s.results.at(p.id).state != CommandState::Queued) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "pending commands do not match queued results");
            return false;
        }
    const CommandId firstPending = nc - s.pending.size();
    if (s.firstRetainedCommand > firstPending) {
        ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "pending commands fall below retained history");
        return false;
    }
    for (const auto &r : s.results)
        if ((r.second.state == CommandState::Queued) != (r.first >= firstPending)) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "orphan or nonterminal command result");
            return false;
        }
    const auto lifetimeAppliedCommands = nc - 1 - s.pending.size();
    if (lifetimeAppliedCommands > rev - tick) {
        ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "revision precedes applied command history");
        return false;
    }
    if (!s.pending.empty() && (tick == UINT64_MAX || s.pending.size() >= UINT64_MAX - rev)) {
        ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "queued commands cannot advance without counter overflow");
        return false;
    }
    for (auto &b : s.buildings) {
        RefreshStall(s, b);
        if (version == "4")
            b.stall = utilityStalls.at(b.id);
    }
    target.impl_->state = std::move(s);
    target.impl_->utilityDiagnostics = {};
    return true;
}

} // namespace openrepublic::economy
