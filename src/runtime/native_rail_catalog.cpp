#include "openrepublic/economy/economy.h"

namespace openrepublic::economy {

Catalog MakeSyntheticRailCatalog() {
    static constexpr char catalogText[] =
        "openrepublic-economy 1\n"
        "resource grain\n"
        "building rail_station 1 1\n"
        "  station\n"
        "  store grain 100000\n"
        "  recipe synthetic_stock every 1 out grain:1\n"
        "end\n"
        "vehicle train capacity 100 speed 3 mode rail\n";
    Catalog catalog;
    ParseError error;
    if (!ParseCatalogText(catalogText, catalog, error)) return Catalog{};
    return catalog;
}

} // namespace openrepublic::economy
