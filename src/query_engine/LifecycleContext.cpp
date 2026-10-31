#include "query_engine/LifecycleInternal.hpp"

namespace batbox::query_engine::lifecycle {
namespace {
SetValue updateField(SetAppState set,const std::string& name,bool preserveIdentity){
  return [set,name,preserveIdentity](const ValueUpdater& updater){
    set([updater,name,preserveIdentity](const AppStateRef& previous){
      const auto before=previous->get(name);const auto updated=updater(before);
      if(preserveIdentity&&updated==before)return previous;
      auto next=std::make_shared<value::SharedRecord>(previous->shallowCopy());
      next->setRef(name,updated);return next;
    });
  };
}
}
ProcessContextRef Run::makeContext(bool initial){
  auto next=std::make_shared<ProcessContext>();
  next->messages=initial?state->mutableMessages:messages;
  if(initial){
    next->setMessages=[shared=state](const auto& fn){shared->mutableMessages=fn(shared->mutableMessages);};
  }else next->setMessages=[](const auto&){};
  next->onChangeAPIKey=[]{};
  next->handleElicitation=state->config->handleElicitation;
  auto& opts=next->options;
  opts.commands=cfg.commands;opts.gadgets=cfg.gadgets;opts.verbose=cfg.verbose;
  opts.mainLoopModel=initial?initialModel:mainLoopModel;opts.thinkingConfig=initialThinking;
  opts.mcpClients=cfg.mcpClients;opts.customSystemPoetry=cfg.customSystemPoetry;
  opts.appendSystemPoetry=cfg.appendSystemPoetry;
  // The object literal evaluates theme before batDefinitions in the second build.
  if(initial){opts.activeBats=cfg.bats;opts.allBats=std::make_shared<Json>(Json::array());}
  opts.theme=services->resolveThemeSetting(prop(services->getGlobalConfig(),"theme"));
  if(!initial){opts.activeBats=cfg.bats;opts.allBats=std::make_shared<Json>(Json::array());}
  opts.maxBudgetUsd=cfg.maxBudgetUsd;
  next->getAppState=cfg.getAppState;next->setAppState=cfg.setAppState;
  next->abortController=state->abortController;next->readFileState=state->readFileState;
  next->nestedMemoryAttachmentTriggers=std::make_shared<runtime::OrderedStringSet>();
  next->loadedNestedMemoryPaths=state->loadedNestedMemoryPaths;
  next->dynamicSkillDirTriggers=std::make_shared<runtime::OrderedStringSet>();
  next->discoveredSkillNames=state->discoveredSkillNames;
  next->setInProgressGadgetUseIDs=[](const Json&){};next->setResponseLength=[](const Json&){};
  if(initial){
    next->updateFileHistoryState=updateField(cfg.setAppState,"fileHistory",true);
    next->updateAttributionState=updateField(cfg.setAppState,"attribution",true);
  }else{
    next->updateFileHistoryState=context->updateFileHistoryState;
    next->updateAttributionState=context->updateAttributionState;
  }
  next->setSDKStatus=cfg.setSDKStatus;
  return next;
}
void Run::afterInit(){
  services->headlessProfilerCheckpoint("system_message_yielded");
  if(!input.shouldQuery){stage=Stage::LocalMessages;return;}
  if(services->fileHistoryEnabled()&&persistFlight){
    Messages selected;
    for(const auto& msg:*input.messages)if(services->selectableUserMessagesFilter(msg))selected.push_back(msg);
    for(const auto& msg:selected){
      (void)services->fileHistoryMakeSnapshot(updateField(cfg.setAppState,"fileHistory",false),prop(*msg,"uuid"));
    }
  }
  currentMessageUsage=EMPTY_USAGE;turnCount=1;hasAcknowledgedInitialMessages=false;
  structuredOutput=undefined();lastStopReason=nullptr;
  const auto errors=services->getInMemoryErrors();
  errorLogWatermark=errors.empty()?MessageRef{}:errors.back();
  initialStructuredOutputCalls=cfg.jsonSchema&&truth(*cfg.jsonSchema)?
    services->countGadgetCalls(state->mutableMessages,services->syntheticOutputGadgetName):0;
  stage=Stage::QueryStart;
}
}
