#include "openrepublic/economy/economy.h"
#include "openrepublic/economy/calendar.h"
#include "openrepublic/economy_api/economy_summary.h"
#include <cstdlib>
#include <iostream>

namespace { void Check(bool value, const char* message) { if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } } }
int main() {
    using namespace openrepublic;
    constexpr const char* text =
        "openrepublic-economy 1\nresource grain\ncalendar ticks_per_day 1 start_year 1980 start_day 90\n"
        "building farm 1 1\n  store grain 100\n  recipe harvest every 1 out grain:10\n"
        "  season spring 50\n  season summer 150\n  season autumn 100\n  season winter 0\nend\n";
    economy::Catalog catalog; economy::ParseError error;
    Check(economy::ParseCatalogText(text, catalog, error), "parse calendar catalog");
    const auto canonical = economy::WriteCatalogText(catalog);
    economy::Catalog again;
    Check(economy::ParseCatalogText(canonical, again, error) && economy::WriteCatalogText(again) == canonical,
          "season and calendar declarations round-trip canonically");
    economy::BuildingTypeId roundTripFarm{};
    Check(again.FindBuildingType("farm", roundTripFarm) && again.BuildingType(roundTripFarm)->recipes[0].seasonalOutputPercent == std::vector<std::uint32_t>({50,150,100,0}),
          "canonical catalog preserves every seasonal modifier");
    economy::BuildingTypeId farm{}; Check(catalog.FindBuildingType("farm", farm), "find farm");
    economy::World world(catalog, 8, 8);
    world.PlaceBuilding(farm, {2,2}); Check(world.Step().accepted, "place farm and harvest in spring");
    auto snapshot = world.Snapshot();
    Check(snapshot.calendarYear == 1980 && snapshot.calendarDay == 91 && snapshot.calendarMonth == 4 && snapshot.calendarSeason == "summer", "snapshot exposes date after first completed day tick");
    Check(snapshot.buildings[0].inventory[0].amount == 5, "spring output is 50 percent with floor rounding");
    Check(world.Step().accepted, "harvest in summer");
    snapshot = world.Snapshot();
    Check(snapshot.calendarDay == 92 && snapshot.calendarSeason == "summer" && snapshot.buildings[0].inventory[0].amount == 20,
          "date advances and summer output is 150 percent");
    const auto summary = economy_api::MakeEconomySummary(catalog, snapshot);
    Check(summary.calendarEnabled && summary.calendarDay == 92 && summary.calendarSeason == "summer", "typed summary exposes calendar");
    const auto save = economy::SaveWorld(world);
    economy::World restored(catalog, 8, 8);
    Check(economy::LoadWorld(save, catalog, restored, error), "load calendar world save");
    Check(world.Step().accepted && restored.Step().accepted && world.Snapshot().stateHash == restored.Snapshot().stateHash,
          "tick persistence reproduces calendar and production after restore");

    const std::string legacy = "openrepublic-economy 1\nresource grain\nbuilding farm 1 1\n  store grain 100\n  recipe harvest every 1 out grain:10\nend\n";
    economy::Catalog legacyCatalog; Check(economy::ParseCatalogText(legacy, legacyCatalog, error), "parse legacy catalog");
    Check(economy::WriteCatalogText(legacyCatalog) == legacy && !legacyCatalog.Calendar().ticksPerDay,
          "catalogs without calendar retain canonical bytes and disabled calendar");
    const auto date = economy::CalendarAt({1, 1980, 360}, 1);
    Check(date.year == 1981 && date.day == 1 && date.month == 1 && date.season == 0, "year rollover is deterministic");
    const auto bounded = economy::CalendarAt({1, UINT32_MAX, 1}, UINT64_MAX);
    Check(bounded.year == UINT32_MAX, "year overflow saturates instead of wrapping");
    std::cout << "Economy calendar contracts passed\n";
}
