#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace openrepublic::runtime {

struct PanelRect { float x{}, y{}, width{}, height{}; };
enum class PanelDock { Top, Left, Right, Bottom, Center };
struct PanelRequest {
    std::string id;
    PanelRect preferred;
    PanelDock dock{PanelDock::Center};
    int priority{};
    bool visible{true};
    bool scrollable{};
};
struct PanelPlacement {
    std::string id;
    PanelRect rect;
    bool visible{};
    bool scrollable{};
};
struct PanelLayout { std::vector<PanelPlacement> panels; };

// Places visible panels by descending priority, clamps them to the viewport, and
// hides lower-priority panels when no collision-free placement is available.
PanelLayout LayoutPanels(float width, float height, float dpiScale,
                         const std::vector<PanelRequest>& requests);

} // namespace openrepublic::runtime
