#include "interactive/context/ContextFactory.hpp"
#include "bats/ForkedBat.hpp"
#include <stdexcept>

namespace batbox::interactive::context {
namespace {
Json messagesJson(const TurnMessagesRef& messages){
  Json result=Json::array();
  for(const auto& message:*messages)result.push_back(*message);
  return result;
}
Json optionsJson(const ProcessOptions& options){
  Json result={{"commands",options.commands?*options.commands:Json()},
    {"debug",options.debug},
    {"verbose",options.verbose},{"mainLoopModel",options.mainLoopModel},
    {"thinkingConfig",options.thinkingConfig?*options.thinkingConfig:Json()},
    {"mcpClients",options.mcpClients?*options.mcpClients:Json()},
    {"mcpResources",options.mcpResources?*options.mcpResources:Json()},
    {"ideInstallationStatus",options.ideInstallationStatus},
    {"isNonInteractiveSession",options.isNonInteractiveFlight},
    {"dynamicMcpConfig",options.dynamicMcpConfig},
    {"theme",options.theme},
    {"agentDefinitions",options.batDefinitions?*options.batDefinitions:Json()}};
  if(options.customSystemPoetry)
    result["customSystemPrompt"]=*options.customSystemPoetry;
  if(options.appendSystemPoetry)
    result["appendSystemPrompt"]=*options.appendSystemPoetry;
  return result;
}
}
InteractiveQueryContext ContextFactory::makeQueryContext(
    const TurnMessagesRef& messages,const TurnRequest& request)const{
  const auto full=make(messages,request.newMessages,request.abort,
                       request.mainLoopModel);
  execution::Context query;
  query.data=Json{{"options",optionsJson(full->options)},
    {"messages",messagesJson(full->messages)},
    {"nestedMemoryAttachmentTriggers",Json::array()},
    {"loadedNestedMemoryPaths",Json::array()},
    {"dynamicSkillDirTriggers",Json::array()}};
  query.gadgets=full->options.gadgets;
  query.baseGadgets=full->options.gadgets;
  query.gadgetSnapshotOwner=full->gadgetSnapshotOwner;
  query.stop=full->abortController->controller.get_token();
  query.readFileState=full->readFileState;
  query.contentReplacementState=full->contentReplacementState;
  query.nestedMemoryAttachmentTriggers=full->nestedMemoryAttachmentTriggers;
  query.loadedNestedMemoryPaths=full->loadedNestedMemoryPaths;
  query.dynamicSkillDirTriggers=full->dynamicSkillDirTriggers;
  query.discoveredSkillNames=full->discoveredSkillNames;
  auto bat=std::make_shared<bats::BatContext>();
  bat->query=query;
  bat->getTypedAppState=full->getAppState;
  bat->setTypedAppState=full->setAppState;
  bat->setTypedAppStateForMissions=full->setAppState;
  bat->updateTypedFileHistoryState=full->updateFileHistoryState;
  bat->updateTypedAttributionState=full->updateAttributionState;
  bat->getAppState=[full]{return full->getAppState()->snapshot();};
  bat->setAppState=bindings_->legacySetAppState;
  bat->setAppStateForMissions=bat->setAppState;
  bat->setResponseLength=[set=full->setResponseLength](const Json& value){
    if(!set)throw std::logic_error("Response length setter is unbound");
    const double length=value::numberOr(value);
    set([length](double){return length;});
  };
  bat->updateResponseLength=[set=full->setResponseLength](
      const std::function<Json(const Json&)>& update){
    if(!set)throw std::logic_error("Response length updater is unbound");
    if(!update)throw std::invalid_argument("Response length update is empty");
    set([update](double current){return value::numberOr(update(Json(current)));});
  };
  bat->currentAbortReason=[abort=full->abortController]{
    if(abort->reason.is_string())return abort->reason.get<std::string>();
    return std::string();
  };
  return {std::move(query),std::move(bat)};
}
} // namespace batbox::interactive::context
