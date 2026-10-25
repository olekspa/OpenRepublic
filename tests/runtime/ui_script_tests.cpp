#include "openrepublic/runtime/ui_script.h"
#include <iostream>

int main() {
    openrepublic::runtime::UiScript script; std::string error;
    if(!openrepublic::runtime::ParseUiScript(R"({"actions":[{"frame":0,"action":"button","label":"Tools"},{"frame":12,"action":"screenshot","path":"a.png"}]})",script,error) || script.actions.size()!=2 || script.actions[1].frame!=12) return 1;
    const char* invalid[]={R"({"actions":[{"frame":-1,"action":"screenshot","path":"x"}]})",R"({"actions":[{"frame":1,"action":"wat"}]})",R"({"actions":[{"frame":2,"action":"button","label":"Tools"},{"frame":1,"action":"button","label":"Run"}]})",R"({"actions":[{"frame":0,"action":"button","label":"Tools","wat":1}]})"};
    for(const auto* text:invalid) if(openrepublic::runtime::ParseUiScript(text,script,error)) return 2;
    std::cout<<"UI script parser checks passed\n"; return 0;
}
