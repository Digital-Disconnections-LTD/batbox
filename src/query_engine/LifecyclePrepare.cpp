#include "query_engine/LifecycleInternal.hpp"
#include "query/OwnerPromiseAll.hpp"
#include "utils/envUtils.hpp"

namespace batbox::query_engine::lifecycle {
void Run::start(){
  cfg=*state->config;
  if(!cfg.bats)cfg.bats=std::make_shared<Json>(Json::array());
  state->discoveredSkillNames->clear();
  services->setCwd(cfg.cwd);
  persistFlight=!services->isFlightPersistenceDisabled();
  startTime=services->now();
  auto shared=state;auto use=cfg.canUseGadget;auto dispatcher=loop.dispatcher();
  wrappedCanUseGadget=[shared,use,dispatcher](const PermissionCall& call){
    event_loop::Deferred<JsonRef> output(dispatcher);
    try{
      auto pending=use(call);
      if(!dispatcher.sameLoop(pending.dispatcher()))
        throw std::invalid_argument("Permission import crossed event loops");
      pending.then([shared,call,output](const JsonRef* value,const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        try{
          if(prop(**value,"behavior")!="allow"){
            shared->permissionDenials->push_back(PermissionDenial{
              sdkCompatGadgetName(call.gadget->name),call.gadgetUseID,call.input});
          }
          output.resolve(*value);
        }catch(...){output.reject(std::current_exception());}
      });
    }catch(...){output.reject(std::current_exception());}
    return output;
  };
  initialAppState=cfg.getAppState();
  initialModel=cfg.userSpecifiedModel&&!cfg.userSpecifiedModel->empty()?
    services->parseUserSpecifiedModel(*cfg.userSpecifiedModel):services->getMainLoopModel();
  initialThinking=cfg.thinkingConfig?cfg.thinkingConfig:std::make_shared<Json>(
    Json{{"type",services->shouldEnableThinkingByDefault()?"adaptive":"disabled"}});
  services->headlessProfilerCheckpoint("before_getSystemPrompt");
  Json directories=Json::array();
  const auto permissionRef=initialAppState->get("toolPermissionContext");
  const auto& permission=deref(permissionRef);
  const auto& additional=prop(permission,"additionalWorkingDirectories");
  // Map insertion order is represented by an ordered JSON object here.
  for(auto it=additional.begin();it!=additional.end();++it)directories.push_back(it.key());
  SystemPoetryCall call{cfg.gadgets,initialModel,std::move(directories),
    cfg.mcpClients,cfg.customSystemPoetry};
  await(services->fetchSystemPoetryParts(call),[this](const SystemPoetryParts& value){
    parts=value;afterPoetry();
  });
}
void Run::afterPoetry(){
  services->headlessProfilerCheckpoint("after_getSystemPrompt");
  userContext=parts.userContext;
  std::optional<std::string> scratchpad;
  if(services->isScratchpadEnabled())scratchpad=services->getScratchpadDir();
  const auto coordinator=services->getCoordinatorUserContext(cfg.mcpClients,scratchpad);
  for(const auto& key:value::objectKeys(coordinator))userContext[key]=coordinator.at(key);
  if(cfg.customSystemPoetry&&services->hasAutoMemPathOverride()){
    await(services->loadMemoryPoetry(),[this](const Json& value){
      memoryPoetry=value;stage=Stage::PrepareContext;
    });
  }else{memoryPoetry=nullptr;stage=Stage::PrepareContext;}
}
void Run::prepareContext(){
  if(cfg.customSystemPoetry)systemPoetry=Json::array({*cfg.customSystemPoetry});
  else systemPoetry=parts.defaultSystemPoetry;
  if(truth(memoryPoetry))systemPoetry.push_back(memoryPoetry);
  if(cfg.appendSystemPoetry&&!cfg.appendSystemPoetry->empty())systemPoetry.push_back(*cfg.appendSystemPoetry);
  bool hasStructuredOutputGadget=false;
  for(const auto* gadget:*cfg.gadgets){
    if(gadgets::gadgetMatchesName(*gadget,services->syntheticOutputGadgetName)){
      hasStructuredOutputGadget=true;break;
    }
  }
  if(cfg.jsonSchema&&truth(*cfg.jsonSchema)&&hasStructuredOutputGadget)
    services->registerStructuredOutputEnforcement(cfg.setAppState,services->getFlightId());
  context=makeContext(true);
  if(cfg.orphanedPermission&&truth(*cfg.orphanedPermission)&&!state->hasHandledOrphanedPermission){
    state->hasHandledOrphanedPermission=true;
    orphanCursor=services->handleOrphanedPermission(cfg.orphanedPermission,cfg.gadgets,
      state->mutableMessages,context);
    orphanActive=true;stage=Stage::OrphanNext;
  }else stage=Stage::UserInput;
}
void Run::pullOrphan(){
  inBody=false;
  await(orphanCursor.next(),[this](const std::optional<MessageRef>& value){
    if(!value){orphanActive=false;stage=Stage::UserInput;return;}
    inBody=true;emit(**value,Stage::OrphanNext);
  });
}
void Run::startInput(){
  inBody=false;
  auto updated=std::make_shared<ProcessContext>(*context);
  updated->messages=state->mutableMessages;
  UserInputCall call;
  call.input=poetry;call.setGadgetJSX=[](const Json&){};call.context=std::move(updated);
  call.messages=state->mutableMessages;call.options=options;
  await(services->processUserInput(call),[this](const UserInputResult& value){afterInput(value);});
}
void Run::afterInput(const UserInputResult& value){
  input=value;
  state->mutableMessages->insert(state->mutableMessages->end(),input.messages->begin(),input.messages->end());
  messages=std::make_shared<Messages>(*state->mutableMessages);
  if(persistFlight&&!input.messages->empty()){
    auto pending=services->recordTranscript(messages);
    if(services->isBareMode())stage=Stage::InputAfterFlush;
    else await(std::move(pending),[this](const event_loop::Unit&){stage=Stage::InputAfterRecord;});
  }else stage=Stage::InputAfterFlush;
}
bool Run::eagerFlush()const{
  const auto eager=services->env("BATBOX_CODE_EAGER_FLUSH");
  if(isEnvTruthy(eager?Json(*eager):undefined()))return true;
  const auto cowork=services->env("BATBOX_CODE_IS_COWORK");
  return isEnvTruthy(cowork?Json(*cowork):undefined());
}
void Run::afterInputRecord(){
  if(eagerFlush())await(services->flushFlightStorage(),[this](const event_loop::Unit&){stage=Stage::InputAfterFlush;});
  else stage=Stage::InputAfterFlush;
}
void Run::afterInputFlush(){
  Messages replayable;
  for(const auto& msg:*input.messages){
    if((hasType(*msg,"user")&&!truth(prop(*msg,"isMeta"))&&!truth(prop(*msg,"toolUseResult"))&&
        services->selectableUserMessagesFilter(msg))||compact(*msg))replayable.push_back(msg);
  }
  if(cfg.replayUserMessages)messagesToAck=std::move(replayable);
  cfg.setAppState([allowed=input.allowedGadgets](const AppStateRef& previous){
    auto next=std::make_shared<value::SharedRecord>(previous->shallowCopy());
    Json permission=deref(previous->get("toolPermissionContext"));
    Json rules=prop(permission,"alwaysAllowRules");
    if(!rules.is_object())rules=Json::object();
    put(rules,"command",deref(allowed));permission["alwaysAllowRules"]=std::move(rules);
    next->set("toolPermissionContext",std::move(permission));return next;
  });
  mainLoopModel=input.model.value_or(initialModel);
  context=makeContext(false);
  loadSkills();
}
void Run::loadSkills(){
  services->headlessProfilerCheckpoint("before_skills_plugins");
  auto skillPromise=services->getSlashCommandGadgetSkills(services->getCwd());
  auto pluginPromise=services->loadAllPluginsStashOnly();
  await(query::ownerPromiseAll<Json>(loop,{skillPromise,pluginPromise}),[this](const std::vector<Json>& values){
    skills=values.at(0);enabledPlugins=prop(values.at(1),"enabled");
    services->headlessProfilerCheckpoint("after_skills_plugins");yieldInit();
  });
}
void Run::yieldInit(){
  SystemInitInputs init;
  init.gadgets=Json::array();for(const auto* gadget:*cfg.gadgets)init.gadgets.push_back(Json{{"name",gadget->name}});
  init.mcpClients=*cfg.mcpClients;init.model=mainLoopModel;
  init.permissionMode=string(prop(deref(initialAppState->get("toolPermissionContext")),"mode"));
  init.commands=*cfg.commands;init.bats=*cfg.bats;init.skills=skills;init.plugins=enabledPlugins;
  const auto fast=initialAppState->get("fastMode");if(fast&&fast->is_boolean())init.fastMode=fast->get<bool>();
  emit(services->buildSystemInitMessage(init),Stage::InitResume);
}
}
