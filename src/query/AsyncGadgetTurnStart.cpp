#include "query/AsyncGadgetTurnInternal.hpp"

namespace batbox::query {
AsyncDecisionCursor makeAsyncGadgetTurnCursor(event_loop::Loop& loop,
  const Params& params,const State& state,
  std::shared_ptr<PreparedIteration> prepared,
  std::shared_ptr<ModelPassResult> modelResult,
  const QueryConfig& config,MemoryPrefetch* memoryPrefetch,
  std::vector<std::string>& consumedCommandUuids,
  AsyncGadgetTurnServices services){
  if(!loop.isOwnerThread())throw std::logic_error("Async tool turn requires loop owner");
  if(!prepared||!modelResult)throw std::invalid_argument("Async tool turn requires model pass");
  auto run=std::make_shared<async_gadget_turn::Run>(loop,params,state,
    std::move(prepared),std::move(modelResult),config,memoryPrefetch,
    consumedCommandUuids,std::move(services));
  return AsyncDecisionCursor{[run]{return run->next();},
    [run]{return run->close();}};
}
void bindAsyncGadgetTurn(event_loop::Loop& loop,AsyncEngineServices& engine,
  AsyncGadgetTurnServices services){
  engine.executeGadgetTurn=[&loop,services=std::move(services)](
      const Params& params,const State& state,
      std::shared_ptr<PreparedIteration> prepared,
      std::shared_ptr<ModelPassResult> result,const QueryConfig& config,
      MemoryPrefetch* memory,std::vector<std::string>& consumed){
    return makeAsyncGadgetTurnCursor(loop,params,state,std::move(prepared),
      std::move(result),config,memory,consumed,services);
  };
}
} // namespace batbox::query

