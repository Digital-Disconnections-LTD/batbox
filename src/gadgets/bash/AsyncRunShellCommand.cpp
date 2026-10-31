#include "gadgets/bash/AsyncRunShellCommand.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace batbox::bash_gadget {
using event_loop::Deferred;
using event_loop::Unit;
namespace {
shell::ExecResult backgroundResult(const std::string& id,bool assistant,bool byUser,
  bool assistantOwnProperty){
  shell::ExecResult result;result.backgroundMissionId=id;
  result.assistantAutoBackgrounded=assistant;result.backgroundedByUser=byUser;
  result.assistantAutoBackgroundedOwnProperty=assistantOwnProperty;
  return result;
}
void require(bool valid,const char* name){
  if(!valid)throw std::invalid_argument(std::string("async runShellCommand import required: ")+name);
}
}
struct AsyncRunShellCommand::State:std::enable_shared_from_this<State>{
  enum class Phase {Dormant,AwaitExec,AwaitExplicit,AwaitInitial,AwaitProgress,Yielded,Done};
  struct Request {
    enum class Kind {Next,Return,Throw};
    Kind kind;
    Deferred<AsyncRunStep> promise;
    std::optional<shell::ExecResult> value;
    std::exception_ptr error;
  };
  event_loop::Loop& loop;
  AsyncRunInput input;
  RunShellSettings settings;
  AsyncRunImports imports;
  Phase phase=Phase::Dormant;
  std::deque<Request> demands;
  std::optional<AsyncShellCommand> command;
  std::optional<Deferred<Unit>> progressSignal;
  std::function<void()> stopPolling;
  std::string fullOutput,lastProgressOutput,backgroundId,foregroundId;
  std::size_t totalLines=0,totalBytes=0;
  double startTime=0,timeoutMs=0;
  bool assistantAutoBackgrounded=false,polling=false,shouldAutoBackground=false;
  bool resumeQueued=false;
  State(event_loop::Loop& loop,AsyncRunInput input,RunShellSettings settings,
    AsyncRunImports imports):loop(loop),input(std::move(input)),settings(std::move(settings)),
    imports(std::move(imports)){
    require(bool(this->imports.exec),"exec");
    require(bool(this->imports.spawnBackgroundMission),"spawnBackgroundTask");
    require(bool(this->imports.backgroundExistingForegroundMission),"backgroundExistingForegroundTask");
    require(bool(this->imports.registerForeground),"registerForeground");
    require(bool(this->imports.unregisterForeground),"unregisterForeground");
    require(bool(this->imports.markMissionNotified),"markTaskNotified");
    require(bool(this->imports.startPolling),"startPolling");
    require(bool(this->imports.isAutoBackgroundAllowed),"isAutoBackgroundAllowed");
    require(bool(this->imports.shouldUseSandbox),"shouldUseSandbox");
    require(bool(this->imports.nowMs),"nowMs");
    require(bool(this->imports.getDefaultTimeoutMs),"getDefaultTimeoutMs");
    require(bool(this->imports.onUnobservedError),"onUnobservedError");
    if(this->settings.showBackgroundUI)require(bool(this->imports.showBackgroundHint),"showBackgroundHint");
  }
  void stop(){
    if(!polling)return;
    polling=false;
    auto callback=std::move(stopPolling);stopPolling={};
    if(callback)callback();
  }
  static void settleClosed(Request request){
    if(request.kind==Request::Kind::Throw){request.promise.reject(request.error);return;}
    AsyncRunStep step;step.done=true;
    if(request.kind==Request::Kind::Return)step.result=std::move(request.value);
    request.promise.resolve(std::move(step));
  }
  void drainClosed(){
    while(!demands.empty()){
      auto request=std::move(demands.front());demands.pop_front();
      settleClosed(std::move(request));
    }
  }
  void done(shell::ExecResult result){
    if(phase==Phase::Done)return;
    try{stop();}catch(...){fail(std::current_exception());return;}
    phase=Phase::Done;progressSignal.reset();
    if(demands.empty())return;
    AsyncRunStep step;step.done=true;step.result=std::move(result);
    auto request=std::move(demands.front());demands.pop_front();
    request.promise.resolve(std::move(step));drainClosed();
  }
  void fail(std::exception_ptr error){
    if(phase==Phase::Done)return;
    try{stop();}catch(...){error=std::current_exception();}
    phase=Phase::Done;progressSignal.reset();
    if(!demands.empty()){
      auto request=std::move(demands.front());demands.pop_front();
      request.promise.reject(error);
    }
    drainClosed();
  }
  void deliver(shell::Progress progress){
    if(demands.empty()){fail(std::make_exception_ptr(std::logic_error("progress without demand")));return;}
    phase=Phase::Yielded;
    AsyncRunStep step;step.progress=std::move(progress);
    auto request=std::move(demands.front());demands.pop_front();
    request.promise.resolve(std::move(step));
    if(!demands.empty())scheduleResume();
  }
  void scheduleResume(){
    if(resumeQueued)return;
    resumeQueued=true;
    loop.queueMicrotask([self=shared_from_this()]{
      self->resumeQueued=false;
      if(self->phase!=Phase::Yielded||self->demands.empty())return;
      auto& next=self->demands.front();
      if(next.kind==Request::Kind::Next){self->waitProgress();return;}
      auto request=std::move(next);self->demands.pop_front();
      try{self->stop();}
      catch(...){self->phase=Phase::Done;request.promise.reject(std::current_exception());
        self->drainClosed();return;}
      self->phase=Phase::Done;self->progressSignal.reset();
      settleClosed(std::move(request));self->drainClosed();
    });
  }
  void start(){
    phase=Phase::AwaitExec;
    try{
      timeoutMs=input.timeoutMs&&*input.timeoutMs!=0&&!std::isnan(*input.timeoutMs)?
        *input.timeoutMs:imports.getDefaultTimeoutMs();
      shouldAutoBackground=!settings.backgroundMissionsDisabled&&
        imports.isAutoBackgroundAllowed(input.command);
      AsyncExecRequest request{input.command,timeoutMs,input.preventCwdChanges,
        imports.shouldUseSandbox(input),shouldAutoBackground};
      imports.exec(request).then([self=shared_from_this()](
        const AsyncShellCommand* value,const std::exception_ptr& error){
        if(error){self->fail(error);return;}
        try{self->afterExec(*value);}catch(...){self->fail(std::current_exception());}
      });
    }catch(...){fail(std::current_exception());}
  }
  void afterExec(AsyncShellCommand value){
    command=std::move(value);
    if(command->missionId.empty()||!command->status||!command->cleanup||
      !command->background||!command->largeOutputInfo)
      throw std::invalid_argument("complete async shell command required");
    if(shouldAutoBackground&&command->onTimeout){
      command->onTimeout([weak=weak_from_this()](const auto& backgroundFn){
        if(auto self=weak.lock())
          try{self->startBackgrounding(backgroundFn);}
          catch(...){self->fail(std::current_exception());}
      });
    }
    if(settings.assistantMode&&settings.isMainThread&&
      !settings.backgroundMissionsDisabled&&!input.runInBackground){
      loop.setTimeout(settings.assistantBlockingBudgetMs,[weak=weak_from_this()]{
        if(auto self=weak.lock()){
          try{if(self->phase!=Phase::Done&&self->command&&
            self->command->status()==shell::CommandStatus::Running&&self->backgroundId.empty()){
            self->assistantAutoBackgrounded=true;self->startBackgrounding();
          }}catch(...){self->fail(std::current_exception());}
        }
      },false);
    }
    if(input.runInBackground&&!settings.backgroundMissionsDisabled){
      phase=Phase::AwaitExplicit;
      imports.spawnBackgroundMission(input,*command).then([self=shared_from_this()](
        const std::string* id,const std::exception_ptr& error){
        if(error){self->fail(error);return;}
        self->done(backgroundResult(*id,false,false,false));
      });
      return;
    }
    startTime=imports.nowMs();
    phase=Phase::AwaitInitial;
    auto initial=std::make_shared<Deferred<std::optional<shell::ExecResult>>>(loop);
    auto timer=std::make_shared<Deferred<Unit>>(loop);
    command->result.then([initial](const shell::ExecResult* result,const std::exception_ptr& error){
      if(error)initial->reject(error);else initial->resolve(*result);
    });
    timer->then([initial](const Unit*,const std::exception_ptr& error){
      if(error)initial->reject(error);else initial->resolve(std::nullopt);
    });
    loop.setTimeout(settings.progressThresholdMs,[timer]{timer->resolve({});},false);
    initial->then([weak=weak_from_this()](const std::optional<shell::ExecResult>* value,
      const std::exception_ptr& error){
      auto self=weak.lock();if(!self)return;
      if(error){self->fail(error);return;}
      try{
        if(*value){self->command->cleanup();self->done(**value);return;}
        if(!self->backgroundId.empty()){
          self->done(backgroundResult(self->backgroundId,self->assistantAutoBackgrounded,false,true));
          return;
        }
        self->startProgressPolling();
      }catch(...){self->fail(std::current_exception());}
    });
  }
  void startProgressPolling(){
    auto weak=weak_from_this();
    stopPolling=imports.startPolling(*command,[weak](const shell::PollSample& sample){
      if(auto self=weak.lock())self->onProgress(sample);
    },[weak](std::exception_ptr error){if(auto self=weak.lock())self->fail(error);});
    if(!stopPolling)throw std::invalid_argument("TaskOutput stop callback required");
    polling=true;waitProgress();
  }
  void onProgress(const shell::PollSample& sample){
    if(phase==Phase::Done)return;
    lastProgressOutput=sample.lastLines;fullOutput=sample.allLines;
    totalLines=sample.totalLines;totalBytes=sample.isIncomplete?sample.totalBytes:0;
    if(progressSignal){auto signal=*progressSignal;progressSignal.reset();signal.resolve({});}
  }
  void startBackgrounding(const std::function<bool(const std::string&)>& backgroundFn={}){
    if(phase==Phase::Done)return;
    if(!foregroundId.empty()){
      if(!imports.backgroundExistingForegroundMission(foregroundId,input,*command))return;
      backgroundId=foregroundId;
      if(backgroundFn)backgroundFn(foregroundId);
      return;
    }
    try{
      imports.spawnBackgroundMission(input,*command).then([self=shared_from_this(),backgroundFn](
        const std::string* id,const std::exception_ptr& error){
        if(error){self->imports.onUnobservedError(error);return;}
        try{
          self->backgroundId=*id;
          if(self->progressSignal){auto signal=*self->progressSignal;
            self->progressSignal.reset();signal.resolve({});}
          if(backgroundFn)backgroundFn(*id);
        }catch(...){self->imports.onUnobservedError(std::current_exception());}
      });
    }catch(...){imports.onUnobservedError(std::current_exception());}
  }
  void waitProgress(){
    if(phase==Phase::Done||demands.empty())return;
    phase=Phase::AwaitProgress;
    auto raced=std::make_shared<Deferred<std::optional<shell::ExecResult>>>(loop);
    progressSignal.emplace(loop);
    command->result.then([raced](const shell::ExecResult* result,const std::exception_ptr& error){
      if(error)raced->reject(error);else raced->resolve(*result);
    });
    progressSignal->then([raced](const Unit*,const std::exception_ptr& error){
      if(error)raced->reject(error);else raced->resolve(std::nullopt);
    });
    raced->then([weak=weak_from_this()](const std::optional<shell::ExecResult>* value,
      const std::exception_ptr& error){
      auto self=weak.lock();if(!self)return;
      if(error){self->fail(error);return;}
      try{self->afterProgressRace(*value);}catch(...){self->fail(std::current_exception());}
    });
  }
  void afterProgressRace(const std::optional<shell::ExecResult>& result){
    if(phase==Phase::Done)return;
    progressSignal.reset();
    if(result){
      auto value=*result;
      if(value.backgroundMissionId){
        imports.markMissionNotified(*value.backgroundMissionId);
        value.backgroundMissionId.reset();
        if(const auto info=command->largeOutputInfo()){
          value.outputFilePath=info->path;value.outputFileSize=info->size;
          value.outputMissionId=info->missionId;
        }
        command->cleanup();done(std::move(value));return;
      }
      if(!foregroundId.empty())imports.unregisterForeground(foregroundId);
      command->cleanup();done(std::move(value));return;
    }
    if(!backgroundId.empty()){
      done(backgroundResult(backgroundId,assistantAutoBackgrounded,false,true));return;
    }
    if(!foregroundId.empty()&&command->status()==shell::CommandStatus::Backgrounded){
      done(backgroundResult(foregroundId,false,true,false));return;
    }
    const auto elapsed=imports.nowMs()-startTime;
    const auto seconds=std::floor(elapsed/1000.0);
    if(!settings.backgroundMissionsDisabled&&backgroundId.empty()&&
      seconds>=settings.progressThresholdMs/1000.0&&settings.showBackgroundUI){
      if(foregroundId.empty())foregroundId=imports.registerForeground(input,*command);
      imports.showBackgroundHint();
    }
    shell::Progress progress;
    progress.lastLines=lastProgressOutput;progress.fullOutput=fullOutput;
    progress.elapsedTimeSeconds=seconds;progress.totalLines=totalLines;
    progress.totalBytes=totalBytes;progress.missionId=command->missionId;
    progress.timeoutMs=input.timeoutMs&&*input.timeoutMs!=0&&!std::isnan(*input.timeoutMs)?
      std::optional<double>(timeoutMs):std::nullopt;
    deliver(std::move(progress));
  }
  Deferred<AsyncRunStep> next(){
    Deferred<AsyncRunStep> output(loop);
    if(phase==Phase::Done){AsyncRunStep terminal;terminal.done=true;
      output.resolve(std::move(terminal));return output;}
    demands.push_back(Request{Request::Kind::Next,output,{},{}});
    if(phase==Phase::Dormant)start();
    else if(phase==Phase::Yielded)scheduleResume();
    return output;
  }
  Deferred<AsyncRunStep> returnValue(std::optional<shell::ExecResult> value){
    Deferred<AsyncRunStep> output(loop);
    Request request{Request::Kind::Return,output,std::move(value),{}};
    if(phase==Phase::Dormant||phase==Phase::Done){
      phase=Phase::Done;settleClosed(std::move(request));return output;
    }
    demands.push_back(std::move(request));
    if(phase==Phase::Yielded)scheduleResume();
    return output;
  }
  Deferred<AsyncRunStep> throwError(std::exception_ptr error){
    if(!error)throw std::invalid_argument("iterator throw error required");
    Deferred<AsyncRunStep> output(loop);
    Request request{Request::Kind::Throw,output,{},error};
    if(phase==Phase::Dormant||phase==Phase::Done){
      phase=Phase::Done;settleClosed(std::move(request));return output;
    }
    demands.push_back(std::move(request));
    if(phase==Phase::Yielded)scheduleResume();
    return output;
  }
};
AsyncRunShellCommand::AsyncRunShellCommand(event_loop::Loop& loop,AsyncRunInput input,
  RunShellSettings settings,AsyncRunImports imports):
  state_(std::make_shared<State>(loop,std::move(input),std::move(settings),std::move(imports))){}
Json AsyncRunStep::toJson()const{
  Json output={{"done",done}};
  if(done){if(result)output["value"]=result->toJson();}
  else if(progress)output["value"]=progress->toJson();
  return output;
}
Deferred<AsyncRunStep> AsyncRunShellCommand::next(){
  if(!state_->loop.isOwnerThread())throw std::logic_error("shell generator owner required");
  return state_->next();
}
Deferred<AsyncRunStep> AsyncRunShellCommand::returnValue(
  std::optional<shell::ExecResult> value){
  if(!state_->loop.isOwnerThread())throw std::logic_error("shell generator owner required");
  return state_->returnValue(std::move(value));
}
Deferred<AsyncRunStep> AsyncRunShellCommand::throwError(std::exception_ptr error){
  if(!state_->loop.isOwnerThread())throw std::logic_error("shell generator owner required");
  return state_->throwError(error);
}
} // namespace batbox::bash_gadget
