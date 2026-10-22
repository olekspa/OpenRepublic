#include "openrepublic/scene_control.h"
#include "json.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace openrepublic::scenectl;
using Json=nlohmann::json;
namespace {
void Check(bool value,const char *message) { if(!value) throw std::runtime_error(message); }
Json Send(SceneControlState &s,const std::string &method,const Json &params=Json::object(),const char *id="t") {
  return Json::parse(s.Apply(Json{{"id",id},{"protocolVersion",0},{"method",method},{"params",params}}.dump()));
}
void Ok(const Json &j) { Check(j.value("ok",false),"expected successful response"); }
void Error(const Json &j,const char *code) { Check(!j.value("ok",true),"expected error response"); Check(j["error"]["code"]==code,"unexpected error code"); }
}
int main() {
  try {
    SceneControlState s;
    Check(s.Revision()==0&&!s.ConsumeDirty(),"initial state");
    auto get=Send(s,"look.get"); Ok(get); Check(get["result"]["sunElevationDegrees"]==35.0,"initial look");
    auto bad=Send(s,"look.set",{{"exposure",0.29}}); Error(bad,"out_of_range"); Check(s.Revision()==0,"bad update changed revision");
    bad=Send(s,"look.set",{{"exposure",2.0},{"camera",{{"yawDegrees",181}}}}); Error(bad,"out_of_range");
    Check(s.CurrentLook().exposure==1.0&&s.Revision()==0,"failed partial update was not atomic");
    bad=Send(s,"look.set",{{"mystery",1}}); Error(bad,"unknown_field");
    bad=Send(s,"camera.set",{{"yawDegrees",181}}); Error(bad,"out_of_range");
    bad=Send(s,"look.set",{{"oceanEnabled",1}}); Error(bad,"invalid_request");
    auto set=Send(s,"look.set",{{"exposure",1.4},{"cloudCoverage",0.4}}); Ok(set);
    Check(s.Revision()==1&&s.ConsumeDirty()&&!s.ConsumeDirty(),"dirty/revision after set");
    set=Send(s,"look.set",{{"exposure",1.4}}); Ok(set); Check(s.Revision()==1&&!s.ConsumeDirty(),"no-op changed revision");
    set=Send(s,"camera.set",{{"yawDegrees",-180},{"elevationDegrees",75},{"distanceFactor",2.5}}); Ok(set); Check(s.Revision()==2,"camera revision");
    for(const char *name:{"golden_hour","noon","dusk","overcast"}) { auto p=Send(s,"look.preset",{{"name",name}}); Ok(p); }
    Error(Send(s,"look.preset",{{"name","night"}}),"invalid_request");
    const auto dawn=LookAtTimeOfDay(6), morning=LookAtTimeOfDay(9), noon=LookAtTimeOfDay(12), afternoon=LookAtTimeOfDay(15), dusk=LookAtTimeOfDay(18);
    Check(dawn.sunElevationDegrees<morning.sunElevationDegrees&&morning.sunElevationDegrees<noon.sunElevationDegrees,"dawn curve is not monotonic");
    Check(noon.sunElevationDegrees>afternoon.sunElevationDegrees&&afternoon.sunElevationDegrees>dusk.sunElevationDegrees,"dusk curve is not monotonic");
    Check(LookAtTimeOfDay(0).sunElevationDegrees==2&&LookAtTimeOfDay(24).sunElevationDegrees==2,"night endpoints");
    Json duplicate=Json::parse(s.Apply(R"({"id":"x","protocolVersion":0,"method":"look.set","params":{"exposure":1,"exposure":2}})")); Error(duplicate,"duplicate_json_key");
    Json tooBig=Json::parse(s.Apply(std::string(MaxRequestLineBytes+1,' '))); Error(tooBig,"request_too_large");
    auto caps=Send(s,"capabilities"); Ok(caps); Check(caps["result"]["apiVersion"]==0,"capabilities version");
    std::cout<<"Scene control tests passed\n";
  } catch(const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
  return 0;
}
