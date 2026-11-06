#include "query/AsyncGadgetTurnInternal.hpp"

namespace batbox::query::async_gadget_turn {
Json Run::historyThroughGadgetResults(bool take){
  const Json& results=modelResult->gadgetResults;
  if(attachmentHistory){
    Json history=take?std::move(*attachmentHistory):*attachmentHistory;
    if(take)attachmentHistory.reset();
    if(results.is_array())
      for(std::size_t i=attachmentHistoryResults;i<results.size();++i)history.push_back(results[i]);
    return history;
  }
  Json history=appendMessages(prepared->messagesForQuery,modelResult->assistantMessages);
  if(results.is_array())for(const Json& item:results)history.push_back(item);
  return history;
}
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
  State next=state;
  next.messages=historyThroughGadgetResults(true);
  next.gadgetUseContext=std::move(updatedContext);
  next.autoCompactTracking=prepared->tracking;
  next.turnCount=nextTurn;
  next.maxOutputTokensRecoveryCount=0;
  next.maxOutputTokensOverride.reset();
  next.transition=Json{{"reason","next_turn"}};
  finish(CompletionDecision{std::move(next),{}});
}
} // namespace batbox::query::async_gadget_turn
