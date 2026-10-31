#include "attachments/Aggregate.hpp"
#include <deque>
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::attachments {
namespace {
using Item=std::optional<model::MessageRef>;
struct CursorState:std::enable_shared_from_this<CursorState> {
  Input input;
  execution::Context& context;
  Services services;
  std::deque<event_loop::Deferred<Item>> waiting;
  Json attachments=Json::array();
  std::exception_ptr error;
  std::size_t index=0;
  bool started=false,ready=false,closed=false;
  CursorState(Input args,execution::Context& turn,Services imports):
    input(std::move(args)),context(turn),services(std::move(imports)){}
  void start(){
    if(started)return;
    started=true;
    try{
      getAttachments(input,context,services).then(
        [self=shared_from_this()](const Json* value,
          const std::exception_ptr& failure){
          self->error=failure;
          if(value)self->attachments=*value;
          self->ready=true;
          self->drain();
        });
    }catch(...){error=std::current_exception();ready=true;drain();}
  }
  void drain(){
    if(!ready&&!closed)return;
    while(!waiting.empty()){
      auto next=waiting.front();waiting.pop_front();
      if(closed){next.resolve(std::nullopt);continue;}
      if(error){next.reject(error);closed=true;continue;}
      if(index>=attachments.size()){
        closed=true;next.resolve(std::nullopt);continue;
      }
      try{
        if(!services.randomUuid||!services.nowIso)
          throw std::invalid_argument("Attachment message identity imports required");
        auto message=interactive::background::createAttachmentMessage(
          attachments[index++],services.randomUuid,services.nowIso);
        next.resolve(Item(std::move(message)));
      }catch(...){error=std::current_exception();closed=true;next.reject(error);}
    }
  }
};
}
MessageCursor getAttachmentMessages(Input input,
  execution::Context& context,const Services& services){
  auto state=std::make_shared<CursorState>(std::move(input),context,services);
  MessageCursor cursor;
  cursor.next=[state]{
    if(!state->services.loop.isOwnerThread())
      throw std::logic_error("Attachment generator requires owner loop");
    event_loop::Deferred<Item> output(state->services.loop);
    state->waiting.push_back(output);
    if(!state->closed)state->start();
    state->drain();
    return output;
  };
  cursor.close=[state]{
    if(!state->services.loop.isOwnerThread())
      throw std::logic_error("Attachment generator requires owner loop");
    state->closed=true;state->drain();
    event_loop::Deferred<event_loop::Unit> result(state->services.loop);
    result.resolve(event_loop::Unit{});return result;
  };
  return cursor;
}
} // namespace batbox::attachments
