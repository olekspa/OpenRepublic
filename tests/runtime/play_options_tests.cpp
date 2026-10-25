#include "openrepublic/runtime/play_options.h"
#include "openrepublic/economy/economy.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

using openrepublic::runtime::ResolvePlayOptions;

namespace
{
void Require(bool value, const char* message)
{
    if (!value)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void Preset(const wchar_t* name, std::vector<std::wstring> expected)
{
    const auto actual = ResolvePlayOptions({L"--play", name});
    Require(actual.error.empty() && actual.selected && actual.arguments == expected,
        "preset composition incorrect");
    for (const auto* option : {L"--native-economy", L"--native-terrain", L"--ambient-traffic"})
    {
        Require(std::count(actual.arguments.begin(), actual.arguments.end(), option) == 1,
            "duplicate or missing common option");
    }
}
}

int main()
{
    const std::vector<std::wstring> old = {L"--native-economy", L"--other", L"value"};
    const auto unchanged = ResolvePlayOptions(old);
    Require(unchanged.error.empty() && !unchanged.selected && unchanged.arguments == old,
        "legacy args changed");
    Preset(L"coal", {L"--native-economy", L"--native-terrain", L"--ambient-traffic",
        L"--native-coal-demo", L"--native-objectives", L"builtin:coal_demo"});
    Preset(L"sites", {L"--native-economy", L"--native-terrain", L"--ambient-traffic",
        L"--native-construction-sites", L"--native-objectives", L"builtin:construction_sites_demo"});
    Preset(L"town", {L"--native-economy", L"--native-terrain", L"--ambient-traffic",
        L"--native-town-demo", L"--native-objectives", L"builtin:growth_town"});
    Require(!ResolvePlayOptions({L"--play", L"village"}).error.empty(), "unknown preset accepted");
    Require(!ResolvePlayOptions({L"--play", L"coal", L"--native-terrain"}).error.empty(),
        "conflicting option accepted");
    const auto extra = ResolvePlayOptions({L"--title", L"my game", L"--play", L"town"});
    Require(extra.arguments.size() == 8 && extra.arguments[6] == L"--title" &&
        extra.arguments[7] == L"my game", "passthrough args lost");

    const auto town = openrepublic::economy::MakeSyntheticTownCatalog();
    openrepublic::economy::BuildingTypeId home{};
    Require(town.CitizensEnabled() && town.FindBuildingType("home", home),
        "town catalog must declare citizen rules and housing");
    for (const auto* name : {"farm", "mill", "bakery", "market", "boiler"})
    {
        openrepublic::economy::BuildingTypeId id{};
        Require(town.FindBuildingType(name, id),
            "town catalog is missing a production or heating building");
    }
    return 0;
}
