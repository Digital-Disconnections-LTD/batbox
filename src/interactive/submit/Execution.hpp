#pragma once

#include "interactive/submit/PoetrySubmit.hpp"
#include <memory>

namespace batbox::interactive::submit {
struct ExecutionRun:std::enable_shared_from_this<ExecutionRun> {
  PoetrySubmitRequest request;
  CommandList commands;
  std::shared_ptr<SubmitServices> services;
  event_loop::Deferred<event_loop::Unit> result;
  std::shared_ptr<TurnAbort> abort;
  TurnMessagesRef newMessages=std::make_shared<TurnMessages>();
  std::shared_ptr<user_input::UserInputServices> currentInputServices;
  std::size_t index=0;
  bool shouldQuery=false,finished=false;
  std::vector<std::string> allowedGadgets;
  std::optional<std::string> model,effort,nextInput;
  std::optional<bool> submitNextInput;
  ExecutionRun(PoetrySubmitRequest request,CommandList commands,
    std::shared_ptr<SubmitServices> services);
  void start();
  void step();
  void accept(const user_input::InputResult& value);
  void finishInputs();
  void afterQuery();
  void finish(std::exception_ptr error={});
  std::optional<std::string> turnWorkload()const;
  template<class T,class F>void await(event_loop::Deferred<T> pending,F next){
    if(!services->loop.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Submit awaited another owner loop");
    auto self=shared_from_this();
    pending.then([self,next=std::move(next)](const T* value,
                                            const std::exception_ptr& error){
      if(error){self->finish(error);return;}
      try{
        if(!value)throw std::logic_error("Submit Promise has no value");
        next(*value);
      }catch(...){self->finish(std::current_exception());}
    });
  }
};
} // namespace batbox::interactive::submit
