#include "bats/RunBat.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& object,const char* key){return value::property(object,key);}
static bool flag(const Json& item,const char* key){return field(item,key)==true;}
static Json run(const Json& item){
 Json trace=Json::array(),yielded=Json::array();
 std::vector<batbox::query::MessageRef> yieldedRefs;
 Json state={{"todos",{{"agent-id",Json::array()},{"other",Json::array({1})}}}};
 ba::RunBatParams params;
 params.batDefinition={{"agentType","worker"},{"hooks",field(item,"hooks")},{"maxTurns",9}};
 params.querySource="agent:worker";
 if(field(item,"maxTurns").is_number_integer())params.maxTurns=field(item,"maxTurns").get<int>();
 if(field(item,"worktreePath").is_string())params.worktreePath=field(item,"worktreePath").get<std::string>();
 if(field(item,"description").is_string())params.description=field(item,"description").get<std::string>();
 params.onQueryProgress=[&]{trace.push_back(Json::array({"progress"}));};
 params.builtInCallback=[&]{trace.push_back(Json::array({"callback"}));};
 params.gadgetUseContext.pushApiMetricsEntry=[&](const Json& value){
  trace.push_back(Json::array({"metric",value}));};
 params.gadgetUseContext.getAppState=[&]{return state;};
 ba::RunBatPreflight preflight;
 preflight.rootSetAppState=[&](const std::function<Json(const Json&)>& updater){
  trace.push_back(Json::array({"setState"}));state=updater(state);};
 ba::RunBatPrepared prepared;
 prepared.batId="agent-id";
 prepared.initialMessages=field(item,"initialMessages");
 prepared.systemPoetry=Json::array({"prompt"});
 prepared.userContext={{"user","context"}};
 prepared.systemContext={{"system","context"}};
 prepared.context.query.data={{"readFileState",Json::object()},
  {"messages",prepared.initialMessages}};
 prepared.exposedStashSafeParams=std::make_shared<ba::StashSafeParams>();
 prepared.exposedStashSafeParams->forkContextMessages=prepared.initialMessages;
 prepared.exposedStashSafeParams->gadgetUseContext.query.data={{"messages",prepared.initialMessages}};
 prepared.mcpCleanup=[&]{trace.push_back(Json::array({"mcpCleanup"}));
  if(flag(item,"cleanupFailure"))throw std::runtime_error("cleanup failed");};
 if(flag(item,"aborted")){
  prepared.abortOwner=std::make_shared<std::stop_source>();
  prepared.context.query.stop=prepared.abortOwner->get_token();
  prepared.abortOwner->request_stop();
 }
 ba::RunBatLoopServices services;
 services.enqueueInitialTranscript=[&](const Json& messages,const std::string& id){
  trace.push_back(Json::array({"record",messages,id,nullptr}));};
 services.enqueueBatMetadata=[&](const std::string& id,const Json& metadata){
  trace.push_back(Json::array({"metadata",id,metadata}));};
 services.recordSidechainTranscript=[&](const Json& messages,const std::string& id,
    const std::optional<std::string>& parent){
  trace.push_back(Json::array({"record",messages,id,parent?Json(*parent):Json(nullptr)}));
  if(flag(item,"recordFailure"))throw std::runtime_error("record failed");};
 services.makeEngineForSubagent=[](const ba::BatContext&,
   const std::function<std::future<Json>(const Json&)>&){return batbox::query::EngineServices{};};
 services.runQuery=[&](const batbox::query::Params& request,
   batbox::query::EngineServices&,const batbox::query::Yield& consume){
  trace.push_back(Json::array({"query",request.messages,request.systemPoetry,
   request.userContext,request.systemContext,request.querySource,
   request.maxTurns?Json(*request.maxTurns):Json(nullptr)}));
  const Json& events=field(item,"events");
  batbox::query::MessageRef lastAssistant;
  if(events.is_array())for(const Json& message:events){
   auto ref=std::make_shared<Json>(message);
   if(field(*ref,"type")=="assistant")lastAssistant=ref;
   if(flag(item,"lateMutation")&&field(*ref,"type")=="stream_event"&&
      field(field(*ref,"event"),"type")=="message_delta"&&lastAssistant){
    (*lastAssistant)["message"]["usage"]=field(field(*ref,"event"),"usage");
    (*lastAssistant)["message"]["stop_reason"]=
      field(field(field(*ref,"event"),"delta"),"stop_reason");
   }
   trace.push_back(Json::array({"next",field(*ref,"type")}));consume(ref);}
  if(flag(item,"queryFailure"))throw std::runtime_error("query failed");
  return Json::object();};
 services.isBuiltInBat=[&](const Json& definition){
  trace.push_back(Json::array({"builtIn",field(definition,"agentType")}));
  return flag(item,"builtIn");};
 services.clearFlightHooks=[&](const auto&,const std::string& id){
  trace.push_back(Json::array({"clearHooks",id}));};
 services.feature=[&](const std::string& name){
  trace.push_back(Json::array({"feature",name}));
  return name=="MONITOR_TOOL"?flag(item,"monitorTool"):flag(item,"cacheTracking");};
 services.cleanupBatTracking=[&](const std::string& id){
  trace.push_back(Json::array({"cleanupTracking",id}));};
 services.clearReadFileStateStash=[&](Json&){trace.push_back(Json::array({"clearFile"}));};
 services.clearBatTranscriptSubdir=[&](const std::string& id){
  trace.push_back(Json::array({"clearSubdir",id}));};
 services.killShellMissionsForBat=[&](const std::string& id,const auto&,const auto&){
  trace.push_back(Json::array({"killShell",id}));};
 services.killMonitorMcpMissionsForBat=[&](const std::string& id,const auto&,const auto&){
  trace.push_back(Json::array({"killMonitor",id}));};
 Json error=nullptr;
 try{ba::runPreparedBat(params,preflight,prepared,services,
  batbox::query::Yield::withReferences([&](const batbox::query::MessageRef& message){
   trace.push_back(Json::array({"yield",field(*message,"type")}));
   yieldedRefs.push_back(message);}));}
 catch(const std::exception& exception){error=exception.what();}
 for(const auto& ref:yieldedRefs)yielded.push_back(*ref);
 return Json{{"trace",trace},{"yielded",yielded},
  {"error",error},{"state",state},
  {"remainingInitialMessages",prepared.initialMessages},
  {"cacheForkMessages",prepared.exposedStashSafeParams->forkContextMessages},
  {"contextMessages",prepared.context.query.data["messages"]}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{
 std::cout<<value::stringify(run(value::parse(line)))<<'\n';
 }catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}}
