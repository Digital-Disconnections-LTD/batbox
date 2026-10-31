#include "bats/RunBat.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& object,const char* key){return value::property(object,key);}
static bool flag(const Json& item,const char* key){return field(item,key)==true;}
static Json run(const Json& item){
 Json trace=Json::array(),yielded=Json::array();
 Json state={{"toolPermissionContext",{{"mode","default"},
  {"alwaysAllowRules",{{"cliArg",Json::array()},{"session",Json::array()}}},
  {"additionalWorkingDirectories",Json::object()}}},
  {"effortValue","low"},{"todos",{{"agent-id",Json::array()},
   {"other",Json::array({1})}}}};
 auto rootSet=[&](const std::function<Json(const Json&)>& update){
  trace.push_back(Json::array({"setState"}));state=update(state);};
 ba::RunBatParams params;
 params.batDefinition={{"agentType",field(item,"agentType").is_string()?
   field(item,"agentType"):Json("worker")},{"source","built-in"},
   {"hooks",field(item,"hooks")},{"skills",Json::array()},{"maxTurns",9}};
 params.poetryMessages=field(item,"initialMessages");
 params.isAsync=flag(item,"isAsync");
 params.querySource="agent:worker";
 if(field(item,"maxTurns").is_number_integer())params.maxTurns=field(item,"maxTurns").get<int>();
 if(field(item,"worktreePath").is_string())params.worktreePath=field(item,"worktreePath").get<std::string>();
 if(field(item,"description").is_string())params.description=field(item,"description").get<std::string>();
 if(field(item,"transcriptSubdir").is_string())params.transcriptSubdir=field(item,"transcriptSubdir").get<std::string>();
 params.onQueryProgress=[&]{trace.push_back(Json::array({"progress"}));};
 params.builtInCallback=[&]{trace.push_back(Json::array({"callback"}));};
 std::shared_ptr<ba::StashSafeParams> stash;
 params.onStashSafeParams=[&](const std::shared_ptr<ba::StashSafeParams>& value){stash=value;};
 batbox::gadgets::Gadget read;read.name="Read";params.availableGadgets={&read};
 params.gadgetUseContext.query.gadgets={&read};
 params.gadgetUseContext.query.data={{"options",{{"mainLoopModel","main"},
   {"mcpClients",Json::array()},{"mcpResources",Json::array()},
   {"agentDefinitions",{{"activeAgents",Json::array()}}},
   {"appendSystemPrompt",Json::array()},{"debug",false},{"verbose",false}}},
   {"readFileState",{{"path","parent"}}}};
 params.gadgetUseContext.getAppState=[&]{return state;};
 params.gadgetUseContext.setAppState=rootSet;
 params.gadgetUseContext.setAppStateForMissions=rootSet;
 params.gadgetUseContext.pushApiMetricsEntry=[&](const Json& value){
  trace.push_back(Json::array({"metric",value}));};
 params.gadgetUseContext.abortOwner=std::make_shared<std::stop_source>();
 params.gadgetUseContext.query.stop=params.gadgetUseContext.abortOwner->get_token();
 if(flag(item,"aborted"))params.gadgetUseContext.abortOwner->request_stop();
 ba::RunBatPreflightServices preflight;
 preflight.getBatModel=[&](const Json&,const Json&,
   const std::optional<std::string>&,const Json&){trace.push_back(Json::array({"model"}));
   return std::string("resolved-model");};
 preflight.createBatId=[&]{trace.push_back(Json::array({"agentId"}));
  return std::string("agent-id");};
 preflight.setBatTranscriptSubdir=[&](const std::string& id,const std::string& subdir){
  trace.push_back(Json::array({"setSubdir",id,subdir}));};
 preflight.cloneFileStateStash=[](const Json& value){return Json{{"cloned",value}};};
 preflight.createFileStateStashWithSizeLimit=[](std::size_t){return Json::object();};
 preflight.readFileStateStashSize=123;
 preflight.getUserContext=[](){return Json{{"user","context"}};};
 preflight.getSystemContext=[](){return Json{{"system","context"}};};
 preflight.getFeatureValue=[](const std::string&,bool){return false;};
 preflight.feature=[&](const std::string& name){
  trace.push_back(Json::array({"feature",name}));return flag(item,"cacheTracking");};
 ba::RunBatSetupServices setup;
 setup.resolveBatGadgets=[](const Json&,const batbox::gadgets::Gadgets& all,bool){return all;};
 setup.additionalWorkingDirectoryKeys=[](const Json&){return Json::array();};
 setup.poetry.getBatPoetry=[](const Json&,const ba::BatContext&){return Json::array({"prompt"});};
 setup.poetry.defaultBatPoetry="default prompt";
 setup.poetry.enhanceWithEnvDetails=[](const Json& poetryEntries,const std::string&,
   const Json&,const Json&){return poetryEntries;};
 setup.asSystemPoetry=[](const Json& value){return value;};
 setup.executeSubagentStartHooks=[](const std::string&,const std::string&,
   std::stop_token){return Json::array();};
 setup.isRestrictedToPluginOnly=[](const std::string&){return false;};
 setup.isSourceAdminTrusted=[](const Json&){return false;};
 setup.registerFrontmatterHooks=[](const auto&,const std::string&,
   const Json&,const std::string&,bool){};
 setup.initializeBatMcpServers=[&](const Json&,const Json&){
  ba::BatMcpSetup result;result.clients=Json::array({"merged"});
  result.cleanup=[&]{trace.push_back(Json::array({"mcpCleanup"}));};
  return result;};
 setup.subagentContext.cloneFileStateStash=[](const Json&){return Json::object();};
 setup.subagentContext.createDenialTrackingState=[](){return Json::object();};
 setup.subagentContext.createBatId=[](const std::optional<std::string>&){
  return std::string("unused");};
 setup.subagentContext.randomUUID=[](){return std::string("chain-id");};
 ba::RunBatLoopServices loop;
 loop.enqueueInitialTranscript=[&](const Json& messages,const std::string& id){
  trace.push_back(Json::array({"record",messages,id,nullptr}));};
 loop.enqueueBatMetadata=[&](const std::string& id,const Json& metadata){
  trace.push_back(Json::array({"metadata",id,metadata}));};
 loop.recordSidechainTranscript=[&](const Json& messages,const std::string& id,
   const std::optional<std::string>& parent){
  trace.push_back(Json::array({"record",messages,id,parent?Json(*parent):Json(nullptr)}));
  if(flag(item,"recordFailure"))throw std::runtime_error("record failed");};
 loop.makeEngineForSubagent=[](const ba::BatContext&,
   const std::function<std::future<Json>(const Json&)>&){return batbox::query::EngineServices{};};
 loop.runQuery=[&](const batbox::query::Params& request,batbox::query::EngineServices&,
   const batbox::query::Yield& yield){
  trace.push_back(Json::array({"query",request.messages,request.systemPoetry,
   request.userContext,request.systemContext,request.querySource,
   request.maxTurns?Json(*request.maxTurns):Json(nullptr)}));
  batbox::query::MessageRef lastAssistant;
  const Json& events=field(item,"events");
  if(events.is_array())for(const Json& message:events){
   auto ref=std::make_shared<Json>(message);
   if(field(*ref,"type")=="assistant")lastAssistant=ref;
   if(flag(item,"lateMutation")&&field(*ref,"type")=="stream_event"&&
    field(field(*ref,"event"),"type")=="message_delta"&&lastAssistant){
    (*lastAssistant)["message"]["usage"]=field(field(*ref,"event"),"usage");
    (*lastAssistant)["message"]["stop_reason"]=
      field(field(field(*ref,"event"),"delta"),"stop_reason");}
   trace.push_back(Json::array({"next",field(*ref,"type")}));yield(ref);}
  if(flag(item,"queryFailure"))throw std::runtime_error("query failed");
  return Json::object();};
 loop.isBuiltInBat=[&](const Json& definition){
  trace.push_back(Json::array({"builtIn",field(definition,"agentType")}));
  return flag(item,"builtIn");};
 loop.clearFlightHooks=[&](const auto&,const std::string& id){
  trace.push_back(Json::array({"clearHooks",id}));};
 loop.feature=[&](const std::string& name){trace.push_back(Json::array({"feature",name}));
  return name=="MONITOR_TOOL"?flag(item,"monitorTool"):flag(item,"cacheTracking");};
 loop.cleanupBatTracking=[&](const std::string& id){
  trace.push_back(Json::array({"cleanupTracking",id}));};
 loop.clearReadFileStateStash=[&](Json&){trace.push_back(Json::array({"clearFile"}));};
 loop.clearBatTranscriptSubdir=[&](const std::string& id){
  trace.push_back(Json::array({"clearSubdir",id}));};
 loop.killShellMissionsForBat=[&](const std::string& id,const auto&,const auto&){
  trace.push_back(Json::array({"killShell",id}));};
 loop.killMonitorMcpMissionsForBat=[&](const std::string& id,const auto&,const auto&){
  trace.push_back(Json::array({"killMonitor",id}));};
 std::vector<batbox::query::MessageRef> refs;
 Json error=nullptr;
 try{ba::runBat(params,preflight,setup,loop,
  batbox::query::Yield::withReferences([&](const batbox::query::MessageRef& ref){
   trace.push_back(Json::array({"yield",field(*ref,"type")}));refs.push_back(ref);}));}
 catch(const std::exception& exception){error=exception.what();}
 for(const auto& ref:refs)yielded.push_back(*ref);
 if(state["toolPermissionContext"].is_object())
  state["toolPermissionContext"].erase("additionalWorkingDirectories");
 return Json{{"trace",trace},{"yielded",yielded},{"error",error},
  {"state",state},{"cacheForkMessages",stash?stash->forkContextMessages:Json(nullptr)}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{
 std::cout<<value::stringify(run(value::parse(line)))<<'\n';
 }catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}}
