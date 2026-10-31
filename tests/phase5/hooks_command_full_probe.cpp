#include "instructions/hooks/runtime/IndigenousCommandFull.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;
  hook::AsyncHookServices rosterServices;
  rosterServices.nowMillis=[](){return 123.0;};
  rosterServices.logDebug=[](const std::string&,const std::string&){};
  rosterServices.startProgressInterval=[](const std::string&,
    const std::string&,const std::string&,
    std::function<event_loop::Deferred<Json>()>){return []{};};
  rosterServices.emitHookResponse=[](const hook::AsyncHookResponseEvent&){};
  rosterServices.invalidateFlightEnvStash=[]{return;};
  rosterServices.stringifyThrown=[](std::exception_ptr){return std::string("Error");};
  hook::AsyncHookRoster roster(loop,std::move(rosterServices));
  hook::IndigenousCommandConfig config;
  config.shellPath="/bin/sh";
  config.getProjectRoot=[](){return std::string("/tmp");};
  config.getCwd=[](){return std::string("/tmp");};
  config.getOriginalCwd=[](){return std::string("/tmp");};
  config.subprocessEnv=[](){return std::map<std::string,std::string>{{"PATH","/usr/bin:/bin"}};};
  config.pathExists=[&](const std::string&){
    event_loop::Deferred<bool> value(loop);value.resolve(true);return value;};
  config.shellPrefix=[](){return std::optional<std::string>{};};
  config.logDebug=[](const std::string&,const std::string&){};
  config.logDiagnostics=[](const std::string&,const Json&){};
  config.nowMillis=[](){return 123.0;};
  hook::FullCommandServices services;
  services.roster=&roster;
  services.parseJson=[](const std::string& text){return value::parse(text);};
  services.stringifyThrown=[](std::exception_ptr error){
    try{std::rethrow_exception(error);}catch(const std::exception& value){
      return std::string(value.what());}
  };
  services.startProgressInterval=[](const std::string&,const std::string&,
    const std::string&,std::function<event_loop::Deferred<Json>()>){return []{};};
  services.background.yieldToIo=[&]{
    event_loop::Deferred<event_loop::Unit> value(loop);
    loop.postMission([value]{value.resolve({});});return value;};
  services.background.emitHookResponse=[](const hook::AsyncHookResponseEvent&){};
  services.background.wrapInSystemReminder=[](const std::string& text){return text;};
  services.background.enqueuePendingNotification=[](const std::string&,
    const std::string&){};
  services.background.onDetachedError=[](std::exception_ptr error){
    std::rethrow_exception(error);};
  hook::FullCommandRequest request;
  request.base.hook=item.at("hook");
  request.base.hookEvent="InstructionsLoaded";
  request.base.hookName="sample";
  request.base.jsonInput="{\"input\":1}";
  request.base.uuid="hook-1";
  Json response=nullptr;
  hook::indigenousFullCommandHook(loop,config,services)(std::move(request)).then(
    [&](const hook::CommandOutcome* value,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      response={{"stdout",value->stdoutText},{"stderr",value->stderrText},
        {"output",value->output},{"status",value->status}};
      if(value->backgrounded)response["backgrounded"]=true;
      else if(roster.size()==0)response["aborted"]=value->aborted;
    });
  loop.run();
  return {{"name",item.at("name")},{"result",response},
    {"registered",roster.size()>0}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<value::stringify(rows)<<'\n';
}
