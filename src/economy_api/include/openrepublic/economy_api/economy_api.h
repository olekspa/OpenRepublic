#pragma once

#include "openrepublic/economy/economy.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace openrepublic::economy_api {

constexpr std::uint32_t ApiVersion = 0;
constexpr std::size_t MaxEntityPageSize = 32;
constexpr std::size_t MaxEventJournalEntries = 128;
constexpr std::size_t MaxEventPageSize = 32;
constexpr std::uint32_t MaxStepTicks = 100000;

enum class Action : std::uint8_t {
    QueryWorld = 1, QueryCatalog, ExplainBuilding, Step, PlaceBuilding, RemoveBuilding,
    BuildRoad, SetRoadEnabled, SetBuildingEnabled, CreateHaulOrder, CancelHaulOrder,
    PollCommand, ReadEvents
};
struct Permissions {
    bool query{true};
    bool control{false};
};
struct Capabilities {
    std::uint32_t apiVersion{ApiVersion};
    std::vector<Action> actions;
};
enum class ErrorCode : std::uint8_t {
    None = 0, PermissionDenied, StaleRevision, InvalidArgument, UnknownCommand,
    InvalidPageLimit, InvalidEventLimit, FutureEventCursor, StepLimitExceeded,
    CommandHandleExhausted
};
struct ResourceInfo {
    economy::ResourceId id{};
    std::string name;
};
struct StorageInfo {
    std::string resource;
    std::uint64_t capacity{};
};
struct RecipeAmountInfo {
    std::string resource;
    std::uint64_t amount{};
};
struct RecipeInfo {
    std::string name;
    std::uint32_t intervalTicks{};
    std::vector<RecipeAmountInfo> inputs;
    std::vector<RecipeAmountInfo> outputs;
};
struct BuildingTypeInfo {
    economy::BuildingTypeId id{};
    std::string name;
    std::uint32_t width{}, depth{};
    std::vector<StorageInfo> storage;
    std::vector<RecipeInfo> recipes;
};
struct VehicleTypeInfo {
    economy::VehicleTypeId id{};
    std::string name;
    std::uint64_t capacity{};
    std::uint32_t cellsPerTick{};
};
struct CatalogView {
    ErrorCode error{ErrorCode::None};
    std::uint32_t apiVersion{ApiVersion};
    std::vector<ResourceInfo> resources;
    std::vector<BuildingTypeInfo> buildingTypes;
    std::vector<VehicleTypeInfo> vehicleTypes;
};
struct InventoryItem {
    std::string resource;
    std::uint64_t amount{}, capacity{};
};
struct BuildingView {
    economy::BuildingId id{};
    std::string type;
    economy::Cell cell{};
    bool enabled{};
    std::vector<InventoryItem> inventory;
    economy::StallReason stallReason{economy::StallReason::Running};
    std::string stallExplanation;
};
struct RoadView {
    economy::RoadId id{};
    economy::Cell a{}, b{};
    bool enabled{};
};
struct VehicleView {
    economy::VehicleId id{};
    economy::OrderId order{};
    std::string type;
    economy::Cell cell{};
    std::uint64_t cargo{};
    economy::HaulState state{economy::HaulState::Idle};
};
struct OrderView {
    economy::OrderId id{};
    economy::BuildingId source{}, destination{};
    std::string resource, vehicleType;
    economy::VehicleId vehicle{};
    economy::HaulState state{economy::HaulState::Idle};
    economy::HaulFailure failure{economy::HaulFailure::None};
    std::uint64_t cargo{};
    economy::Cell vehicleCell{};
};
struct WorldView {
    std::uint32_t apiVersion{ApiVersion};
    std::uint64_t revision{}, tick{}, stateHash{};
    std::vector<BuildingView> buildings;
    std::vector<RoadView> roads;
    std::vector<VehicleView> vehicles;
    std::vector<OrderView> orders;
};
struct WorldQuery {
    ErrorCode error{ErrorCode::None};
    WorldView world;
};
struct EntityPage {
    std::uint64_t revision{};
    std::size_t offset{}, nextOffset{}, total{};
    bool hasMore{};
    // A page slices one collection selected by kind: buildings, roads, vehicles, orders.
    std::string kind;
    std::vector<BuildingView> buildings;
    std::vector<RoadView> roads;
    std::vector<VehicleView> vehicles;
    std::vector<OrderView> orders;
};
struct PageQuery {
    ErrorCode error{ErrorCode::None};
    EntityPage page;
};
struct Explanation {
    ErrorCode error{ErrorCode::None};
    economy::StallRecord stall;
    std::string text;
};
// CommandAccepted reports queue admission; execution may later emit CommandRejected.
enum class EventType : std::uint8_t {
    CommandAccepted = 1, CommandRejected, BuildingStalled, BuildingUnstalled,
    DeliveryCompleted, TickAdvanced
};
struct Event {
    std::uint64_t sequence{};
    EventType type{EventType::TickAdvanced};
    std::uint64_t tick{}, commandHandle{};
    economy::BuildingId building{};
    economy::CommandFailure failure{economy::CommandFailure::None};
    economy::StallReason stallReason{economy::StallReason::Running};
};
struct EventPage {
    bool cursorFellBehind{}, hasMore{};
    std::uint64_t oldestAvailableSequence{}, newestAvailableSequence{}, nextCursor{};
    std::vector<Event> events;
};
struct EventQuery {
    ErrorCode error{ErrorCode::None};
    EventPage page;
};
struct Submission {
    ErrorCode error{ErrorCode::None};
    std::uint64_t handle{};
    economy::CommandState state{economy::CommandState::Unknown};
};
struct PollResult {
    ErrorCode error{ErrorCode::None};
    economy::CommandResult result;
};

