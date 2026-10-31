#include "user_input/InputInternal.hpp"
#include "instructions/hooks/runtime/HookBlockingMessages.hpp"
#include "messages/TextContent.hpp"
#include "user_input/CommandMessages.hpp"
#include <stdexcept>

namespace batbox::user_input::internal {
namespace {
using instructions::hooks::runtime::HookEventGenerator;
std::string interpolate(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_array()){
    std::string joined;
    bool first=true;
    for(const auto& item:value){
      if(!first)joined+=',';
      first=false;
      joined+=interpolate(item);
    }
    return joined;
  }
  if(value.is_object())return "[object Object]";
  if(value.is_null())return "null";
  return value::stringify(value);
}
std::string truncateHookOutput(const std::string& content){
  constexpr double maxLength=10000;
  if(value::stringLength(content)<=maxLength)return content;
  return value::substring(content,0,maxLength)+
    "… [output truncated - exceeded 10000 characters]";
}
void closeWithResult(const std::shared_ptr<InputState>& state,
    const std::shared_ptr<HookEventGenerator>& hooks,InputResult result){
  try{
    hooks->returnValue().then([state,result=std::move(result)](
        const instructions::hooks::runtime::HookYield*,
        const std::exception_ptr& error)mutable{
      if(error)fail(state,error);
      else state->result.resolve(std::move(result));
    });
  }catch(...){fail(state,std::current_exception());}
}
void closeWithError(const std::shared_ptr<InputState>& state,
    const std::shared_ptr<HookEventGenerator>& hooks,std::exception_ptr failure){
  try{
    hooks->returnValue().then([state,failure](
        const instructions::hooks::runtime::HookYield*,
        const std::exception_ptr&){fail(state,failure);});
  }catch(...){fail(state,failure);}
}
void nextHook(const std::shared_ptr<InputState>& state,
  const std::shared_ptr<HookEventGenerator>& hooks,
  const std::shared_ptr<InputResult>& result){
  try{
    auto pending=hooks->next();
    pending.then([state,hooks,result]
      (const instructions::hooks::runtime::HookYield* step,
       const std::exception_ptr& error){
      if(error){fail(state,error);return;}
      try{
        if(step->done){
          checkpoint(*state,"query_hooks_end");
          state->result.resolve(*result);return;
        }
        const auto& hook=step->value;
        const auto& message=value::property(hook,"message");
        if(value::property(message,"type")=="progress"){
          nextHook(state,hooks,result);return;
        }
        const auto& blocked=value::property(hook,"blockingError");
        if(value::truthy(blocked)){
          instructions::hooks::runtime::HookBlockingError detail;
          detail.blockingError=blocked.is_string()?blocked.get<std::string>():
            text(value::property(blocked,"blockingError"));
          detail.command=text(value::property(blocked,"command"));
          const auto body=instructions::hooks::runtime::
            getUserPoetrySubmitHookBlockingMessage(detail);
          InputResult stopped;
          stopped.messages.push_back(createSystemMessage(body+
            "\n\nOriginal prompt: "+interpolate(state->request.input),
            "warning",state->services.messages));
          stopped.allowedGadgets=result->allowedGadgets;
          closeWithResult(state,hooks,std::move(stopped));return;
        }
        if(value::truthy(value::property(hook,"preventContinuation"))){
          const auto reason=text(value::property(hook,"stopReason"));
          result->messages.push_back(messages::createUserMessage(Json{{"content",
            reason.empty()?"Operation stopped by hook":
              "Operation stopped by hook: "+reason}},state->services.messages));
          result->shouldQuery=false;
          closeWithResult(state,hooks,*result);return;
        }
        const auto& extra=value::property(hook,"additionalContexts");
        if(extra.is_array()&&!extra.empty()){
          if(!state->services.messages.randomUUID||!state->services.messages.nowISO)
            throw std::invalid_argument("Missing attachment identity binding");
          Json contents=Json::array();
          for(const auto& item:extra)contents.push_back(truncateHookOutput(interpolate(item)));
          Json attachment={{"type","hook_additional_context"},
            {"content",contents},{"hookName","UserPromptSubmit"},
            {"toolUseID","hook-"+state->services.messages.randomUUID()},
            {"hookEvent","UserPromptSubmit"}};
          result->messages.push_back(Json{{"attachment",attachment},{"type","attachment"},
            {"uuid",state->services.messages.randomUUID()},
            {"timestamp",state->services.messages.nowISO()}});
        }
        if(!message.is_null()){
          if(value::property(value::property(message,"attachment"),"type")=="hook_success"){
            const auto& content=value::property(value::property(message,"attachment"),"content");
            if(value::truthy(content)){
              Json adjusted=message;
              adjusted["attachment"]["content"]=truncateHookOutput(interpolate(content));
              result->messages.push_back(std::move(adjusted));
            }
          }else result->messages.push_back(message);
        }
        nextHook(state,hooks,result);
      }catch(...){closeWithError(state,hooks,std::current_exception());}
    });
  }catch(...){fail(state,std::current_exception());}
}
}
void runHooks(const std::shared_ptr<InputState>& state,InputResult result){
  try{
    checkpoint(*state,"query_hooks_start");
    if(!state->services.executeUserPoetrySubmitHooks)
      throw std::invalid_argument("Missing user input binding: executeUserPromptSubmitHooks");
    const auto input=messages::getContentText(state->request.input).value_or("");
    auto hooks=std::make_shared<HookEventGenerator>(
      state->services.executeUserPoetrySubmitHooks(input,
        state->permissionMode.value_or(""),
        state->request.abort));
    nextHook(state,hooks,std::make_shared<InputResult>(std::move(result)));
  }catch(...){fail(state,std::current_exception());}
}
} // namespace batbox::user_input::internal
