#include "bats/ResumeBat.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& object,const char* key){return value::property(object,key);}
static Json gadgetNames(const batbox::gadgets::Gadgets& values){
 Json names=Json::array();for(const auto* gadget:values)if(gadget)names.push_back(gadget->name);return names;
}
static std::future<std::optional<Json>> ready(std::optional<Json> value){
 std::promise<std::optional<Json>> promise;promise.set_value(std::move(value));return promise.get_future();
}
static Json run(const Json& item){
 Json trace=Json::array(),launched=nullptr;
 batbox::gadgets::Gadget read,assembled;read.name="Read";assembled.name="assembled";
 ba::ResumeParams params;
 params.batId=field(item,"agentId").get<std::string>();
 params.poetry=field(item,"prompt").get<std::string>();
 if(field(item,"invokingRequestId").is_string())
  params.invokingRequestId=field(item,"invokingRequestId").get<std::string>();
 params.gadgetUseContext.query.gadgets={&read};
 params.gadgetUseContext.query.data={{"options",{{"mainLoopModel","main-model"},
   {"tools",Json::array({Json{{"name","Read"}}})},
   {"agentDefinitions",{{"activeAgents",field(item,"activeAgents").is_array()?
     field(item,"activeAgents"):Json::array()}}},
   {"mcpClients",Json::array({"mcp"})},
   {"customSystemPrompt",Json::array({"custom"})},
   {"appendSystemPrompt",Json::array({"append"})}}},
   {"contentReplacementState",Json{{"parent",true}}},
   {"toolUseId","tool-use-id"}};
 if(!field(item,"renderedSystemPrompt").is_null())
  params.gadgetUseContext.query.data["renderedSystemPrompt"]=field(item,"renderedSystemPrompt");
 if(field(item,"nativeState")==true){
  params.gadgetUseContext.query.contentReplacementState=
    batbox::gadget_results::createContentReplacementState();
  params.gadgetUseContext.query.contentReplacementState->seenIds.add("parent-tool");
 }
 params.gadgetUseContext.getAppState=[&]{trace.push_back(Json::array({"getAppState"}));
  return field(item,"appState");};
 params.gadgetUseContext.setAppState=[](const auto&){};
 params.gadgetUseContext.setAppStateForMissions=[](const auto&){};
 ba::ResumeServices services;
 services.nowMillis=[]{return std::int64_t{1000};};
 services.getBatTranscript=[&](const std::string& id){trace.push_back(Json::array({"transcript",id}));
  const Json& transcript=field(item,"transcript");
  return ready(transcript.is_null()?std::nullopt:std::optional<Json>(transcript));};
 services.readBatMetadata=[&](const std::string& id){trace.push_back(Json::array({"metadata",id}));
  const Json& meta=field(item,"meta");
  return ready(meta.is_null()?std::nullopt:std::optional<Json>(meta));};
 services.filterUnresolvedGadgetUses=[&](const Json& messages){
  trace.push_back(Json::array({"filterUnresolved"}));return messages;};
 services.filterOrphanedThinkingOnlyMessages=[&](const Json& messages){
  trace.push_back(Json::array({"filterThinking"}));return messages;};
 services.filterWhitespaceOnlyAssistantMessages=[&](const Json& messages){
  trace.push_back(Json::array({"filterWhitespace"}));return messages;};
 services.reconstructForSubagentResume=[&](const Json& parent,const Json& messages,
      const Json& replacements){
   trace.push_back(Json::array({"reconstruct",parent,messages,replacements}));
   return Json{{"reconstructed",true}};};
 std::shared_ptr<batbox::gadget_results::ContentReplacementState> reconstructed;
 if(field(item,"nativeState")==true)
  services.reconstructForSubagentResumeIndigenous=[&](const auto& parent,const Json&,
      const Json&){
    if(parent!=params.gadgetUseContext.query.contentReplacementState)
      throw std::runtime_error("native reconstruction lost parent pointer");
    reconstructed=batbox::gadget_results::cloneContentReplacementState(*parent);
    reconstructed->seenIds.add("resumed-tool");
    return reconstructed;
  };
 services.isDirectory=[&](const std::string& path){trace.push_back(Json::array({"stat",path}));
  if(field(item,"statThrows")==true)throw std::runtime_error("missing");
  return field(item,"worktreeExists")==true;};
 services.touchWorktree=[&](const std::string& path){trace.push_back(Json::array({"touch",path}));};
 services.additionalWorkingDirectoryKeys=[](const Json& values){
  return values.is_array()?values:Json::array();};
 services.forkBatDefinition={{"agentType","fork"},{"builtIn",true}};
 services.generalPurposeBatDefinition={{"agentType","general-purpose"},{"builtIn",true}};
 services.createUserMessage=[&](const Json& args){trace.push_back(Json::array({"user",args}));
  return Json{{"type","user"},{"message",{{"content",field(args,"content")}}}};};
 services.getSystemPoetry=[&](const batbox::gadgets::Gadgets& gadgets,const Json& model,
      const Json& directories,const Json& clients){
   trace.push_back(Json::array({"systemPrompt",gadgetNames(gadgets),model,directories,clients}));
   return Json::array({"default prompt"});};
 services.buildEffectiveSystemPoetry=[&](const Json& main,const ba::BatContext&,
      const Json& custom,const Json& base,const Json& append){
   trace.push_back(Json::array({"effective",main,custom,base,append}));
   return field(item,"effectivePrompt").is_null()?Json::array({"effective prompt"}):
     field(item,"effectivePrompt");};
 services.getBatModel=[&](const Json& bat,const Json& main,const Json& override,
      const Json& mode){trace.push_back(Json::array({"model",bat,main,override,mode}));
   return std::string("resolved-model");};
 services.assembleGadgetPool=[&](const Json& permission,const Json& mcpGadgets){
  trace.push_back(Json::array({"assemble",permission,mcpGadgets}));
  return batbox::gadgets::Gadgets{&assembled};};
 services.isBuiltInBat=[&](const Json& bat){trace.push_back(Json::array({"builtIn",field(bat,"agentType")}));
  return field(bat,"builtIn")==true;};
 services.getQuerySourceForBat=[&](const std::string& type,bool builtIn){
  trace.push_back(Json::array({"querySource",type,builtIn}));
  return std::string("agent:")+type+":"+(builtIn?"true":"false");};
 services.registerAsyncBat=[&](const Json& args,const auto& rootSetAppState){
  trace.push_back(Json::array({"register",Json{{"agentId",field(args,"agentId")},
    {"description",field(args,"description")},{"prompt",field(args,"prompt")},
    {"selectedAgent",field(args,"selectedAgent")},{"toolUseId",field(args,"toolUseId")},
    {"rootShared",static_cast<bool>(rootSetAppState)}}}));
  return ba::RegisteredAsyncBat{"task-"+params.batId,std::make_shared<std::stop_source>()};};
 services.getParentFlightId=[&]{trace.push_back(Json::array({"parentSession"}));
  return std::string("parent-session");};
 services.runWithBatContext=[&](const Json& context,const auto& fn){
  trace.push_back(Json::array({"agentContext",context}));fn();};
 services.runWithCwdOverride=[&](const std::string& cwd,const auto& fn){
  trace.push_back(Json::array({"cwd",cwd}));fn();};
 services.isCoordinatorMode=[&]{trace.push_back(Json::array({"coordinator"}));
  return field(item,"coordinator")==true;};
 services.isForkSubagentEnabled=[&]{trace.push_back(Json::array({"forkEnabled"}));
  return field(item,"forkEnabled")==true;};
 services.getSdkBatProgressSummariesEnabled=[&]{trace.push_back(Json::array({"sdkSummaries"}));
  return field(item,"sdkSummaries")==true;};
 services.runAsyncBatLifecycle=[&](const ba::ResumeLaunch& launch){
  trace.push_back(Json::array({"lifecycle",Json{{"taskId",launch.mission.batId},
    {"metadata",launch.metadata},{"description",launch.description},
    {"agentIdForCleanup",launch.batIdForCleanup},
    {"enableSummarization",launch.enableSummarization},
    {"rootShared",static_cast<bool>(launch.rootSetAppState)}}}));
  const auto request=launch.makeRunBatParams([](const auto&){});
  launched={{"agentDefinition",request.batDefinition},
    {"promptMessages",request.poetryMessages},{"isAsync",request.isAsync},
    {"querySource",request.querySource},{"availableTools",gadgetNames(request.availableGadgets)},
    {"overrideSystemPrompt",field(request.override,"systemPrompt")},
    {"overrideAgentId",field(request.override,"agentId")},
    {"overrideAbort",request.overrideAbortOwner==launch.mission.abortOwner?"taskAbort":"missing"},
    {"useExactTools",request.useExactGadgets},
    {"worktreePath",request.worktreePath?Json(*request.worktreePath):Json(nullptr)},
    {"description",request.description?Json(*request.description):Json(nullptr)},
    {"contentReplacementState",request.contentReplacementState?
      *request.contentReplacementState:Json(nullptr)},
    {"forkContextMessages",request.forkContextMessages?
      *request.forkContextMessages:Json(nullptr)},
    {"hasCacheCallback",static_cast<bool>(request.onStashSafeParams)}};
  if(field(item,"nativeState")==true){
    launched["nativeReplacementIdentity"]=request.indigenousContentReplacementState==reconstructed;
    launched["nativeReplacementParentSeen"]=request.indigenousContentReplacementState &&
      request.indigenousContentReplacementState->seenIds.contains("parent-tool");
    launched["nativeReplacementResumedSeen"]=request.indigenousContentReplacementState &&
      request.indigenousContentReplacementState->seenIds.contains("resumed-tool");
  }
 };
 services.getMissionOutputPath=[&](const std::string& id){
  trace.push_back(Json::array({"outputPath",id}));return "/tasks/"+id+".txt";};
 try{
  const auto result=ba::resumeBatBackground(params,services);
  return Json{{"trace",trace},{"launched",launched},
   {"result",{{"agentId",result.batId},{"description",result.description},
     {"outputFile",result.outputFile}}},{"error",nullptr}};
 }catch(const std::exception& error){
  return Json{{"trace",trace},{"launched",launched},{"result",nullptr},
    {"error",error.what()}};}
}
int main(){std::string line;while(std::getline(std::cin,line)){
 try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
 catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}}
