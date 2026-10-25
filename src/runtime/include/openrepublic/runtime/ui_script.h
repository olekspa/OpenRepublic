#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openrepublic::runtime {

struct UiScriptAction {
    std::uint64_t frame{};
    std::string action;
    std::string label;
    std::string path;
    std::string entry;
    std::string type;
    std::string widget;
    std::string text;
    std::int32_t x{}, z{}, x2{}, z2{};
    std::uint64_t ticks{};
};

struct UiScript {
    std::vector<UiScriptAction> actions;
};

// Bounded parser for the app-owned UI smoke script. Unknown fields/actions fail closed.
bool ParseUiScript(std::string_view json, UiScript& result, std::string& error);

} // namespace openrepublic::runtime
