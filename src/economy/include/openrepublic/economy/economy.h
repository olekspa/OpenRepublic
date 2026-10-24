#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::economy {

using ResourceId = std::uint32_t;
using BuildingTypeId = std::uint32_t;
using VehicleTypeId = std::uint32_t;
using BuildingId = std::uint32_t;
using RoadId = std::uint32_t;
using RailId = std::uint32_t;
using OrderId = std::uint32_t;
using VehicleId = std::uint32_t;
using CommandId = std::uint64_t;

constexpr ResourceId InvalidResourceId = UINT32_MAX;
constexpr BuildingTypeId InvalidBuildingTypeId = UINT32_MAX;
constexpr VehicleTypeId InvalidVehicleTypeId = UINT32_MAX;
constexpr std::size_t MaxWorldSaveBytes = 1024 * 1024;
constexpr std::uint32_t MaxFleetVehiclesPerDepotType = 4096;
constexpr std::uint32_t MaxCommandHistoryRetention = 4096;
constexpr std::uint32_t MaxWorldGridSize = INT32_MAX;

struct Cell {
    std::int32_t x{};
    std::int32_t z{};
};
/** Compares two grid cells by both coordinates. */
bool operator==(Cell left, Cell right) noexcept;
/** Returns true when either grid coordinate differs. */
bool operator!=(Cell left, Cell right) noexcept;

struct RecipeAmount {
    ResourceId resource{InvalidResourceId};
    std::uint64_t amount{};
};
struct StorageDef {
    ResourceId resource{InvalidResourceId};
    std::uint64_t capacity{};
};
struct RecipeDef {
    std::string name;
    std::uint32_t intervalTicks{1};
    std::vector<RecipeAmount> inputs;
    std::vector<RecipeAmount> outputs;
    // Optional OpenRepublic synthetic output percentages in season order spring, summer, autumn, winter.
    std::vector<std::uint32_t> seasonalOutputPercent;
    std::vector<std::uint8_t> seasonalOutputDeclared;
};
struct CalendarDef { std::uint32_t ticksPerDay{}, startYear{}, startDay{}; };
struct FoodDef {
    ResourceId resource{InvalidResourceId};
    std::uint64_t amount{};
    std::uint32_t intervalTicks{1};
};
struct ResourceDef {
    ResourceId id{};
    std::string name;
    bool utility{};
    bool tradeDeclared{};
    std::uint64_t buyPrice{}, sellPrice{};
};
struct TradePriceDef { ResourceId resource{InvalidResourceId}; std::uint64_t buyPrice{}, sellPrice{}; };
struct FleetDef { VehicleTypeId vehicleType{InvalidVehicleTypeId}; std::uint32_t count{}; };
struct BuildingTypeDef {
    BuildingTypeId id{};
    std::string name;
    std::uint32_t width{1};
    std::uint32_t depth{1};
    std::vector<StorageDef> storage;
    std::vector<RecipeDef> recipes;
    bool tradePost{};
    bool depot{};
    std::vector<FleetDef> fleet;
    std::uint32_t loadTicks{1}, unloadTicks{1};
    bool station{};
    std::uint64_t housing{};
    std::uint64_t workers{};
    std::vector<RecipeAmount> constructionMaterials;
    std::uint32_t constructionWorkTicks{};
};
struct VehicleTypeDef {
    VehicleTypeId id{};
    std::string name;
    std::uint64_t capacity{};
    std::uint32_t cellsPerTick{1};
    bool railOnly{};
};

enum class CatalogError : std::uint8_t {
    None = 0,
    EmptyName,
    NameTooLong,
    DuplicateName,
    InvalidDimension,
    InvalidCapacity,
    InvalidAmount,
    InvalidInterval,
    UnknownResource,
    UnknownBuildingType,
    DuplicateStorage,
    DuplicateRecipe,
    DuplicateVehicleType,
    MissingStorage,
    EmptyCatalog,
    TooManyDefinitions,
    InvalidRecipeName,
    UtilityHasStorage,
    InvalidUtilityRecipe,
    InvalidTradePrice
};
struct CatalogErrorInfo {
    CatalogError code{CatalogError::None};
    std::string message;
};

