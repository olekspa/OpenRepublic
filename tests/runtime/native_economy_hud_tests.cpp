#include "openrepublic/runtime/native_economy_hud.h"

#include <cstdlib>
#include <iostream>

namespace { void Check(bool ok, const char* message) { if (!ok) { std::cerr << message << '\n'; std::exit(1); } } }

int main()
{
	using namespace openrepublic;
	auto catalog = economy::MakeSyntheticCatalog();
	const auto resource = catalog.Resources().front().id;
	economy::WorldSnapshot a, b;
	a.tick = 10; a.treasury = 20; a.population = 1;
	economy::BuildingSnapshot site; site.id = 4; site.constructionSite = true; site.constructionState = 1;
	a.buildings.push_back(site);
	economy::HaulOrderSnapshot order; order.id = 8; order.resource = resource; order.cargo = 5;
	a.orders.push_back(order);
	b = a; b.tick = 11; b.treasury = 18; b.population = 2;
	b.buildings.front().constructionSite = false;
	b.buildings.front().stall.reason = economy::StallReason::MissingInput;
	b.buildings.front().stall.resource = resource;
	b.orders.front().cargo = 0;
	const auto events = runtime::DiffHudEvents(a, b, catalog);
	Check(events.size() == 5, "detect construction, shortage, delivery, trade, and population transitions");
	Check(events[0].kind == runtime::HudEventKind::ConstructionCompleted, "construction completion first");
	Check(events[1].kind == runtime::HudEventKind::Shortage, "new stall reported as shortage");
	Check(events[2].kind == runtime::HudEventKind::Delivery, "cargo reaching zero reported");
	Check(events[3].kind == runtime::HudEventKind::BorderTrade, "treasury transition reported as trade");
	Check(runtime::DiffHudEvents(b, a, catalog).empty(), "reversed tick has no events");
	Check(runtime::DiffHudEvents(a, a, catalog).empty(), "same tick has no events");
	economy::Catalog citizenCatalog;
	economy::ParseError parseError;
	Check(economy::ParseCatalogText("openrepublic-economy 1\nresource food\nfood food 1 every 2\ncitizens\nbuilding home 1 1\nhousing 2\nend\n",
		citizenCatalog, parseError), "citizen HUD catalog parses");
	economy::WorldSnapshot citizenBefore, citizenAfter;
	citizenBefore.tick = 10; citizenBefore.population = 1;
	citizenAfter.tick = 11; citizenAfter.population = 2;
	const auto citizenEvents = runtime::DiffHudEvents(citizenBefore, citizenAfter, citizenCatalog);
	Check(citizenEvents.size() == 1 && citizenEvents.front().kind == runtime::HudEventKind::Immigration,
		"population growth under declared citizen rules is labeled synthetic immigration");
	const auto summary = runtime::BuildHudSummary(b, catalog);
	Check(summary.tick == 11 && summary.treasury == 18 && !summary.hasCalendar && !summary.populationAvailable, "summary copies declared fields and omits absent calendar/population");
	runtime::HudEventHistory history(2);
	Check(history.Observe(a, catalog).empty(), "first snapshot seeds event history");
	history.Observe(b, catalog);
	Check(history.Events().size() == 2, "event history is bounded");
	a.tick = 1;
	Check(history.Observe(a, catalog).empty() && history.Events().empty(), "tick rewind resets history for loaded world");
	std::cout << "Native economy HUD checks passed\n";
}
