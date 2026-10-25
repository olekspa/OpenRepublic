#include "openrepublic/runtime/panel_layout.h"

#include <algorithm>
#include <cmath>

namespace openrepublic::runtime {
namespace {
bool Overlaps(const PanelRect& a, const PanelRect& b) {
    return a.x < b.x + b.width && b.x < a.x + a.width &&
           a.y < b.y + b.height && b.y < a.y + a.height;
}
}

PanelLayout LayoutPanels(float width, float height, float dpiScale,
                         const std::vector<PanelRequest>& requests) {
    PanelLayout out;
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0) return out;
    const float scale = std::isfinite(dpiScale) && dpiScale > 0 ? dpiScale : 1.0f;
    width /= scale; height /= scale;
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < requests.size(); ++i) if (requests[i].visible) order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return requests[a].priority > requests[b].priority;
    });
    out.panels.reserve(requests.size());
    for (const auto& request : requests) out.panels.push_back({request.id, {}, false, request.scrollable});
    std::vector<PanelRect> placed;
    for (const auto index : order) {
        const auto& request = requests[index];
        auto rect = request.preferred;
        if (!std::isfinite(rect.x) || !std::isfinite(rect.y) || !std::isfinite(rect.width) || !std::isfinite(rect.height) || rect.width <= 0 || rect.height <= 0) continue;
        rect.x /= scale; rect.y /= scale; rect.width /= scale; rect.height /= scale;
        rect.width = std::min(rect.width, width); rect.height = std::min(rect.height, height);
        const float margin = std::min(18.0f, std::min(width, height) * 0.025f);
        auto locate = [&](PanelDock dock, int step) {
            if (step == 0) {
                const float margin = std::min(18.0f, std::min(width, height) * 0.025f);
                if (dock == PanelDock::Left) rect.x = margin;
                else if (dock == PanelDock::Right) rect.x = width - rect.width - margin;
                else if (dock == PanelDock::Center) { rect.x = (width - rect.width) * 0.5f; rect.y = (height - rect.height) * 0.5f; }
                return;
            }
            switch (dock) {
            case PanelDock::Top: rect.x = margin; rect.y = margin + step * (rect.height + 8); break;
            case PanelDock::Left: rect.x = margin + step * (rect.width + 8); rect.y = std::max(margin, std::min(request.preferred.y, height - rect.height - margin)); break;
            case PanelDock::Right: rect.x = width - rect.width - margin - step * (rect.width + 8); rect.y = std::max(margin, std::min(request.preferred.y, height - rect.height - margin)); break;
            case PanelDock::Bottom: rect.x = margin; rect.y = height - rect.height - margin - step * (rect.height + 8); break;
            case PanelDock::Center: rect.x = (width - rect.width) * 0.5f; rect.y = (height - rect.height) * 0.5f; break;
            }
        };
        bool found = false;
        for (int step = 0; step < 4 && !found; ++step) {
            locate(request.dock, step);
            rect.x = std::clamp(rect.x, 0.0f, width - rect.width);
            rect.y = std::clamp(rect.y, 0.0f, height - rect.height);
            found = std::none_of(placed.begin(), placed.end(), [&](const auto& other) { return Overlaps(rect, other); });
        }
        if (!found) continue;
        out.panels[index].rect = rect; out.panels[index].visible = true; placed.push_back(rect);
    }
    return out;
}
} // namespace openrepublic::runtime