class Catalog final {
public:
    /** Constructs an empty catalog; use CatalogBuilder to create validated definitions. */
    Catalog();
    /** Returns resources in dense immutable ID order. */
    const std::vector<ResourceDef> &Resources() const noexcept;
    /** Returns building types in dense immutable ID order. */
    const std::vector<BuildingTypeDef> &BuildingTypes() const noexcept;
    /** Returns vehicle types in dense immutable ID order. */
    const std::vector<VehicleTypeDef> &VehicleTypes() const noexcept;
    const FoodDef &Food() const noexcept;
    const CalendarDef &Calendar() const noexcept;
    /** Looks up an exact resource name without modifying the immutable catalog. */
    bool FindResource(std::string_view name, ResourceId &id) const noexcept;
    /** Looks up an exact building type name without modifying the immutable catalog. */
    bool FindBuildingType(std::string_view name, BuildingTypeId &id) const noexcept;
    /** Looks up an exact vehicle type name without modifying the immutable catalog. */
    bool FindVehicleType(std::string_view name, VehicleTypeId &id) const noexcept;
    /** Returns a resource definition or nullptr for an invalid ID. */
    const ResourceDef *Resource(ResourceId id) const noexcept;
    /** Returns a building definition or nullptr for an invalid ID. */
    const BuildingTypeDef *BuildingType(BuildingTypeId id) const noexcept;
    /** Returns a vehicle definition or nullptr for an invalid ID. */
    const VehicleTypeDef *VehicleType(VehicleTypeId id) const noexcept;

private:
    friend class CatalogBuilder;
    std::vector<ResourceDef> resources_;
    std::vector<BuildingTypeDef> buildingTypes_;
    std::vector<VehicleTypeDef> vehicleTypes_;
    FoodDef food_;
    CalendarDef calendar_;
};

class CatalogBuilder final {
public:
    bool FindVehicleType(std::string_view name, VehicleTypeId &id) const noexcept;
    /** Adds a unique resource and returns its dense ID. */
    bool AddResource(std::string_view name, ResourceId *outId, CatalogErrorInfo &error);
    /** Adds a resource that is supplied through explicit utility connections and cannot be stored or hauled. */
    bool AddUtilityResource(std::string_view name, ResourceId *outId, CatalogErrorInfo &error);
    /** Adds a positive-footprint building type and returns its dense ID. */
    bool AddBuildingType(std::string_view name, std::uint32_t width, std::uint32_t depth,
                         BuildingTypeId *outId, CatalogErrorInfo &error);
    /** Adds one positive-capacity storage slot and rejects duplicates. */
    bool AddStorage(BuildingTypeId building, ResourceId resource, std::uint64_t capacity,
                    CatalogErrorInfo &error);
    /** Adds a recipe after validating interval, amounts, and storage slots. */
    bool AddRecipe(BuildingTypeId building, const RecipeDef &recipe, CatalogErrorInfo &error);
    /** Adds a positive-capacity, positive-speed vehicle type and returns its dense ID. */
    bool AddVehicleType(std::string_view name, std::uint64_t capacity, std::uint32_t cellsPerTick,
                        VehicleTypeId *outId, CatalogErrorInfo &error);
    bool SetRailOnly(VehicleTypeId vehicle, bool enabled, CatalogErrorInfo &error);
    bool AddTradePrice(const TradePriceDef &price, CatalogErrorInfo &error);
    bool SetTradePost(BuildingTypeId building, bool enabled, CatalogErrorInfo &error);
    bool SetDepot(BuildingTypeId building, bool enabled, CatalogErrorInfo &error);
    bool AddFleet(BuildingTypeId building, VehicleTypeId vehicleType, std::uint32_t count, CatalogErrorInfo &error);
    bool SetHandlingTicks(BuildingTypeId building, std::uint32_t loadTicks, std::uint32_t unloadTicks, CatalogErrorInfo &error);
    bool SetStation(BuildingTypeId building, bool enabled, CatalogErrorInfo &error);
    bool SetHousing(BuildingTypeId building, std::uint64_t capacity, CatalogErrorInfo &error);
    bool SetWorkers(BuildingTypeId building, std::uint64_t requirement, CatalogErrorInfo &error);
    bool SetConstruction(BuildingTypeId building, const std::vector<RecipeAmount>& materials,
                         std::uint32_t workTicks, CatalogErrorInfo &error);
    bool SetFood(ResourceId resource, std::uint64_t amount, std::uint32_t intervalTicks,
                 CatalogErrorInfo &error);
    bool SetCalendar(const CalendarDef &calendar, CatalogErrorInfo &error);
    bool SetSeasonalOutput(BuildingTypeId building, std::string_view recipe,
                           std::string_view season, std::uint32_t percent, CatalogErrorInfo &error);
    /** Resolves a resource name and adds its storage slot. */
    bool AddStorage(BuildingTypeId building, std::string_view resource, std::uint64_t capacity,
                    CatalogErrorInfo &error);
    /** Verifies recipe IDs against a catalog and adds the recipe. */
    bool AddRecipe(BuildingTypeId building, const RecipeDef &recipe, const Catalog &names,
                   CatalogErrorInfo &error);
    /** Copies the validated draft into the output catalog. */
    bool Build(Catalog &outCatalog, CatalogErrorInfo &error) const;

private:
    Catalog draft_;
};

