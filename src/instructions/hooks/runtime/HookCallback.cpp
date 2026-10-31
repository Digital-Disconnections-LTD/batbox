#include "instructions/hooks/runtime/HookCallback.hpp"
#include "instructions/hooks/runtime/OutsideRepl.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
event_loop::Deferred<HookCallbackResult> executeHookCallback(
  event_loop::Loop& loop,const HookCallbackCall& call,
  const ProcessHookJsonServices& processor){
  if(!loop.isOwnerThread())throw std::logic_error("Hook callback owner required");
  event_loop::Deferred<HookCallbackResult> output(loop);
  try{
    if(!call.hook||!call.hook->callback)
      throw std::invalid_argument("Hook callback import required");
    std::optional<HookCallbackContext> context;
    if(call.gadgetUseContext)context=HookCallbackContext{
      call.gadgetUseContext->getAppState,
      call.gadgetUseContext->updateAttributionState};
    auto reply=call.hook->callback(call.hookInput,call.gadgetUseID,
      call.signal,call.hookIndex,context);
    event_loop::Deferred<Json> callbackResult(loop);
    if(std::holds_alternative<Json>(reply))
      callbackResult.resolve(std::get<Json>(std::move(reply)));
    else callbackResult=std::get<event_loop::Deferred<Json>>(std::move(reply));
    const auto hook=call.hook;
    const auto gadgetUseID=call.gadgetUseID,hookEvent=call.hookEvent;
    callbackResult.then([output,hook,gadgetUseID,hookEvent,processor](
      const Json* json,const std::exception_ptr& failure){
      if(failure){output.reject(failure);return;}
      try{
        HookCallbackResult result;result.hook=hook;
        if(!isAsyncHookJsonOutput(*json)){
          ProcessHookJsonInput input;
          input.json=*json;input.command="callback";
          input.hookName=hookEvent+":Callback";
          input.gadgetUseID=gadgetUseID;input.hookEvent=hookEvent;
          input.expectedHookEvent=hookEvent;
          auto processed=processHookJSONOutput(input,processor);
          result.fields=std::move(processed.fields);
          result.ownUndefinedKeys=std::move(processed.ownUndefinedKeys);
        }
        result.fields["outcome"]="success";
        output.resolve(std::move(result));
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::instructions::hooks::runtime
