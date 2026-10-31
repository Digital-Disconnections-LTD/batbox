#include "flight_restore/FlightRestore.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace batbox;
using namespace batbox::flight_restore;
using event_loop::Deferred;
using event_loop::Unit;
namespace {
Json field(const Json& value,const char* key){return value::property(value,key);}
std::optional<std::string> optString(const Json& value){
  return value.is_string()?std::optional(value.get<std::string>()):std::nullopt;}
std::string str(const Json& value){return value.is_string()?value.get<std::string>():"";}
}
int main(){
  const Json input=value::parse(std::string(std::istreambuf_iterator<char>(std::cin),{}));
  event_loop::Loop loop;
  Json calls=Json::array(),appState=field(input,"appState");
  if(appState.is_null())appState=Json{{"todos",Json::object()}};
  std::string cwd="/base";
  Json currentWorktree=field(input,"currentWorktree");
  Json modelOverride=field(input,"modelOverride");
  auto push=[&calls](Json value){calls.push_back(std::move(value));};
  Services s;s.loop=&loop;
  s.feature=[&](const std::string& name){push(Json::array({"feature",name}));
    return value::truthy(value::property(field(input,"features"),name));};
  s.setAppState=[&](const AppStateUpdate& update){push(Json::array({"setAppState"}));
    appState=update(appState);};
  s.restoreFileHistory=[&](const Json& snapshots,const std::function<void(const Json&)>& callback){
    push(Json::array({"restoreFileHistory",snapshots}));callback(Json{{"restored",snapshots.size()}});};
  s.restoreAttribution=[&](const Json& snapshots,const std::function<void(const Json&)>& callback){
    push(Json::array({"restoreAttribution",snapshots}));callback(Json{{"restored",snapshots.size()}});};
  s.computeAttribution=[&](const Json& snapshots){push(Json::array({"computeAttribution",snapshots}));
    return Json{{"restored",snapshots.size()}};};
  s.restoreContextCollapse=[&](const Json& commits,const Json& snapshot){
    push(Json::array({"require","flight_restore/ContextCollapse"}));
    push(Json::array({"restoreCollapse",commits,snapshot}));};
  s.isTodoV2Enabled=[&]{push(Json::array({"isTodoV2"}));return value::truthy(field(input,"todoV2"));};
  s.parseTodoList=[&](const Json& value){push(Json::array({"parseTodos",value}));
    if(!value.is_array())return Json::array();
    for(const Json& item:value)if(!item.is_object()||!value::property(item,"content").is_string())
      return Json::array();
    return value;};
  s.getFlightId=[&]{push(Json::array({"getSessionId"}));return "session-new";};
  s.setMainThreadBatType=[&](std::optional<std::string> value){
    push(Json::array({"setAgentType",value?Json(*value):Json(nullptr)}));};
  s.getMainLoopModelOverride=[&]{push(Json::array({"getModelOverride"}));return modelOverride;};
  s.setMainLoopModelOverride=[&](const Json& value){push(Json::array({"setModelOverride",value}));modelOverride=value;};
  s.parseUserSpecifiedModel=[&](const std::string& value){push(Json::array({"parseModel",value}));return Json("parsed:"+value);};
  s.logForDebugging=[&](const std::string& value){push(Json::array({"debug",value}));};
  s.clearBatDefinitionsStash=[&]{push(Json::array({"clearAgentCache"}));};
  s.getBatDefinitionsWithOverrides=[&](const std::string& currentCwd){
    push(Json::array({"getAgentDefinitions",currentCwd}));Deferred<Json> d(loop);
    if(field(input,"failAt")=="getAgentDefinitions")d.reject(std::make_exception_ptr(std::runtime_error("getAgentDefinitions failed")));
    else{
      Json defs=field(input,"freshDefinitions");
      if(defs.is_null())defs=Json{{"allAgents",Json::array({Json{{"agentType","fresh"}}})},
                                  {"activeAgents",Json::array()},{"extra","fresh"}};
      d.resolve(std::move(defs));
    }return d;};
  s.getActiveBatsFromList=[&](const Json& all){push(Json::array({"getActiveAgents",all}));
    Json active=Json::array();for(const Json& bat:all)if(!value::truthy(field(bat,"disabled")))active.push_back(bat);
    return active;};
  s.getCurrentWorktreeFlight=[&]{push(Json::array({"getCurrentWorktree"}));return currentWorktree;};
  s.saveWorktreeState=[&](const Json& value){push(Json::array({"saveWorktree",value}));};
  s.restoreWorktreeFlight=[&](const Json& value){push(Json::array({"restoreWorktree",value}));currentWorktree=value;};
  s.chdir=[&](const std::string& path){push(Json::array({"chdir",path}));
    if(path=="/bad")throw std::runtime_error("ENOENT");cwd=path;};
  s.setCwd=[&](const std::string& value){push(Json::array({"setCwd",value}));};
  s.setOriginalCwd=[&](const std::string& value){push(Json::array({"setOriginalCwd",value}));};
  s.getCwd=[&]{push(Json::array({"getCwd"}));return cwd;};
  s.clearMemoryFileStashes=[&]{push(Json::array({"clearMemory"}));};
  s.clearSystemPoetrySections=[&]{push(Json::array({"clearSections"}));};
  s.clearPlansDirectoryStash=[&]{push(Json::array({"clearPlans"}));};
  s.createSystemMessage=[&](const std::string& value,const std::string& kind){
    push(Json::array({"createSystem",value,kind}));return Json{{"type","system"},{"message",value},{"kind",kind}};};
  s.matchFlightMode=[&](std::optional<std::string> mode){push(Json::array({"matchMode",mode?Json(*mode):Json(nullptr)}));
    return optString(field(input,"modeWarning"));};
  s.isCoordinatorMode=[&]{push(Json::array({"isCoordinator"}));return value::truthy(field(input,"coordinator"));};
  s.switchFlight=[&](const std::string& id,std::optional<std::string> project){
    push(Json::array({"asSessionId",id}));
    push(Json::array({"switchSession",id,project?Json(*project):Json(nullptr)}));};
  s.dirname=[](const std::string& path){return std::filesystem::path(path).parent_path().string();};
  s.renameRecordingForFlight=[&]{push(Json::array({"renameRecording"}));Deferred<Unit> d(loop);
    if(field(input,"failAt")=="rename")d.reject(std::make_exception_ptr(std::runtime_error("rename failed")));
    else d.resolve(Unit{});return d;};
  s.resetFlightFilePointer=[&]{push(Json::array({"resetPointer"}));Deferred<Unit> d(loop);
    if(field(input,"failAt")=="reset")d.reject(std::make_exception_ptr(std::runtime_error("reset failed")));
    else d.resolve(Unit{});return d;};
  s.restoreCostStateForFlight=[&](const std::string& value){push(Json::array({"restoreCost",value}));};
  s.recordContentReplacement=[&](const Json& value){push(Json::array({"recordReplacements",value}));Deferred<Unit> d(loop);
    if(field(input,"failAt")=="record")d.reject(std::make_exception_ptr(std::runtime_error("record failed")));
    else d.resolve(Unit{});return d;};
  s.restoreFlightMetadata=[&](const Json& value){push(Json::array({"restoreMetadata",value}));};
  s.adoptResumedFlightFile=[&]{push(Json::array({"adoptFile"}));};
  s.saveMode=[&](const std::string& value){push(Json::array({"saveMode",value}));};
  s.updateFlightName=[&](std::optional<std::string> value){push(Json::array({"updateName",value?Json(*value):Json(nullptr)}));};

  Json value=nullptr;std::string status="fulfilled";
  try{
    const std::string method=str(field(input,"method"));
    Json result=field(input,"result");if(result.is_null())result=Json::object();
    if(method=="todos")value=extractTodosFromTranscript(field(result,"messages"),s);
    else if(method=="restoreState")restoreFlightStateFromLog(result,s);
    else if(method=="computeAttribution"){
      auto v=computeRestoredAttributionState(result,s);if(v)value=*v;}
    else if(method=="standalone"){
      auto v=computeStandaloneBatContext(optString(field(input,"agentName")),
        optString(field(input,"agentColor")));if(v)value=*v;}
    else if(method=="agent")value=restoreBatFromFlight(
      optString(field(input,"agentSetting")),field(input,"currentAgent"),
      field(input,"agentDefinitions").is_null()?Json{{"activeAgents",Json::array()}}:
        field(input,"agentDefinitions"),s);
    else if(method=="refresh"){
      Json defs=field(input,"agentDefinitions");if(defs.is_null())defs=Json{{"allAgents",Json::array()},{"activeAgents",Json::array()}};
      Json cli=field(input,"cliAgents");if(cli.is_null())cli=Json::array();
      refreshBatDefinitionsForModeSwitch(value::truthy(field(input,"switched")),
        str(field(input,"cwd")).empty()?"/base":str(field(input,"cwd")),cli,defs,s).then(
        [&](const Json* item,const std::exception_ptr& error){if(error){status="rejected";
          try{std::rethrow_exception(error);}catch(const std::exception& e){value=e.what();}}
          else value=*item;});
      loop.run();
    }else if(method=="worktree")restoreWorktreeForResume(field(input,"worktreeSession"),s);
    else if(method=="exitWorktree")exitRestoredWorktree(s);
    else if(method=="process"){
      auto loaded=std::make_shared<Json>(result);
      if(!loaded->contains("messages"))(*loaded)["messages"]=Json::array();
      Json ctx={{"modeApi",value::truthy(field(input,"modeApi"))?Json::object():Json(nullptr)},
                {"agentDefinitions",field(input,"agentDefinitions").is_null()?Json{{"allAgents",Json::array()},{"activeAgents",Json::array()}}:field(input,"agentDefinitions")},
                {"currentCwd","/base"},{"cliAgents",field(input,"cliAgents").is_null()?Json::array():field(input,"cliAgents")},
                {"initialState",appState}};
      const Json& currentBat=field(input,"currentAgent");if(!currentBat.is_null())ctx["mainThreadAgentDefinition"]=currentBat;
      if(!value::truthy(field(input,"modeApi")))s.matchFlightMode={};
      Json opts=field(input,"opts");if(opts.is_null())opts=Json{{"forkSession",false}};
      processResumedConversation(loaded,opts,ctx,s).then(
        [&](const Json* item,const std::exception_ptr& error){if(error){status="rejected";
          try{std::rethrow_exception(error);}catch(const std::exception& e){value=e.what();}}
          else value=*item;});
      loop.run();
    }
  }catch(const std::exception& e){status="rejected";value=e.what();}
  std::cout<<value::stringify(Json{{"status",status},{"value",value},{"calls",calls},
    {"appState",appState},{"cwd",cwd},{"currentWorktree",currentWorktree},
    {"modelOverride",modelOverride}});
}