enum class ParseErrorCode : std::uint8_t {
    None = 0,
    EmptyInput,
    InvalidHeader,
    UnexpectedToken,
    InvalidNumber,
    NumberOutOfRange,
    DuplicateDefinition,
    UnknownName,
    MissingEnd,
    LimitExceeded,
    CatalogInvalid
};
struct ParseError {
    ParseErrorCode code{ParseErrorCode::None};
    std::uint32_t line{};
    std::string message;
};

/** Parses the strict version-1 text format transactionally. */
bool ParseCatalogText(std::string_view text, Catalog &outCatalog, ParseError &error);
/** Writes canonical text accepted by ParseCatalogText. */
std::string WriteCatalogText(const Catalog &catalog);
/** Builds a synthetic farm, mill, bakery, and market catalog owned by this project. */
Catalog MakeSyntheticCatalog();
/** Like MakeSyntheticCatalog plus planks/bricks, a sawmill and a brickworks, and `build` requirements on the
 *  mill, bakery and market (OpenRepublic synthetic construction-site rules; see docs/CONSTRUCTION_SITES.md). */
Catalog MakeSyntheticConstructionCatalog();
/** OpenRepublic-only border coal, depot fleet, storage, and heating fixture. */
Catalog MakeSyntheticCoalCatalog();

enum class StallReason : std::uint8_t {
    Running = 0,
    MissingInput,
    OutputFull,
    Disabled,
    MissingHeating,
    MissingWater
};
struct StallRecord {
    BuildingId building{};
    StallReason reason{StallReason::Running};
    ResourceId resource{InvalidResourceId};
    std::uint32_t recipeIndex{};
};

