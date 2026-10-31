#include "query/StopHooks.hpp"
#include <iostream>
#include <stop_token>
using namespace batbox;
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
bool enabled(const Json& item,const char* key){return value::truthy(field(item,key));}
Json user(const Json& args){return Json{{"factory","user"},{"args",args}};}
}
int main(){std::string line;while(std::getline(std::cin,line))try{
  const Json item=value::parse(line);
  Json emitted=Json::array(),saved=Json::array(),notifications=Json::array();
  query::Params params;params.querySource=item.value("querySource",std::string("sdk"));
  execution::Context context;
  if(item.contains("agentId"))context.data["agentId"]=item.at("agentId");
  if(item.contains("agentType"))context.data["agentType"]=item.at("agentType");
  std::stop_source stop;
  if(enabled(item,"aborted"))stop.request_stop();
  context.stop=stop.get_token();
  query::StopHookServices services;
  services.feature=[](const std::string&){return false;};
  services.createStashSafeParams=[](const Json& context){
    return Json{{"querySource",context.at("querySource")},{"messages",context.at("messages")}};
  };
  services.saveStashSafeParams=[&](const Json& value){saved.push_back(value);};
  services.isBareMode=[]{return true;};
  services.getAppState=[](const execution::Context&){
    return Json{{"toolPermissionContext",{{"mode","default"}}}};
  };
  services.executeStopHooks=[&](const Json&,const execution::Context&,
      const query::HookResultSink& yield){
    const Json& events=field(item,"events");
    if(events.is_array())for(const Json& event:events)if(!yield(event))break;
  };
  services.createUserMessage=user;
  services.createAttachmentMessage=[](const Json& attachment){
    return Json{{"factory","attachment"},{"attachment",attachment}};
  };
  services.createUserInterruptionMessage=[](bool gadgetUse){
    return Json{{"factory","interruption"},{"args",{{"toolUse",gadgetUse}}}};
  };
  services.createStopHookSummaryMessage=[](int count,const Json& infos,const Json& errors,
    bool prevented,const std::string& reason,bool hasOutput,
    const std::string& suggestion,const std::string& id){
    return Json{{"factory","summary"},{"hookCount",count},{"hookInfos",infos},
      {"hookErrors",errors},{"preventedContinuation",prevented},
      {"stopReason",reason},{"hasOutput",hasOutput},{"suggestion",suggestion},
      {"stopHookToolUseID",id}};
  };
  services.getStopHookMessage=[](const Json& error){return field(error,"blockingError");};
  services.getMissionCompletedHookMessage=services.getStopHookMessage;
  services.getPartnerIdleHookMessage=services.getStopHookMessage;
  services.getShortcutDisplay=[]{return "ctrl+o";};
  services.addNotification=[&](const Json& value){notifications.push_back(value);};
  services.isPartner=[&]{return enabled(item,"teammate");};
  services.getBatName=[]{return std::optional<std::string>("worker");};
  services.getTeamName=[]{return std::optional<std::string>("team");};
  services.getMissionListId=[]{return "tasks";};
  services.listMissions=[&](const std::string&){return field(item,"tasks").is_array()?field(item,"tasks"):Json::array();};
  services.executeMissionCompletedHooks=[&](const Json&,const query::HookResultSink& yield){
    const Json& events=field(item,"taskEvents");
    if(events.is_array())for(const Json& event:events)if(!yield(event))break;
  };
  services.executePartnerIdleHooks=[&](const Json&,const query::HookResultSink& yield){
    const Json& events=field(item,"idleEvents");
    if(events.is_array())for(const Json& event:events)if(!yield(event))break;
  };
  services.createSystemMessage=[](const std::string& text,const std::string& level){
    return Json{{"factory","system"},{"text",text},{"level",level}};
  };
  const Json terminal=query::handleStopHooks(field(item,"messages").is_array()?field(item,"messages"):Json::array(),
    field(item,"assistants").is_array()?field(item,"assistants"):Json::array(),
    params,context,item.contains("stopHookActive")?
      std::optional<bool>(item.at("stopHookActive").get<bool>()):std::nullopt,
    services,[&](const Json& message){emitted.push_back(message);});
  std::cout<<value::stringify(Json{{"emitted",emitted},{"terminal",terminal},
    {"saved",saved},{"notifications",notifications}})<<'\n';
}catch(const std::exception& error){std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';}}
