#include "application_bats/IndigenousBatApplication.hpp"
#include "runtime/Version.hpp"
#include "application_bats/GeneralPurpose.hpp"
#include "application_bats/Memory.hpp"
#include "application_bats/BatMcp.hpp"
#include "application_skills/Provider.hpp"
#include "application_hooks/Provider.hpp"
#include "interactive/background/NotificationAttachments.hpp"
#include "application/Async.hpp"
#include "bats/BatGadgets.hpp"
#include "runtime/Platform.hpp"
#include "settings/PermissionRules.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::application_bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string string(const Json& value){return value.is_string()?value.get<std::string>():"";}
Json directoryKeys(const Json& directories){
  Json keys=Json::array();for(const auto& key:value::objectKeys(directories))keys.push_back(key);
  return keys;
}
bool namesAgent(const Json& spec){
  if(!spec.is_string())return false;
  const auto& text=spec.get_ref<const std::string&>();
  return text=="Agent"||text.starts_with("Agent(");
}
bool listsAgent(const Json& specs){
  if(specs.is_array())for(const auto& spec:specs)if(namesAgent(spec))return true;
  return false;
}
// Whether a Bat definition would take the Agent Gadget when its depth allows.
bool definitionTakesAgent(const Json& definition){
  if(listsAgent(field(definition,"disallowedTools")))return false;
  const auto& tools=field(definition,"tools");
  if(!definition.is_object()||!definition.contains("tools"))return true;
  if(tools.is_array()&&tools.size()==1&&tools[0]=="*")return true;
  return listsAgent(tools);
}
std::optional<std::size_t> nestDepth(const Json& definition){
  const auto& depth=field(definition,"nestDepth");
  if(depth.is_number_unsigned())return depth.get<std::size_t>();
  if(depth.is_number_integer()&&depth.get<long long>()>0)
    return static_cast<std::size_t>(depth.get<long long>());
  return std::nullopt;
}
}
std::string IndigenousBatApplication::nestingPoetry(std::size_t depth,
    const Json& definition)const{
  const auto limit=inputs_.config.maxBatDepth;
  if(!mayLaunchBats(depth))
    return "You cannot launch Bats: this Bat is at the deepest level Bats may nest "
      "(max_bat_depth "+std::to_string(limit)+"), so the Agent Gadget is not "
      "available to you. Do the work yourself with the Gadgets you have.";
  if(!definitionTakesAgent(definition))return {};
  const auto below=limit-depth;
  return "You can launch Bats of your own with the Agent Gadget, nested up to "+
    std::to_string(below)+(below==1?" level":" levels")+" below you. They run in "
    "the foreground: you wait for each result, run_in_background is ignored, and "
    "a Bat you launch stops when you finish or are stopped. Its result comes back "
    "to you as the Agent result, so do not launch a second Bat for work one has "
    "already done.";
}
void IndigenousBatApplication::prepareNestedRun(bats::RunBatParams& params)const{
  const auto& id=field(params.override,"agentId");
  if(!id.is_string())return;
  const auto node=batNode(id.get<std::string>());
  if(!node)return;
  if(!params.batDefinition.is_object())params.batDefinition=Json::object();
  params.batDefinition["nestDepth"]=node->depth;
  auto& data=params.gadgetUseContext.query.data;
  if(!data.is_object())return;
  if(mayLaunchBats(node->depth)){
    if(data.contains("unavailableGadgets")&&data["unavailableGadgets"].is_object())
      data["unavailableGadgets"].erase("Agent");
    return;
  }
  if(!data.contains("unavailableGadgets")||!data["unavailableGadgets"].is_object())
    data["unavailableGadgets"]=Json::object();
  data["unavailableGadgets"]["Agent"]="Agent is not available to this Bat: it is "
    "at the maximum Bat depth (max_bat_depth "+std::to_string(inputs_.config.maxBatDepth)+
    "), so it cannot launch Bats. Do this work yourself.";
}
bats::RunBatPreflightServices IndigenousBatApplication::makeRunPreflight(){
  bats::RunBatPreflightServices services;
  const auto model=inputs_.model;
  const auto fallback=inputs_.config.model;
  services.getBatModel=[model,fallback](const Json& definition,const Json& parent,
      const std::optional<std::string>& override,const Json&){
    Json chosen=override?Json(*override):definition;
    if(!chosen.is_string())chosen=parent;
    if(!chosen.is_string())return fallback;
    const auto name=chosen.get<std::string>();
    if((name=="fast"||name=="haiku")&&
        !model->config().smallModel.empty())
      return model->config().smallModel;
    if(name=="inherit")return parent.is_string()?parent.get<std::string>():fallback;
    if(name=="standard"||name=="premium"||name=="sonnet")return fallback;
    return model->findProfile(name)?name:fallback;
  };
  services.createBatId=inputs_.messages->randomUUID;
  auto weak=weak_from_this();
  services.setBatTranscriptSubdir=[weak](const std::string& id,
      const std::string& subdir){if(auto self=weak.lock()){
        auto paths=self->pathsForBat(id);
        std::lock_guard lock(self->pathsMutex_);
        paths->setBatTranscriptSubdir(id,subdir);
      }};
  services.cloneFileStateStash=[](const Json& stash){return stash;};
  services.createFileStateStashWithSizeLimit=[](std::size_t){return Json::object();};
  const auto dispatcher=inputs_.owner.dispatcher();
  if(inputs_.getUserContextAsync)
    services.getUserContext=[get=inputs_.getUserContextAsync,dispatcher]{
      return application::awaitOnOwner<Json>(dispatcher,get);
    };
  else services.getUserContext=[get=inputs_.getUserContext,dispatcher]{
    return application::onOwnerAndWait<Json>(dispatcher,get);
  };
  if(inputs_.getSystemContextAsync)
    services.getSystemContext=[get=inputs_.getSystemContextAsync,dispatcher]{
      return application::awaitOnOwner<Json>(dispatcher,get);
    };
  else services.getSystemContext=[get=inputs_.getSystemContext,dispatcher]{
    return application::onOwnerAndWait<Json>(dispatcher,get);
  };
  return services;
}
bats::RunBatSetupServices IndigenousBatApplication::makeRunSetup(){
  bats::RunBatSetupServices services;
  const auto dispatcher=inputs_.owner.dispatcher();
  auto weak=weak_from_this();
  bats::BatGadgetFilterImports filters;
  filters.allBatDisallowed={"Agent","TaskOutput","TaskStop","AskUserQuestion",
    "EnterPlanMode","ExitPlanMode"};
  filters.customBatDisallowed=filters.allBatDisallowed;
  filters.asyncBatAllowed={"Read","Grep","Glob","Bash","Write","Edit",
    "WebSearch","WebFetch","Skill","SyntheticOutput"};
  filters.exitPlanModeGadgetName="ExitPlanMode";
  filters.batGadgetName="Agent";
  filters.gadgetMatchesName=gadgets::gadgetMatchesName;
  filters.permissionRuleValueFromString=[](const std::string& spec){
    const auto parsed=settings::permissionRuleValueFromString(spec,
      settings::PermissionRuleServices{});
    Json value={{"toolName",parsed.gadgetName}};
    if(parsed.ruleContent)value["ruleContent"]=*parsed.ruleContent;
    return value;
  };
  // Bats below the depth limit take the Agent Gadget, in foreground and
  // resumed (async) runs alike.
  auto nestedFilters=filters;
  nestedFilters.allBatDisallowed.erase("Agent");
  nestedFilters.customBatDisallowed.erase("Agent");
  nestedFilters.asyncBatAllowed.insert("Agent");
  services.resolveBatGadgets=[filters,nestedFilters,weak](const Json& definition,
      const gadgets::Gadgets& available,bool async)mutable{
    const auto self=weak.lock();
    const auto depth=nestDepth(definition);
    const bool nests=self&&depth&&self->mayLaunchBats(*depth);
    Json effective=definition;
    const auto& scope=field(definition,"memory");
    if(scope.is_string()&&field(definition,"tools").is_array())
      if(self)
        if(batMemoryEnabled(self->inputs_.config,self->appState(),
            self->inputs_.projectRoot)){
          auto& configured=effective["tools"];
          for(const std::string name:{"Write","Edit","Read"})
            if(std::find(configured.begin(),configured.end(),Json(name))==
                configured.end())configured.push_back(name);
        }
    auto resolved=bats::resolveBatGadgets(effective,available,async,false,
      nests?nestedFilters:filters).resolvedGadgets;
    // An explicit tools list names Agent for the main thread only; a Bat that
    // may nest and lists it takes it here.
    if(nests&&listsAgent(field(effective,"tools"))&&
        !listsAgent(field(effective,"disallowedTools"))&&
        !gadgets::findGadgetByName(resolved,"Agent"))
      if(const auto* agent=gadgets::findGadgetByName(available,"Agent"))
        resolved.push_back(agent);
    return resolved;
  };
  services.additionalWorkingDirectoryKeys=directoryKeys;
  services.poetry.getBatPoetry=[weak](const Json& definition,
      const bats::BatContext&){
    if(auto self=weak.lock()){
      auto text=batSystemPoetryText(definition,generalPurposePoetry(),
        self->inputs_.config,self->appState(),self->inputs_.projectRoot,
        [self](const std::string& value){
          if(self->inputs_.logError)
            self->inputs_.logError(std::runtime_error(value));});
      if(const auto depth=nestDepth(definition)){
        const auto note=self->nestingPoetry(*depth,definition);
        if(!note.empty())text+="\n\n"+note;
      }
      return Json(std::move(text));
    }
    throw std::runtime_error("Agent prompt owner closed");
  };
  services.poetry.enhanceWithEnvDetails=inputs_.enhanceSystemPoetry;
  services.poetry.defaultBatPoetry=generalPurposePoetry();
  services.asSystemPoetry=[](const Json& value){return value;};
  services.executeSubagentStartHooks=[hooks=inputs_.hooks,dispatcher](
      const std::string& id,const std::string& type,std::stop_token signal){
    if(!hooks)return Json::array();
    const auto rows=application::awaitOnOwner<std::vector<Json>>(dispatcher,
      [hooks,id,type,signal]{
        return hooks->run("SubagentStart",
          Json{{"agent_id",id},{"agent_type",type}},type,signal);});
    return Json(rows);};
  services.createAttachmentMessage=[messages=inputs_.messages](
      const Json& value)->Json{
    return *interactive::background::createAttachmentMessage(value,
      messages->randomUUID,messages->nowISO);};
  services.randomUUID=inputs_.messages->randomUUID;
  services.isRestrictedToPluginOnly=[](const std::string&){return false;};
  services.isSourceAdminTrusted=[](const Json&){return false;};
  services.registerFrontmatterHooks=[hooks=inputs_.hooks,dispatcher](
      const auto&,const std::string& id,const Json& configured,
      const std::string& label,bool bat){
    if(!hooks)throw std::runtime_error("Agent hook provider is unavailable");
    application::onOwnerAndWait<void>(dispatcher,
      [hooks,id,configured,label,bat]{
        hooks->registerFrontmatterHooks(id,configured,label,bat);});};
  services.getProjectRoot=[cwd=inputs_.config.cwd]{return cwd.string();};
  services.getSkillGadgetCommands=[skills=inputs_.skills,dispatcher](
      const std::string&)->Json{
    if(!skills)throw std::runtime_error("Configured agent skills need a native skill provider");
    return application::onOwnerAndWait<Json>(dispatcher,[skills]{
      Json commands=Json::array();
      for(const auto& command:skills->allCommands())commands.push_back(command);
      return commands;});};
  services.hasCommand=[](const std::string& name,const Json& all){
    if(!all.is_array())return false;
    for(const auto& command:all)if(field(command,"name")==name)return true;
    return false;};
  services.getCommand=[](const std::string& name,const Json& all){
    if(all.is_array())for(const auto& command:all)
      if(field(command,"name")==name)return command;
    return Json();};
  services.getSkillPoetry=[skills=inputs_.skills](const Json& command,
      const std::string& args,const bats::BatContext& context){
    std::promise<Json> promise;auto future=promise.get_future();
    try{
      if(!skills)throw std::runtime_error("Agent skill provider is unavailable");
      promise.set_value(skills->poetryForCommand(command,args,context));
    }catch(...){promise.set_exception(std::current_exception());}
    return future;};
  services.formatSkillLoadingMetadata=[](const std::string& name,
      const Json& progress){
    return skills::process::formatSkillLoadingMetadata(name,
      progress.is_string()?progress.get<std::string>():"loading");};
  services.createUserMessage=[messages=inputs_.messages](const Json& args){
    return messages::createUserMessage(args,*messages);
  };
  services.initializeBatMcpServers=[get=inputs_.batMcpRuntime,
      dispatcher,log=inputs_.logError](const Json& definition,
      const Json& clients){
    auto diagnostic=[log](const std::string& message){if(log)
      log(std::runtime_error(message));};
    return application_bats::initializeBatMcpServers(definition,clients,
      get?get():nullptr,dispatcher,diagnostic);
  };
  services.subagentContext.cloneFileStateStash=[](const Json& stash){return stash;};
  services.subagentContext.clearFileStateStash=[](Json& stash){
    if(stash.is_object()||stash.is_array())stash.clear();
  };
  services.subagentContext.cloneContentReplacementState=[](const Json& value){
    return value;
  };
  services.subagentContext.createDenialTrackingState=[](){return Json::object();};
  services.subagentContext.createBatId=[uuid=inputs_.messages->randomUUID](
      const std::optional<std::string>& requested){
    return requested?*requested:uuid();
  };
  services.subagentContext.randomUUID=inputs_.messages->randomUUID;
  return services;
}
void IndigenousBatApplication::recordBatMessages(const Json& messages,
    const std::string& batId,const std::optional<std::string>& parent,bool awaitWrites){
  if(!messages.is_array())return;
  std::optional<std::string> parentId=parent;
  const auto target=transcriptPath(batId);
  const auto flight=parentFlightId(batId);
  for(const auto& message:messages){
    if(!message.is_object())continue;
    const auto& uuid=field(message,"uuid");
    Json entry=message;
    entry["isSidechain"]=true;
    entry["agentId"]=batId;
    entry["sessionId"]=flight;
    entry["cwd"]=inputs_.config.cwd.string();
    entry["entrypoint"]="cli";
    entry["userType"]="external";
    entry["version"]=runtime::batboxVersion();
    std::shared_future<void> written;
    {
      std::lock_guard lock(conversationMutex_);
      if(uuid.is_string()&&writtenMessageIds_[batId].contains(uuid.get<std::string>()))continue;
      // A direct input may have been journaled between two query emissions.
      // Its canonical UUID becomes the next logical parent in this chain.
      if(const auto at=lastTranscriptUuids_.find(batId);at!=lastTranscriptUuids_.end())parentId=at->second;
      entry["parentUuid"]=parentId?Json(*parentId):Json();
      written=writes_->enqueueWrite(target,std::move(entry));
      if(uuid.is_string()){
        writtenMessageIds_[batId].insert(uuid.get<std::string>());
        if(field(message,"type")!="progress")lastTranscriptUuids_[batId]=uuid.get<std::string>();
      }
    }
    // A failed drain leaves its entries' futures pending forever, so waiting on
    // the future alone hangs the Bat. flush() reports the failure instead.
    if(awaitWrites){writes_->flush();written.get();}
    if(field(message,"uuid").is_string())parentId=string(field(message,"uuid"));
  }
}
bats::RunBatLoopServices IndigenousBatApplication::makeRunLoop(){
  bats::RunBatLoopServices services;
  auto weak=weak_from_this();
  services.enqueueInitialTranscript=[weak](const Json& messages,
      const std::string& id){if(auto self=weak.lock())
        self->recordBatMessages(messages,id,std::nullopt);};
  services.enqueueBatMetadata=[weak](const std::string& id,const Json& data){
    if(auto self=weak.lock()){
      const auto metadata=self->withLineage(id,data);
      auto paths=self->pathsForBat(id);
      std::lock_guard lock(self->pathsMutex_);
      paths->writeBatMetadata(id,metadata);
    }
  };
  services.recordSidechainTranscript=[weak](const Json& messages,
      const std::string& id,const std::optional<std::string>& parent){
    if(auto self=weak.lock())self->recordBatMessages(messages,id,parent,true);
  };
  services.isBuiltInBat=[](const Json& definition){
    return field(definition,"source")=="built-in";
  };
  services.clearFlightHooks=[hooks=inputs_.hooks,
      dispatcher=inputs_.owner.dispatcher()](const auto&,
      const std::string& id){
    if(!hooks)return;
    application::onOwnerAndWait<void>(dispatcher,[hooks,id]{
      hooks->clearFlightHooks(id);});};
  services.clearReadFileStateStash=[](Json& stash){
    if(stash.is_array()||stash.is_object())stash.clear();
  };
  services.clearBatTranscriptSubdir=[weak](const std::string& id){
    if(auto self=weak.lock()){
      auto paths=self->pathsForBat(id);
      std::lock_guard lock(self->pathsMutex_);
      paths->clearBatTranscriptSubdir(id);
    }
  };
  services.killShellMissionsForBat=[kill=inputs_.killShellMissionsForBat](
      const std::string& id,const bats::GetAppState&,const auto&){kill(id);};
  return services;
}
} // namespace batbox::application_bats