enum class CommandFailure : std::uint8_t {
    None = 0,
    InvalidCommand,
    UnknownBuildingType,
    UnknownBuilding,
    UnknownResource,
    UnknownVehicleType,
    UnknownRoad,
    UnknownOrder,
    OutOfBounds,
    InvalidRoadGeometry,
    DuplicateRoad,
    RoadOnBuilding,
    BuildingCollision,
    AccessCellUnavailable,
    ReferencedByOrder,
    DuplicateOrder,
    InvalidOrderEndpoint,
    NoRoute,
    SourceEmpty,
    DestinationFull,
    RoadDisabled,
    StepLimitExceeded,
    SaveInvalid,
    ParseInvalid,
    CargoInTransit,
    DuplicateUtilityConnection,
    MissingUtilityConnection,
    InvalidUtilityConnection,
    ReferencedByUtilityConnection,
    NotTradePost,
    TradeUnavailable,
    InsufficientTradeStock,
    InsufficientTreasury,
    TradeRateExceeded,
    UnknownRail,
    InvalidRailGeometry,
    DuplicateRail,
    RailOnBuilding
};
enum class CommandState : std::uint8_t {
    Unknown = 0,
    Queued,
    Accepted,
    Rejected,
    Expired
};
struct CommandResult {
    CommandId commandId{};
    CommandState state{CommandState::Unknown};
    CommandFailure failure{CommandFailure::None};
    std::string message;
    BuildingId building{};
    RoadId road{};
    OrderId order{};
    RailId rail{};
};
struct CommandHistorySnapshot {
    CommandId firstRetainedCommandId{1};
    CommandId nextCommandId{1};
    std::uint64_t retainedTerminalResults{};
    std::uint64_t pendingCommands{};
};
enum class CommandHistoryError : std::uint8_t {
    None = 0,
    InvalidRetention,
    RevisionExhausted
};
struct CommandHistoryCompactionResult {
    CommandHistoryError error{CommandHistoryError::None};
    std::uint64_t removedResults{};
    CommandHistorySnapshot history;
};
struct StepResult {
    bool accepted{};
    CommandFailure failure{CommandFailure::None};
    std::string message;
};
struct InventorySnapshot {
    ResourceId resource{};
    std::uint64_t amount{};
    std::uint64_t capacity{};
};
struct RecipeProgressSnapshot {
    std::uint32_t remainingTicks{};
    std::uint64_t completedCycles{};
};
struct ConstructionMaterialSnapshot {
    ResourceId resource{};
    std::uint64_t required{}, held{};
};
struct BuildingSnapshot {
    BuildingId id{};
    BuildingTypeId type{};
    Cell origin{};
    bool enabled{true};
    Cell access{};
    std::vector<InventorySnapshot> inventory;
    std::vector<RecipeProgressSnapshot> recipes;
    StallRecord stall;
    bool constructionSite{};
    std::vector<ConstructionMaterialSnapshot> constructionMaterials;
    std::uint8_t constructionState{}; // 0 waiting materials, 1 building, 2 done
    double constructionProgress{};
};
struct RoadSnapshot {
    RoadId id{};
    Cell a{};
    Cell b{};
    bool enabled{true};
};
struct RailSnapshot { RailId id{}; Cell a{}, b{}; };
enum class HaulMode : std::uint8_t { Road = 0, Rail = 1 };
enum class HaulState : std::uint8_t {
    Idle = 0,
    ToDestination,
    ToSource,
    Waiting,
    NoRoute
};
enum class VehicleLeg : std::uint8_t { None = 0, ToSource, Loading, ToDestination, Unloading, Returning };
enum class HaulFailure : std::uint8_t {
    None = 0,
    NoRoute,
    SourceEmpty,
    DestinationFull,
    RoadDisabled
};
struct VehicleSnapshot {
    VehicleId id{};
    OrderId order{};
    VehicleTypeId type{};
    Cell cell{};
    std::uint64_t cargo{};
    HaulState state{HaulState::Idle};
    BuildingId ownerDepot{};
    VehicleLeg leg{VehicleLeg::None};
    std::uint32_t legTicksRemaining{};
};
struct HaulOrderSnapshot {
    OrderId id{};
    BuildingId source{};
    BuildingId destination{};
    ResourceId resource{};
    VehicleTypeId vehicleType{};
    VehicleId vehicle{};
    HaulState state{HaulState::Idle};
    HaulFailure failure{HaulFailure::None};
    std::uint64_t cargo{};
    Cell vehicleCell{};
    HaulMode mode{HaulMode::Road};
};
struct UtilityConnectionSnapshot {
    ResourceId resource{};
    BuildingId a{};
    BuildingId b{};
};
/** Exact unsigned amount used by utility diagnostics; decimal formatting is portable to MSVC. */
struct UtilityAmount {
    std::uint64_t low{};
    std::uint64_t high{};
};
std::string FormatUtilityAmount(UtilityAmount amount);
/** Last-tick measurement for one connected utility component. These observations are not world state. */
struct UtilityNetworkDiagnostic {
    ResourceId resource{};
    BuildingId component{};
    std::vector<BuildingId> members;
    UtilityAmount generated;
    UtilityAmount requested;
    UtilityAmount consumed;
    UtilityAmount unused;
    std::uint64_t starvedRecipes{};
};
/** A copy of the most recent completed utility production phase, if one exists. */
struct UtilityDiagnostics {
    bool available{};
    std::uint64_t tick{};
    std::vector<UtilityNetworkDiagnostic> networks;
};
struct WorldSnapshot {
    std::uint32_t gridWidth{};
    std::uint32_t gridDepth{};
    std::uint64_t tick{};
    std::uint64_t revision{};
    std::uint64_t stateHash{};
    CommandHistorySnapshot commandHistory;
    std::vector<BuildingSnapshot> buildings;
    std::vector<RoadSnapshot> roads;
    std::vector<RailSnapshot> rails;
    std::vector<VehicleSnapshot> vehicles;
    std::vector<HaulOrderSnapshot> orders;
    std::vector<UtilityConnectionSnapshot> utilityConnections;
    std::uint64_t treasury{};
    std::uint64_t population{};
    std::uint64_t workersAssigned{};
    std::uint64_t workersRequired{};
    std::uint32_t calendarYear{}, calendarDay{}, calendarMonth{};
    std::string calendarSeason;
};

