#include "instructions/hooks/runtime/HookPerHook.hpp"
#include <deque>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
const Json& field(const Json& object,const char* key){
  return value::property(object,key);
}
std::string stringField(const Json& object,const char* key){
  const auto& value=field(object,key);
  return value.is_string()?value.get<std::string>():std::string{};
}
double hookTimeout(const Json& hook,double fallback){
  const auto& value=field(hook,"timeout");
  return value::truthy(value)?value.get<double>()*1000:fallback;
}
struct State:std::enable_shared_from_this<State>{
  event_loop::Loop& loop;
  HookPerHookInput input;
  HookPerHookServices services;
  std::deque<event_loop::Deferred<GeneratorStep<HookExecutionResult>>> demands;
  std::shared_ptr<CombinedAbort> combined;
  enum class Stage { Initial,Waiting,Yielded,Closed } stage=Stage::Initial;
  State(event_loop::Loop& owner,HookPerHookInput call,HookPerHookServices imports):
    loop(owner),input(std::move(call)),services(std::move(imports)){}
  void done(){
    stage=Stage::Closed;
    while(!demands.empty()){
      auto next=demands.front();demands.pop_front();
      next.resolve(GeneratorStep<HookExecutionResult>{true,{}});
    }
  }
  void fail(std::exception_ptr failure){
    if(combined)combined->cleanup();
    if(!demands.empty()){
      auto next=demands.front();demands.pop_front();next.reject(failure);
    }
    done();
  }
  void yield(HookExecutionResult result){
    stage=Stage::Yielded;
    auto next=demands.front();demands.pop_front();
    next.resolve(GeneratorStep<HookExecutionResult>{false,std::move(result)});
    while(!demands.empty())done();
  }
  void callback(){
    const auto& hook=input.match.hook->fields;
    combined=CombinedAbort::create(loop,input.signal,std::nullopt,
      hookTimeout(hook,input.timeoutMs));
    if(!input.match.callback)
      throw std::invalid_argument("Hook callback import required");
    HookCallbackCall call;
    call.gadgetUseID=input.gadgetUseID;call.hookEvent=input.hookEvent;
    call.hookInput=input.hookInput;call.signal=combined->signal();
    call.hookIndex=input.hookIndex;call.hook=input.match.callback;
    if(input.gadgetUseContext)call.gadgetUseContext=HookCallbackContext{
      input.gadgetUseContext->getAppState,
      input.gadgetUseContext->updateAttributionState};
    stage=Stage::Waiting;
    const auto weak=weak_from_this();
    executeHookCallback(loop,call,services.processJson).then(
      [weak](const HookCallbackResult* value,const std::exception_ptr& failure){
        if(auto state=weak.lock()){
          state->combined->cleanup();
          if(failure){state->fail(failure);return;}
          HookExecutionResult result;
          result.fields=value->fields;
          result.outcome=stringField(result.fields,"outcome");
          result.hook=state->input.match.hook;
          state->yield(std::move(result));
        }
      });
  }
  void function(){
    if(!input.messages){
      if(!services.createAttachmentMessage)
        throw std::invalid_argument("Hook attachment factory required");
      const Json attachment={{"type","hook_error_during_execution"},
        {"hookName",input.hookName},{"toolUseID",input.gadgetUseID},
        {"hookEvent",input.hookEvent},
        {"content","Messages not provided for function hook"}};
      HookExecutionResult result;
      result.fields={{"message",services.createAttachmentMessage(attachment)},
        {"outcome","non_blocking_error"}};
      result.outcome="non_blocking_error";result.hook=input.match.hook;
      yield(std::move(result));return;
    }
    if(!services.functionHost)
      throw std::invalid_argument("Function hook host required");
    if(!input.match.function)
      throw std::invalid_argument("Function hook callback import required");
    FunctionHookCall call;
    call.hook=input.match.function;call.messages=*input.messages;
    call.hookName=input.hookName;call.gadgetUseID=input.gadgetUseID;
    call.hookEvent=input.hookEvent;call.timeoutMs=input.timeoutMs;
    call.signal=input.signal;
    stage=Stage::Waiting;
    const auto weak=weak_from_this();
    services.functionHost->execute(std::move(call)).then(
      [weak](const FunctionHookResult* value,const std::exception_ptr& failure){
        if(auto state=weak.lock()){
          if(failure){state->fail(failure);return;}
          HookExecutionResult result;
          result.fields={{"outcome",value->outcome}};
          if(value->message)result.fields["message"]=*value->message;
          if(value->blockingError)result.fields["blockingError"]={{
            "blockingError",value->blockingError->blockingError},
            {"command",value->blockingError->command}};
          result.outcome=value->outcome;
          result.hook=state->input.match.hook;
          state->yield(std::move(result));
        }
      });
  }
  void start(){
    try{
      if(!input.match.hook)
        throw std::invalid_argument("Hook identity required");
      const auto type=stringField(input.match.hook->fields,"type");
      if(type=="callback"){callback();return;}
      if(type=="function"){function();return;}
      throw std::invalid_argument("Unknown hook type");
    }catch(...){fail(std::current_exception());}
  }
  event_loop::Deferred<GeneratorStep<HookExecutionResult>> next(){
    if(!loop.isOwnerThread())throw std::logic_error("Hook generator owner required");
    event_loop::Deferred<GeneratorStep<HookExecutionResult>> output(loop);
    if(stage==Stage::Closed||stage==Stage::Yielded){
      stage=Stage::Closed;
      output.resolve(GeneratorStep<HookExecutionResult>{true,{}});
    }else{
      demands.push_back(output);
      if(stage==Stage::Initial)start();
    }
    return output;
  }
};
}
GeneratorNext<HookExecutionResult> makeHookPerHookGenerator(
  event_loop::Loop& loop,HookPerHookInput input,HookPerHookServices services){
  if(input.match.hook&&stringField(input.match.hook->fields,"type")=="command")
    return makeHookCommandGenerator(loop,std::move(input),std::move(services));
  if(input.match.hook){
    const auto type=stringField(input.match.hook->fields,"type");
    if(type=="prompt"||type=="agent")
      return makeHookModelGenerator(loop,std::move(input),std::move(services));
    if(type=="http")
      return makeHookHttpGenerator(loop,std::move(input),std::move(services));
  }
  auto state=std::make_shared<State>(loop,std::move(input),std::move(services));
  return [state]{return state->next();};
}
} // namespace batbox::instructions::hooks::runtime
