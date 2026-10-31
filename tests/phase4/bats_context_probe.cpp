#include "bats/ForkedBat.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& value,const char* key){return value::property(value,key);}
static Json defaulted(const Json& value,const Json& fallback){return value.is_null()?fallback:value;}
static std::optional<std::string> text(const Json& value){
 return value.is_string()?std::optional<std::string>(value.get<std::string>()):std::nullopt;
}
static Json run(const Json& item){
 Json trace=Json::array();std::stop_source parentStop,overrideStop;
 ba::BatContext parent;
 parent.query.stop=parentStop.get_token();
 parent.query.data={{"readFileState",defaulted(field(item,"readFileState"),Json{{"path","parent"}})},
  {"options",defaulted(field(item,"options"),Json{{"model","parent"}})},
  {"messages",defaulted(field(item,"messages"),Json::array())},
  {"localDenialTracking",Json{{"denial","parent"}}}};
 if(!field(item,"parentReplacement").is_null())
  parent.query.data["contentReplacementState"]=field(item,"parentReplacement");
 if(!field(item,"queryTracking").is_null())parent.query.data["queryTracking"]=field(item,"queryTracking");
 parent.getAppState=[&]{return Json{{"toolPermissionContext",
  Json{{"shouldAvoidPermissionPrompts",field(item,"avoidPrompts")==true}}}};};
 parent.setAppState=[&](const auto&){trace.push_back(Json::array({"setAppState"}));};
 parent.setAppStateForMissions=[&](const auto&){trace.push_back(Json::array({"setAppStateForTasks"}));};
 parent.setResponseLength=[&](const Json&){trace.push_back(Json::array({"setResponseLength"}));};
 parent.pushApiMetricsEntry=[&](const Json&){trace.push_back(Json::array({"metrics"}));};
 parent.updateFileHistoryState=[&](const Json&){trace.push_back(Json::array({"fileHistory"}));};
 parent.updateAttributionState=[&](const auto&){trace.push_back(Json::array({"attribution"}));};
 ba::ContextServices services;
 services.cloneFileStateStash=[&](const Json& state){
  trace.push_back(Json::array({"cloneFile",state}));return Json{{"clone",state}};};
 services.cloneContentReplacementState=[&](const Json& state){
  trace.push_back(Json::array({"cloneReplacement",state}));return Json{{"clone",state}};};
 services.createDenialTrackingState=[&]{trace.push_back(Json::array({"denial"}));
  return Json{{"denial",true}};};
 services.createBatId=[&](const auto&){trace.push_back(Json::array({"agentId"}));
  return std::string("child-id");};
 services.randomUUID=[&]{trace.push_back(Json::array({"uuid"}));return std::string("chain-id");};
 ba::SubagentOverrides overrides;
 const Json& supplied=field(item,"overrides");
 overrides.batId=text(field(supplied,"agentId"));
 overrides.batType=text(field(supplied,"agentType"));
 if(supplied.is_object()&&supplied.contains("options"))overrides.options=field(supplied,"options");
 if(supplied.is_object()&&supplied.contains("messages"))overrides.messages=field(supplied,"messages");
 if(supplied.is_object()&&supplied.contains("readFileState"))overrides.readFileState=field(supplied,"readFileState");
 if(supplied.is_object()&&supplied.contains("contentReplacementState"))
  overrides.contentReplacementState=field(supplied,"contentReplacementState");
 overrides.shareAbortController=field(supplied,"shareAbortController")==true;
 overrides.shareSetAppState=field(supplied,"shareSetAppState")==true;
 overrides.shareSetResponseLength=field(supplied,"shareSetResponseLength")==true;
 overrides.criticalSystemReminder=text(field(supplied,"criticalSystemReminder_EXPERIMENTAL"));
 if(field(supplied,"requireCanUseTool").is_boolean())
  overrides.requireCanUseGadget=field(supplied,"requireCanUseTool").get<bool>();
 if(field(item,"overrideAbort")==true)overrides.abortToken=overrideStop.get_token();
 if(field(item,"overrideGetAppState")==true)
  overrides.getAppState=[]{return Json{{"toolPermissionContext",Json{{"override",true}}}};};
 ba::BatContext child=ba::createSubagentContext(parent,overrides,services);
 if(child.setAppState)child.setAppState([](const Json& state){return state;});
 if(child.setAppStateForMissions)child.setAppStateForMissions([](const Json& state){return state;});
 if(child.setResponseLength)child.setResponseLength("short");
 if(child.pushApiMetricsEntry)child.pushApiMetricsEntry(Json::object());
 if(child.updateFileHistoryState)child.updateFileHistoryState(Json::object());
 if(child.updateAttributionState)child.updateAttributionState([](const Json& state){return state;});
 std::string abort="child";
 if(child.query.stop==overrideStop.get_token())abort="override";
 else if(child.query.stop==parentStop.get_token())abort="parent";
 return Json{{"trace",trace},{"result",{{"abort",abort},
  {"getAppState",child.getAppState()},
  {"readFileState",field(child.query.data,"readFileState")},
  {"replacement",field(child.query.data,"contentReplacementState")},
  {"nested",field(child.query.data,"nestedMemoryAttachmentTriggers")},
  {"loaded",field(child.query.data,"loadedNestedMemoryPaths")},
  {"dynamic",field(child.query.data,"dynamicSkillDirTriggers")},
  {"toolDecisions",field(child.query.data,"toolDecisions")},
  {"localDenialTracking",field(child.query.data,"localDenialTracking")},
  {"options",field(child.query.data,"options")},
  {"messages",field(child.query.data,"messages")},
  {"agentId",field(child.query.data,"agentId")},
  {"agentType",field(child.query.data,"agentType")},
  {"queryTracking",field(child.query.data,"queryTracking")},
  {"criticalSystemReminder",field(child.query.data,"criticalSystemReminder_EXPERIMENTAL")},
  {"requireCanUseTool",field(child.query.data,"requireCanUseTool")}}}};
}
int main(){std::string line;while(std::getline(std::cin,line)){
 try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
 catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}}
