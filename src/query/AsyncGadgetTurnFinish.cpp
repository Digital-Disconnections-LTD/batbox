#include "query/AsyncGadgetTurnInternal.hpp"

namespace batbox::query::async_gadget_turn {
void Run::consumeCommands(){
  Json consumed=Json::array();
  for(const Json& command:queuedCommands)
    if(field(command,"mode")=="prompt"||
       field(command,"mode")=="task-notification")consumed.push_back(command);
  if(!consumed.empty()){
    need(services.sync.notifyCommandLifecycle,"notifyCommandLifecycle");
    need(services.sync.removeFromQueue,"removeFromQueue");
    for(const Json& command:consumed){
      const Json& uuid=field(command,"uuid");
      if(value::truthy(uuid)){
        const std::string id=uuid.is_string()?uuid.get<std::string>():
          value::stringify(uuid);
        consumedCommandUuids.push_back(id);
        services.sync.notifyCommandLifecycle(id,"started");
      }
    }
    services.sync.removeFromQueue(consumed);
  }
  if(services.sync.refreshGadgets){
    if(auto refreshed=services.sync.refreshGadgets(updatedContext))
      if(*refreshed!=updatedContext.gadgets)updatedContext.gadgets=*refreshed;
  }
  updatedContext.data["queryTracking"]=prepared->queryTracking;
  stage=Stage::Final;
}
void Run::finalize(){
  const Json& batId=field(prepared->gadgetUseContext.data,"agentId");
  need(services.sync.feature,"feature");
  if(services.sync.feature("BG_SESSIONS")&&!value::truthy(batId)){
    need(services.sync.shouldGenerateMissionSummary,"shouldGenerateTaskSummary");
    if(services.sync.shouldGenerateMissionSummary()){
      need(services.sync.maybeGenerateMissionSummary,"maybeGenerateTaskSummary");
      Json history=appendMessages(prepared->messagesForQuery,
        modelResult->assistantMessages);
      history=appendMessages(history,modelResult->gadgetResults);
      services.sync.maybeGenerateMissionSummary(Json{{"systemPrompt",params.systemPoetry},
        {"userContext",params.userContext},{"systemContext",params.systemContext},
        {"toolUseContext",prepared->gadgetUseContext.data},
        {"forkContextMessages",history}});
    }
  }
  const int nextTurn=state.turnCount+1;
  if(params.maxTurns&&*params.maxTurns!=0&&nextTurn>*params.maxTurns){
    need(services.sync.createAttachmentMessage,"createAttachmentMessage");
    emit(std::make_shared<Json>(services.sync.createAttachmentMessage(
      Json{{"type","max_turns_reached"},{"maxTurns",*params.maxTurns},
        {"turnCount",nextTurn}})),Stage::AfterMaxTurns);
    return;
  }
  need(services.queryCheckpoint,"queryCheckpoint");
  services.queryCheckpoint("query_recursive_call");
  Json history=appendMessages(prepared->messagesForQuery,
    modelResult->assistantMessages);
  history=appendMessages(history,modelResult->gadgetResults);
  State next=state;
  next.messages=std::move(history);
  next.gadgetUseContext=updatedContext;
  next.autoCompactTracking=prepared->tracking;
  next.turnCount=nextTurn;
  next.maxOutputTokensRecoveryCount=0;
  next.hasAttemptedReactiveCompact=false;
  next.pendingGadgetUseSummary.reset();
  next.pendingGadgetUseSummaryAsync=nextPendingSummary;
  next.maxOutputTokensOverride.reset();
  next.transition=Json{{"reason","next_turn"}};
  finish(CompletionDecision{next,{}});
}
} // namespace batbox::query::async_gadget_turn
