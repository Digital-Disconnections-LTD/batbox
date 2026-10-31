#include "instructions/hooks/runtime/IndigenousCommandFull.hpp"
#include "instructions/hooks/runtime/AsyncHookDetection.hpp"
#include "instructions/hooks/runtime/HookPoetryStream.hpp"
#include "instructions/hooks/runtime/IndigenousCommandPreparation.hpp"
#include "instructions/hooks/runtime/IndigenousManagedHookProcess.hpp"
#include "runtime/JsonValue.hpp"
#include <memory>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
bool diagnosticEvent(const std::string& event){
  return event=="SessionStart"||event=="Setup"||event=="SessionEnd";
}
bool truthField(const Json& value,const char* key){
  return value.is_object()&&value.contains(key)&&value::truthy(value.at(key));
}
std::string field(const Json& value,const char* key){
  return value.is_object()&&value.contains(key)&&value.at(key).is_string()
    ?value.at(key).get<std::string>():"";
}
struct FullRun:std::enable_shared_from_this<FullRun>{
  event_loop::Loop& loop;
  IndigenousCommandConfig config;
  FullCommandServices services;
  FullCommandRequest request;
  event_loop::Deferred<CommandOutcome> output;
  std::shared_ptr<IndigenousManagedHookProcess> process;
  std::shared_ptr<AsyncHookDetection> detection;
  std::shared_ptr<HookPoetryStream> poetry;
  std::function<void()> stopProgress;
  double startedMs=0;
  bool emitDiagnostics=false,startedProgress=false,transferred=false,finished=false;
  FullRun(event_loop::Loop& owner,IndigenousCommandConfig settings,
    FullCommandServices imports,FullCommandRequest invocation):
    loop(owner),config(std::move(settings)),services(std::move(imports)),
    request(std::move(invocation)),output(owner){}
  void start(){
    try{
      if(!services.roster||!services.parseJson||!services.stringifyThrown||
        !services.startProgressInterval||!config.logDebug)
        throw std::invalid_argument("Full hook command imports required");
      emitDiagnostics=diagnosticEvent(request.base.hookEvent);
      if(!config.nowMillis)throw std::invalid_argument("Hook clock required");
      startedMs=config.nowMillis();
      prepareIndigenousHookCommand(loop,config,request).then(
        [self=shared_from_this()](const IndigenousProcessRequest* prepared,
          const std::exception_ptr& error){
          if(error){self->output.reject(error);return;}
          try{self->afterPrepared(*prepared);}
          catch(...){
            if(self->process){self->process->kill();self->process->cleanup();}
            self->output.reject(std::current_exception());
          }
        });
    }catch(...){output.reject(std::current_exception());}
  }
  void afterPrepared(IndigenousProcessRequest prepared){
    process=std::make_shared<IndigenousManagedHookProcess>(
      startIndigenousManagedHookProcess(loop,std::move(prepared),
        static_cast<bool>(request.requestPoetry),services.currentAbortReason));
    const auto processId="async_hook_"+std::to_string(process->pid());
    if((truthField(request.base.hook,"async")||
      truthField(request.base.hook,"asyncRewake"))&&!request.forceSyncExecution){
      config.logDebug("Hooks: Config-based async hook, backgrounding process "+
        processId,"info");
      BackgroundHookRequest backgroundRequest;
      backgroundRequest.processId=processId;
      backgroundRequest.hookId=request.base.uuid;
      backgroundRequest.hookEvent=request.base.hookEvent;
      backgroundRequest.hookName=request.base.hookName;
      backgroundRequest.command=field(request.base.hook,"command");
      backgroundRequest.pluginId=request.base.pluginId;
      backgroundRequest.asyncResponse={{"async",true},
        {"asyncTimeout",config.defaultTimeoutMs}};
      if(request.base.hook.contains("timeout")&&
        value::truthy(request.base.hook.at("timeout"))&&
        request.base.hook.at("timeout").is_number())
        backgroundRequest.asyncResponse["asyncTimeout"]=
          request.base.hook.at("timeout").get<double>()*1000;
      backgroundRequest.asyncRewake=truthField(request.base.hook,"asyncRewake");
      backgroundRequest.process=process->rosterProcess();
      backgroundRequest.background=[handle=*process](const std::string& id){
        return handle.background(id);};
      if(executeInBackground(loop,*services.roster,std::move(backgroundRequest),
        services.background)){
        output.resolve({"","","",0,false,true});
        return;
      }
    }
    startForeground(processId);
  }
  void startForeground(const std::string& processId){
    const auto weak=weak_from_this();
    if(request.requestPoetry){
      HookPoetryServices imports;
      imports.parseJson=services.parseJson;
      imports.requestPoetry=request.requestPoetry;
      imports.writeStdin=[handle=*process](const std::string& value){
        handle.writeStdin(value);};
      imports.destroyStdin=[handle=*process]{handle.destroyStdin();};
      imports.logDebug=[this](const std::string& text){config.logDebug(text,"info");};
      imports.stringifyThrown=services.stringifyThrown;
      poetry=std::make_shared<HookPoetryStream>(loop,std::move(imports));
    }
    BackgroundHookRequest backgroundRequest;
    backgroundRequest.processId=processId;
    backgroundRequest.hookId=request.base.uuid;
    backgroundRequest.hookEvent=request.base.hookEvent;
    backgroundRequest.hookName=request.base.hookName;
    backgroundRequest.command=field(request.base.hook,"command");
    backgroundRequest.pluginId=request.base.pluginId;
    backgroundRequest.process=process->rosterProcess();
    backgroundRequest.background=[handle=*process](const std::string& id){
      return handle.background(id);};
    AsyncDetectionServices detectorServices;
    detectorServices.logDebug=[this](const std::string& text){
      config.logDebug(text,"info");};
    detectorServices.parseJson=services.parseJson;
    detectorServices.stringifyJson=[](const Json& value){return value::stringify(value);};
    detectorServices.stringifyThrown=services.stringifyThrown;
    detectorServices.executeBackground=[weak](BackgroundHookRequest value){
      const auto self=weak.lock();
      if(!self)return false;
      return executeInBackground(self->loop,*self->services.roster,
        std::move(value),self->services.background);
    };
    detection=std::make_shared<AsyncHookDetection>(loop,
      std::move(backgroundRequest),std::move(detectorServices),
      request.forceSyncExecution);
    process->onStdout([weak](const std::string& data){
      if(const auto self=weak.lock()){
        if(self->poetry)self->poetry->onStdout(data);
        self->detection->onStdout(data);
      }
    });
    process->onStderr([weak](const std::string& data){
      if(const auto self=weak.lock())self->detection->onStderr(data);
    });
    stopProgress=services.startProgressInterval(request.base.uuid,
      request.base.hookName,request.base.hookEvent,[weak]{
        const auto self=weak.lock();
        if(!self)throw std::runtime_error("Hook command no longer active");
        event_loop::Deferred<Json> value(self->loop);
        value.resolve({{"stdout",self->detection->stdoutText()},
          {"stderr",self->detection->stderrText()},
          {"output",self->detection->outputText()}});
        return value;
      });
    startedProgress=true;
    if(emitDiagnostics){
      if(!config.logDiagnostics)throw std::invalid_argument("Hook diagnostic logger required");
      config.logDiagnostics("hook_spawn_started",
        Json{{"hook_event_name",request.base.hookEvent},
          {"index",request.hookIndex?Json(*request.hookIndex):Json(nullptr)}});
    }
    event_loop::Deferred<CommandOutcome> childClosed(loop);
    process->closeCode().then([weak,childClosed](const int* code,
      const std::exception_ptr& error){
      const auto self=weak.lock();if(!self)return;
      if(error){childClosed.reject(error);return;}
      try{
        if(const auto stdinError=self->process->stdinError()){
          childClosed.reject(std::make_exception_ptr(
            std::runtime_error(*stdinError)));return;
        }
        const auto stdoutText=self->poetry?
          self->poetry->removeProcessedLines(self->detection->stdoutText()):
          self->detection->stdoutText();
        childClosed.resolve({stdoutText,self->detection->stderrText(),
          self->detection->outputText(),*code,
          self->request.base.signal.stop_requested(),false});
      }catch(...){childClosed.reject(std::current_exception());}
    });
    event_loop::race(loop,std::vector<event_loop::Deferred<CommandOutcome>>{
      detection->asyncResult(),childClosed}).then(
      [self=shared_from_this()](const CommandOutcome* result,
        const std::exception_ptr& error){
        self->afterRace(result,error);
      });
  }
  void afterRace(const CommandOutcome* result,const std::exception_ptr& error){
    if(error){finishError(error);return;}
    const auto value=*result;
    if(poetry){
      poetry->pending().then([self=shared_from_this(),value](
        const event_loop::Unit*,const std::exception_ptr& poetryError){
        if(poetryError)self->finishError(poetryError);
        else self->finishSuccess(value);
      });
    }else finishSuccess(value);
  }
  void finishSuccess(CommandOutcome value){
    if(finished)return;
    finished=true;
    transferred=detection&&detection->transferred();
    try{
      if(emitDiagnostics)config.logDiagnostics("hook_spawn_completed",
        Json{{"hook_event_name",request.base.hookEvent},
          {"index",request.hookIndex?Json(*request.hookIndex):Json(nullptr)},
          {"duration_ms",config.nowMillis()-startedMs},
          {"exit_code",value.status},{"aborted",value.aborted}});
      if(startedProgress)stopProgress();
      if(process&&!transferred){process->onStdout({});process->onStderr({});
        process->cleanup();}
      else if(process){auto handle=*process;
        process->resultCode().then([self=shared_from_this(),handle](
          const int*,const std::exception_ptr&){
          handle.onStdout({});handle.onStderr({});
          self->poetry.reset();self->detection.reset();
        });}
      output.resolve(std::move(value));
    }catch(...){output.reject(std::current_exception());}
  }
  void finishError(std::exception_ptr error){
    if(finished)return;
    finished=true;
    std::string code;
    try{std::rethrow_exception(error);}catch(const std::exception& value){code=value.what();}
    catch(...){code="unknown";}
    CommandOutcome value;
    value.status=1;
    if(code=="EPIPE"){
      config.logDebug("EPIPE error while writing to hook stdin (hook command likely closed early)","info");
      value.stderrText="Hook command closed stdin before hook input was fully written (EPIPE)";
    }else if(code=="ABORT_ERR"){
      value.stderrText="Hook cancelled";value.aborted=true;
    }else value.stderrText="Error occurred while executing hook command: "+
      services.stringifyThrown(error);
    value.output=value.stderrText;
    try{
      if(emitDiagnostics)config.logDiagnostics("hook_spawn_completed",
        Json{{"hook_event_name",request.base.hookEvent},
          {"index",request.hookIndex?Json(*request.hookIndex):Json(nullptr)},
          {"duration_ms",config.nowMillis()-startedMs},
          {"exit_code",1},{"aborted",value.aborted}});
      if(startedProgress)stopProgress();
      if(process){process->onStdout({});process->onStderr({});process->cleanup();}
      output.resolve(std::move(value));
    }catch(...){output.reject(std::current_exception());}
  }
};
}
std::function<event_loop::Deferred<CommandOutcome>(FullCommandRequest)>
indigenousFullCommandHook(event_loop::Loop& loop,IndigenousCommandConfig config,
  FullCommandServices services){
  return [&loop,config=std::move(config),services=std::move(services)](
    FullCommandRequest request){
    if(!loop.isOwnerThread())throw std::logic_error("Hook process owner required");
    auto run=std::make_shared<FullRun>(loop,config,services,std::move(request));
    run->start();return run->output;
  };
}
std::function<event_loop::Deferred<CommandOutcome>(CommandRequest)>
bindIndigenousFullCommandHook(event_loop::Loop& loop,IndigenousCommandConfig config,
  FullCommandServices services){
  auto full=indigenousFullCommandHook(loop,std::move(config),std::move(services));
  return [full=std::move(full)](CommandRequest request){
    FullCommandRequest invocation;
    invocation.hookIndex=request.index;
    invocation.skillRoot=request.skillRoot;
    invocation.base=std::move(request);
    return full(std::move(invocation));
  };
}
} // namespace batbox::instructions::hooks::runtime