enum class SnapshotCollection : std::uint8_t { Buildings, Roads, Vehicles, Orders, UtilityConnections, Rails };
struct WorldSnapshotPage {
    std::uint64_t revision{};
    std::size_t total{}, offset{}, count{};
    std::vector<BuildingSnapshot> buildings;
    std::vector<RoadSnapshot> roads;
    std::vector<RailSnapshot> rails;
    std::vector<VehicleSnapshot> vehicles;
    std::vector<HaulOrderSnapshot> orders;
    std::vector<UtilityConnectionSnapshot> utilityConnections;
};

// Tick-boundary transitions required by the economy API event journal.
struct TickEventSnapshot {
    struct Building { BuildingId id{}; StallRecord stall; };
    struct Order { OrderId id{}; std::uint64_t cargo{}; };
    std::uint64_t tick{};
    std::vector<Building> buildings;
    std::vector<Order> orders;
};

/** Worker threads for the per-building production phase (0 = hardware concurrency, 1 = serial).
 *  Results are bit-identical for every setting: buildings are independent in that phase. */
void SetProductionThreads(unsigned threads) noexcept;

class World final {
public:
    /** Constructs an empty grid with dimensions 1..MaxWorldGridSize; otherwise throws invalid_argument.
     *  The catalog must outlive the world. */
    World(const Catalog &catalog, std::uint32_t gridWidth, std::uint32_t gridDepth);
    World(const World &) = delete;
    World &operator=(const World &) = delete;
    /** Transfers ownership of an existing world without copying catalog state. */
    World(World &&other) noexcept;
    /** Replaces this world by moving ownership from other. */
    World &operator=(World &&other) noexcept;
    /** Releases world-owned state without changing the catalog. */
    ~World();
    // Command queue methods return 0 without mutation when command/tick/revision space is exhausted.
    /** Queues placement; access is the cell below the footprint minimum-z edge center.
     *  Footprints cannot overlap buildings, reserved access cells, or enabled/disabled roads.
     *  Access must be in bounds and outside footprints; a road there is allowed, not required. */
    CommandId PlaceBuilding(BuildingTypeId type, Cell origin);
    CommandId PlaceConstructionSite(BuildingTypeId type, Cell origin);
    /** Queues removal and rejects buildings referenced by haul orders. */
    CommandId RemoveBuilding(BuildingId building);
    /** Queues one in-bounds axis-aligned unit-step road segment. */
    CommandId BuildRoad(Cell a, Cell b);
    /** Queues one in-bounds axis-aligned unit-step rail segment. */
    CommandId BuildRail(Cell a, Cell b);
    /** Queues road state; vehicles re-route or wait if a segment closes. */
    CommandId SetRoadEnabled(RoadId road, bool enabled);
    /** Queues a production enabled-state change. */
    CommandId SetBuildingEnabled(BuildingId building, bool enabled);
    /** Queues a repeating haul order with one vehicle spawned at source access. */
    CommandId CreateHaulOrder(BuildingId source, BuildingId destination, ResourceId resource,
                              VehicleTypeId vehicleType);
    CommandId DispatchTruck(VehicleId vehicle, BuildingId source, BuildingId destination, ResourceId resource,
                            std::uint64_t amount = 0);
    CommandId CreateHaulOrder(BuildingId source, BuildingId destination, ResourceId resource,
                              VehicleTypeId vehicleType, HaulMode mode);
    /** Queues cancellation of an empty order and its vehicle; loaded cargo prevents cancellation. */
    CommandId CancelHaulOrder(OrderId order);
    /** Queues a new destination while preserving the order, vehicle, source, resource, and cargo.
     *  A loaded vehicle routes from its current cell; an empty vehicle returns to source first. */
    CommandId RetargetHaulOrder(OrderId order, BuildingId destination);
    /** Queues a canonical undirected utility link between two live buildings. */
    CommandId ConnectUtility(ResourceId resource, BuildingId a, BuildingId b);
    /** Queues removal of an existing undirected utility link. */
    CommandId DisconnectUtility(ResourceId resource, BuildingId a, BuildingId b);
    CommandId TradeExport(BuildingId building, ResourceId resource, std::uint64_t amount, std::uint64_t rateCap);
    CommandId TradeImport(BuildingId building, ResourceId resource, std::uint64_t amount, std::uint64_t rateCap);
    /** Swaps in a tuned catalog of identical structure (names, ids, recipe and storage layout) between ticks.
     *  Rejects the swap, leaving the world untouched, if the structure differs or building inventory
     *  or onboard cargo would exceed a reduced capacity. Population opt-ins and food-resource
     *  bindings must match; occupied housing cannot shrink below its resident count.
     *  The new catalog must outlive the world. */
    bool ReplaceCatalog(const Catalog &catalog, std::string &error);
    /** Returns queued, accepted, rejected, or unknown command status. */
    CommandResult GetCommandResult(CommandId command) const;
    /** Returns command-history retention and counter state without mutation. */
    CommandHistorySnapshot CommandHistory() const noexcept;
    /** Explicitly drops all but the newest keepRecent terminal results, retaining every pending command. */
    CommandHistoryCompactionResult CompactCommandHistory(std::uint32_t keepRecent);
    /** Applies queued commands, production, and haul movement for each bounded tick. */
    StepResult Step(std::uint32_t ticks = 1);
    /** Returns ID-ordered values and an FNV-1a hash of canonical little-endian state fields. */
    WorldSnapshot Snapshot() const;
    /** Copies one ID-ordered entity slice without hashing or materializing other collections. */
    WorldSnapshotPage SnapshotPage(SnapshotCollection collection, std::size_t offset,
                                   std::size_t limit) const;
    TickEventSnapshot CaptureTickEvents() const;
    /** Returns the current disabled, missing-input, output-full, or running explanation. */
    StallRecord Explain(BuildingId building) const;
    /** Returns last-tick utility observations. Demand includes only enabled recipes whose stock inputs and output
     * room were available when visited. With atomic multi-utility recipes, each network can show requested demand
     * even when another required network is short. starvedRecipes counts eligible recipes that lacked this
     * network's utility; one atomic recipe may be counted in more than one network. The copy is unavailable before
     * a utility tick and after load or catalog replacement; receipts are excluded from saves and state hashes. */
    UtilityDiagnostics QueryUtilityDiagnostics() const;
    /** Returns the fixed simulation tick count. */
    std::uint64_t Tick() const noexcept;
    std::uint64_t Treasury() const noexcept;
    /** Returns the monotonically increasing state revision. */
    std::uint64_t Revision() const noexcept;
    /** Returns the catalog referenced by this world. */
    const Catalog &GetCatalog() const noexcept;

private:
    friend /** Serializes state, queues, and command results in versioned text. */
        std::string
        SaveWorld(const World &world);
    friend /** Validates and loads transactionally; failure leaves world unchanged. */
        bool
        LoadWorld(std::string_view text, const Catalog &catalog, World &world, ParseError &error);
    struct Impl;
    Impl *impl_{};
};

/** Serializes state, queues, and command results; returns empty if MaxWorldSaveBytes is exceeded. */
std::string SaveWorld(const World &world);
/** Validates and loads transactionally; failure leaves world unchanged. */
bool LoadWorld(std::string_view text, const Catalog &catalog, World &world, ParseError &error);

} // namespace openrepublic::economy
