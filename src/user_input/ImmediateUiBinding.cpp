#include "user_input/ImmediateUiBinding.hpp"
#include "commands/Roster.hpp"
#include <stdexcept>

namespace batbox::user_input {
namespace {
struct Run {
  event_loop::Deferred<event_loop::Unit> ready;
  std::function<void(const interactive::CommandCompletion&)> onDone;
  bool done=false,callReturned=false,failed=false;
  Run(event_loop::Loop& loop,std::function<void(const interactive::CommandCompletion&)> callback):
    ready(loop),onDone(std::move(callback)){}
};
}
ImmediateUiInvoker bindImmediateUi(event_loop::Loop& loop,
  std::shared_ptr<interactive::CommandUi> ui,ImmediateUiLoader loader){
  if(!ui||!loader)throw std::invalid_argument("Immediate UI requires view and loader");
  return [&loop,ui=std::move(ui),loader=std::move(loader)](
    const CommandEntry& entry,const std::string& args,
    interactive::submit::ImmediateContext context,
    std::function<void(const interactive::CommandCompletion&)> onDone){
    if(!loop.isOwnerThread())throw std::logic_error("Immediate UI requires owner thread");
    if(!onDone)throw std::invalid_argument("Immediate UI onDone required");
    auto state=std::make_shared<Run>(loop,std::move(onDone));
    interactive::CommandDescriptor descriptor;
    descriptor.name=commands::getCommandName(entry.descriptor);
    descriptor.args=args;
    descriptor.immediate=true;
    descriptor.fullscreen=value::property(entry.descriptor,"fullscreen")==true;
    try{
      ui->start(std::move(descriptor),[state,entry,args,context=std::move(context),loader](
        interactive::CommandUi::Done done,
        interactive::CommandUi::Ready ready,
        interactive::CommandUi::Error error){
        try{
        auto call=loader(entry,args,context);
        if(!call)throw std::invalid_argument("Immediate UI command call required");
        call(
          [state,done,error](interactive::CommandCompletion completion){
            if(state->done||state->failed)return;
            state->done=true;
            try{state->onDone(completion);done(std::move(completion));}
            catch(const std::exception& failure){
              state->failed=true;error(failure.what());
              state->ready.reject(std::current_exception());
            }
          },
          [state,ready,done](std::optional<interactive::UiRequest> view,
            interactive::CommandUi::Action action){
            if(state->callReturned||state->failed)return;
            state->callReturned=true;
            if(view&&!state->done)ready(std::move(view),std::move(action));
            else if(!state->done)done(interactive::CommandCompletion{});
            state->ready.resolve(event_loop::Unit{});
          },
          [state,error](std::string failure){
            if(state->failed)return;
            state->failed=true;
            error(failure);
            state->ready.reject(std::make_exception_ptr(
              std::runtime_error(std::move(failure))));
          });
        }catch(...){
          state->failed=true;
          state->ready.reject(std::current_exception());
          throw;
        }
      });
    }catch(...){state->ready.reject(std::current_exception());}
    return state->ready;
  };
}
} // namespace batbox::user_input
