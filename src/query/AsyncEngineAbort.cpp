#include "query/AsyncEngineInternal.hpp"

namespace batbox::query::async_engine {
namespace {
std::vector<Json> missingGadgetArgs(const Json& assistants){
  std::vector<Json> out;
  if(!assistants.is_array())return out;
  for(const Json& assistant:assistants){
    const Json& content=field(field(assistant,"message"),"content");
    if(!content.is_array())throw std::invalid_argument("Assistant content must be an array");
    for(const Json& block:content)if(field(block,"type")=="tool_use"){
      const Json result={{"type","tool_result"},
        {"content","Interrupted by user"},{"is_error",true},
        {"tool_use_id",field(block,"id")}};
      out.push_back(Json{{"content",Json::array({result})},
        {"toolUseResult","Interrupted by user"},
        {"sourceToolAssistantUUID",field(assistant,"uuid")}});
    }
  }
  return out;
}
}
void Run::startAbort(){
  if(prepared->streamingGadgetExecutor){
    if(!abortCursor){
      need(services.abortRemainingResults,"abortRemainingResults");
      abortCursor=services.abortRemainingResults(prepared);
      need(abortCursor->next,"abortRemainingResults cursor.next");
    }
    pullAbort();
  }else{
    missingArgs=missingGadgetArgs(modelResult->assistantMessages);
    missingIndex=0;stage=Stage::AbortMissing;
  }
}
void Run::pullAbort(){
  auto pending=abortCursor->next();
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Async abort results belong to another event loop");
  stage=Stage::AbortAwait;
  auto self=shared_from_this();
  pending.then([self](const std::optional<model::MessageRef>* value,
      const std::exception_ptr& error){
    if(self->stage==Stage::Done)return;
    if(error){self->fail(error);return;}
    try{
      if(*value){self->emit(**value,Stage::AbortStart);return;}
      self->stage=Stage::AbortCleanup;self->pump();
    }catch(...){self->fail(std::current_exception());}
  });
}
void Run::emitMissing(){
  if(missingIndex<missingArgs.size()){
    need(services.model.sync.createUserMessage,"createUserMessage");
    emit(std::make_shared<Json>(services.model.sync.createUserMessage(
      missingArgs[missingIndex++])),Stage::AbortMissing);
    return;
  }
  stage=Stage::AbortCleanup;
}
void Run::startAbortCleanup(){
  need(services.setup.sync.feature,"feature");
  if(services.setup.sync.feature("CHICAGO_MCP")&&
     !value::truthy(field(prepared->gadgetUseContext.data,"agentId"))){
    need(services.cleanupComputerUseAfterTurn,"cleanupComputerUseAfterTurn");
    event_loop::Deferred<event_loop::Unit> pending=[&]{
      try{return services.cleanupComputerUseAfterTurn(prepared->gadgetUseContext);}
      catch(...){event_loop::Deferred<event_loop::Unit> swallowed(loop);
        swallowed.resolve({});return swallowed;}
    }();
    if(!loop.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Async computer cleanup belongs to another event loop");
    stage=Stage::AbortCleanupAwait;
    auto self=shared_from_this();
    pending.then([self](const event_loop::Unit*,const std::exception_ptr&){
      if(self->stage==Stage::Done)return;
      self->stage=Stage::AbortInterruption;
      self->pump();
    });
  }else stage=Stage::AbortInterruption;
}
void Run::emitAbortInterruption(){
  need(services.gadgetTurn.currentAbortReason,"currentAbortReason");
  if(services.gadgetTurn.currentAbortReason()!="interrupt"){
    need(services.gadgetTurn.createUserInterruptionMessage,
      "createUserInterruptionMessage");
    emit(std::make_shared<Json>(services.gadgetTurn.createUserInterruptionMessage(false)),
      Stage::AbortTerminal);
  }else stage=Stage::AbortTerminal;
}
} // namespace batbox::query::async_engine
