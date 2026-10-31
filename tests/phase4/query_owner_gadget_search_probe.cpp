#include "query/OwnerPreflight.hpp"
#include <iostream>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  Json trace=Json::array();
  model::PreflightInputs input;
  input.options={{"model","batbox-model"},{"querySource","side_query"}};
  model::PreflightServices services;
  services.isSubscriber=[](){return true;};
  services.isNonCustomPrimaryModel=[](const std::string&){return false;};
  services.getAPIProvider=[](){return std::string("local");};
  services.getMergedBetas=[](const std::string&,bool){return Json::array();};
  services.isAdvisorEnabled=[](){return false;};
  event_loop::Deferred<bool> searchGate(loop);
  query::OwnerPreflightAwaits awaits;
  awaits.gadgetSearchEnabled=[&](const std::string&,const Json&,const Json&){
    trace.push_back("search-await");return searchGate;};
  awaits.afterGadgetSearchSegment=[&](const model::PreflightInputs&,
      const model::PreflightServices&,model::PreflightState seed,bool enabled){
    trace.push_back("search-remaining");seed.useGadgetSearch=enabled;
    return seed;
  };
  auto pending=query::startOwnerPreflight(loop,input,services,awaits);
  auto heartbeat=loop.setTimeout(1,[&]{trace.push_back("search-heartbeat");});
  auto release=loop.setTimeout(20,[&]{searchGate.resolve(true);});
  bool enabled=false;std::exception_ptr failure;
  pending.then([&](const model::PreflightState* state,
      const std::exception_ptr& error){
    if(state)enabled=state->useGadgetSearch;failure=error;loop.stop();
  });
  auto hold=loop.hold();loop.run();hold.release();
  if(failure)std::rethrow_exception(failure);
  std::cout<<Json{{"trace",trace},{"enabled",enabled}}.dump()<<'\n';
}
