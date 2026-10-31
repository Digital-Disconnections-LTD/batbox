#include "instructions/hooks/runtime/HookPerHook.hpp"
#include "instructions/hooks/runtime/OutsideRepl.hpp"
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
  std::string hookId,hookCommand;
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
  void yield(Json fields,const std::string& outcome){
    fields["outcome"]=outcome;
    HookExecutionResult result{std::move(fields),outcome,input.match.hook};
    stage=Stage::Yielded;
    auto next=demands.front();demands.pop_front();
    next.resolve(GeneratorStep<HookExecutionResult>{false,std::move(result)});
    if(!demands.empty())done();
  }
  Json attachmentBase(const char* type){
    return {{"type",type},{"hookName",input.hookName},
      {"toolUseID",input.gadgetUseID},{"hookEvent",input.hookEvent}};
  }
  Json attach(const Json& value){
    require(static_cast<bool>(services.createAttachmentMessage),
      "Hook attachment factory required");
    return services.createAttachmentMessage(value);
  }
  double duration(){
    require(static_cast<bool>(services.nowMillis),"Hook clock import required");
    return services.nowMillis()-hookStartMs;
  }
  void emit(const std::string& output,const std::string& stdoutText,
    const std::string& stderrText,std::optional<int> status,
    const std::string& outcome){
    Json event={{"hookId",hookId},{"hookName",input.hookName},
      {"hookEvent",input.hookEvent},{"output",output},
      {"stdout",stdoutText},{"stderr",stderrText},{"outcome",outcome}};
    if(status){
      event["exitCode"]=*status;
      require(static_cast<bool>(services.emitHookResponse),
        "Hook response event import required");
      services.emitHookResponse(event);
    }else{
      require(static_cast<bool>(services.emitHookResponseWithUndefined),
        "Hook undefined-exit response event import required");
      services.emitHookResponseWithUndefined(event,{"exitCode"});
    }
  }
  void onError(std::exception_ptr failure){
    try{
      cleanup();
      require(static_cast<bool>(services.errorMessage),
        "Hook thrown-error formatter required");
      const auto message="Failed to run: "+services.errorMessage(failure);
      emit(message,"",message,1,"error");
      auto payload=attachmentBase("hook_non_blocking_error");
      payload["stderr"]=message;payload["stdout"]="";
      payload["exitCode"]=1;payload["command"]=hookCommand;
      payload["durationMs"]=duration();
      yield({{"message",attach(payload)}},"non_blocking_error");
    }catch(...){fail(std::current_exception());}
  }
  void onResult(const HttpOutcome& result){
    try{
      cleanup();
      if(result.aborted){
        emit("Hook cancelled","","",std::nullopt,"cancelled");
        yield({{"message",attach(attachmentBase("hook_cancelled"))}},
          "cancelled");return;
      }
      if((result.error&&!result.error->empty())||!result.ok){
        const auto stderrText=result.error&&!result.error->empty()
          ?*result.error:"HTTP "+(result.statusCode?
            std::to_string(*result.statusCode):"undefined")+" from "+
            stringField(input.match.hook->fields,"url");
        emit(stderrText,"",stderrText,result.statusCode,"error");
        auto payload=attachmentBase("hook_non_blocking_error");
        payload["stderr"]=stderrText;payload["stdout"]="";
        payload["exitCode"]=result.statusCode.value_or(0);
        yield({{"message",attach(payload)}},"non_blocking_error");
        return;
      }
      require(static_cast<bool>(services.parseHttpHookOutput),
        "HTTP hook output parser required");
      const auto parsed=services.parseHttpHookOutput(result.body);
      if(parsed.validationError){
        const auto stderrText="JSON validation failed: "+*parsed.validationError;
        emit(result.body,result.body,stderrText,result.statusCode,"error");
        auto payload=attachmentBase("hook_non_blocking_error");
        payload["stderr"]=stderrText;payload["stdout"]=result.body;
        payload["exitCode"]=result.statusCode.value_or(0);
        yield({{"message",attach(payload)}},"non_blocking_error");
        return;
      }
      if(parsed.json&&value::truthy(*parsed.json)&&
        isAsyncHookJsonOutput(*parsed.json)){
        emit(result.body,result.body,"",result.statusCode,"success");
        yield(Json::object(),"success");return;
      }
      if(parsed.json&&value::truthy(*parsed.json)){
        ProcessHookJsonInput processing;
        processing.json=*parsed.json;
        processing.command=stringField(input.match.hook->fields,"url");
        processing.hookName=input.hookName;
        processing.gadgetUseID=input.gadgetUseID;
        processing.hookEvent=input.hookEvent;
        processing.expectedHookEvent=input.hookEvent;
        processing.stdoutText=result.body;
        processing.stderrText="";
        if(result.statusCode)processing.exitCode=*result.statusCode;
        auto processed=processHookJSONOutput(processing,services.processJson);
        emit(result.body,result.body,"",result.statusCode,"success");
        yield(std::move(processed.fields),"success");return;
      }
      done();
    }catch(...){onError(std::current_exception());}
  }
  void start(){
    if(!input.match.hook){fail(std::make_exception_ptr(
      std::invalid_argument("Hook identity required")));return;}
    const auto& hook=input.match.hook->fields;
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
        auto payload=attachmentBase("hook_error_during_execution");
        payload["content"]="Failed to prepare hook input: "+
          services.errorMessage(prepared.error);
        payload["command"]=hookCommand;
        payload["durationMs"]=duration();
        cleanupOnReturn=true;
        yield({{"message",attach(payload)}},"non_blocking_error");return;
      }
      require(static_cast<bool>(services.emitHookStarted),
        "Hook started event import required");
      services.emitHookStarted(hookId,input.hookName,input.hookEvent);
      require(static_cast<bool>(services.execHttpHook),
        "HTTP hook executor required");
      HookHttpCall call{hook,input.hookEvent,prepared.value,input.signal};
      stage=Stage::Waiting;
      const auto weak=weak_from_this();
      services.execHttpHook(std::move(call)).then(
        [weak](const HttpOutcome* result,const std::exception_ptr& failure){
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
      if(cleanupOnReturn){cleanupOnReturn=false;cleanup();}
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
GeneratorNext<HookExecutionResult> makeHookHttpGenerator(
  event_loop::Loop& loop,HookPerHookInput input,HookPerHookServices services){
  auto state=std::make_shared<State>(loop,std::move(input),std::move(services));
  return [state]{return state->next();};
}
} // namespace batbox::instructions::hooks::runtime
