#include "bats/ForkedBat.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound subagent import: ")+name);
}
const Json& field(const Json& object,const char* name){return value::property(object,name);}
void erase(Json& object,const char* key){if(object.is_object())object.erase(key);}
}

BatContext createSubagentContext(const BatContext& parent,
  const SubagentOverrides& overrides,ContextServices& services){
  BatContext child;
  child.query=parent.query;
  // The child inherits the current live getAppState closure. Move the Indigenous
  // query-side wrapper into that closure before applying child rules.
  child.getAppState=parent.getAppState;
  child.getTypedAppState=parent.getTypedAppState;
  materializeAppStateModifier(child);
  if(overrides.abortToken){
    child.query.stop=*overrides.abortToken;
    child.currentAbortReason=overrides.currentAbortReason;
  }else if(overrides.shareAbortController){
    child.query.stop=parent.query.stop;
    child.abortOwner=parent.abortOwner;
    child.parentAbortLink=parent.parentAbortLink;
    child.currentAbortReason=parent.currentAbortReason;
  }else{
    child.abortOwner=std::make_shared<std::stop_source>();
    auto callback=std::make_shared<std::stop_callback<std::function<void()>>>(
      parent.query.stop,[source=child.abortOwner]{source->request_stop();});
    child.parentAbortLink=callback;
    child.query.stop=child.abortOwner->get_token();
    child.currentAbortReason=parent.currentAbortReason;
  }
  child.query.abortReason=parent.query.abortReason;
  need(parent.getAppState,"parent.getAppState");
  if(overrides.getAppState)child.getAppState=overrides.getAppState;
  else if(!overrides.shareAbortController)child.getAppState=[base=child.getAppState]{
    Json state=base();
    const Json& permission=field(state,"toolPermissionContext");
    if(value::truthy(field(permission,"shouldAvoidPermissionPrompts")))return state;
    state["toolPermissionContext"]["shouldAvoidPermissionPrompts"]=true;
    return state;
  };
  if(overrides.getTypedAppState)child.getTypedAppState=overrides.getTypedAppState;
  else if(overrides.getAppState)child.getTypedAppState={};
  else if(!overrides.shareAbortController&&child.getTypedAppState){
    child.getTypedAppState=[base=child.getTypedAppState]{
      const auto state=base();
      if(!state)throw std::logic_error("Typed app state required");
      const auto& original=state->permissionContext;
      if(!original)throw std::logic_error("Typed permission context required");
      const auto flag=original->get("shouldAvoidPermissionPrompts");
      if(flag&&value::truthy(*flag))return state;
      auto next=std::make_shared<app_state::AppState>(state->shallowCopy());
      auto permission=std::make_shared<value::SharedRecord>(original->shallowCopy());
      permission->set("shouldAvoidPermissionPrompts",true);
      next->setPermissionContext(std::move(permission));
      return next;
    };
  }
  if(overrides.indigenousReadFileState||parent.query.readFileState){
    const auto& source=overrides.indigenousReadFileState?
      overrides.indigenousReadFileState:parent.query.readFileState;
    child.query.readFileState=file_state_stash::cloneFileStateStash(*source);
    erase(child.query.data,"readFileState");
  }else{
    need(services.cloneFileStateStash,"cloneFileStateCache");
    child.query.data["readFileState"]=services.cloneFileStateStash(
      overrides.readFileState?*overrides.readFileState:field(parent.query.data,"readFileState"));
  }
  child.query.data["nestedMemoryAttachmentTriggers"]=Json::array();
  child.query.data["loadedNestedMemoryPaths"]=Json::array();
  child.query.data["dynamicSkillDirTriggers"]=Json::array();
  child.query.nestedMemoryAttachmentTriggers=
    std::make_shared<runtime::OrderedStringSet>();
  child.query.loadedNestedMemoryPaths=
    std::make_shared<runtime::OrderedStringSet>();
  child.query.dynamicSkillDirTriggers=
    std::make_shared<runtime::OrderedStringSet>();
  child.query.discoveredSkillNames=
    std::make_shared<runtime::OrderedStringSet>();
  child.query.data["discoveredSkillNames"]=Json::array();
  erase(child.query.data,"toolDecisions");
  if(overrides.indigenousContentReplacementState || parent.query.contentReplacementState)
    erase(child.query.data,"contentReplacementState");
  else if(overrides.contentReplacementState && value::truthy(*overrides.contentReplacementState))
    child.query.data["contentReplacementState"]=*overrides.contentReplacementState;
  else if(value::truthy(field(parent.query.data,"contentReplacementState"))){
    need(services.cloneContentReplacementState,"cloneContentReplacementState");
    child.query.data["contentReplacementState"]=services.cloneContentReplacementState(
      field(parent.query.data,"contentReplacementState"));
  }else erase(child.query.data,"contentReplacementState");
  if(overrides.indigenousContentReplacementState)
    child.query.contentReplacementState=overrides.indigenousContentReplacementState;
  else if(parent.query.contentReplacementState)
    child.query.contentReplacementState=gadget_results::cloneContentReplacementState(
      *parent.query.contentReplacementState);
  else child.query.contentReplacementState.reset();
  child.setAppState=overrides.shareSetAppState?parent.setAppState:
    std::function<void(const std::function<Json(const Json&)>&)>{[](const auto&) {}};
  child.setAppStateForMissions=parent.setAppStateForMissions?parent.setAppStateForMissions:parent.setAppState;
  child.setTypedAppState=overrides.shareSetAppState?parent.setTypedAppState:
    std::function<void(const app_state::AppStateUpdater&)>{[](const auto&) {}};
  child.setTypedAppStateForMissions=parent.setTypedAppStateForMissions?
    parent.setTypedAppStateForMissions:parent.setTypedAppState;
  if(overrides.shareSetAppState)child.query.data["localDenialTracking"]=
    field(parent.query.data,"localDenialTracking");
  else{
    need(services.createDenialTrackingState,"createDenialTrackingState");
    child.query.data["localDenialTracking"]=services.createDenialTrackingState();
  }
  child.setInProgressGadgetUseIDs=[](const std::string&){};
  child.updateFileHistoryState=[](const Json&){};
  child.updateAttributionState=parent.updateAttributionState;
  child.updateTypedFileHistoryState=[](const BatContext::TypedValueUpdater&){};
  child.updateTypedAttributionState=parent.updateTypedAttributionState;
  child.setResponseLength=overrides.shareSetResponseLength?parent.setResponseLength:
    std::function<void(const Json&)>{[](const Json&) {}};
  child.pushApiMetricsEntry=overrides.shareSetResponseLength?parent.pushApiMetricsEntry:
    std::function<void(const Json&)>{};
  for(const char* ui:{"addNotification","setToolJSX","setStreamMode","setSDKStatus","openMessageSelector"})
    erase(child.query.data,ui);
  if(overrides.options)child.query.data["options"]=*overrides.options;
  if(overrides.gadgets)child.query.gadgets=*overrides.gadgets;
  if(overrides.messages)child.query.data["messages"]=*overrides.messages;
  need(services.createBatId,"createAgentId");
  child.query.data["agentId"]=overrides.batId?*overrides.batId:services.createBatId(std::nullopt);
  if(overrides.batType)child.query.data["agentType"]=*overrides.batType;
  else erase(child.query.data,"agentType");
  need(services.randomUUID,"randomUUID");
  const Json& parentTracking=field(parent.query.data,"queryTracking");
  const Json& priorDepth=field(parentTracking,"depth");
  const int depth=priorDepth.is_number_integer()?priorDepth.get<int>()+1:0;
  child.query.data["queryTracking"]={{"chainId",services.randomUUID()},{"depth",depth}};
  if(overrides.criticalSystemReminder)
    child.query.data["criticalSystemReminder_EXPERIMENTAL"]=*overrides.criticalSystemReminder;
  else erase(child.query.data,"criticalSystemReminder_EXPERIMENTAL");
  if(overrides.requireCanUseGadget)child.query.data["requireCanUseTool"]=*overrides.requireCanUseGadget;
  else erase(child.query.data,"requireCanUseTool");
  return child;
}
} // namespace batbox::bats
