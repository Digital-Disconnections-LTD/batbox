#include "query/OwnerPreflight.hpp"
#include <iostream>
#include <vector>

using namespace batbox;
int main(int argc,char** argv){
  const std::string scenario=argc>1?argv[1]:"config_false";
  event_loop::Loop loop;
  Json trace=Json::array();
  model::PreflightInputs input;
  input.options={{"model",scenario=="profile"?
    "application-inference-profile/test":"batbox-model"}};
  model::PreflightServices services;
  services.isSubscriber=[&]{trace.push_back("subscriber");return scenario=="profile";};
  services.isNonCustomPrimaryModel=[&](const std::string&){
    trace.push_back("primary");return true;};
  services.offSwitchMessage=[](const std::string& model){
    return Json{{"type","assistant"},{"error","off"},{"model",model}};};
  services.getAPIProvider=[&]{trace.push_back("provider");
    return scenario=="profile"?std::string("bedrock"):std::string("local");};
  event_loop::Deferred<bool> configGate(loop);
  event_loop::Deferred<std::optional<std::string>> profileGate(loop);
  query::OwnerPreflightAwaits awaits;
  awaits.offSwitchActivated=[&]{trace.push_back("config-await");return configGate;};
  awaits.inferenceProfileBackingModel=[&](const std::string&){
    trace.push_back("profile-await");return profileGate;};
  awaits.remainingSegment=[&](const model::PreflightInputs&,
      const model::PreflightServices&,model::PreflightState seed){
    trace.push_back("remaining");return seed;};
  auto pending=query::startOwnerPreflight(loop,input,services,awaits);
  auto heartbeat=loop.setTimeout(1,[&]{trace.push_back("heartbeat");});
  auto release=loop.setTimeout(20,[&]{
    if(scenario=="profile")profileGate.resolve(std::optional<std::string>("backing-model"));
    else configGate.resolve(scenario=="config_true");
  });
  std::optional<model::PreflightState> value;
  std::exception_ptr failure;
  pending.then([&](const model::PreflightState* state,
      const std::exception_ptr& error){
    if(state)value=*state;failure=error;loop.stop();
  });
  auto hold=loop.hold();loop.run();hold.release();
  if(failure)std::rethrow_exception(failure);
  if(!value)throw std::logic_error("Owner preflight did not settle");
  Json outcome;
  if(value->offSwitch)outcome={{"done",false},{"value",value->offSwitchMessage}};
  else outcome={{"done",false},{"value",{{"resolvedModel",value->resolvedModel},
    {"previousRequestId",value->previousRequestId?
      Json(*value->previousRequestId):Json(nullptr)}}}};
  std::cout<<Json{{"trace",trace},{"outcome",outcome}}.dump()<<'\n';
}
