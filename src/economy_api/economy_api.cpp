#include "openrepublic/economy_api/economy_api.h"
#include "openrepublic/economy/checkpoint.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace openrepublic::economy_api {
namespace {
std::string StallText(const economy::Catalog& c, const economy::StallRecord& s) {
    switch (s.reason) {
    case economy::StallReason::Running:
        return "Production is running.";
    case economy::StallReason::Disabled:
        return "Building is disabled.";
    case economy::StallReason::MissingInput: {
        const auto* r = c.Resource(s.resource);
        return std::string("Waiting for input resource ") + (r ? r->name : "unknown") + ".";
    }
    case economy::StallReason::OutputFull: {
        const auto* r = c.Resource(s.resource);
        return std::string("Output storage is full for resource ") + (r ? r->name : "unknown") + ".";
    }
    }
    return "Unknown production state.";
}
BuildingView MakeBuilding(const economy::Catalog& c, const economy::BuildingSnapshot& b) {
    BuildingView v;
    v.id = b.id;
    const auto* type = c.BuildingType(b.type);
    v.type = type ? type->name : "";
    v.cell = b.origin;
    v.enabled = b.enabled;
    v.constructionSite=b.constructionSite;
    const auto* constructionType=c.BuildingType(b.type);
    if (constructionType && constructionType->constructionWorkTicks)
        v.constructionState=b.constructionState==0?"waiting_materials":(b.constructionState==1?"building":"done");
    v.constructionProgress=b.constructionProgress;
    v.stallReason = b.stall.reason;
    v.stallExplanation = StallText(c, b.stall);
    for (const auto& i : b.inventory) {
        const auto* r = c.Resource(i.resource);
        v.inventory.push_back({r ? r->name : "", i.amount, i.capacity});
    }
    for (const auto& item : b.constructionMaterials) {
        const auto* r = c.Resource(item.resource);
        v.constructionMaterials.push_back({r ? r->name : "", item.required, item.held});
    }
    return v;
}
WorldView MakeWorld(const economy::Catalog& c, const economy::WorldSnapshot& s) {
    WorldView v;
    v.revision = s.revision;
    v.tick = s.tick;
    v.stateHash = s.stateHash;
    v.treasury = s.treasury;
    v.tradeEnabled = std::any_of(c.Resources().begin(), c.Resources().end(),
        [](const auto& resource) { return resource.tradeDeclared; });
    v.commandHistory = s.commandHistory;
    for (const auto& b : s.buildings) {
        v.buildings.push_back(MakeBuilding(c, b));
    }
    for (const auto& r : s.roads) {
        v.roads.push_back({r.id, r.a, r.b, r.enabled});
    }
    for (const auto& x : s.vehicles) {
        const auto* t = c.VehicleType(x.type);
        v.vehicles.push_back({x.id, x.order, t ? t->name : "", x.cell, x.cargo, x.state, x.ownerDepot, x.leg, x.legTicksRemaining});
    }
    for (const auto& x : s.orders) {
        const auto* r = c.Resource(x.resource);
        const auto* t = c.VehicleType(x.vehicleType);
        v.orders.push_back({x.id, x.source, x.destination, r ? r->name : "",
            t ? t->name : "", x.vehicle, x.state, x.failure, x.cargo, x.vehicleCell});
    }
    for (const auto& x : s.utilityConnections) {
        const auto* r = c.Resource(x.resource);
        v.utilityConnections.push_back({r ? r->name : "", x.a, x.b});
    }
    return v;
}
}

EconomyApi::EconomyApi(economy::World& world, Permissions p)
    : catalog_(&world.GetCatalog()), world_(&world), permissions_(p) {}
