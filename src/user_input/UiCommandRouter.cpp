#include "user_input/UiCommandRouter.hpp"
#include "commands/Roster.hpp"
#include <stdexcept>

namespace batbox::user_input {
UiCommandRouter::UiCommandRouter(event_loop::Loop& loop,Projection projection):
  loop_(loop),projection_(std::move(projection)){}
std::shared_ptr<UiCommandRouter> UiCommandRouter::create(event_loop::Loop& loop,
  Projection projection){
  auto router=std::shared_ptr<UiCommandRouter>(new UiCommandRouter(loop,
    std::move(projection)));
  router->initialize();
  return router;
}
void UiCommandRouter::initialize(){
  auto weak=weak_from_this();
  interactive::CommandUi::Callbacks callbacks;
  callbacks.onUiChanged=[weak](std::optional<interactive::UiRequest> request){
    if(auto self=weak.lock())self->projection_.onUiChanged(std::move(request));
  };
  callbacks.onSettled=[weak](const interactive::CommandDescriptor& descriptor,
    const interactive::CommandCompletion& completion,bool failed){
    if(auto self=weak.lock())self->settle(descriptor,completion,failed);
  };
  callbacks.onError=[weak](const std::string& error){
    if(auto self=weak.lock())self->projection_.onError(error);
  };
  callbacks.onQueueBlocked=[weak](bool value){
    if(auto self=weak.lock())self->projection_.onQueueBlocked(value);
  };
  callbacks.onPoetryFocus=[weak](bool value){
    if(auto self=weak.lock())self->projection_.onPoetryFocus(value);
  };
  if(!projection_.onUiChanged||!projection_.onError||
     !projection_.onQueueBlocked||!projection_.onPoetryFocus)
    throw std::invalid_argument("Command UI projection callbacks required");
  ui_=std::make_shared<interactive::CommandUi>(loop_,std::move(callbacks));
}
event_loop::Deferred<UiCompletion> UiCommandRouter::invoke(const Json& command,
  const std::string& args,interactive::CommandUi::Invoke call){
  if(!loop_.isOwnerThread())throw std::logic_error("UI invocation requires owner thread");
  if(pending_)throw std::logic_error("UI command already pending");
  event_loop::Deferred<UiCompletion> output(loop_);
  pending_=output;
  interactive::CommandDescriptor descriptor;
  descriptor.name=commands::getCommandName(command);
  descriptor.args=args;
  descriptor.immediate=value::property(command,"immediate")==true;
  descriptor.fullscreen=value::property(command,"fullscreen")==true;
  try{ui_->start(std::move(descriptor),std::move(call));}
  catch(...){pending_.reset();throw;}
  return output;
}
void UiCommandRouter::settle(const interactive::CommandDescriptor&,
  const interactive::CommandCompletion& completion,bool failed){
  if(!pending_)return;
  auto output=*pending_;
  pending_.reset();
  UiCompletion result;
  if(!failed){
    result.result=completion.result;
    switch(completion.options.display){
      case interactive::DoneOptions::Display::Default:result.display="user";break;
      case interactive::DoneOptions::Display::System:result.display="system";break;
      case interactive::DoneOptions::Display::Skip:result.display="skip";break;
    }
    result.shouldQuery=completion.options.shouldQuery;
    result.metaMessages=completion.options.metaMessages;
    result.nextInput=completion.options.nextInput;
    result.submitNextInput=completion.options.submitNextInput;
  }else result.display="skip";
  output.resolve(std::move(result));
}
CommandEntry bindUiCommand(Json descriptor,std::shared_ptr<UiCommandRouter> router,
  std::function<interactive::CommandUi::Invoke(const std::string&)> loadAndInvoke){
  if(!router||!loadAndInvoke)
    throw std::invalid_argument("UI command router and invocation required");
  CommandEntry entry;
  entry.descriptor=std::move(descriptor);
  entry.invokeUi=[router,command=entry.descriptor,loadAndInvoke=std::move(loadAndInvoke)]
    (const std::string& args){
    return router->invoke(command,args,loadAndInvoke(args));
  };
  return entry;
}
} // namespace batbox::user_input
