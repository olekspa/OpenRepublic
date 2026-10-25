#include "openrepublic/runtime/ui_script.h"

#include <charconv>
#include <cctype>
#include <limits>

namespace openrepublic::runtime {
namespace {
class Parser {
public:
    explicit Parser(std::string_view input) : input_(input) {}
    bool Parse(UiScript& out, std::string& error) {
        Skip();
        if (!Take('{')) return Fail("expected object", error);
        Skip();
        if (!TakeString("actions")) return Fail("expected actions property", error);
        Skip(); if (!Take(':')) return Fail("expected ':'", error);
        Skip(); if (!Take('[')) return Fail("expected actions array", error);
        Skip();
        while (!Take(']')) {
            if (out.actions.size() >= 4096) return Fail("script exceeds 4096 actions", error);
            UiScriptAction item;
            if (!Object(item, error)) return false;
            out.actions.push_back(std::move(item));
            Skip(); if (Take(']')) break;
            if (!Take(',')) return Fail("expected ',' between actions", error);
            Skip();
        }
        Skip(); if (!Take('}') || (Skip(), position_ != input_.size())) return Fail("trailing or invalid JSON", error);
        return true;
    }
private:
    void Skip() { while (position_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[position_]))) ++position_; }
    bool Take(char c) { Skip(); if (position_ == input_.size() || input_[position_] != c) return false; ++position_; return true; }
    bool TakeString(std::string_view expected) { std::string value; return String(value) && value == expected; }
    bool String(std::string& value) {
        Skip(); if (position_ >= input_.size() || input_[position_++] != '"') return false;
        value.clear();
        while (position_ < input_.size()) {
            const char c = input_[position_++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return false;
            if (c == '\\') {
                if (position_ >= input_.size()) return false;
                const char escaped = input_[position_++];
                if (escaped != '"' && escaped != '\\' && escaped != '/') return false;
                value.push_back(escaped);
            } else value.push_back(c);
            if (value.size() > 4096) return false;
        }
        return false;
    }
    bool Object(UiScriptAction& item, std::string& error) {
        if (!Take('{')) return Fail("expected action object", error);
        bool haveFrame=false, haveAction=false, haveX=false, haveZ=false, haveX2=false, haveZ2=false, haveTicks=false;
        for (;;) {
            std::string key; if (!String(key)) return Fail("expected action property", error);
            if (!Take(':')) return Fail("expected ':' after property", error);
            if (key == "frame") {
                std::string number; Skip(); const auto start=position_;
                while(position_<input_.size() && input_[position_]>='0' && input_[position_]<='9') ++position_;
                if (start==position_) return Fail("frame must be a nonnegative integer", error);
                const auto parsed=std::from_chars(input_.data()+start,input_.data()+position_,item.frame);
                if(parsed.ec!=std::errc{}) return Fail("frame is out of range", error);
                haveFrame=true;
            } else if(key=="action") { if(!String(item.action)) return Fail("action must be a string",error); haveAction=true; }
            else if(key=="label") { if(!String(item.label)) return Fail("label must be a string",error); }
            else if(key=="path") { if(!String(item.path)) return Fail("path must be a string",error); }
            else if(key=="entry") { if(!String(item.entry)) return Fail("entry must be a string",error); }
            else if(key=="type") { if(!String(item.type)) return Fail("type must be a string",error); }
            else if(key=="widget") { if(!String(item.widget)) return Fail("widget must be a string",error); }
            else if(key=="text") { if(!String(item.text)) return Fail("text must be a string",error); }
            else if(key=="x") { if(!Signed(item.x)) return Fail("x must be an integer",error); haveX=true; }
            else if(key=="z") { if(!Signed(item.z)) return Fail("z must be an integer",error); haveZ=true; }
            else if(key=="x2") { if(!Signed(item.x2)) return Fail("x2 must be an integer",error); haveX2=true; }
            else if(key=="z2") { if(!Signed(item.z2)) return Fail("z2 must be an integer",error); haveZ2=true; }
            else if(key=="ticks") { if(!Unsigned(item.ticks)) return Fail("ticks must be a positive integer",error); haveTicks=true; }
            else return Fail("unsupported action property",error);
            Skip(); if(Take('}')) break; if(!Take(',')) return Fail("expected ',' or '}'",error);
        }
        const bool valid = item.action == "button" ? !item.label.empty() :
            item.action == "screenshot" ? !item.path.empty() :
            item.action == "select" ? !item.label.empty() && !item.entry.empty() :
            item.action == "place_building" ? !item.type.empty() && haveX && haveZ :
            item.action == "build_road" ? haveX && haveZ && haveX2 && haveZ2 :
            item.action == "click_cell" ? haveX && haveZ :
            item.action == "wait_ticks" ? haveTicks && item.ticks > 0 :
            item.action == "assert_text" ? !item.widget.empty() && !item.text.empty() : false;
        if(!haveFrame || !haveAction || !valid)
            return Fail("action requires frame and a valid action payload",error);
        return true;
    }
    bool Integer(std::int32_t& out) {
        Skip(); const auto start = position_;
        if (position_ < input_.size() && input_[position_] == '-') ++position_;
        while(position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
        if (start == position_) return false;
        const auto parsed = std::from_chars(input_.data()+start, input_.data()+position_, out);
        return parsed.ec == std::errc{};
    }
    bool Signed(std::int32_t& out) { return Integer(out); }
    bool Unsigned(std::uint64_t& out) {
        Skip(); const auto start = position_;
        while(position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
        if (start == position_) return false;
        const auto parsed = std::from_chars(input_.data()+start, input_.data()+position_, out);
        return parsed.ec == std::errc{};
    }
    bool Fail(const char* message, std::string& error) { error=message; return false; }
    std::string_view input_; std::size_t position_{};
};
}

bool ParseUiScript(std::string_view json, UiScript& result, std::string& error) {
    if (json.size() > 1024 * 1024) { error="script exceeds 1 MiB"; return false; }
    UiScript parsed;
    if (!Parser(json).Parse(parsed,error)) return false;
    for(std::size_t i=1;i<parsed.actions.size();++i)
        if(parsed.actions[i].frame < parsed.actions[i-1].frame) { error="actions must be ordered by frame"; return false; }
    result=std::move(parsed); error.clear(); return true;
}
} // namespace openrepublic::runtime
