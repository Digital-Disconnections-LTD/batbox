#include "query/OwnerPreflight.hpp"
#include <iostream>

using namespace batbox;
int main(int argc,char** argv){
  const std::string scenario=argc>1?argv[1]:"cached";
  event_loop::Loop loop;
  Json trace=Json::array();
  model::PreflightInputs input;
  input.options={{"model","batbox-model"},{"querySource","side_query"}};
  input.gadgets=Json::array({Json{{"name","slow"}},Json{{"name","fast"}}});
  model::PreflightServices services;
  services.isSubscriber=[](){return true;};
  services.isNonCustomPrimaryModel=[](const std::string&){return false;};
  services.getAPIProvider=[](){return std::string("local");};
  services.getMergedBetas=[](const std::string&,bool){return Json::array();};
  services.isAdvisorEnabled=[](){return false;};
  services.isDeferredGadget=[](const Json&){return false;};
  services.gadgetMatchesName=[](const Json&,const std::string&){return false;};
  services.stashedMicrocompactFeature=[&]{return scenario=="cached";};
  services.isStashedMicrocompactEnabled=[](){return true;};
  services.isModelSupportedForStashEditing=[](const std::string&){return true;};
  services.stashEditingBetaHeader=[](){return std::string("cache-editing");};
  services.shouldUseGlobalStashScope=[](){return false;};
  query::OwnerPreflightAwaits awaits;
  awaits.gadgetSearchEnabled=[&](const std::string&,const Json&,const Json&){
    event_loop::Deferred<bool> ready(loop);ready.resolve(false);return ready;};
  awaits.loadStashedModule=[&]{
    trace.push_back("cached-import-start");
    event_loop::Deferred<event_loop::Unit> pending(loop);
    loop.setTimeout(20,[&trace,pending]{
      trace.push_back("cached-import-done");pending.resolve(event_loop::Unit{});});
    return pending;
  };
  awaits.loadBetasModule=[&]{
    trace.push_back("betas-import-start");
    event_loop::Deferred<event_loop::Unit> pending(loop);
    loop.setTimeout(20,[&trace,pending]{
      trace.push_back("betas-import-done");pending.resolve(event_loop::Unit{});});
    return pending;
  };
  awaits.gadgetToAPISchema=[&](const Json& gadget,bool,const Json&,const Json&){
    const std::string name=gadget.at("name").get<std::string>();
    trace.push_back("schema-start:"+name);
    event_loop::Deferred<Json> pending(loop);
    loop.setTimeout(name=="slow"?20:3,[&trace,pending,name]{
      trace.push_back("schema-done:"+name);
      pending.resolve(Json{{"name",name}});
    });
    return pending;
  };
  awaits.afterSchemasSegment=[&](const model::PreflightInputs&,
      const model::PreflightServices&,model::PreflightState seed,Json schemas){
    trace.push_back("remaining");seed.gadgetSchemas=std::move(schemas);return seed;};
  auto pending=query::startOwnerPreflight(loop,input,services,awaits);
  auto heartbeat=loop.setTimeout(1,[&]{trace.push_back("heartbeat");});
  std::optional<model::PreflightState> value;std::exception_ptr failure;
  pending.then([&](const model::PreflightState* state,
      const std::exception_ptr& error){
    if(state)value=*state;failure=error;loop.stop();
  });
  auto hold=loop.hold();loop.run();hold.release();
  if(failure)std::rethrow_exception(failure);
  if(!value)throw std::logic_error("Owner cached/schema stage did not settle");
  Json result={{"cachedMCEnabled",value->stashedMCEnabled},
    {"cacheEditingBetaHeader",value->stashEditingHeader},
    {"toolSchemas",value->gadgetSchemas}};
  std::cout<<Json{{"trace",trace},{"value",result}}.dump()<<'\n';
}
