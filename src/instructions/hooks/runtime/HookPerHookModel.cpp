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
struct State:std::enable_shared_from_this<State>{
  enum class Stage { Initial,Waiting,Yielded,Closed } stage=Stage::Initial;
  event_loop::Loop& loop;
  HookPerHookInput input;
  HookPerHookServices services;
  std::deque<event_loop::Deferred<GeneratorStep<HookExecutionResult>>> demands;
  std::shared_ptr<CombinedAbort> combined;
  std::string hookId,hookCommand,hookType;
  double hookStartMs=0;
  bool cleanupOnReturn=false;
  State(event_loop::Loop& owner,HookPerHookInput call,HookPerHookServices imports):
    loop(owner),input(std::move(call)),services(std::move(imports)){}
  void require(bool supplied,const char* message){
    if(!supplied)throw std::invalid_argument(message);
  }
  void cleanup(){if(combined)combined->cleanup();}
  void done(){
    if(cleanupOnReturn){cleanupOnReturn=false;cleanup();}
    stage=Stage::Closed;
    while(!demands.empty()){
      auto next=demands.front();demands.pop_front();
      next.resolve(GeneratorStep<HookExecutionResult>{true,{}});
    }
  }
  void fail(std::exception_ptr failure){
    if(!demands.empty()){
      auto next=demands.front();demands.pop_front();next.reject(failure);
    }
    done();
  }
  void yield(HookExecutionResult result){
    stage=Stage::Yielded;
    auto next=demands.front();demands.pop_front();
    next.resolve(GeneratorStep<HookExecutionResult>{false,std::move(result)});
    if(!demands.empty())done();
  }
  HookExecutionResult attachment(const Json& payload,
    const std::string& outcome){
    require(static_cast<bool>(services.createAttachmentMessage),
      "Hook attachment factory required");
    HookExecutionResult result;
    result.fields={{"message",services.createAttachmentMessage(payload)},
      {"outcome",outcome}};
    result.outcome=outcome;result.hook=input.match.hook;
    return result;
  }
  Json base(const char* type){
    return {{"type",type},{"hookName",input.hookName},
      {"toolUseID",input.gadgetUseID},{"hookEvent",input.hookEvent}};
  }
  double duration(){
    require(static_cast<bool>(services.nowMillis),"Hook clock import required");
    return services.nowMillis()-hookStartMs;
  }
  void onError(std::exception_ptr failure){
    try{
      cleanup();
      require(static_cast<bool>(services.errorMessage),
        "Hook thrown-error formatter required");
      const auto message="Failed to run: "+services.errorMessage(failure);
      require(static_cast<bool>(services.emitHookResponse),
        "Hook response event import required");
      services.emitHookResponse({{"hookId",hookId},
        {"hookName",input.hookName},{"hookEvent",input.hookEvent},
        {"output",message},{"stdout",""},{"stderr",message},
        {"exitCode",1},{"outcome","error"}});
      auto payload=base("hook_non_blocking_error");
      payload["stderr"]=message;payload["stdout"]="";
      payload["exitCode"]=1;payload["command"]=hookCommand;
      payload["durationMs"]=duration();
      yield(attachment(payload,"non_blocking_error"));
    }catch(...){fail(std::current_exception());}
  }
  void onResult(HookExecutionResult result){
    try{
      const auto& message=field(result.fields,"message");
      if(value::truthy(message)&&stringField(message,"type")=="attachment"){
        auto& attachment=result.fields["message"]["attachment"];
        const auto type=stringField(attachment,"type");
        if(type=="hook_success"||type=="hook_non_blocking_error"){
          attachment["command"]=hookCommand;
          attachment["durationMs"]=duration();
        }
      }
      cleanupOnReturn=true;
      yield(std::move(result));
    }catch(...){onError(std::current_exception());}
  }
  void start(){
    if(!input.match.hook){fail(std::make_exception_ptr(
      std::invalid_argument("Hook identity required")));return;}
    const auto& hook=input.match.hook->fields;
    hookType=stringField(hook,"type");
    try{
      const auto& timeout=field(hook,"timeout");
      combined=CombinedAbort::create(loop,input.signal,std::nullopt,
        value::truthy(timeout)?timeout.get<double>()*1000:input.timeoutMs);
      require(static_cast<bool>(services.randomUUID),"Hook UUID import required");
      hookId=services.randomUUID();
      require(static_cast<bool>(services.nowMillis),"Hook clock import required");
      hookStartMs=services.nowMillis();
      require(static_cast<bool>(services.getHookDisplayText),
        "Hook display import required");
      hookCommand=services.getHookDisplayText(input.match);
    }catch(...){fail(std::current_exception());return;}
    try{
      require(static_cast<bool>(services.jsonInput),
        "Hook lazy JSON input required");
      const auto& prepared=services.jsonInput->get();
      if(!prepared.ok){
        require(static_cast<bool>(services.errorMessage),
          "Hook thrown-error formatter required");
        auto payload=base("hook_error_during_execution");
        payload["content"]="Failed to prepare hook input: "+
          services.errorMessage(prepared.error);
        payload["command"]=hookCommand;
        payload["durationMs"]=duration();
        cleanupOnReturn=true;
        yield(attachment(payload,"non_blocking_error"));return;
      }
      if(!input.gadgetUseContext){
        throw std::runtime_error(hookType=="prompt"?
          "ToolUseContext is required for prompt hooks. This is a bug.":
          "ToolUseContext is required for agent hooks. This is a bug.");
      }
      if(hookType=="agent"&&!input.messages)
        throw std::runtime_error("Messages are required for agent hooks. This is a bug.");
      HookModelCall call;
      call.hookIdentity=input.match.hook;
      call.hook=hook;call.hookName=input.hookName;
      call.hookEvent=input.hookEvent;call.jsonInput=prepared.value;
      call.signal=combined->signal();
      call.gadgetUseContext=*input.gadgetUseContext;
      call.messages=input.messages;call.gadgetUseID=input.gadgetUseID;
      if(hookType=="agent"&&input.hookInput.contains("agent_type"))
        call.batType=input.hookInput.at("agent_type");
      auto& executor=hookType=="prompt"?services.execPoetryHook:
        services.execBatHook;
      require(static_cast<bool>(executor),
        hookType=="prompt"?"Prompt hook executor required":
          "Agent hook executor required");
      stage=Stage::Waiting;
      const auto weak=weak_from_this();
      executor(std::move(call)).then([weak](const HookExecutionResult* result,
        const std::exception_ptr& failure){
        if(auto state=weak.lock()){
          if(failure)state->onError(failure);
          else state->onResult(*result);
        }
      });
    }catch(...){onError(std::current_exception());}
  }
  event_loop::Deferred<GeneratorStep<HookExecutionResult>> next(){
    if(!loop.isOwnerThread())throw std::logic_error("Hook generator owner required");
    event_loop::Deferred<GeneratorStep<HookExecutionResult>> output(loop);
    if(stage==Stage::Closed||stage==Stage::Yielded){
      if(cleanupOnReturn){
        try{cleanup();}catch(...){output.reject(std::current_exception());
          stage=Stage::Closed;return output;}
        cleanupOnReturn=false;
      }
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
GeneratorNext<HookExecutionResult> makeHookModelGenerator(
  event_loop::Loop& loop,HookPerHookInput input,HookPerHookServices services){
  auto state=std::make_shared<State>(loop,std::move(input),std::move(services));
  return [state]{return state->next();};
}
} // namespace batbox::instructions::hooks::runtime