EconomyApi::EconomyApi(const economy::Catalog& catalog, std::uint32_t w, std::uint32_t d,
    Permissions p) : ownedCatalog_(std::make_unique<economy::Catalog>(catalog)),
    permissions_(p) {
    catalog_ = ownedCatalog_.get();
    ownedWorld_ = std::make_unique<economy::World>(*catalog_, w, d);
    world_ = ownedWorld_.get();
}
bool EconomyApi::Allowed(bool control) const noexcept {
    return permissions_.query && (!control || permissions_.control);
}
Capabilities EconomyApi::DiscoverCapabilities() const {
    Capabilities c;
    if (!permissions_.query) {
        return c;
    }
    c.actions = {Action::QueryWorld, Action::QueryCatalog, Action::ExplainBuilding,
        Action::PollCommand, Action::ReadEvents, Action::ExportCheckpoint,
        Action::QueryUtilityDiagnostics};
    if (permissions_.control && permissions_.query) {
        c.actions.insert(c.actions.end(), {Action::Step, Action::PlaceBuilding, Action::RemoveBuilding,
            Action::BuildRoad, Action::SetRoadEnabled, Action::SetBuildingEnabled,
            Action::CreateHaulOrder, Action::DispatchTruck, Action::CancelHaulOrder, Action::RetargetHaulOrder,
            Action::CompactCommandHistory, Action::ConnectUtility, Action::DisconnectUtility,
            Action::TradeExport, Action::TradeImport, Action::PlaceConstructionSite});
    }
    return c;
}
CheckpointExport EconomyApi::ExportCheckpoint(std::uint64_t expectedRevision) const {
    CheckpointExport result;
    if (!permissions_.query) {
        result.error = ErrorCode::PermissionDenied;
        return result;
    }
    if (expectedRevision != world_->Revision()) {
        result.error = ErrorCode::StaleRevision;
        return result;
    }
    economy::ParseError error;
    if (!economy::SaveCheckpoint(*world_, result.text, error)) {
        result.error = ErrorCode::PersistenceFailed;
        result.message = error.message;
        return result;
    }
    result.revision = world_->Revision();
    result.tick = world_->Tick();
    return result;
}
HistoryCompaction EconomyApi::CompactCommandHistory(std::uint64_t expectedRevision,
    std::uint32_t keepRecent) {
    HistoryCompaction result;
    if (!permissions_.query || !permissions_.control) {
        result.error = ErrorCode::PermissionDenied;
        return result;
    }
    if (expectedRevision != world_->Revision()) {
        result.error = ErrorCode::StaleRevision;
        return result;
    }
    const auto compacted = world_->CompactCommandHistory(keepRecent);
    switch (compacted.error) {
    case economy::CommandHistoryError::None:
        break;
    case economy::CommandHistoryError::InvalidRetention:
        result.error = ErrorCode::InvalidArgument;
        return result;
    case economy::CommandHistoryError::RevisionExhausted:
        result.error = ErrorCode::RevisionExhausted;
        return result;
    }
    result.removedResults = compacted.removedResults;
    result.history = compacted.history;
    result.revision = world_->Revision();

    // A world may also be compacted directly while this API is attached. Drop all handles
    // below its authoritative retention floor, even when this API call itself was a no-op.
    for (auto it = commands_.begin(); it != commands_.end();) {
        if (it->second < result.history.firstRetainedCommandId) {
            terminalCommands_.erase(it->first);
            it = commands_.erase(it);
            ++result.expiredHandles;
        } else {
            ++it;
        }
    }
    return result;
}
CatalogView EconomyApi::DescribeCatalog() const {
    CatalogView v;
    if (!permissions_.query) {
        v.error = ErrorCode::PermissionDenied;
        return v;
    }
    for (const auto& r : catalog_->Resources()) {
        v.resources.push_back({r.id, r.name, r.utility, r.tradeDeclared, r.buyPrice, r.sellPrice});
    }
    for (const auto& b : catalog_->BuildingTypes()) {
        BuildingTypeInfo x{b.id, b.name, b.width, b.depth, b.tradePost, {}, {}, {}, b.constructionWorkTicks};
        for (const auto& s : b.storage) {
            const auto* r = catalog_->Resource(s.resource);
            x.storage.push_back({r ? r->name : "", s.capacity});
        }
        for (const auto& recipe : b.recipes) {
            RecipeInfo ri;
            ri.name = recipe.name;
            ri.intervalTicks = recipe.intervalTicks;
            for (const auto& a : recipe.inputs) {
                const auto* r = catalog_->Resource(a.resource);
                ri.inputs.push_back({r ? r->name : "", a.amount});
            }
            for (const auto& a : recipe.outputs) {
                const auto* r = catalog_->Resource(a.resource);
                ri.outputs.push_back({r ? r->name : "", a.amount});
            }
            x.recipes.push_back(std::move(ri));
        }
        x.depot = b.depot;
        x.loadTicks = b.loadTicks;
        x.unloadTicks = b.unloadTicks;
        for (const auto &fleet : b.fleet) { const auto *vehicle = catalog_->VehicleType(fleet.vehicleType); x.fleet.push_back({vehicle ? vehicle->name : "", fleet.count}); }
        for (const auto& a : b.constructionMaterials) { const auto* r=catalog_->Resource(a.resource); x.constructionMaterials.push_back({r?r->name:"",a.amount}); }
        v.buildingTypes.push_back(std::move(x));
    }
    for (const auto& t : catalog_->VehicleTypes()) {
        v.vehicleTypes.push_back({t.id, t.name, t.capacity, t.cellsPerTick});
    }
    return v;
}
WorldQuery EconomyApi::QueryWorld() const {
    if (!permissions_.query) {
        return {ErrorCode::PermissionDenied, {}};
    }
    return {ErrorCode::None, MakeWorld(*catalog_, world_->Snapshot())};
}
EconomySummary EconomyApi::Summary() const {
    if (!permissions_.query)
        return {};
    return MakeEconomySummary(*catalog_, world_->Snapshot());
}
PageQuery EconomyApi::QueryWorld(std::uint64_t revision, const std::string& kind,
    std::size_t offset, std::size_t limit) const {
    if (!permissions_.query) {
        return {ErrorCode::PermissionDenied, {}};
    }
    if (limit == 0 || limit > MaxEntityPageSize) {
        return {ErrorCode::InvalidPageLimit, {}};
    }
    if (world_->Revision() != revision)
        return {ErrorCode::StaleRevision, {}};
    economy::SnapshotCollection collection;
    if (kind == "buildings") collection = economy::SnapshotCollection::Buildings;
    else if (kind == "roads") collection = economy::SnapshotCollection::Roads;
    else if (kind == "vehicles") collection = economy::SnapshotCollection::Vehicles;
    else if (kind == "orders") collection = economy::SnapshotCollection::Orders;
    else if (kind == "utility_connections")
        collection = economy::SnapshotCollection::UtilityConnections;
    else
        return {ErrorCode::InvalidArgument, {}};
    const auto snapshot = world_->SnapshotPage(collection, offset, limit);
    if (snapshot.revision != revision)
        return {ErrorCode::StaleRevision, {}};
    EntityPage p;
    p.revision = snapshot.revision;
    p.offset = offset;
    p.kind = kind;
    p.total = snapshot.total;
    if (offset > p.total)
        return {ErrorCode::InvalidArgument, {}};
    if (kind == "buildings") {
        p.buildings.reserve(snapshot.buildings.size());
        for (const auto& value : snapshot.buildings)
            p.buildings.push_back(MakeBuilding(*catalog_, value));
    } else if (kind == "roads") {
        p.roads.reserve(snapshot.roads.size());
        for (const auto& road : snapshot.roads)
            p.roads.push_back({road.id, road.a, road.b, road.enabled});
    } else if (kind == "vehicles") {
        p.vehicles.reserve(snapshot.vehicles.size());
        for (const auto& vehicle : snapshot.vehicles) {
            const auto* type = catalog_->VehicleType(vehicle.type);
            p.vehicles.push_back({vehicle.id, vehicle.order, type ? type->name : "", vehicle.cell,
                                  vehicle.cargo, vehicle.state, vehicle.ownerDepot, vehicle.leg, vehicle.legTicksRemaining});
        }
    } else if (kind == "orders") {
        p.orders.reserve(snapshot.orders.size());
        for (const auto& order : snapshot.orders) {
            const auto* resource = catalog_->Resource(order.resource);
            const auto* vehicleType = catalog_->VehicleType(order.vehicleType);
            p.orders.push_back({order.id, order.source, order.destination,
                resource ? resource->name : "", vehicleType ? vehicleType->name : "", order.vehicle,
                order.state, order.failure, order.cargo, order.vehicleCell});
        }
    } else {
        p.utilityConnections.reserve(snapshot.utilityConnections.size());
        for (const auto& edge : snapshot.utilityConnections) {
            const auto* resource = catalog_->Resource(edge.resource);
            p.utilityConnections.push_back({resource ? resource->name : "", edge.a, edge.b});
        }
    }
    p.nextOffset = offset + snapshot.count;
    p.hasMore = p.nextOffset < p.total;
    return {ErrorCode::None, std::move(p)};
}
UtilityDiagnosticsPage EconomyApi::QueryUtilityDiagnostics(std::uint64_t expectedRevision,
    std::size_t offset, std::size_t limit,
    std::optional<economy::BuildingId> buildingId) const {
    UtilityDiagnosticsPage page;
    if (!permissions_.query) {
        page.error = ErrorCode::PermissionDenied;
        return page;
    }
    if (limit == 0 || limit > MaxUtilityDiagnosticsPageSize) {
        page.error = ErrorCode::InvalidPageLimit;
        return page;
    }
    const auto snapshot = world_->Snapshot();
    if (snapshot.revision != expectedRevision) {
        page.error = ErrorCode::StaleRevision;
        return page;
    }
    if (buildingId && std::none_of(snapshot.buildings.begin(), snapshot.buildings.end(),
            [buildingId](const auto& building) { return building.id == *buildingId; })) {
        page.error = ErrorCode::InvalidArgument;
        return page;
    }

    const auto diagnostics = world_->QueryUtilityDiagnostics();
    page.available = diagnostics.available;
    page.tick = diagnostics.tick;
    page.revision = snapshot.revision;
    page.offset = offset;
    std::vector<const economy::UtilityNetworkDiagnostic*> networks;
    networks.reserve(diagnostics.networks.size());
    for (const auto& network : diagnostics.networks) {
        if (!buildingId || std::find(network.members.begin(), network.members.end(), *buildingId) !=
                network.members.end()) {
            networks.push_back(&network);
        }
    }
    page.total = networks.size();
    if (offset > page.total) {
        page.error = ErrorCode::InvalidArgument;
        return page;
    }
    const auto count = std::min(limit, page.total - offset);
    page.networks.reserve(count);
    for (std::size_t index = offset; index < offset + count; ++index) {
        const auto& network = *networks[index];
        const auto* resource = catalog_->Resource(network.resource);
        UtilityNetworkDiagnosticView view;
        view.resource = resource ? resource->name : "unknown";
        view.component = network.component;
        view.memberCount = network.members.size();
        const auto sampleSize = std::min(network.members.size(), MaxUtilityDiagnosticMembers);
        view.members.assign(network.members.begin(), network.members.begin() + sampleSize);
        view.membersComplete = sampleSize == network.members.size();
        view.generated = economy::FormatUtilityAmount(network.generated);
        view.requested = economy::FormatUtilityAmount(network.requested);
        view.consumed = economy::FormatUtilityAmount(network.consumed);
        view.unused = economy::FormatUtilityAmount(network.unused);
        view.starvedRecipes = network.starvedRecipes;
        page.networks.push_back(std::move(view));
    }
    page.hasMore = offset + count < page.total;
    return page;
}
Explanation EconomyApi::Explain(economy::BuildingId id) const {
    if (!permissions_.query) {
        return {ErrorCode::PermissionDenied, {}, {}};
    }
    const auto snap = world_->Snapshot();
    auto it = std::find_if(snap.buildings.begin(), snap.buildings.end(),
        [id](const auto& b) { return b.id == id; });
    if (it == snap.buildings.end()) {
        return {ErrorCode::InvalidArgument, {}, "Unknown building."};
    }
    auto stall = world_->Explain(id);
    return {ErrorCode::None, stall, StallText(*catalog_, stall)};
}
Submission EconomyApi::Submit(std::uint64_t revision, economy::CommandId id) {
    if (!permissions_.query || !permissions_.control) {
        return {ErrorCode::PermissionDenied, 0, {}};
    }
    if (world_->Revision() != revision) {
        return {ErrorCode::StaleRevision, 0, {}};
    }
    if (id == 0)
        return {ErrorCode::CommandQueueExhausted, 0, {}};
    if (nextHandle_ == 0 || nextHandle_ == UINT64_MAX)
        return {ErrorCode::CommandHandleExhausted, 0, {}};
    const auto handle = nextHandle_++;
    commands_[handle] = id;
    Record({0, EventType::CommandAccepted, world_->Tick(), handle});
    return {ErrorCode::None, handle, economy::CommandState::Queued};
}
Submission EconomyApi::PlaceBuilding(std::uint64_t revision, const std::string& type, economy::Cell c) {
    if (!Allowed(true)) {
        return {ErrorCode::PermissionDenied, 0, {}};
    }
    if (world_->Revision() != revision) {
        return {ErrorCode::StaleRevision, 0, {}};
    }
    economy::BuildingTypeId id{};
    if (!catalog_->FindBuildingType(type, id)) {
        return {ErrorCode::InvalidArgument, 0, {}};
    }
    if (nextHandle_ == 0 || nextHandle_ == UINT64_MAX)
        return {ErrorCode::CommandHandleExhausted, 0, {}};
    return Submit(revision, world_->PlaceBuilding(id, c));
}
Submission EconomyApi::PlaceConstructionSite(std::uint64_t revision, const std::string& type, economy::Cell c) {
    if (!Allowed(true)) return {ErrorCode::PermissionDenied,0,{}};
    if (world_->Revision()!=revision) return {ErrorCode::StaleRevision,0,{}};
    economy::BuildingTypeId id{};
    if (!catalog_->FindBuildingType(type,id) || !catalog_->BuildingType(id)->constructionWorkTicks) return {ErrorCode::InvalidArgument,0,{}};
    if (nextHandle_==0 || nextHandle_==UINT64_MAX) return {ErrorCode::CommandHandleExhausted,0,{}};
    return Submit(revision,world_->PlaceConstructionSite(id,c));
}
Submission EconomyApi::RemoveBuilding(std::uint64_t revision, economy::BuildingId id) {
    if (!Allowed(true)) {
        return {ErrorCode::PermissionDenied, 0, {}};
    }
    if (world_->Revision() != revision) {
        return {ErrorCode::StaleRevision, 0, {}};
    }
    if (nextHandle_ == 0 || nextHandle_ == UINT64_MAX)
        return {ErrorCode::CommandHandleExhausted, 0, {}};
    return Submit(revision, world_->RemoveBuilding(id));
}
Submission EconomyApi::BuildRoad(std::uint64_t revision, economy::Cell a, economy::Cell b) {
    if (!Allowed(true)) {
        return {ErrorCode::PermissionDenied, 0, {}};
    }
    if (world_->Revision() != revision) {
        return {ErrorCode::StaleRevision, 0, {}};
    }
    if (nextHandle_ == 0 || nextHandle_ == UINT64_MAX)
        return {ErrorCode::CommandHandleExhausted, 0, {}};
    return Submit(revision, world_->BuildRoad(a, b));
}
Submission EconomyApi::SetRoadEnabled(std::uint64_t revision, economy::RoadId id, bool enabled) {
    if (!Allowed(true)) {
        return {ErrorCode::PermissionDenied, 0, {}};
    }
    if (world_->Revision() != revision) {
        return {ErrorCode::StaleRevision, 0, {}};
    }
    if (nextHandle_ == 0 || nextHandle_ == UINT64_MAX)
        return {ErrorCode::CommandHandleExhausted, 0, {}};
    return Submit(revision, world_->SetRoadEnabled(id, enabled));
}
Submission EconomyApi::SetBuildingEnabled(std::uint64_t revision, economy::BuildingId id, bool enabled) {
    if (!Allowed(true)) {
        return {ErrorCode::PermissionDenied, 0, {}};
    }
    if (world_->Revision() != revision) {
        return {ErrorCode::StaleRevision, 0, {}};
    }
    if (nextHandle_ == 0 || nextHandle_ == UINT64_MAX)
        return {ErrorCode::CommandHandleExhausted, 0, {}};
    return Submit(revision, world_->SetBuildingEnabled(id, enabled));
}
Submission EconomyApi::CreateHaulOrder(std::uint64_t revision, economy::BuildingId s, economy::BuildingId d,
    const std::string& resource, const std::string& vehicle) {
    if (!Allowed(true)) {
        return {ErrorCode::PermissionDenied, 0, {}};
    }
    if (world_->Revision() != revision) {
        return {ErrorCode::StaleRevision, 0, {}};
    }
    economy::ResourceId r{};
    economy::VehicleTypeId v{};
    if (!catalog_->FindResource(resource, r) || !catalog_->FindVehicleType(vehicle, v))
        return {ErrorCode::InvalidArgument, 0, {}};
    if (nextHandle_ == 0 || nextHandle_ == UINT64_MAX)
        return {ErrorCode::CommandHandleExhausted, 0, {}};
    return Submit(revision, world_->CreateHaulOrder(s, d, r, v));
}
Submission EconomyApi::DispatchTruck(std::uint64_t revision, economy::VehicleId vehicle, economy::BuildingId source,
    economy::BuildingId destination, const std::string& resource, std::uint64_t amount) {
    if (!Allowed(true)) return {ErrorCode::PermissionDenied, 0, {}};
    if (world_->Revision() != revision) return {ErrorCode::StaleRevision, 0, {}};
    economy::ResourceId id{};
    if (!catalog_->FindResource(resource, id)) return {ErrorCode::InvalidArgument, 0, {}};
    if (nextHandle_ == 0 || nextHandle_ == UINT64_MAX) return {ErrorCode::CommandHandleExhausted, 0, {}};
    return Submit(revision, world_->DispatchTruck(vehicle, source, destination, id, amount));
}
Submission EconomyApi::CancelHaulOrder(std::uint64_t revision, economy::OrderId id) {
    if (!Allowed(true)) {
        return {ErrorCode::PermissionDenied, 0, {}};
    }
    if (world_->Revision() != revision) {
        return {ErrorCode::StaleRevision, 0, {}};
    }
    if (nextHandle_ == 0 || nextHandle_ == UINT64_MAX)
        return {ErrorCode::CommandHandleExhausted, 0, {}};
    return Submit(revision, world_->CancelHaulOrder(id));
}
Submission EconomyApi::RetargetHaulOrder(std::uint64_t revision, economy::OrderId id,
    economy::BuildingId destination) {
    if (!Allowed(true)) {
        return {ErrorCode::PermissionDenied, 0, {}};
    }
    if (world_->Revision() != revision) {
        return {ErrorCode::StaleRevision, 0, {}};
    }
    if (nextHandle_ == 0 || nextHandle_ == UINT64_MAX)
        return {ErrorCode::CommandHandleExhausted, 0, {}};
    return Submit(revision, world_->RetargetHaulOrder(id, destination));
}
Submission EconomyApi::ConnectUtility(std::uint64_t revision, const std::string& resource,
    economy::BuildingId a, economy::BuildingId b) {
    if (!Allowed(true)) {
        return {ErrorCode::PermissionDenied, 0, {}};
    }
    if (world_->Revision() != revision) {
        return {ErrorCode::StaleRevision, 0, {}};
    }
    economy::ResourceId id{};
    const auto* definition = catalog_->FindResource(resource, id) ? catalog_->Resource(id) : nullptr;
    if (!definition || !definition->utility) {
        return {ErrorCode::InvalidArgument, 0, {}};
    }
    if (nextHandle_ == 0 || nextHandle_ == UINT64_MAX)
        return {ErrorCode::CommandHandleExhausted, 0, {}};
    return Submit(revision, world_->ConnectUtility(id, a, b));
}
Submission EconomyApi::DisconnectUtility(std::uint64_t revision, const std::string& resource,
    economy::BuildingId a, economy::BuildingId b) {
    if (!Allowed(true)) {
        return {ErrorCode::PermissionDenied, 0, {}};
    }
    if (world_->Revision() != revision) {
        return {ErrorCode::StaleRevision, 0, {}};
    }
    economy::ResourceId id{};
    const auto* definition = catalog_->FindResource(resource, id) ? catalog_->Resource(id) : nullptr;
    if (!definition || !definition->utility) {
        return {ErrorCode::InvalidArgument, 0, {}};
    }
    if (nextHandle_ == 0 || nextHandle_ == UINT64_MAX)
        return {ErrorCode::CommandHandleExhausted, 0, {}};
    return Submit(revision, world_->DisconnectUtility(id, a, b));
}
Submission EconomyApi::TradeExport(std::uint64_t revision, economy::BuildingId building,
    const std::string& resource, std::uint64_t amount, std::uint64_t rateCap) {
    if (!Allowed(true)) return {ErrorCode::PermissionDenied, 0, {}};
    if (world_->Revision() != revision) return {ErrorCode::StaleRevision, 0, {}};
    economy::ResourceId id{};
    const auto* definition = catalog_->FindResource(resource, id) ? catalog_->Resource(id) : nullptr;
    if (!definition || !definition->tradeDeclared || !amount || !rateCap)
        return {ErrorCode::InvalidArgument, 0, {}};
    if (nextHandle_ == 0 || nextHandle_ == UINT64_MAX)
        return {ErrorCode::CommandHandleExhausted, 0, {}};
    return Submit(revision, world_->TradeExport(building, id, amount, rateCap));
}
Submission EconomyApi::TradeImport(std::uint64_t revision, economy::BuildingId building,
    const std::string& resource, std::uint64_t amount, std::uint64_t rateCap) {
    if (!Allowed(true)) return {ErrorCode::PermissionDenied, 0, {}};
    if (world_->Revision() != revision) return {ErrorCode::StaleRevision, 0, {}};
    economy::ResourceId id{};
    const auto* definition = catalog_->FindResource(resource, id) ? catalog_->Resource(id) : nullptr;
    if (!definition || !definition->tradeDeclared || !amount || !rateCap)
        return {ErrorCode::InvalidArgument, 0, {}};
    if (nextHandle_ == 0 || nextHandle_ == UINT64_MAX)
        return {ErrorCode::CommandHandleExhausted, 0, {}};
    return Submit(revision, world_->TradeImport(building, id, amount, rateCap));
}
PollResult EconomyApi::PollCommand(std::uint64_t h) const {
    if (!permissions_.query) {
        return {ErrorCode::PermissionDenied, {}};
    }
    auto it = commands_.find(h);
    if (it == commands_.end()) {
        if (h != 0 && h < nextHandle_) {
            economy::CommandResult expired;
            expired.state = economy::CommandState::Expired;
            expired.failure = economy::CommandFailure::None;
            expired.message = "command handle expired";
            return {ErrorCode::CommandExpired, std::move(expired)};
        }
        return {ErrorCode::UnknownCommand, {}};
    }
    auto result = world_->GetCommandResult(it->second);
    if (result.state == economy::CommandState::Expired) {
        return {ErrorCode::CommandExpired, std::move(result)};
    }
    return {ErrorCode::None, std::move(result)};
}
void EconomyApi::Record(Event e) {
    if (sequence_ == std::numeric_limits<std::uint64_t>::max()) {
        return;
    }
    e.sequence = ++sequence_;
    events_.push_back(e);
    if (events_.size() > MaxEventJournalEntries) {
        events_.pop_front();
    }
}
void EconomyApi::CaptureChanges(const economy::TickEventSnapshot& before) {
    const auto after = world_->CaptureTickEvents();
    for (const auto& b : after.buildings) {
        auto i = std::lower_bound(before.buildings.begin(), before.buildings.end(), b.id,
            [](const auto& x, economy::BuildingId id) { return x.id < id; });
        const bool existed = i != before.buildings.end() && i->id == b.id;
        const bool newlyStalled = !existed &&
            b.stall.reason != economy::StallReason::Running;
        const bool changedStall = existed &&
            (i->stall.reason != b.stall.reason || i->stall.resource != b.stall.resource ||
                i->stall.recipeIndex != b.stall.recipeIndex);
        if (newlyStalled || changedStall) {
            const auto type = b.stall.reason == economy::StallReason::Running
                ? EventType::BuildingUnstalled : EventType::BuildingStalled;
            Record({0, type, after.tick, 0, b.id, economy::CommandFailure::None,
                b.stall.reason});
        }
    }
    for (const auto& o : after.orders) {
        auto i = std::lower_bound(before.orders.begin(), before.orders.end(), o.id,
            [](const auto& x, economy::OrderId id) { return x.id < id; });
        if (i != before.orders.end() && i->id == o.id && i->cargo > 0 && o.cargo == 0) {
            Record({0, EventType::DeliveryCompleted, after.tick});
        }
    }
}
ErrorCode EconomyApi::ReplaceCatalog(const economy::Catalog& catalog, std::string& error) {
    if (!Allowed(true)) {
        return ErrorCode::PermissionDenied;
    }
    if (!world_->ReplaceCatalog(catalog, error)) {
        return ErrorCode::InvalidArgument;
    }
    catalog_ = &catalog;
    Record({0, EventType::RulesReloaded, world_->Tick()});
    return ErrorCode::None;
}
ErrorCode EconomyApi::Step(std::uint32_t ticks) {
    if (!Allowed(true)) {
        return ErrorCode::PermissionDenied;
    }
    if (ticks > MaxStepTicks) {
        return ErrorCode::StepLimitExceeded;
    }
    for (std::uint32_t i = 0; i < ticks; ++i) {
        auto before = world_->CaptureTickEvents();
        const auto result = world_->Step(1);
        if (!result.accepted) {
            return ErrorCode::InvalidArgument;
        }
        CaptureChanges(before);
        for (const auto& pair : commands_) {
            const auto r = world_->GetCommandResult(pair.second);
            if (r.state == economy::CommandState::Rejected &&
                terminalCommands_.insert(pair.first).second) {
                Record({0, EventType::CommandRejected, world_->Tick(), pair.first, r.building, r.failure});
            }
        }
        Record({0, EventType::TickAdvanced, world_->Tick()});
    }
    return ErrorCode::None;
}
EventQuery EconomyApi::ReadEvents(std::uint64_t cursor, std::size_t limit) const {
    if (!permissions_.query) {
        return {ErrorCode::PermissionDenied, {}};
    }
    if (!limit || limit > MaxEventPageSize) {
        return {ErrorCode::InvalidEventLimit, {}};
    }
    EventPage p;
    p.newestAvailableSequence = sequence_;
    p.oldestAvailableSequence = events_.empty() ? sequence_ : events_.front().sequence;
    if (cursor > sequence_) {
        return {ErrorCode::FutureEventCursor, {}};
    }
    p.cursorFellBehind = !events_.empty() &&
        cursor < p.oldestAvailableSequence - 1;
    const auto start = p.cursorFellBehind ? p.oldestAvailableSequence - 1 : cursor;
    for (const auto& e : events_) {
        if (e.sequence > start && p.events.size() < limit) {
            p.events.push_back(e);
        }
    }
    p.nextCursor = p.events.empty() ? start : p.events.back().sequence;
    p.hasMore = p.nextCursor < sequence_;
    return {ErrorCode::None, std::move(p)};
}
} // namespace openrepublic::economy_api
