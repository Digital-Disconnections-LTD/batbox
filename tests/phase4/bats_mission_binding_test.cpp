#include "bats/BatMissionBinding.hpp"
#include "bats/BatMissionRuntime.hpp"
#include <cassert>
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& object,const char* key){return value::property(object,key);}
int main(){
 Json state={{"tasks",{{"task",{{"type","local_agent"},{"status","running"},
  {"retain",true},{"notified",false},{"messages",Json::array()},
  {"progress",Json::object()},{"pendingMessages",Json::array()},
  {"startTime",100},{"toolUseId","tool-id"}}}}},
  {"toolPermissionContext",{{"mode","default"}}}};
 std::vector<std::string> order;
 ba::RootSetAppState rootSet=[&](const std::function<Json(const Json&)>& updater){
  state=updater(state);};
 ba::BatMissionStateServices mission;
 mission.setAppState=rootSet;mission.getAppState=[&]{return state;};
 mission.nowMillis=[](){return std::int64_t(1000);};
 mission.abortMission=[](const std::string&){};
 mission.unregisterCleanup=[&](const std::string&){order.push_back("unregister");};
 mission.evictMissionOutput=[&](const std::string&){order.push_back("evictOutput");};
 mission.emitMissionProgress=[](const Json&){};
 ba::BatMissionNotificationServices notification;
 notification.setAppState=rootSet;
 notification.abortSpeculation=[&](const ba::RootSetAppState&){
  order.push_back("abortSpeculation");};
 notification.getMissionOutputPath=[](const std::string&){return "/tmp/task.txt";};
 notification.enqueuePendingNotification=[&](const Json& value){
  order.push_back("enqueue");
  const std::string message=field(value,"value").get<std::string>();
  assert(message.find("Agent \"description\" completed")!=std::string::npos);
  assert(message.find("<task-id>task</task-id>")!=std::string::npos);};
 ba::BatMissionProgressImports progress;
 progress.syntheticOutputGadgetName="StructuredOutput";
 progress.getGadgetSearchOrReadInfo=[](const std::string&,const Json&,
   const batbox::gadgets::Gadgets&){return Json::object();};
 ba::AsyncLifecycleImports lifecycle;
 ba::bindLocalBatMissionLifecycle(lifecycle,mission,notification,progress);
 lifecycle.result.getLastAssistantMessage=[](const Json& messages){
  for(auto it=messages.rbegin();it!=messages.rend();++it)
   if(field(*it,"type")=="assistant")return *it;
  return Json(nullptr);};
 lifecycle.result.getTokenCountFromUsage=[](const Json& usage){
  return field(usage,"input_tokens").get<double>();};
 lifecycle.result.extractTextContent=[](const Json& blocks,const std::string&){
  return field(blocks[0],"text").get<std::string>();};
 lifecycle.result.nowMillis=[](){return std::int64_t(1000);};
 lifecycle.feature=[](const std::string&){return false;};
 lifecycle.errorMessage=[](std::exception_ptr error){try{std::rethrow_exception(error);}
  catch(const std::exception& exception){return std::string(exception.what());}};
 lifecycle.clearInvokedSkillsForBat=[&](const std::string&){order.push_back("clearSkills");};
 ba::AsyncLifecycleParams params;
 params.missionId="task";params.description="description";
 params.batIdForCleanup="agent-id";params.rootSetAppState=rootSet;
 params.abortOwner=std::make_shared<std::stop_source>();
 params.metadata={"prompt","model","worker",false,true,900};
 params.gadgetUseContext.getAppState=[&]{return state;};
 params.gadgetUseContext.query.data={{"toolUseId","tool-id"}};
 params.makeStream=[](const auto&,const batbox::query::Yield& yield){
  auto ref=std::make_shared<Json>(Json{{"type","assistant"},{"uuid","fixture-assistant"},
    {"message",{{"content",Json::array({Json{{"type","text"},
      {"text","finished"}}})},{"usage",{{"input_tokens",26},
      {"output_tokens",0}}},{"stop_reason",nullptr}}}});
  yield(ref);
  (*ref)["message"]["usage"]["input_tokens"]=42;
  (*ref)["message"]["stop_reason"]="end_turn";
 };
 params.getWorktreeResult=[](){return Json::object();};
 ba::runAsyncBatLifecycle(params,lifecycle);
 const Json& bat=field(field(state,"tasks"),"task");
 assert(field(bat,"status")=="completed");
 assert(field(bat,"notified")==true);
 assert(field(field(bat,"result"),"usage")["input_tokens"]==42);
 assert(field(field(bat,"messages")[0],"message")["usage"]["input_tokens"]==42);
 assert(field(field(bat,"progress"),"tokenCount")==26);
 assert((order==std::vector<std::string>{"unregister","evictOutput",
   "abortSpeculation","enqueue","clearSkills"}));
 ba::BatMissionRuntime runtime;
 runtime.bind(mission);
 ba::BatRegistrationServices registration;
 registration.state=&mission;
 registration.getMissionOutputPath=[](const std::string& id){return "/tmp/"+id+".txt";};
 registration.getBatTranscriptPath=[](const std::string& id){
  return "/tmp/"+id+".jsonl";};
 registration.initMissionOutputAsSymlink=[](const std::string&,const std::string&){};
 registration.registerCleanup=[&](const std::function<void()>&){
  return std::function<void()>([&]{order.push_back("registeredCleanup");});};
 registration.enqueueSdkEvent=[](const Json&){};
 ba::BatRegistrationParams spawn;
 spawn.batId="registered";spawn.description="registered job";
 spawn.poetry="prompt";spawn.selectedBat={{"agentType","worker"}};
 auto registered=ba::registerAsyncBat(spawn,registration);
 runtime.remember(spawn.batId,registered);
 ba::killAsyncBat(spawn.batId,mission);
 assert(registered.abortOwner->stop_requested());
 assert(field(field(field(state,"tasks"),"registered"),"status")=="killed");
 assert(order.back()=="evictOutput");
 std::cout<<"async lifecycle to local task state, notification and retained message passed\n";
}
