#include "instructions/hooks/runtime/HookPerHook.hpp"
#include "instructions/hooks/runtime/OutsideRepl.hpp"
#include <deque>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string stringField(const Json& value,const char* key){
  const auto& found=field(value,key);
  return found.is_string()?found.get<std::string>():std::string{};
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
  void yield(Json fields,std::string outcome){
    fields["outcome"]=outcome;
    HookExecutionResult result{std::move(fields),std::move(outcome),
      input.match.hook};
    stage=Stage::Yielded;
    auto next=demands.front();demands.pop_front();
    next.resolve(GeneratorStep<HookExecutionResult>{false,std::move(result)});
    if(!demands.empty())done();
  }
  double duration(){
    require(static_cast<bool>(services.nowMillis),"Hook clock import required");
    return services.nowMillis()-hookStartMs;
  }
  Json attach(Json fields){
    require(static_cast<bool>(services.createAttachmentMessage),
      "Hook attachment factory required");
    return services.createAttachmentMessage(fields);
  }
  void emit(const std::string& output,const std::string& stdoutText,
    const std::string& stderrText,int status,const std::string& outcome){
    require(static_cast<bool>(services.emitHookResponse),
      "Hook response event import required");
    services.emitHookResponse({{"hookId",hookId},{"hookName",input.hookName},
      {"hookEvent",input.hookEvent},{"output",output},
      {"stdout",stdoutText},{"stderr",stderrText},
      {"exitCode",status},{"outcome",outcome}});
  }
  Json attachmentBase(const char* type){
    return {{"type",type},{"hookName",input.hookName},
      {"toolUseID",input.gadgetUseID},{"hookEvent",input.hookEvent}};
  }
  void onResult(const CommandOutcome& result){
    try{
      cleanup();
      const auto durationMs=duration();
      if(result.backgrounded){yield(Json::object(),"success");return;}
      if(result.aborted){
        emit(result.output,result.stdoutText,result.stderrText,result.status,
          "cancelled");
        auto attachment=attachmentBase("hook_cancelled");
        attachment["command"]=hookCommand;
        attachment["durationMs"]=durationMs;
        yield({{"message",attach(std::move(attachment))}},"cancelled");
        return;
      }
      require(static_cast<bool>(services.parseHookOutput),
        "Hook output parser required");
      const auto parsed=services.parseHookOutput(result.stdoutText);
      if(parsed.validationError){
        const auto stderrText="JSON validation failed: "+*parsed.validationError;
        emit(result.output,result.stdoutText,stderrText,1,"error");
        auto attachment=attachmentBase("hook_non_blocking_error");
        attachment["stderr"]=stderrText;
        attachment["stdout"]=result.stdoutText;
        attachment["exitCode"]=1;
        attachment["command"]=hookCommand;
        attachment["durationMs"]=durationMs;
        yield({{"message",attach(std::move(attachment))}},
          "non_blocking_error");return;
      }
      if(parsed.json&&value::truthy(*parsed.json)){
        if(isAsyncHookJsonOutput(*parsed.json)){
          yield(Json::object(),"success");return;
        }
        ProcessHookJsonInput processing;
        processing.json=*parsed.json;processing.command=hookCommand;
        processing.hookName=input.hookName;
        processing.gadgetUseID=input.gadgetUseID;
        processing.hookEvent=input.hookEvent;
        processing.expectedHookEvent=input.hookEvent;
        processing.stdoutText=result.stdoutText;
        processing.stderrText=result.stderrText;
        processing.exitCode=result.status;
        processing.durationMs=durationMs;
        auto processed=processHookJSONOutput(processing,services.processJson);
        if(isSyncHookJsonOutput(*parsed.json)&&
          !value::truthy(field(*parsed.json,"suppressOutput"))&&
          parsed.plainText&&value::truthy(*parsed.plainText)&&result.status==0){
          require(static_cast<bool>(services.bold),
            "Hook terminal bold import required");
          const auto content=services.bold(input.hookName)+" completed";
          emit(result.output,result.stdoutText,result.stderrText,
            result.status,"success");
          if(!value::truthy(field(processed.fields,"message"))){
            auto attachment=attachmentBase("hook_success");
            attachment["content"]=content;
            attachment["stdout"]=result.stdoutText;
            attachment["stderr"]=result.stderrText;
            attachment["exitCode"]=result.status;
            attachment["command"]=hookCommand;
            attachment["durationMs"]=durationMs;
            processed.fields["message"]=attach(std::move(attachment));
          }
          yield(std::move(processed.fields),"success");return;
        }
        emit(result.output,result.stdoutText,result.stderrText,result.status,
          result.status==0?"success":"error");
        yield(std::move(processed.fields),"success");return;
      }
      if(result.status==0){
        emit(result.output,result.stdoutText,result.stderrText,result.status,
          "success");
        auto attachment=attachmentBase("hook_success");
        attachment["content"]=value::trim(result.stdoutText);
        attachment["stdout"]=result.stdoutText;
        attachment["stderr"]=result.stderrText;
        attachment["exitCode"]=result.status;
        attachment["command"]=hookCommand;
        attachment["durationMs"]=durationMs;
        yield({{"message",attach(std::move(attachment))}},"success");return;
      }
      if(result.status==2){
        emit(result.output,result.stdoutText,result.stderrText,result.status,
          "error");
        const auto command=stringField(input.match.hook->fields,"command");
        yield({{"blockingError",{{"blockingError","["+command+"]: "+
          (result.stderrText.empty()?"No stderr output":result.stderrText)},
          {"command",command}}}},"blocking");return;
      }
      emit(result.output,result.stdoutText,result.stderrText,result.status,
        "error");
      auto attachment=attachmentBase("hook_non_blocking_error");
      const auto trimmed=value::trim(result.stderrText);
      attachment["stderr"]="Failed with non-blocking status code: "+
        (trimmed.empty()?"No stderr output":trimmed);
      attachment["stdout"]=result.stdoutText;
      attachment["exitCode"]=result.status;
      attachment["command"]=hookCommand;
      attachment["durationMs"]=durationMs;
      yield({{"message",attach(std::move(attachment))}},
        "non_blocking_error");
    }catch(...){onError(std::current_exception());}
  }
  void onError(std::exception_ptr failure){
    try{
      cleanup();
      require(static_cast<bool>(services.errorMessage),
        "Hook thrown-error formatter required");
      const auto message="Failed to run: "+services.errorMessage(failure);
      emit(message,"",message,1,"error");
      auto attachment=attachmentBase("hook_non_blocking_error");
      attachment["stderr"]=message;
      attachment["stdout"]="";
      attachment["exitCode"]=1;
      attachment["command"]=hookCommand;
      attachment["durationMs"]=duration();
      yield({{"message",attach(std::move(attachment))}},
        "non_blocking_error");
    }catch(...){fail(std::current_exception());}
  }
  void start(){
    if(!input.match.hook){fail(std::make_exception_ptr(
      std::invalid_argument("Hook identity required")));return;}
    const auto& hook=input.match.hook->fields;
    const auto& timeout=field(hook,"timeout");
    try{
      const auto timeoutMs=value::truthy(timeout)?timeout.get<double>()*1000:
        input.timeoutMs;
      combined=CombinedAbort::create(loop,input.signal,std::nullopt,timeoutMs);
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
        auto attachment=attachmentBase("hook_error_during_execution");
        attachment["content"]="Failed to prepare hook input: "+
          services.errorMessage(prepared.error);
        attachment["command"]=hookCommand;
        attachment["durationMs"]=duration();
        cleanupOnReturn=true;
        yield({{"message",attach(std::move(attachment))}},
          "non_blocking_error");return;
      }
      require(static_cast<bool>(services.emitHookStarted),
        "Hook started event import required");
      services.emitHookStarted(hookId,input.hookName,input.hookEvent);
      require(static_cast<bool>(services.execCommandHook),
        "Command hook executor required");
      HookCommandCall call;
      call.hook=hook;call.hookEvent=input.hookEvent;
      call.hookName=input.hookName;call.jsonInput=prepared.value;
      call.signal=combined->signal();call.hookId=hookId;
      call.hookIndex=input.hookIndex;
      call.pluginRoot=input.match.pluginRoot;
      call.pluginId=input.match.pluginId;
      call.skillRoot=input.match.skillRoot;
      call.forceSyncExecution=input.forceSyncExecution;
      call.boundRequestPoetry=input.boundRequestPoetry;
      stage=Stage::Waiting;
      const auto weak=weak_from_this();
      services.execCommandHook(std::move(call)).then(
        [weak](const CommandOutcome* result,const std::exception_ptr& failure){
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
GeneratorNext<HookExecutionResult> makeHookCommandGenerator(
  event_loop::Loop& loop,HookPerHookInput input,HookPerHookServices services){
  auto state=std::make_shared<State>(loop,std::move(input),std::move(services));
  return [state]{return state->next();};
}
} // namespace batbox::instructions::hooks::runtime
