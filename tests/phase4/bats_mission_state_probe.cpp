#include "bats/BatMissionState.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& object,const char* key){return value::property(object,key);}
static Json run(const Json& item){
 Json trace=Json::array(),results=Json::array();
 Json mission={{"type","local_agent"},{"id","agent-id"},{"agentId","agent-id"},
  {"status",field(item,"status")},{"agentType",field(item,"agentType")},
  {"retain",field(item,"retain")},{"notified",field(item,"notified")},
  {"selectedAgent",{{"agentType","worker"}}},
  {"abortController",Json::object()},{"startTime",100},
  {"toolUseId","tool-id"},{"pendingMessages",field(item,"pending")},
  {"messages",field(item,"messages")},{"progress",field(item,"progress")}};
 Json state={{"tasks",{{"agent-id",mission}}}};
 ba::BatMissionStateServices services;
 services.setAppState=[&](const std::function<Json(const Json&)>& update){
  trace.push_back(Json::array({"setState"}));state=update(state);};
 services.getAppState=[&]{return state;};
 services.nowMillis=[&]{trace.push_back(Json::array({"now"}));return std::int64_t(1000);};
 services.abortMission=[&](const std::string&){trace.push_back(Json::array({"abort"}));};
 services.unregisterCleanup=[&](const std::string&){trace.push_back(Json::array({"unregister"}));};
 services.evictMissionOutput=[&](const std::string& id){
  trace.push_back(Json::array({"evictOutput",id}));};
 services.sdkProgressSummariesEnabled=[&]{trace.push_back(Json::array({"sdkSummary"}));
  return field(item,"sdkSummary")==true;};
 services.emitMissionProgress=[&](const Json& event){
  trace.push_back(Json::array({"emitProgress",event}));};
 const Json& operations=field(item,"operations");
 if(operations.is_array())for(const Json& operation:operations){
  if(operation=="queue")ba::queuePendingMessage("agent-id","new message",services);
  if(operation=="append")ba::appendMessageToLocalBat("agent-id",
    Json{{"type","user"},{"uuid","new"}},services);
  if(operation=="drain")results.push_back(Json::array({"drain",
    ba::drainPendingMessages("agent-id",services)}));
  if(operation=="progress")ba::updateBatProgress("agent-id",
    Json{{"toolUseCount",2},{"tokenCount",42}},services);
  if(operation=="summary")ba::updateBatSummary("agent-id","working",services);
  if(operation=="complete")ba::completeBatMission(
    Json{{"agentId","agent-id"},{"content",Json::array()}},services);
  if(operation=="fail")ba::failBatMission("agent-id","broken",services);
  if(operation=="kill")ba::killAsyncBat("agent-id",services);
  if(operation=="killAll")ba::killAllRunningBatMissions(field(state,"tasks"),services);
  if(operation=="notified")ba::markBatsNotified("agent-id",services);
  results.push_back(Json::array({"panel",ba::isPanelBatMission(
    field(field(state,"tasks"),"agent-id"))}));
 }
 return Json{{"trace",trace},{"state",state},{"results",results}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{
 std::cout<<value::stringify(run(value::parse(line)))<<'\n';
 }catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}}
