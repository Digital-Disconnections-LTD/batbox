#include "bats/RunBat.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& value,const char* key){return value::property(value,key);}
static Json gadgetNames(const batbox::gadgets::Gadgets& gadgets){Json names=Json::array();
 for(const auto* gadget:gadgets)if(gadget)names.push_back(gadget->name);
 return names;}
static std::future<Json> ready(Json value){std::promise<Json> promise;
 promise.set_value(std::move(value));return promise.get_future();}
static Json run(const Json& item){
 Json trace=Json::array(),stash=nullptr;
 std::vector<batbox::gadgets::Gadget> owned,mcpOwned;
 const Json& available=field(item,"availableTools");
 for(const Json& name:available){batbox::gadgets::Gadget gadget;gadget.name=name.get<std::string>();
  owned.push_back(std::move(gadget));}
 const Json& mcpNames=field(item,"mcpTools");
 for(const Json& name:mcpNames){batbox::gadgets::Gadget gadget;gadget.name=name.get<std::string>();
  mcpOwned.push_back(std::move(gadget));}
 ba::RunBatParams params;
 params.batDefinition={{"agentType",field(item,"agentType")},
  {"source",field(item,"source")},{"hooks",field(item,"hooks")},
  {"skills",field(item,"skills")},
  {"criticalSystemReminder_EXPERIMENTAL",field(item,"reminder")}};
 params.isAsync=field(item,"isAsync")==true;
 params.useExactGadgets=field(item,"useExactTools")==true;
 params.preserveGadgetUseResults=field(item,"preserveToolUseResults")==true;
 params.querySource="agent:worker";
 if(!field(item,"promptOverride").is_null())
  params.override={{"systemPrompt",field(item,"promptOverride")}};
 if(!field(item,"replacementState").is_null())params.contentReplacementState=field(item,"replacementState");
 for(const auto& gadget:owned)params.availableGadgets.push_back(&gadget);
 Json parentOptions={{"isNonInteractiveSession",field(item,"parentNoninteractive")},
  {"appendSystemPrompt",Json::array({"append"})},
  {"tools",gadgetNames(params.availableGadgets)},
  {"debug",false},{"verbose",true},{"mainLoopModel","main"},
  {"thinkingConfig",{{"type","enabled"}}},
  {"mcpClients",Json::array({"parent-mcp"})},
  {"mcpResources",Json::array()},
  {"agentDefinitions",{{"activeAgents",Json::array()}}}};
 params.gadgetUseContext.query.data={{"options",parentOptions},
  {"readFileState",{{"path","parent"}}}};
 params.gadgetUseContext.query.gadgets=params.availableGadgets;
 params.gadgetUseContext.getAppState=[&]{return field(item,"appState");};
 params.gadgetUseContext.setAppState=[](const auto&){};
 params.gadgetUseContext.setAppStateForMissions=[](const auto&){};
 params.onStashSafeParams=[&](const std::shared_ptr<ba::StashSafeParams>& value){
  stash={{"systemPrompt",value->systemPoetry},{"userContext",value->userContext},
    {"systemContext",value->systemContext},
    {"forkContextMessages",value->forkContextMessages},
    {"agentId",field(value->gadgetUseContext.query.data,"agentId")}};
  trace.push_back(Json::array({"cache",stash}));};
 ba::RunBatPreflight preflight;
 preflight.batId="agent-id";
 preflight.initialMessages=field(item,"initialMessages");
 preflight.batReadFileState={{"cache","initial"}};
 preflight.appState=field(item,"appState");
 preflight.resolvedBatModel="resolved-model";
 preflight.userContext={{"user","context"}};
 preflight.systemContext={{"system","context"}};
 preflight.rootSetAppState=[](const auto&){};
 preflight.batGetAppState=[&]{return field(item,"appState");};
 ba::RunBatSetupServices services;
 services.resolveBatGadgets=[&](const Json& definition,
     const batbox::gadgets::Gadgets& gadgets,bool isAsync){
   trace.push_back(Json::array({"resolveTools",field(definition,"agentType"),
     gadgetNames(gadgets),isAsync}));
   batbox::gadgets::Gadgets result;for(const auto* gadget:gadgets)if(gadget->name!="Hidden")result.push_back(gadget);
   return result;};
 services.additionalWorkingDirectoryKeys=[](const Json& dirs){return dirs;};
 services.poetry.defaultBatPoetry="default prompt";
 services.poetry.getBatPoetry=[&](const Json&,const ba::BatContext&){
  trace.push_back(Json::array({"agentPrompt"}));return Json("agent prompt");};
 services.poetry.enhanceWithEnvDetails=[&](const Json& poetryEntries,const std::string& model,
     const Json& dirs,const Json& enabled){
   trace.push_back(Json::array({"enhance",poetryEntries,model,dirs,enabled}));return poetryEntries;};
 services.asSystemPoetry=[&](const Json& poetry){
  trace.push_back(Json::array({"asSystemPrompt",poetry}));return poetry;};
 services.executeSubagentStartHooks=[&](const std::string& id,
     const std::string& type,std::stop_token){
   trace.push_back(Json::array({"startHooks",id,type}));return field(item,"hookResults");};
 services.createAttachmentMessage=[&](const Json& attachment){
  trace.push_back(Json::array({"attachment",attachment}));
  return Json{{"type","attachment"},{"attachment",attachment}};};
 services.randomUUID=[&]{trace.push_back(Json::array({"uuid"}));return std::string("uuid");};
 services.isRestrictedToPluginOnly=[&](const std::string& name){
  trace.push_back(Json::array({"restricted",name}));return field(item,"restricted")==true;};
 services.isSourceAdminTrusted=[&](const Json& source){
  trace.push_back(Json::array({"trusted",source}));return field(item,"trusted")==true;};
 services.registerFrontmatterHooks=[&](const auto&,const std::string& id,
     const Json& hooks,const std::string& label,bool bat){
   trace.push_back(Json::array({"registerHooks",id,hooks,label,bat}));};
 services.getProjectRoot=[&]{trace.push_back(Json::array({"projectRoot"}));return "/project";};
 services.getSkillGadgetCommands=[&](const std::string& root){
  trace.push_back(Json::array({"skillCommands",root}));return field(item,"skillCommands");};
 services.hasCommand=[&](const std::string& name,const Json& all){
  trace.push_back(Json::array({"hasCommand",name}));
  for(const Json& command:all)if(field(command,"name")==name)return true;
  return false;};
 services.getCommand=[&](const std::string& name,const Json& all){
  trace.push_back(Json::array({"getCommand",name}));
  for(const Json& command:all)if(field(command,"name")==name)return command;
  return Json(nullptr);};
 services.getSkillPoetry=[&](const Json& skill,const std::string& args,const ba::BatContext&){
  const std::string name=field(skill,"name").get<std::string>();
  trace.push_back(Json::array({"skillPrompt",name,args}));
  return ready(Json::array({Json{{"type","text"},{"text","content:"+name}}}));};
 services.formatSkillLoadingMetadata=[&](const std::string& name,const Json& progress){
  trace.push_back(Json::array({"skillMetadata",name,progress}));return "loading:"+name;};
 services.createUserMessage=[&](const Json& args){trace.push_back(Json::array({"user",args}));
  return Json{{"type","user"},{"message",{{"content",field(args,"content")}}},
   {"isMeta",field(args,"isMeta")}};};
 services.initializeBatMcpServers=[&](const Json& def,const Json& clients){
  trace.push_back(Json::array({"mcp",field(def,"agentType"),clients}));
  ba::BatMcpSetup result;result.clients=Json::array({"merged"});
  for(const auto& gadget:mcpOwned)result.gadgets.push_back(&gadget);
  result.cleanup=[]{};return result;};
 services.subagentContext.cloneFileStateStash=[&](const Json& state){
  trace.push_back(Json::array({"cloneFile",state}));return Json{{"cloned",state}};};
 services.subagentContext.cloneContentReplacementState=[](const Json& state){return state;};
 services.subagentContext.createDenialTrackingState=[&]{
  trace.push_back(Json::array({"denial"}));return Json::object();};
 services.subagentContext.createBatId=[&](const auto&){
  trace.push_back(Json::array({"agentId"}));return std::string("unused");};
 services.subagentContext.randomUUID=services.randomUUID;
 try{
  const auto result=ba::setupRunBat(params,preflight,services);
  Json options=result.options;
  Json childOptions=field(result.context.query.data,"options");
  return Json{{"trace",trace},{"result",{{"initialMessages",result.initialMessages},
    {"agentSystemPrompt",result.systemPoetry},{"agentOptions",options},
    {"allTools",gadgetNames(result.context.query.gadgets)},
    {"child",{{"agentId",field(result.context.query.data,"agentId")},
      {"agentType",field(result.context.query.data,"agentType")},
      {"readFileState",field(result.context.query.data,"readFileState")},
      {"options",childOptions},{"messages",field(result.context.query.data,"messages")},
      {"preserveToolUseResults",field(result.context.query.data,"preserveToolUseResults")}}}}},
    {"cache",stash},{"error",nullptr}};
 }catch(const std::exception& error){return Json{{"trace",trace},{"result",nullptr},
   {"cache",stash},{"error",error.what()}};}
}
int main(){std::string line;while(std::getline(std::cin,line)){
 try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
 catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}}
