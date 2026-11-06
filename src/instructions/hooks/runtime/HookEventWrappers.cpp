#include "instructions/hooks/runtime/HookEventWrappers.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
HookYield closedYield(std::optional<Json> value={}){
  return {true,value.value_or(Json(nullptr)),
    value?std::vector<std::string>{}:std::vector<std::string>{"value"}};
}
}
HookEventGenerator::HookEventGenerator(event_loop::Loop& loop,
  std::function<std::optional<ExecuteHooksInput>()> prepare,
  std::function<HookYieldCursor(ExecuteHooksInput)> executeHooks):
  loop_(loop),prepare_(std::move(prepare)),
  executeHooks_(std::move(executeHooks)){}
event_loop::Deferred<HookYield> HookEventGenerator::next(){
  if(!loop_.isOwnerThread())throw std::logic_error("Hook wrapper owner required");
  event_loop::Deferred<HookYield> output(loop_);
  if(closed_){output.resolve(closedYield());return output;}
  try{
    if(!cursor_){
      if(!prepare_||!executeHooks_)
        throw std::invalid_argument("Hook wrapper executor import required");
      auto invocation=prepare_();
      if(!invocation){closed_=true;output.resolve(closedYield());return output;}
      cursor_=executeHooks_(std::move(*invocation));
    }
    return cursor_->next();
  }catch(...){closed_=true;output.reject(std::current_exception());return output;}
}
event_loop::Deferred<HookYield> HookEventGenerator::returnValue(
  std::optional<Json> value){
  if(!loop_.isOwnerThread())throw std::logic_error("Hook wrapper owner required");
  if(cursor_)return cursor_->returnValue(std::move(value));
  closed_=true;
  event_loop::Deferred<HookYield> output(loop_);
  output.resolve(closedYield(std::move(value)));
  return output;
}
} // namespace batbox::instructions::hooks::runtime
