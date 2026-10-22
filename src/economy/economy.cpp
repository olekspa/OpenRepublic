#include "openrepublic/economy/economy.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <utility>

namespace openrepublic::economy {
namespace {
constexpr std::size_t MaxName = 96;
constexpr std::size_t MaxDefinitions = 1024;
constexpr std::size_t MaxText = 1024 * 1024;
constexpr std::size_t MaxLine = 4096;
constexpr std::uint32_t MaxStep = 100000;

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
    Cancel
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
    std::vector<Building> buildings;
    std::vector<Road> roads;
    std::vector<Vehicle> vehicles;
    std::vector<Order> orders;
    std::vector<Pending> pending;
    std::map<CommandId, CommandResult> results;
};
std::size_t Slot(const BuildingTypeDef &t, ResourceId r) {
    for (std::size_t i = 0; i < t.storage.size(); ++i)
        if (t.storage[i].resource == r)
            return i;
    return t.storage.size();
}
Building *Find(State &s, BuildingId id) {
    for (auto &x : s.buildings)
        if (x.id == id)
            return &x;
    return nullptr;
}
const Building *Find(const State &s, BuildingId id) {
    for (const auto &x : s.buildings)
        if (x.id == id)
            return &x;
    return nullptr;
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
bool Occupied(const State &s, Cell c) {
    for (const auto &b : s.buildings) {
        const auto *t = s.catalog->BuildingType(b.type);
        for (std::uint32_t x = 0; x < t->width; ++x)
            for (std::uint32_t z = 0; z < t->depth; ++z)
                if (b.origin.x + static_cast<std::int32_t>(x) == c.x &&
                    b.origin.z + static_cast<std::int32_t>(z) == c.z)
                    return true;
    }
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
bool Fits(const State &s, const BuildingTypeDef &t, Cell o) {
    if (o.x < 0 || o.z < 0 || static_cast<std::uint64_t>(o.x) + t.width > s.width ||
        static_cast<std::uint64_t>(o.z) + t.depth > s.depth)
        return false;
    for (std::uint32_t x = 0; x < t.width; ++x)
        for (std::uint32_t z = 0; z < t.depth; ++z)
            if (Occupied(s, {o.x + static_cast<std::int32_t>(x), o.z + static_cast<std::int32_t>(z)}))
                return false;
    return InGrid({o.x + static_cast<std::int32_t>((t.width - 1) / 2), o.z - 1}, s.width, s.depth);
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
        if (!Fits(s, *t, p.a)) {
            reject(CommandFailure::BuildingCollision, "building footprint or access cell unavailable");
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
    if (p.kind == Kind::Cancel) {
        auto *o = FindOrder(s, p.order);
        if (!o) {
            reject(CommandFailure::UnknownOrder, "unknown haul order");
            return;
        }
        auto v = o->vehicle;
        s.orders.erase(
            std::remove_if(s.orders.begin(), s.orders.end(), [&](const Order &x) { return x.id == p.order; }),
            s.orders.end());
        s.vehicles.erase(
            std::remove_if(s.vehicles.begin(), s.vehicles.end(), [&](const Vehicle &x) { return x.id == v; }),
            s.vehicles.end());
        Result(s, p, true, CommandFailure::None, "haul order cancelled", 0, 0, p.order);
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
        if (v->cargo == 0 && (!v->toDestination || v->state != HaulState::NoRoute)) {
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
        } else if (v->state == HaulState::NoRoute || v->state == HaulState::Waiting)
            v->state = v->toDestination ? HaulState::ToDestination : HaulState::ToSource;
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
void AdvanceTick(State &s) {
    auto pending = std::move(s.pending);
    s.pending.clear();
    for (const auto &p : pending) {
        Apply(s, p);
        ++s.revision;
    }
    std::sort(s.buildings.begin(), s.buildings.end(),
              [](const Building &a, const Building &b) { return a.id < b.id; });
    for (auto &b : s.buildings)
        Production(s, b);
    std::sort(s.orders.begin(), s.orders.end(), [](const Order &a, const Order &b) { return a.id < b.id; });
    for (auto &o : s.orders)
        Haul(s, o);
    ++s.tick;
    ++s.revision;
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
    ResourceDef x{static_cast<ResourceId>(draft_.resources_.size()), std::string(n)};
    draft_.resources_.push_back(x);
    if (out)
        *out = x.id;
    e = {};
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
            if (Slot(*t, a.resource) >= t->storage.size()) {
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
        if (w[0] == "resource" && w.size() == 2) {
            if (!b.AddResource(w[1], nullptr, ce)) {
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
        o += "resource " + r.name + "\n";
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
};
World::World(const Catalog &c, std::uint32_t w, std::uint32_t d) : impl_(new Impl) {
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
std::uint64_t World::Tick() const noexcept {
    return impl_->state.tick;
}
std::uint64_t World::Revision() const noexcept {
    return impl_->state.revision;
}
CommandId Queue(State &s, Pending p) {
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
CommandResult World::GetCommandResult(CommandId id) const {
    auto i = impl_->state.results.find(id);
    if (i != impl_->state.results.end())
        return i->second;
    return {id, CommandState::Unknown, CommandFailure::InvalidCommand, "unknown command", 0, 0, 0};
}
StepResult World::Step(std::uint32_t n) {
    if (n > MaxStep)
        return {false, CommandFailure::StepLimitExceeded, "step exceeds 100000 ticks"};
    for (std::uint32_t i = 0; i < n; ++i)
        AdvanceTick(impl_->state);
    return {true, CommandFailure::None, {}};
}
StallRecord World::Explain(BuildingId id) const {
    auto *b = Find(impl_->state, id);
    if (!b)
        return {id, StallReason::MissingInput, InvalidResourceId, 0};
    return b->stall;
}
WorldSnapshot World::Snapshot() const {
    const auto &s = impl_->state;
    WorldSnapshot o;
    o.gridWidth = s.width;
    o.gridDepth = s.depth;
    o.tick = s.tick;
    o.revision = s.revision;
    o.stateHash = Hash(s);
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
    return o;
}

std::string SaveWorld(const World &w) {
    const auto &s = w.impl_->state;
    std::string o = "OPENREPUBLIC_ECONOMY_WORLD 1\n";
    o += "GRID " + Num(s.width) + " " + Num(s.depth) + "\nTICK " + Num(s.tick) + "\nREVISION " +
         Num(s.revision) + "\nNEXT " + Num(s.nextBuilding) + " " + Num(s.nextRoad) + " " + Num(s.nextOrder) +
         " " + Num(s.nextVehicle) + " " + Num(s.nextCommand) + "\nBUILDINGS " + Num(s.buildings.size()) +
         "\n";
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
    }
    o += "ROADS " + Num(s.roads.size()) + "\n";
    for (const auto &r : s.roads)
        o += "R " + Num(r.id) + " " + std::to_string(r.a.x) + " " + std::to_string(r.a.z) + " " +
             std::to_string(r.b.x) + " " + std::to_string(r.b.z) + " " + Num(r.enabled) + "\n";
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
    }
    o += "PENDING " + Num(s.pending.size()) + "\n";
    for (const auto &p : s.pending)
        o += "Q " + Num(p.id) + " " + Num(static_cast<std::uint8_t>(p.kind)) + " " + Num(p.type) + " " +
             Num(p.building) + " " + std::to_string(p.a.x) + " " + std::to_string(p.a.z) + " " +
             std::to_string(p.b.x) + " " + std::to_string(p.b.z) + " " + Num(p.road) + " " + Num(p.enabled) +
             " " + Num(p.source) + " " + Num(p.destination) + " " + Num(p.resource) + " " +
             Num(p.vehicleType) + " " + Num(p.order) + "\n";
    o += "RESULTS " + Num(s.results.size()) + "\n";
    for (const auto &r : s.results)
        o += "C " + Num(r.first) + " " + Num(static_cast<std::uint8_t>(r.second.state)) + " " +
             Num(static_cast<std::uint8_t>(r.second.failure)) + " " + Num(r.second.building) + " " +
             Num(r.second.road) + " " + Num(r.second.order) + "\n";
    o += "END\n";
    return o;
}

bool LoadWorld(std::string_view text, const Catalog &c, World &target, ParseError &e) {
    e = {};
    if (text.empty() || text.size() > MaxText) {
        ParseErr(e, ParseErrorCode::LimitExceeded, 0, "invalid world save size");
        return false;
    }
    std::istringstream in{std::string(text)};
    std::string tag, version;
    if (!(in >> tag >> version) || tag != "OPENREPUBLIC_ECONOMY_WORLD" || version != "1") {
        ParseErr(e, ParseErrorCode::InvalidHeader, 1, "invalid world header");
        return false;
    }
    std::uint32_t w{}, d{};
    std::uint64_t tick{}, rev{}, nb{}, nr{}, no{}, nv{}, nc{}, count{};
    if (!(in >> tag >> w >> d) || tag != "GRID" || !(in >> tag >> tick) || tag != "TICK" ||
        !(in >> tag >> rev) || tag != "REVISION" || !(in >> tag >> nb >> nr >> no >> nv >> nc) ||
        tag != "NEXT") {
        ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "invalid world preamble");
        return false;
    }
    World temp(c, w, d);
    State &s = temp.impl_->state;
    s.tick = tick;
    s.revision = rev;
    s.nextBuilding = static_cast<BuildingId>(nb);
    s.nextRoad = static_cast<RoadId>(nr);
    s.nextOrder = static_cast<OrderId>(no);
    s.nextVehicle = static_cast<VehicleId>(nv);
    s.nextCommand = nc;
    if (!(in >> tag >> count) || tag != "BUILDINGS" || count > MaxDefinitions) {
        ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "invalid building count");
        return false;
    }
    for (std::uint64_t n = 0; n < count; ++n) {
        Building b;
        std::uint32_t enabled{};
        if (!(in >> tag >> b.id >> b.type >> b.origin.x >> b.origin.z >> enabled) || tag != "B" ||
            !s.catalog->BuildingType(b.type) || !Fits(s, *s.catalog->BuildingType(b.type), b.origin)) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid building");
            return false;
        }
        b.enabled = enabled != 0;
        auto *t = s.catalog->BuildingType(b.type);
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
            if (!(in >> x)) {
                ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid completion");
                return false;
            }
        for (std::size_t i = 0; i < b.inventory.size(); ++i)
            if (b.inventory[i] > t->storage[i].capacity) {
                ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "inventory exceeds definition");
                return false;
            }
        for (std::size_t i = 0; i < b.remaining.size(); ++i)
            if (b.remaining[i] > t->recipes[i].intervalTicks) {
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
    if (!(in >> tag >> count) || tag != "ROADS" || count > MaxDefinitions) {
        ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "invalid road count");
        return false;
    }
    for (std::uint64_t n = 0; n < count; ++n) {
        Road r;
        std::uint32_t en{};
        if (!(in >> tag >> r.id >> r.a.x >> r.a.z >> r.b.x >> r.b.z >> en) || tag != "R" ||
            !InGrid(r.a, w, d) || !InGrid(r.b, w, d) || !Adjacent(r.a, r.b) || Occupied(s, r.a) ||
            Occupied(s, r.b) || RoadBetween(s, r.a, r.b, false)) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid road");
            return false;
        }
        r.enabled = en != 0;
        if (!r.id || Less(r.b, r.a) || FindRoad(s, r.id)) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid or duplicate road ID");
            return false;
        }
        s.roads.push_back(r);
    }
    if (!(in >> tag >> count) || tag != "ORDERS" || count > MaxDefinitions) {
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
            !s.catalog->VehicleType(q.vehicleType) || st > static_cast<std::uint32_t>(HaulState::NoRoute) ||
            fail > static_cast<std::uint32_t>(HaulFailure::RoadDisabled)) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid order");
            return false;
        }
        q.vehicle = vehicle;
        if (!q.id || FindOrder(s, q.id) || !vehicle || FindVehicle(s, vehicle)) {
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
    if (!(in >> tag >> count) || tag != "PENDING" || count > MaxDefinitions) {
        ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "invalid pending count");
        return false;
    }
    for (std::uint64_t n = 0; n < count; ++n) {
        Pending p;
        std::uint32_t kind{};
        if (!(in >> tag >> p.id >> kind >> p.type >> p.building >> p.a.x >> p.a.z >> p.b.x >> p.b.z >>
              p.road >> p.enabled >> p.source >> p.destination >> p.resource >> p.vehicleType >> p.order) ||
            tag != "Q" || kind < 1 || kind > 7) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid pending command");
            return false;
        }
        p.kind = static_cast<Kind>(kind);
        s.pending.push_back(p);
    }
    if (!(in >> tag >> count) || tag != "RESULTS" || count > MaxDefinitions) {
        ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "invalid result count");
        return false;
    }
    for (std::uint64_t n = 0; n < count; ++n) {
        CommandResult r;
        std::uint32_t st{}, fail{};
        if (!(in >> tag >> r.commandId >> st >> fail >> r.building >> r.road >> r.order) || tag != "C" ||
            st > 3 || fail > static_cast<std::uint32_t>(CommandFailure::ParseInvalid)) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "invalid command result");
            return false;
        }
        r.state = static_cast<CommandState>(st);
        r.failure = static_cast<CommandFailure>(fail);
        if (!r.commandId || s.results.find(r.commandId) != s.results.end()) {
            ParseErr(e, ParseErrorCode::CatalogInvalid, 0, "duplicate or zero command result ID");
            return false;
        }
        r.message = r.state == CommandState::Queued ? "queued" : "restored result";
        s.results[r.commandId] = r;
    }
    if (!(in >> tag) || tag != "END") {
        ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "missing END");
        return false;
    }
    if (in >> tag) {
        ParseErr(e, ParseErrorCode::UnexpectedToken, 0, "trailing world data");
        return false;
    }
    for (auto &b : s.buildings)
        RefreshStall(s, b);
    target.impl_->state = std::move(s);
    return true;
}

} // namespace openrepublic::economy
