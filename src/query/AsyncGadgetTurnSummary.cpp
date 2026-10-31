#include "query/AsyncGadgetTurnInternal.hpp"

namespace batbox::query::async_gadget_turn {
namespace {
Json summaryInfo(const ModelPassResult& result){
  Json infos=Json::array();
  for(const Json& block:result.gadgetUseBlocks){
    Json output;
    bool found=false;
    const Json& id=field(block,"id");
    for(const Json& message:result.gadgetResults){
      if(field(message,"type")!="user")continue;
      const Json& content=field(field(message,"message"),"content");
      if(!content.is_array())continue;
      for(const Json& item:content)
        if(field(item,"type")=="tool_result"&&field(item,"tool_use_id")==id){
          if(item.contains("content"))output=field(item,"content");
          found=true;
          break;
        }
      if(found)break;
    }
    infos.push_back(Json{{"name",field(block,"name")},
      {"input",field(block,"input")},{"output",output}});
  }
  return infos;
}
}
void Run::startSummary(){
  if(!config.emitGadgetUseSummaries||modelResult->gadgetUseBlocks.empty()||
     prepared->gadgetUseContext.stop.stop_requested()||
     value::truthy(field(prepared->gadgetUseContext.data,"agentId")))return;
  need(services.generateGadgetUseSummary,"generateToolUseSummary");
  need(services.sync.createGadgetUseSummaryMessage,"createToolUseSummaryMessage");
  Json ids=Json::array();
  for(const Json& block:modelResult->gadgetUseBlocks)ids.push_back(field(block,"id"));
  Json args={{"tools",summaryInfo(*modelResult)},
    {"isNonInteractiveSession",field(field(prepared->gadgetUseContext.data,
      "options"),"isNonInteractiveSession")}};
  if(!modelResult->assistantMessages.empty()){
    const Json& content=field(field(modelResult->assistantMessages.back(),
      "message"),"content");
    if(content.is_array())for(const Json& block:content)
      if(field(block,"type")=="text"&&block.contains("text"))
        args["lastAssistantText"]=field(block,"text");
  }
  auto pending=services.generateGadgetUseSummary(args,prepared->gadgetUseContext);
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Async tool summary belongs to another event loop");
  nextPendingSummary=std::make_shared<event_loop::Deferred<Json>>(loop);
  auto self=shared_from_this();
  pending.then([self,ids](const std::optional<std::string>* summary,
      const std::exception_ptr& error){
    if(error||!summary||!*summary||(*summary)->empty()){
      self->nextPendingSummary->resolve(Json());return;
    }
    try{self->nextPendingSummary->resolve(
      self->services.sync.createGadgetUseSummaryMessage(**summary,ids));}
    catch(...){self->nextPendingSummary->resolve(Json());}
  });
}
void Run::afterSummary(){
  if(prepared->gadgetUseContext.stop.stop_requested()){
    startAbortCleanup();return;
  }
  if(shouldPreventContinuation){
    finish(CompletionDecision{{},Json{{"reason","hook_stopped"}}});return;
  }
  if(value::truthy(field(prepared->tracking,"compacted"))){
    const Json& previous=field(prepared->tracking,"turnCounter");
    prepared->tracking["turnCounter"]=(previous.is_number_integer()?
      previous.get<int>():0)+1;
  }
  startAttachments();
}
void Run::startAbortCleanup(){
  need(services.sync.feature,"feature");
  if(services.sync.feature("CHICAGO_MCP")&&
     !value::truthy(field(prepared->gadgetUseContext.data,"agentId"))){
    need(services.cleanupComputerUseAfterTurn,"cleanupComputerUseAfterTurn");
    event_loop::Deferred<event_loop::Unit> pending=[&]{
      try{return services.cleanupComputerUseAfterTurn(prepared->gadgetUseContext);}
      catch(...){event_loop::Deferred<event_loop::Unit> swallowed(loop);
        swallowed.resolve({});return swallowed;}
    }();
    if(!loop.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Async tool cleanup belongs to another event loop");
    stage=Stage::AbortCleanupAwait;
    auto self=shared_from_this();
    pending.then([self](const event_loop::Unit*,const std::exception_ptr&){
      if(self->stage==Stage::Done)return;
      self->stage=Stage::AbortInterruption;self->pump();
    });
  }else stage=Stage::AbortInterruption;
}
void Run::emitAbortInterruption(){
  need(services.sync.currentAbortReason,"currentAbortReason");
  if(services.sync.currentAbortReason()!="interrupt"){
    need(services.sync.createUserInterruptionMessage,
      "createUserInterruptionMessage");
    emit(std::make_shared<Json>(services.sync.createUserInterruptionMessage(true)),
      Stage::AbortMaxTurns);
  }else stage=Stage::AbortMaxTurns;
}
void Run::abortMaxTurns(){
  const int nextTurn=state.turnCount+1;
  if(params.maxTurns&&*params.maxTurns!=0&&nextTurn>*params.maxTurns){
    need(services.sync.createAttachmentMessage,"createAttachmentMessage");
    emit(std::make_shared<Json>(services.sync.createAttachmentMessage(
      Json{{"type","max_turns_reached"},{"maxTurns",*params.maxTurns},
        {"turnCount",nextTurn}})),Stage::AfterAbortMax);
  }else stage=Stage::AbortTerminal;
}
} // namespace batbox::query::async_gadget_turn
