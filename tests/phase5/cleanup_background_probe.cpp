#include "cleanup/Background.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace batbox;
namespace {
struct Case {std::string name;int errors=0;bool explicitKey=false;
  std::optional<std::string> userType;int removed=0;
  std::optional<std::string> fail;};
Json run(const Case& item){
  event_loop::Loop loop;Json logs=Json::array();
  cleanup::BackgroundServices services;
  services.settingsErrorCount=[&]{logs.push_back("settings");return static_cast<std::size_t>(item.errors);};
  services.rawSettingsContainsKey=[&](const std::string& key){
    logs.push_back("raw:"+key);return item.explicitKey;};
  services.debug=[&](const std::string& value){logs.push_back("debug:"+value);};
  auto resultMission=[&](const std::string& name){
    return [&,name]{logs.push_back(name);
      event_loop::Deferred<cleanup::Result> pending(loop);
      if(item.fail&&*item.fail==name)pending.reject(
        std::make_exception_ptr(std::runtime_error("failed "+name)));
      else pending.resolve({1,0});return pending;};
  };
  auto unitMission=[&](const std::string& name){
    return [&,name]{logs.push_back(name);
      event_loop::Deferred<event_loop::Unit> pending(loop);
      if(item.fail&&*item.fail==name)pending.reject(
        std::make_exception_ptr(std::runtime_error("failed "+name)));
      else pending.resolve(event_loop::Unit{});return pending;};
  };
  services.oldMessageFiles=resultMission("message");
  services.oldFlightFiles=resultMission("session");
  services.oldPlanFiles=resultMission("plan");
  services.oldFileHistoryBackups=resultMission("history");
  services.oldFlightEnvDirs=resultMission("env");
  services.oldDebugLogs=resultMission("debug");
  services.oldImageStashes=unitMission("image");
  double cutoff=100;
  services.getCutoffDateMs=[&]{logs.push_back("cutoff");return cutoff++;};
  services.oldPastes=[&](double value){logs.push_back("pastes:"+value::numberToString(value));
    event_loop::Deferred<event_loop::Unit> pending(loop);pending.resolve(event_loop::Unit{});return pending;};
  services.staleBatWorktrees=[&](double value){logs.push_back("worktrees:"+value::numberToString(value));
    event_loop::Deferred<int> pending(loop);pending.resolve(item.removed);return pending;};
  services.packageStashCleanup=unitMission("package");
  services.userType=item.userType;services.internalUserType="ant";
  auto pending=cleanup::cleanupOldMessageFilesInBackground(loop,services);
  std::string value;
  pending.then([&](const event_loop::Unit*,const std::exception_ptr& error){
    if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){
      value="ERROR:"+std::string(e.what());}}
    else value="resolved";
  });
  loop.run();
  return Json{{"name",item.name},{"value",value},{"logs",logs}};
}
}
int main(){
  const std::vector<Case> cases={{"normal"},{"invalid-explicit",1,true},
    {"invalid-other",1},{"internal",0,false,"ant",2},
    {"failure",0,false,{},0,"session"}};
  Json out=Json::array();for(const auto& item:cases)out.push_back(run(item));
  std::cout<<value::stringify(out)<<'\n';
}
