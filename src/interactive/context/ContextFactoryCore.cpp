#include "interactive/context/ContextFactoryInternal.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive::context {
namespace {
const Json& field(const Json& value,const char* name){return value::property(value,name);}
void need(const auto& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Context binding required: ")+name);
}
}
ContextFactory::ContextFactory(ContextBindings bindings):
  bindings_(std::make_shared<const ContextBindings>(std::move(bindings))){
  need(bindings_->store,"store");
  need(bindings_->getMcpGadgets,"getMcpTools");
  need(bindings_->feature,"feature");
  need(bindings_->commands,"commands");
  need(bindings_->thinkingConfig,"thinkingConfig");
  need(bindings_->loadedNestedMemoryPaths,"loadedNestedMemoryPaths");
  need(bindings_->discoveredSkillNames,"discoveredSkillNames");
  need(bindings_->readFileState,"readFileState");
}
gadgets::Gadgets ContextFactory::computeGadgets(const ContextBindings& b){
  const auto state=b.store->getState();
  need(state,"app state");
  const auto snapshot=state->snapshot();
  const auto& permission=field(snapshot,"toolPermissionContext");
  const auto mcpGadgets=b.getMcpGadgets(state);
  auto assembled=b.assembleGadgets?b.assembleGadgets(permission,mcpGadgets):
    gadgets::assembleGadgetPool(permission,mcpGadgets,b.catalog,b.catalogSettings);
  const auto& mode=field(permission,"mode");
  auto merged=mergeAndFilterGadgets(b.combinedInitialGadgets,assembled,
    mode.is_string()?mode.get<std::string>():std::string(),b.gadgetPool);
  if(!b.mainThreadBatDefinition||!value::truthy(*b.mainThreadBatDefinition))
    return merged;
  auto imports=b.batFilter;
  return bats::resolveBatGadgets(*b.mainThreadBatDefinition,merged,
    false,true,imports).resolvedGadgets;
}
std::shared_ptr<GadgetUseContext> ContextFactory::make(
    TurnMessagesRef messages,TurnMessagesRef newMessages,
    std::shared_ptr<TurnAbort> abort,std::string mainLoopModel)const{
  (void)newMessages;
  if(!messages||!abort)throw std::invalid_argument("Context messages and abort required");
  const auto b=bindings_;
  const auto state=b->store->getState();
  need(state,"app state");
  const auto snapshot=state->snapshot();
  auto context=std::make_shared<GadgetUseContext>();
  context->store=b->store;
  context->abortController=std::move(abort);
  auto& options=context->options;
  options.commands=b->commands;
  auto gadgetOwners=std::make_shared<std::vector<std::shared_ptr<const void>>>();
  const auto retainGadgets=[b,gadgetOwners]{
    if(!b->gadgetSnapshotOwner)return;
    auto owner=b->gadgetSnapshotOwner();
    if(owner&&std::find(gadgetOwners->begin(),gadgetOwners->end(),owner)==gadgetOwners->end())
      gadgetOwners->push_back(std::move(owner));
  };
  retainGadgets();context->gadgetSnapshotOwner=gadgetOwners;
  options.gadgets=computeGadgets(*b);
  options.debug=b->debug;
  options.verbose=value::truthy(field(snapshot,"verbose"));
  options.mainLoopModel=std::move(mainLoopModel);
  const auto& enabled=field(snapshot,"thinkingEnabled");
  options.thinkingConfig=!(b->explicitThinkingOverride&&
      *b->explicitThinkingOverride)&&enabled.is_boolean()&&!enabled.get<bool>()?
    std::make_shared<Json>(Json{{"type","disabled"}}):b->thinkingConfig;
  const auto& mcp=field(snapshot,"mcp");
  const auto& clients=field(mcp,"clients");
  options.mcpClients=b->initialMcpClients&&b->initialMcpClients->is_array()&&
    (!clients.is_array()||clients.empty())?b->initialMcpClients:
    std::make_shared<Json>(mergeClients(
      b->initialMcpClients?*b->initialMcpClients:Json(),clients));
  options.mcpResources=std::make_shared<Json>(field(mcp,"resources"));
  options.ideInstallationStatus=b->ideInstallationStatus;
  options.isNonInteractiveFlight=false;
  options.dynamicMcpConfig=b->dynamicMcpConfig;
  options.theme=b->theme;
  options.batDefinitions=state->fieldRef("agentDefinitions");
  if(b->allowedBatTypes){
    Json definitions=options.batDefinitions?*options.batDefinitions:Json::object();
    if(!definitions.is_object())definitions=Json::object();
    definitions["allowedAgentTypes"]=*b->allowedBatTypes;
    options.batDefinitions=std::make_shared<Json>(std::move(definitions));
  }
  options.customSystemPoetry=b->customSystemPoetry;
  options.appendSystemPoetry=b->appendSystemPoetry;
  options.refreshGadgets=[b,retainGadgets]{retainGadgets();return computeGadgets(*b);};

  context->messages=std::move(messages);
  context->readFileState=b->readFileState;
  context->loadedNestedMemoryPaths=b->loadedNestedMemoryPaths;
  context->discoveredSkillNames=b->discoveredSkillNames;
  context->contentReplacementState=b->contentReplacementState;
  context->nestedMemoryAttachmentTriggers=
    std::make_shared<runtime::OrderedStringSet>();
  context->dynamicSkillDirTriggers=
    std::make_shared<runtime::OrderedStringSet>();
  context->getAppState=[store=b->store]{return store->getState();};
  context->setAppState=[store=b->store](const StateUpdater& update){
    need(update,"setAppState updater");store->setState(update);
  };
  context->setMessages=b->setMessages;
  context->openMessageSelector=[disabled=b->disabled,
      show=b->showMessageSelector]{
    if(!disabled){need(show,"showMessageSelector");show();}
  };
  context->onChangeAPIKey=b->onChangeAPIKey;
  context->setLocalUi=b->setLocalUi;
  context->addNotification=b->addNotification;
  context->sendOSNotification=b->sendOSNotification;
  context->onChangeDynamicMcpConfig=b->onChangeDynamicMcpConfig;
  context->onInstallIDEExtension=b->onInstallIDEExtension;
  context->setResponseLength=b->setResponseLength;
  context->setStreamMode=b->setStreamMode;
  context->setInProgressGadgetUseIDs=b->setInProgressGadgetUseIDs;
  context->setHasInterruptibleGadgetInProgress=b->setHasInterruptibleGadgetInProgress;
  context->resume=b->resume;
  context->setConversationId=b->setConversationId;
  if(b->feature&&b->feature("HOOK_PROMPTS")){
    need(b->requestPoetry,"requestPrompt");
    context->requestPoetry=b->requestPoetry;
  }
  bindContextCallbacks(*context,b);
  return context;
}
} // namespace batbox::interactive::context
