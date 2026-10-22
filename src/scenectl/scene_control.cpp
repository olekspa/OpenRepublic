#include "openrepublic/scene_control.h"

#include "json.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_set>
#include <vector>

namespace openrepublic::scenectl {
namespace {
using Json = nlohmann::json;
constexpr std::size_t MaxJsonDepth = 32;
constexpr std::size_t MaxJsonNodes = 4096;

Json Error(const Json &id, const char *code, const std::string &message) {
  return Json{{"id", id}, {"protocolVersion", ProtocolVersion}, {"ok", false},
              {"error", Json{{"code", code}, {"message", message}}}};
}
Json Success(const Json &id, Json result) {
  return Json{{"id", id}, {"protocolVersion", ProtocolVersion}, {"ok", true}, {"result", std::move(result)}};
}
bool Parse(std::string_view text, Json &out, std::string &code, std::string &message) {
  if (text.size() > MaxRequestLineBytes) {
    code = "request_too_large"; message = "request line exceeds the configured byte limit"; return false;
  }
  std::size_t depth = 0; bool inString = false, escaped = false;
  for (const char c : text) {
    if (inString) { if (escaped) escaped = false; else if (c == '\\') escaped = true; else if (c == '"') inString = false; }
    else if (c == '"') inString = true;
    else if (c == '{' || c == '[') { if (++depth > MaxJsonDepth) { code = "json_too_deep"; message = "JSON nesting exceeds the configured depth limit"; return false; } }
    else if ((c == '}' || c == ']') && depth) --depth;
  }
  struct State { std::size_t nodes{}; bool duplicate{}, many{}, deep{}; std::vector<std::unordered_set<std::string>> keys; } state;
  const auto callback = [&state](int level, Json::parse_event_t event, Json &value) {
    if (++state.nodes > MaxJsonNodes) state.many = true;
    if (level > static_cast<int>(MaxJsonDepth)) state.deep = true;
    if (event == Json::parse_event_t::object_start) state.keys.emplace_back();
    else if (event == Json::parse_event_t::key && !state.keys.empty()) {
      if (!state.keys.back().insert(value.get<std::string>()).second) state.duplicate = true;
    }
    else if (event == Json::parse_event_t::object_end && !state.keys.empty()) state.keys.pop_back();
    return true;
  };
  try { out = Json::parse(text.begin(), text.end(), callback, true, false); }
  catch (...) { code = "malformed_json"; message = "request line is not valid JSON"; return false; }
  if (state.duplicate) { code = "duplicate_json_key"; message = "duplicate object keys are not allowed"; return false; }
  if (state.deep) { code = "json_too_deep"; message = "JSON nesting exceeds the configured depth limit"; return false; }
  if (state.many) { code = "json_too_complex"; message = "JSON structure exceeds the configured node limit"; return false; }
  return true;
}
bool Range(double v, double lo, double hi) { return std::isfinite(v) && v >= lo && v <= hi; }
bool SetNumber(const Json &j, const char *key, double lo, double hi, double &value, std::string &code, std::string &msg) {
  if (!j.contains(key)) return true;
  if (!j[key].is_number()) { code = "invalid_request"; msg = std::string(key) + " must be a number"; return false; }
  const double candidate = j[key].get<double>();
  if (!Range(candidate, lo, hi)) { code = "out_of_range"; msg = std::string(key) + " is outside its allowed range"; return false; }
  value = candidate; return true;
}
bool SetBool(const Json &j, const char *key, bool &value, std::string &msg) {
  if (!j.contains(key)) return true;
  if (!j[key].is_boolean()) { msg = std::string(key) + " must be a boolean"; return false; }
  value = j[key].get<bool>(); return true;
}
bool KnownFields(const Json &j, const std::set<std::string> &allowed, std::string &msg) {
  if (!j.is_object()) { msg = "params must be an object"; return false; }
  for (auto it = j.begin(); it != j.end(); ++it) if (!allowed.count(it.key())) { msg = "unknown field: " + it.key(); return false; }
  return true;
}
const char *FieldErrorCode(const std::string &message) {
  return message.rfind("unknown field:", 0) == 0 ? "unknown_field" : "invalid_request";
}
Json ToJson(const SceneLook &l, std::uint64_t revision) {
  return Json{{"revision", std::to_string(revision)}, {"sunElevationDegrees", l.sunElevationDegrees},
    {"sunYawOffsetDegrees", l.sunYawOffsetDegrees}, {"sunWarmthKelvin", l.sunWarmthKelvin}, {"exposure", l.exposure},
    {"cloudCoverage", l.cloudCoverage}, {"fogDensity", l.fogDensity}, {"oceanEnabled", l.oceanEnabled},
    {"shadowsEnabled", l.shadowsEnabled}, {"bloomEnabled", l.bloomEnabled}, {"ssaoEnabled", l.ssaoEnabled},
    {"rtaoEnabled", l.rtaoEnabled}, {"rayTracedReflectionsEnabled", l.rayTracedReflectionsEnabled},
    {"camera", {{"yawDegrees", l.camera.yawDegrees}, {"elevationDegrees", l.camera.elevationDegrees}, {"distanceFactor", l.camera.distanceFactor}}}};
}
bool Equal(const SceneLook &a, const SceneLook &b) {
  return a.sunElevationDegrees == b.sunElevationDegrees && a.sunYawOffsetDegrees == b.sunYawOffsetDegrees &&
    a.sunWarmthKelvin == b.sunWarmthKelvin && a.exposure == b.exposure && a.cloudCoverage == b.cloudCoverage &&
    a.fogDensity == b.fogDensity && a.oceanEnabled == b.oceanEnabled && a.shadowsEnabled == b.shadowsEnabled &&
    a.bloomEnabled == b.bloomEnabled && a.ssaoEnabled == b.ssaoEnabled && a.rtaoEnabled == b.rtaoEnabled &&
    a.rayTracedReflectionsEnabled == b.rayTracedReflectionsEnabled && a.camera.yawDegrees == b.camera.yawDegrees &&
    a.camera.elevationDegrees == b.camera.elevationDegrees && a.camera.distanceFactor == b.camera.distanceFactor;
}
bool Update(const Json &p, SceneLook &l, bool cameraOnly, std::string &code, std::string &msg) {
  const std::set<std::string> all{"sunElevationDegrees","sunYawOffsetDegrees","sunWarmthKelvin","exposure","cloudCoverage","fogDensity","oceanEnabled","shadowsEnabled","bloomEnabled","ssaoEnabled","rtaoEnabled","rayTracedReflectionsEnabled","camera"};
  const std::set<std::string> camera{"yawDegrees","elevationDegrees","distanceFactor"};
  if (!KnownFields(p, cameraOnly ? camera : all, msg)) { code = msg.rfind("unknown field:", 0) == 0 ? "unknown_field" : "invalid_request"; return false; }
  if (cameraOnly) {
    if (!SetNumber(p,"yawDegrees",-180,180,l.camera.yawDegrees,code,msg) || !SetNumber(p,"elevationDegrees",5,75,l.camera.elevationDegrees,code,msg) || !SetNumber(p,"distanceFactor",0.5,2.5,l.camera.distanceFactor,code,msg)) return false;
    return true;
  }
  if (p.contains("camera")) {
    if (!p["camera"].is_object()) { code="invalid_request"; msg="camera must be an object"; return false; }
    if (!Update(p["camera"],l,true,code,msg)) return false;
  }
  if (!SetNumber(p,"sunElevationDegrees",2,80,l.sunElevationDegrees,code,msg) || !SetNumber(p,"sunYawOffsetDegrees",-180,180,l.sunYawOffsetDegrees,code,msg) || !SetNumber(p,"sunWarmthKelvin",2000,8000,l.sunWarmthKelvin,code,msg) || !SetNumber(p,"exposure",0.3,3,l.exposure,code,msg) || !SetNumber(p,"cloudCoverage",0,1,l.cloudCoverage,code,msg) || !SetNumber(p,"fogDensity",0,1,l.fogDensity,code,msg)) return false;
  for (auto pair : {std::pair<const char*,bool*>{"oceanEnabled",&l.oceanEnabled},{"shadowsEnabled",&l.shadowsEnabled},{"bloomEnabled",&l.bloomEnabled},{"ssaoEnabled",&l.ssaoEnabled},{"rtaoEnabled",&l.rtaoEnabled},{"rayTracedReflectionsEnabled",&l.rayTracedReflectionsEnabled}})
    if (!SetBool(p,pair.first,*pair.second,msg)) { code="invalid_request"; return false; }
  return true;
}
SceneLook Preset(const std::string &name, SceneLook l) {
  if (name=="golden_hour") { l.sunElevationDegrees=8; l.sunYawOffsetDegrees=-35; l.sunWarmthKelvin=2800; l.exposure=1.15; l.cloudCoverage=0.12; }
  else if (name=="noon") { l.sunElevationDegrees=68; l.sunYawOffsetDegrees=0; l.sunWarmthKelvin=6000; l.exposure=1; l.cloudCoverage=0; }
  else if (name=="dusk") { l.sunElevationDegrees=3; l.sunYawOffsetDegrees=120; l.sunWarmthKelvin=2600; l.exposure=1.1; l.cloudCoverage=0.18; }
  else if (name=="overcast") { l.sunElevationDegrees=38; l.sunYawOffsetDegrees=0; l.sunWarmthKelvin=6000; l.exposure=0.85; l.cloudCoverage=0.9; l.shadowsEnabled=false; }
  return l;
}
} // namespace

SceneLook LookAtTimeOfDay(double hours) {
  SceneLook l;
  if (!std::isfinite(hours) || hours < 0 || hours > 24) return l;
  const double f=std::clamp((hours-6.0)/12.0,0.0,1.0);
  const double s=(f==0.0 || f==1.0) ? 0.0 : std::sin(3.14159265358979323846*f);
  l.sunElevationDegrees=2.0+68.0*s;
  l.sunYawOffsetDegrees=-90.0+180.0*f;
  l.sunWarmthKelvin=2600.0+3400.0*s;
  return l;
}
SceneControlState::SceneControlState(SceneLook initial) : look_(initial) {}
std::string SceneControlState::Apply(std::string_view line) {
  Json req; std::string code,msg;
  if (!Parse(line,req,code,msg)) return Error(nullptr,code.c_str(),msg).dump();
  if (!req.is_object() || !req.contains("id") || !req["id"].is_string() || req["id"].get<std::string>().empty() || req["id"].get<std::string>().size()>128)
    return Error(nullptr,"invalid_request","id must be a non-empty string of at most 128 bytes").dump();
  const Json id=req["id"];
  if (!req.contains("protocolVersion") || !req["protocolVersion"].is_number_integer()) return Error(id,"invalid_request","protocolVersion must be integer 0").dump();
  const auto &version=req["protocolVersion"];
  const bool versionZero=version.is_number_unsigned() ? version.get<std::uint64_t>()==ProtocolVersion : version.get<std::int64_t>()==ProtocolVersion;
  if (!versionZero) return Error(id,"unsupported_protocol_version","unsupported protocolVersion").dump();
  if (!req.contains("method") || !req["method"].is_string()) return Error(id,"invalid_request","method must be a string").dump();
  const auto method=req["method"].get<std::string>();
  const std::set<std::string> requestFields{"id","protocolVersion","method","params"};
  for (auto it=req.begin();it!=req.end();++it) if (!requestFields.count(it.key())) return Error(id,"unknown_field","unknown request field: "+it.key()).dump();
  Json params=req.value("params",Json::object());
  std::lock_guard<std::mutex> lock(mutex_);
  if (method=="capabilities") {
    if (!KnownFields(params,{},msg)) return Error(id,FieldErrorCode(msg),msg).dump();
    return Success(id,Json{{"apiVersion",0},{"role","controller"},{"methods",{"capabilities","look.get","look.set","look.preset","camera.set"}},{"maxRequestLineBytes",MaxRequestLineBytes}}).dump();
  }
  if (method=="look.get") { if (!KnownFields(params,{},msg)) return Error(id,FieldErrorCode(msg),msg).dump(); return Success(id,ToJson(look_,revision_)).dump(); }
  SceneLook candidate=look_;
  if (method=="look.set" || method=="camera.set") {
    if (!Update(params,candidate,method=="camera.set",code,msg)) return Error(id,code.c_str(),msg).dump();
  } else if (method=="look.preset") {
    if (!KnownFields(params,{"name"},msg)) return Error(id,FieldErrorCode(msg),msg).dump();
    if (!params.contains("name") || !params["name"].is_string()) return Error(id,"invalid_request","name must be a string").dump();
    auto name=params["name"].get<std::string>();
    if (name!="golden_hour"&&name!="noon"&&name!="dusk"&&name!="overcast") return Error(id,"invalid_request","unknown preset name").dump();
    candidate=Preset(name,candidate);
  } else return Error(id,"invalid_request","unknown method").dump();
  if (!Equal(candidate,look_)) { look_=candidate; ++revision_; dirty_=true; }
  return Success(id,ToJson(look_,revision_)).dump();
}
bool SceneControlState::ConsumeDirty() { std::lock_guard<std::mutex> lock(mutex_); bool value=dirty_; dirty_=false; return value; }
SceneLook SceneControlState::CurrentLook() const { std::lock_guard<std::mutex> lock(mutex_); return look_; }
std::uint64_t SceneControlState::Revision() const { std::lock_guard<std::mutex> lock(mutex_); return revision_; }
} // namespace openrepublic::scenectl
