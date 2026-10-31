#include "instructions/hooks/runtime/StopFailureHooks.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json trace=Json::array(),dispatch=nullptr,error=nullptr;
  bool ownsErrorDetails=false,ownsLast=false,hasGetAppState=false;
  hook::StopFailureServices services;services.defaultTimeoutMs=600000;
  services.getFlightId=[&]{trace.push_back("getSessionId");return "s1";};
  services.presence.getHooksConfigFromSnapshot=[&]()->std::optional<Json>{
    trace.push_back("snapshot");return item.value("snapshot",false)
      ?Json{{"StopFailure",Json::array({Json::object()})}}:Json::object();};
  services.presence.getRegisteredHooks=[&]()->std::optional<Json>{
    trace.push_back("registered");return item.value("registered",false)
      ?Json{{"StopFailure",Json::array({Json::object()})}}:Json::object();};
  services.presence.hasFlightHook=[&](const std::optional<Json>&,
    const std::string&,const std::string&){trace.push_back("session");
      return item.value("session",false);};
  hook::StopFailureContext context;
  context.getAppState=[&]()->Json{
    trace.push_back("getAppState");
    if(item.contains("getterError"))throw std::runtime_error(item.at("getterError").get<std::string>());
    return item.value("nullState",false)?Json(nullptr):Json::object();};
  const auto contextPtr=item.value("context",false)?&context:nullptr;
  services.createBaseHookInput=[&](const hook::StopFailureContext* passed){
    trace.push_back(passed==contextPtr?"base-context":"base-other");
    return hook::BaseHookInput{"s1","/tmp/t","/work"};};
  services.executeHooksOutsideRepl=[&](instructions::hooks::HookDispatch value,
    const std::function<Json()>& getAppState){
    trace.push_back("execute");
    hasGetAppState=static_cast<bool>(getAppState);
    for(const auto& key:value.ownUndefinedKeys){
      if(key=="error_details")ownsErrorDetails=true;
      if(key=="last_assistant_message")ownsLast=true;}
    if(value.hookInput.contains("error_details"))ownsErrorDetails=true;
    if(value.hookInput.contains("last_assistant_message"))ownsLast=true;
    dispatch={{"hookInput",value.hookInput},{"timeoutMs",value.timeoutMs},
      {"matchQuery",value.matchQuery}};
    event_loop::Deferred<std::vector<hook::HookResult>> result(loop);
    if(item.contains("executionError"))result.reject(std::make_exception_ptr(
      std::runtime_error(item.at("executionError").get<std::string>())));
    else result.resolve({});
    return result;};
  hook::StopFailureMessage message;
  message.content=item.at("content");
  if(item.contains("error"))message.error=item.at("error").get<std::string>();
  if(item.contains("errorDetails"))message.errorDetails=item.at("errorDetails");
  std::optional<double> timeout;
  if(item.contains("timeout"))timeout=item.at("timeout").get<double>();
  hook::executeStopFailureHooks(loop,services,message,contextPtr,timeout).then(
    [&](const event_loop::Unit*,const std::exception_ptr& failure){
      if(failure)try{std::rethrow_exception(failure);}catch(const std::exception& e){error=e.what();}
    });
  loop.run();
  return {{"name",item.at("name")},{"trace",trace},{"dispatch",dispatch},
    {"ownsErrorDetails",ownsErrorDetails},{"ownsLast",ownsLast},
    {"hasGetAppState",hasGetAppState},{"error",error}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<rows.dump()<<'\n';
}
