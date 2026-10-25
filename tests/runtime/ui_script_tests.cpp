#include "openrepublic/runtime/ui_script.h"
#include <iostream>

int main() {
    openrepublic::runtime::UiScript script; std::string error;
    if(!openrepublic::runtime::ParseUiScript(R"({"actions":[{"frame":0,"action":"button","label":"Tools"},{"frame":12,"action":"screenshot","path":"a.png"},{"frame":13,"action":"select","label":"NativeEconomyBuildingType","entry":"mill"},{"frame":14,"action":"place_building","type":"mill","x":4,"z":5},{"frame":15,"action":"build_road","x":4,"z":4,"x2":8,"z2":4},{"frame":16,"action":"click_cell","x":4,"z":5},{"frame":17,"action":"wait_ticks","ticks":25},{"frame":18,"action":"assert_text","widget":"NativeObjectivesChecklist","text":"complete"}]})",script,error) || script.actions.size()!=8 || script.actions[1].frame!=12 || script.actions[3].x!=4 || script.actions[6].ticks!=25) return 1;
    const char* invalid[]={R"({"actions":[{"frame":-1,"action":"screenshot","path":"x"}]})",R"({"actions":[{"frame":1,"action":"wat"}]})",R"({"actions":[{"frame":2,"action":"button","label":"Tools"},{"frame":1,"action":"button","label":"Run"}]})",R"({"actions":[{"frame":0,"action":"button","label":"Tools","wat":1}]})",R"({"actions":[{"frame":0,"action":"assert_text","widget":"x"}]})",R"({"actions":[{"frame":0,"action":"wait_ticks","ticks":0}]})"};
    for(const auto* text:invalid) if(openrepublic::runtime::ParseUiScript(text,script,error)) return 2;
    std::cout<<"UI script parser checks passed\n"; return 0;
}