namespace batbox::query::async_gadget_turn {
event_loop::Deferred<AsyncDecisionStep> Run::next(){
  if(!loop.isOwnerThread())throw std::logic_error("Async tool turn next() requires loop owner");
  event_loop::Deferred<AsyncDecisionStep> request(loop);
  requests.push_back(request);pump();return request;
}
void Run::emit(const model::MessageRef& event,Stage after){
  stage=after;pausedAfterYield=true;
  auto request=requests.front();requests.pop_front();
  request.resolve(AsyncDecisionStep{false,event,{}});
  if(!requests.empty()){
    auto self=shared_from_this();loop.queueMicrotask([self]{self->pump();});
  }
}
void Run::finish(CompletionDecision value){
  stage=Stage::Done;
  auto request=requests.front();requests.pop_front();
  request.resolve(AsyncDecisionStep{true,{},
    std::make_shared<CompletionDecision>(std::move(value))});
  while(!requests.empty()){
    auto queued=requests.front();requests.pop_front();
    queued.resolve(AsyncDecisionStep{true,{},{}});
  }
}
void Run::fail(std::exception_ptr error){
  failure=std::move(error);stage=Stage::Done;
  if(!requests.empty()){
    auto request=requests.front();requests.pop_front();request.reject(failure);
  }
  while(!requests.empty()){
    auto queued=requests.front();requests.pop_front();
    queued.resolve(AsyncDecisionStep{true,{},{}});
  }
}
void Run::pump(){
  if(requests.empty())return;
  try{
    while(!requests.empty()){
      switch(stage){
        case Stage::Start:start();break;
        case Stage::GadgetAwait:case Stage::AbortCleanupAwait:
        case Stage::AttachmentAwait:case Stage::MemoryAwait:
        case Stage::SkillAwait:return;
        case Stage::AfterGadgetYield:afterGadgetYield();break;
        case Stage::AfterGadgets:afterGadgets();break;
        case Stage::AbortInterruption:emitAbortInterruption();break;
        case Stage::AbortMaxTurns:abortMaxTurns();break;
        case Stage::AbortTerminal:finish(CompletionDecision{{},
          Json{{"reason","aborted_tools"}}});break;
        case Stage::AfterAbortMax:finish(CompletionDecision{{},
          Json{{"reason","aborted_tools"}}});break;
        case Stage::AfterSummary:afterSummary();break;
        case Stage::AfterAttachmentYield:afterAttachmentYield();break;
        case Stage::MemoryStart:startMemory();break;
        case Stage::MemoryYield:yieldMemory();break;
        case Stage::AfterMemoryYield:afterMemoryYield();break;
        case Stage::SkillStart:startSkills();break;
        case Stage::SkillYield:yieldSkills();break;
        case Stage::AfterSkillYield:afterSkillYield();break;
        case Stage::Commands:consumeCommands();break;
        case Stage::Final:finalize();break;
        case Stage::AfterMaxTurns:finish(CompletionDecision{{},
          Json{{"reason","max_turns"},{"turnCount",state.turnCount+1}}});break;
        case Stage::Done:{
          auto request=requests.front();requests.pop_front();
          request.resolve(AsyncDecisionStep{true,{},{}});break;
        }
      }
      if(pausedAfterYield){pausedAfterYield=false;return;}
    }
  }catch(...){fail(std::current_exception());}
}
void Run::start(){
  need(services.queryCheckpoint,"queryCheckpoint");
  services.queryCheckpoint("query_tool_execution_start");
  need(services.gadgetUpdates,"toolUpdates");
  std::vector<Json> blocks;
  if(modelResult->gadgetUseBlocks.is_array())
    for(const Json& block:modelResult->gadgetUseBlocks)blocks.push_back(block);
  gadgetCursor=services.gadgetUpdates(blocks,modelResult->assistantMessages,
    prepared,prepared->streamingGadgetExecutor!=nullptr);
  need(gadgetCursor.next,"toolUpdates cursor.next");
  pullGadgetUpdate();
}
void Run::pullGadgetUpdate(){
  auto pending=gadgetCursor.next();
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Async tool updates belong to another event loop");
  stage=Stage::GadgetAwait;
  auto self=shared_from_this();
  pending.then([self](const std::optional<AsyncGadgetUpdate>* value,
      const std::exception_ptr& error){
    if(self->stage==Stage::Done)return;
    if(error){self->fail(error);return;}
    try{
      if(!*value){self->stage=Stage::AfterGadgets;self->pump();return;}
      self->currentGadgetUpdate=**value;
      if(self->currentGadgetUpdate->message){
        self->emit(self->currentGadgetUpdate->message,Stage::AfterGadgetYield);
        return;
      }
      self->afterGadgetYield();self->pump();
    }catch(...){self->fail(std::current_exception());}
  });
}
void Run::afterGadgetYield(){
  if(!currentGadgetUpdate)throw std::logic_error("Async tool update missing");
  if(currentGadgetUpdate->message){
    const Json& message=*currentGadgetUpdate->message;
    if(field(message,"type")=="attachment"&&
       field(field(message,"attachment"),"type")=="hook_stopped_continuation")
      shouldPreventContinuation=true;
    need(services.sync.normalizeMessagesForAPI,"normalizeMessagesForAPI");
    const Json normalized=services.sync.normalizeMessagesForAPI(
      Json::array({message}),prepared->gadgetUseContext.gadgets);
    if(normalized.is_array())for(const Json& item:normalized)
      if(field(item,"type")=="user")modelResult->gadgetResults.push_back(item);
  }
  if(currentGadgetUpdate->newContext){
    updatedContext=*currentGadgetUpdate->newContext;
    updatedContext.data["queryTracking"]=prepared->queryTracking;
  }
  currentGadgetUpdate.reset();pullGadgetUpdate();
}
void Run::afterGadgets(){
  services.queryCheckpoint("query_tool_execution_end");
  startSummary();
  stage=Stage::AfterSummary;
}
} // namespace batbox::query::async_gadget_turn
