#include "query/AsyncEngineInternal.hpp"

namespace batbox::query::async_engine {
void Run::startModel(){
  model.emplace(makeAsyncModelPassIterator(loop,params,state,prepared,
    remaining,services.model));
  stage=Stage::ModelStart;
}
void Run::pullModel(){
  if(!model){startModel();}
  auto pending=model->next();
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Async model belongs to another event loop");
  stage=Stage::ModelAwait;
  auto self=shared_from_this();
  pending.then([self](const AsyncModelStep* value,const std::exception_ptr& error){
    if(self->stage==Stage::Done)return;
    if(error){self->fail(error);return;}
    try{
      if(value->done){
        if(!value->value)throw std::logic_error("Async model lost pass result");
        self->modelResult=value->value;
        self->model.reset();
        if(self->modelResult->terminal){self->finish(*self->modelResult->terminal);return;}
        self->stage=Stage::AfterModel;
        self->pump();
      }else self->emit(value->event,Stage::ModelStart,value->tombstoneTarget);
    }catch(...){self->fail(std::current_exception());}
  });
}
void Run::afterModel(){
  if(!modelResult->assistantMessages.empty()&&services.executePostSamplingHooks){
    services.executePostSamplingHooks(concatMessages(prepared->messagesForQuery,
      modelResult->assistantMessages),params,prepared->gadgetUseContext);
  }
  stage=prepared->gadgetUseContext.stop.stop_requested()?
    Stage::AbortStart:Stage::DecisionStart;
}
void Run::startDecision(){
  if(!decision){
    if(modelResult->needsFollowUp){
      need(services.executeGadgetTurn,"executeToolTurn");
      decision=services.executeGadgetTurn(params,state,prepared,modelResult,
        &memoryPrefetch,consumedCommandUuids);
    }else{
      need(services.completeWithoutGadgets,"completeWithoutTools");
      decision=services.completeWithoutGadgets(params,state,prepared,modelResult);
    }
  }
  need(decision->next,"async decision cursor.next");
  pullDecision();
}
void Run::pullDecision(){
  auto pending=decision->next();
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Async decision belongs to another event loop");
  stage=Stage::DecisionAwait;
  auto self=shared_from_this();
  pending.then([self](const AsyncDecisionStep* value,const std::exception_ptr& error){
    if(self->stage==Stage::Done)return;
    if(error){self->fail(error);return;}
    try{
      if(!value->done){self->emit(value->event,Stage::DecisionStart);return;}
      if(!value->value)throw std::logic_error("Async decision lost transition");
      if(value->value->terminal){self->finish(*value->value->terminal);return;}
      if(!value->value->next)
        throw std::logic_error("Async decision returned no transition");
      self->state=std::move(*value->value->next);
      self->decision.reset();self->modelResult.reset();self->prepared.reset();
      self->startSetup();self->pump();
    }catch(...){self->fail(std::current_exception());}
  });
}
} // namespace batbox::query::async_engine