class EconomyApi final {
public:
    /** Attaches to a world and its catalog; both must outlive this API. */
    explicit EconomyApi(economy::World& world, Permissions permissions = {});
    /** Owns a copied catalog and a fresh world with the requested grid dimensions. */
    EconomyApi(const economy::Catalog& catalog, std::uint32_t width, std::uint32_t depth,
        Permissions permissions = {});
    /** Lists the actions enabled by the current permission profile. */
    Capabilities DiscoverCapabilities() const;
    /** Returns named catalog definitions; query permission is required. */
    CatalogView DescribeCatalog() const;
    /** Returns all world collections and the matching tick, revision, and state hash. */
    WorldQuery QueryWorld() const;
    /** Returns one bounded collection page, rejecting stale revisions and unknown kinds. */
    PageQuery QueryWorld(std::uint64_t expectedRevision, const std::string& kind,
        std::size_t offset, std::size_t limit) const;
    /** Returns the current stall record and a human-readable explanation for a building. */
    Explanation Explain(economy::BuildingId id) const;
    /** Queues placement by exact catalog type name if revision and permissions allow it. */
    Submission PlaceBuilding(std::uint64_t revision, const std::string& type, economy::Cell cell);
    /** Queues removal of a stable building ID after checking the expected revision. */
    Submission RemoveBuilding(std::uint64_t revision, economy::BuildingId id);
    /** Queues an axis-aligned road segment after checking the expected revision. */
    Submission BuildRoad(std::uint64_t revision, economy::Cell a, economy::Cell b);
    /** Queues road enablement after checking the expected revision. */
    Submission SetRoadEnabled(std::uint64_t revision, economy::RoadId id, bool enabled);
    /** Queues building enablement after checking the expected revision. */
    Submission SetBuildingEnabled(std::uint64_t revision, economy::BuildingId id, bool enabled);
    /** Queues a haul order using exact resource and vehicle type names. */
    Submission CreateHaulOrder(std::uint64_t revision, economy::BuildingId source,
        economy::BuildingId destination, const std::string& resource, const std::string& vehicleType);
    /** Queues cancellation of a haul order after checking the expected revision. */
    Submission CancelHaulOrder(std::uint64_t revision, economy::OrderId id);
    /** Returns the queued or terminal simulation result associated with a command handle. */
    PollResult PollCommand(std::uint64_t handle) const;
    /** Advances up to MaxStepTicks; command outcomes are evaluated at tick boundaries. */
    ErrorCode Step(std::uint32_t ticks = 1);
    /** Reads up to MaxEventPageSize entries after an exclusive cursor. */
    EventQuery ReadEvents(std::uint64_t afterSequence, std::size_t limit) const;
private:
    Submission Submit(std::uint64_t revision, economy::CommandId command);
    bool Allowed(bool control) const noexcept;
    void Record(Event event);
    void CaptureChanges(const economy::WorldSnapshot& before);
    std::unique_ptr<economy::Catalog> ownedCatalog_;
    std::unique_ptr<economy::World> ownedWorld_;
    const economy::Catalog* catalog_{};
    economy::World* world_{};
    Permissions permissions_;
    std::map<std::uint64_t, economy::CommandId> commands_;
    std::set<std::uint64_t> terminalCommands_;
    std::deque<Event> events_;
    std::uint64_t nextHandle_{1}, sequence_{};
};
} // namespace openrepublic::economy_api
