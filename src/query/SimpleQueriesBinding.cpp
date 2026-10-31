#include "query/SimpleQueriesBinding.hpp"
#include <memory>
#include <stdexcept>

namespace batbox::query {
event_loop::Deferred<model::MessageRef> collectLastAssistantMessage(
  event_loop::Loop& loop,AsyncModelCursor cursor,std::stop_token signal){
  struct Run:std::enable_shared_from_this<Run> {
    event_loop::Deferred<model::MessageRef> output;
    AsyncModelCursor cursor;std::stop_token signal;
    event_loop::Loop::KeepAlive hold;
    model::MessageRef assistant;
    Run(event_loop::Loop& loop,AsyncModelCursor input,std::stop_token stop):
      output(loop),cursor(std::move(input)),signal(stop),hold(loop.hold()){}
    void next(){
      auto self=shared_from_this();
      try{
        cursor.next().then([self](const std::optional<model::MessageRef>* event,
          const std::exception_ptr& error){
          if(error){self->hold.release();self->output.reject(error);return;}
          if(!*event){
            self->hold.release();
            if(self->assistant)self->output.resolve(self->assistant);
            else if(self->signal.stop_requested())self->output.reject(
              std::make_exception_ptr(model::UserAbortError()));
            else self->output.reject(std::make_exception_ptr(
              std::runtime_error("No assistant message found")));
            return;
          }
          if(value::property(***event,"type")=="assistant")self->assistant=**event;
          self->next();
        });
      }catch(...){hold.release();output.reject(std::current_exception());}
    }
  };
  auto run=std::make_shared<Run>(loop,std::move(cursor),signal);
  run->next();return run->output;
}
std::function<event_loop::Deferred<model::MessageRef>(model::SimpleQueryRequest)>
bindSimpleQueryModel(event_loop::Loop& loop,SimpleQueryFactory factory,
  OwnerPreflightAwaits awaitedImports,OwnerRequestAwaits requestAwaits){
  if(!factory)throw std::invalid_argument("Simple query native services factory required");
  return [&loop,factory=std::move(factory),awaitedImports=std::move(awaitedImports),
    requestAwaits=std::move(requestAwaits)](model::SimpleQueryRequest request){
    auto services=factory(request);services.signal=request.signal;
    auto cursor=makeOwnerSegmentQueryModelCursor(loop,std::move(request.input),
      std::move(services),awaitedImports,{},requestAwaits);
    return collectLastAssistantMessage(loop,std::move(cursor),request.signal);
  };
}
}
