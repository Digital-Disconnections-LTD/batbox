#include "bats/BatGadgetForegroundFinalize.hpp"
#include "runtime/MessageSnapshot.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string text(const Json& value){return value.is_string()?value.get<std::string>():"";}
template<class F>void need(const F& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("BatGadget finalize requires ")+name);
}
Json materialize(const BatMessageRefs& refs){
  Json result=Json::array();
  for(const auto& ref:refs)result.push_back(runtime::snapshotMessage(ref));
  return result;
}
} // namespace
std::optional<Json> finalizeBatGadgetForeground(BatGadgetForegroundFinalState& state,
    BatGadgetForegroundFinalServices& services){
  // Finalization begins by clearing the optional UI hint.
  if(state.context.setGadgetJSX)state.context.setGadgetJSX(Json());
  if(state.stopForegroundSummarization)state.stopForegroundSummarization();
  if(state.foregroundMissionId){
    if(services.unregisterForeground)
      services.unregisterForeground(*state.foregroundMissionId);
    else{
      if(!services.foreground)throw std::invalid_argument("BatGadget foreground roster is unbound");
      unregisterBatForeground(*state.foregroundMissionId,*services.foreground);
    }
    if(!state.wasBackgrounded){
      need(services.getProgressUpdate,"getProgressUpdate");
      need(services.enqueueSdkEvent,"enqueueSdkEvent");
      need(services.nowMillis,"nowMillis");
      const Json progress=services.getProgressUpdate(state.syncTracker);
      Json event={{"type","system"},{"subtype","task_notification"},
        {"task_id",*state.foregroundMissionId},
        {"status",state.syncBatError?"failed":state.wasAborted?"stopped":"completed"},
        {"output_file",""},{"summary",state.description},
        {"usage",{{"total_tokens",field(progress,"tokenCount")},
          {"tool_uses",field(progress,"toolUseCount")},
          {"duration_ms",services.nowMillis()-state.batStartTime}}}};
      if(!state.gadgetUseId.is_null())event["tool_use_id"]=state.gadgetUseId;
      services.enqueueSdkEvent(event);
    }
  }
  need(services.clearInvokedSkillsForBat,"clearInvokedSkillsForAgent");
  services.clearInvokedSkillsForBat(state.syncBatId);

  if(state.cancelAutoBackground)state.cancelAutoBackground();
  Json worktree=Json::object();
  if(!state.wasBackgrounded){
    if(state.cleanupWorktree)worktree=state.cleanupWorktree();
    else if(state.worktree)worktree=cleanupBatWorktreeIfNeeded(
      *state.worktree,services.worktree);
  }
  if(state.wasBackgrounded)return std::nullopt;
  if(state.wasAborted){
    if(state.abortError)std::rethrow_exception(state.abortError);
    throw BatAbortError();
  }
  const Json messages=materialize(state.batMessages);
  for(auto it=messages.rbegin();it!=messages.rend();++it){
    const auto& type=field(*it,"type");
    if(type=="system" || type=="progress")continue;
    need(services.isSyntheticMessage,"isSyntheticMessage");
    if(services.isSyntheticMessage(*it))throw BatAbortError();
    break;
  }
  if(state.syncBatError){
    bool hasAssistant=false;
    for(const auto& message:messages)
      if(field(message,"type")=="assistant"){hasAssistant=true;break;}
    if(!hasAssistant)std::rethrow_exception(state.syncBatError);
  }
  Json batResult=finalizeBatGadget(messages,state.syncBatId,state.metadata,
    services.result);
  need(services.feature,"feature");
  if(services.feature("TRANSCRIPT_CLASSIFIER")){
    need(services.classifyHandoffIfNeeded,"classifyHandoffIfNeeded");
    need(state.context.getAppState,"toolUseContext.getAppState");
    const Json current=effectiveBatGetAppState(state.context)();
    const auto warning=services.classifyHandoffIfNeeded(messages,
      state.context.query.gadgets,field(current,"toolPermissionContext"),
      state.context.abortOwner?state.context.abortOwner->get_token():
        state.context.query.stop,text(field(state.selectedBat,"agentType")),
      field(batResult,"totalToolUseCount").get<int>());
    if(warning){
      Json content=Json::array({Json{{"type","text"},{"text",*warning}}});
      for(const auto& block:batResult.at("content"))content.push_back(block);
      batResult["content"]=std::move(content);
    }
  }
  Json data={{"status","completed"},{"prompt",state.poetry}};
  for(auto it=batResult.begin();it!=batResult.end();++it)
    data[it.key()]=it.value();
  if(worktree.is_object())for(auto it=worktree.begin();it!=worktree.end();++it)
    data[it.key()]=it.value();
  return Json{{"data",data}};
}
} // namespace batbox::bats
