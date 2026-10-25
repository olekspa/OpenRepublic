#include "openrepublic/runtime/panel_layout.h"
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
using namespace openrepublic::runtime;
int main() {
    int failures = 0;
    for (const auto size : {std::pair<float,float>{1280.0f,720.0f}, {1920.0f,1080.0f}, {2560.0f,1440.0f}}) {
        std::vector<PanelRequest> requests{
            {"toolbar", {0,0,size.first,54}, PanelDock::Top, 100, true, false},
            {"overview", {0,70,size.first*.43f,size.second*.62f}, PanelDock::Left, 80, true, true},
            {"game", {0,70,size.first*.43f,size.second*.62f}, PanelDock::Right, 90, true, true},
            {"events", {0,70,size.first*.35f,180}, PanelDock::Right, 40, true, true},
            {"tools", {0,60,size.first*.7f,240}, PanelDock::Bottom, 20, true, true},
        };
        const auto layout = LayoutPanels(size.first, size.second, 1, requests);
        for (const auto& panel : layout.panels) if (panel.visible) {
            if (panel.rect.x < 0 || panel.rect.y < 0 || panel.rect.x + panel.rect.width > size.first + .01f || panel.rect.y + panel.rect.height > size.second + .01f) { std::cerr << "bounds " << size.first << " " << panel.id << '\n'; ++failures; }
            for (const auto& other : layout.panels) if (other.visible && panel.id < other.id &&
                panel.rect.x < other.rect.x + other.rect.width && other.rect.x < panel.rect.x + panel.rect.width &&
                panel.rect.y < other.rect.y + other.rect.height && other.rect.y < panel.rect.y + panel.rect.height) { std::cerr << "overlap " << size.first << " " << panel.id << " " << other.id << '\n'; ++failures; }
        }
        if (!layout.panels[0].visible || !layout.panels[1].visible || !layout.panels[2].visible) { std::cerr << "required hidden " << size.first << '\n'; ++failures; }
    }
    const auto dpi = LayoutPanels(2560, 1440, 2, {{"x", {0,0,900,500}, PanelDock::Center, 1, true, false}});
    if (!dpi.panels[0].visible || std::abs(dpi.panels[0].rect.width - 450) > .01f) { std::cerr << "dpi " << dpi.panels[0].visible << " " << dpi.panels[0].rect.width << '\n'; ++failures; }
    const auto hidden = LayoutPanels(0, 720, 1, {{"x", {0,0,20,20}, PanelDock::Center, 1, true, false}});
    if (!hidden.panels.empty()) ++failures;
    if (failures) std::cerr << "panel layout failures: " << failures << '\n';
    return failures ? 1 : 0;
}
