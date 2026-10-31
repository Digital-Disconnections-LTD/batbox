#include "instructions/hooks/runtime/BackgroundHook.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json calls=Json::array();
  hook::AsyncHookServices rosterServices;
  rosterServices.nowMillis=[](){return 123.0;};
  rosterServices.logDebug=[](const std::string&,const std::string&){};
  rosterServices.startProgressInterval=[](const std::string&,const std::string&,
    const std::string&,std::function<event_loop::Deferred<Json>()>){
    return []{};};
  rosterServices.emitHookResponse=[](const hook::AsyncHookResponseEvent&){};
  rosterServices.invalidateFlightEnvStash=[]{return;};
  rosterServices.stringifyThrown=[](std::exception_ptr){return std::string{};};
  hook::AsyncHookRoster roster(loop,std::move(rosterServices));
  auto process=std::make_shared<hook::AsyncHookProcess>();
  process->resultCode=[&,item]{event_loop::Deferred<int> value(loop);
    value.resolve(item.at("code").get<int>());return value;};
  process->getStdout=[&,item]{calls.push_back("stdout");
    event_loop::Deferred<std::string> value(loop);
    value.resolve(item.at("stdout").get<std::string>());return value;};
  process->getStderr=[&,item]{calls.push_back("stderr");
    return item.at("stderr").get<std::string>();};
  process->status=[](){return std::string("running");};
  process->cleanup=[&]{calls.push_back("cleanup");};
  process->kill=[]{return;};
  hook::BackgroundHookRequest request;
  request.processId="async_hook_7";request.hookId="hook-7";
  request.hookEvent="InstructionsLoaded";request.hookName="my-hook";
  request.command="run hook";request.pluginId="plugin-x";
  request.asyncResponse={{"async",true},{"asyncTimeout",1234}};
  request.asyncRewake=item.at("asyncRewake").get<bool>();request.process=process;
  request.background=[&,item](const std::string& id){
    calls.push_back("background:"+id);return item.at("accepted").get<bool>();};
  hook::BackgroundHookServices services;
  services.yieldToIo=[&]{event_loop::Deferred<event_loop::Unit> value(loop);
    loop.postMission([value]{value.resolve({});});return value;};
  services.emitHookResponse=[&](const hook::AsyncHookResponseEvent& event){
    Json value={{"hookId",event.hookId},{"hookName",event.hookName},
      {"hookEvent",event.hookEvent},{"output",event.output},
      {"stdout",event.stdoutText},{"stderr",event.stderrText},
      {"exitCode",event.exitCode},{"outcome",event.outcome}};
    calls.push_back("emit:"+value::stringify(value));};
  services.wrapInSystemReminder=[&](const std::string& text){
    calls.push_back("wrap:"+text);return "<"+text+">";};
  services.enqueuePendingNotification=[&](const std::string& value,
    const std::string& mode){
    calls.push_back("queue:"+value::stringify(Json{{"value",value},{"mode",mode}}));};
  services.onDetachedError=[](std::exception_ptr error){std::rethrow_exception(error);};
  const auto result=hook::executeInBackground(loop,roster,std::move(request),services);
  if(roster.size()){
    const auto registered=roster.getPendingAsyncHooks().at(0);
    Json value={{"processId",registered.processId},{"hookId",registered.hookId},
      {"hookEvent",registered.hookEvent},{"hookName",registered.hookName},
      {"command",registered.command},{"asyncResponse",Json{{"async",true},{"asyncTimeout",1234}}},
      {"pluginId",*registered.pluginId}};
    calls.push_back("register:"+value::stringify(value));
  }
  loop.run();
  return {{"name",item.at("name")},{"result",result},{"calls",calls}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<value::stringify(rows)<<'\n';
}
