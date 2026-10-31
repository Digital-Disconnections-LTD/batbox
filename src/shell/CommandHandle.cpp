#include "runtime/AsyncLaunch.hpp"
#include "shell/CommandHandle.hpp"
#include <mutex>
#include <stdexcept>
#include <thread>

namespace batbox::shell {
struct CommandHandle::State {
  std::shared_ptr<ManagedShell> shell;
  std::string missionId;
  std::promise<ExecResult> promise;
  std::shared_future<ExecResult> future;
  std::shared_ptr<CommandStartSignal> startSignal;
  std::mutex timeoutMutex;
  std::condition_variable timeoutCv;
  bool expectTimeoutCallback=false;
  bool workerDone=false;
  TimeoutCallback timeoutCallback;
  std::mutex outputMutex;
  std::optional<LargeOutputInfo> outputInfo;
};
const std::string& CommandHandle::missionId()const{return state_->missionId;}
std::string CommandHandle::outputPath()const{return state_->shell->outputPath(state_->missionId);}
std::optional<CommandHandle::LargeOutputInfo> CommandHandle::largeOutputInfo()const{
  std::lock_guard lock(state_->outputMutex);return state_->outputInfo;
}
std::shared_future<ExecResult> CommandHandle::result()const{return state_->future;}
CommandStatus CommandHandle::status()const{
  const auto status=state_->shell->status(state_->missionId);
  if(status)return *status;
  if(state_->future.wait_for(std::chrono::seconds(0))==std::future_status::ready){
    try{const auto result=state_->future.get();
      return result.interrupted||result.code==143?CommandStatus::Killed:CommandStatus::Completed;}
    catch(...){return CommandStatus::Killed;}
  }
  return CommandStatus::Running;
}
bool CommandHandle::background(const std::string& id,bool byUser)const{
  if(id!=state_->missionId)return false;
  return state_->shell->background(id,byUser);
}
void CommandHandle::kill()const{(void)state_->shell->requestKill(state_->missionId);}
void CommandHandle::cleanup()const{(void)state_->shell->release(state_->missionId);}
void CommandHandle::onTimeout(TimeoutCallback callback)const{
  std::lock_guard lock(state_->timeoutMutex);
  if(state_->workerDone)return;
  state_->timeoutCallback=std::move(callback);
  state_->timeoutCv.notify_all();
}
std::function<bool(const std::string&)> CommandHandle::weakBackgroundCallback()const{
  std::weak_ptr<State> weak=state_;
  return [weak](const std::string& id){
    auto state=weak.lock();if(!state||id!=state->missionId)return false;
    return state->shell->background(id,false);
  };
}
CommandHandle startManagedCommand(std::shared_ptr<ManagedShell> shell,RunOptions options,
  bool expectTimeoutCallback){
  if(!shell)throw std::invalid_argument("managed shell required");
  auto state=std::make_shared<CommandHandle::State>();
  state->shell=std::move(shell);state->missionId=state->shell->allocateMissionId();
  state->future=state->promise.get_future().share();
  state->startSignal=std::make_shared<CommandStartSignal>();
  state->expectTimeoutCallback=expectTimeoutCallback;
  options.reservedMissionId=state->missionId;options.startSignal=state->startSignal;
  std::weak_ptr<CommandHandle::State> weak=state;
  options.onTimeout=[weak](const std::function<bool(const std::string&)>& backgroundFn){
    auto state=weak.lock();if(!state)return false;
    CommandHandle::TimeoutCallback callback;
    {std::unique_lock lock(state->timeoutMutex);
      if(state->expectTimeoutCallback)state->timeoutCv.wait(lock,[&]{return bool(state->timeoutCallback);});
      callback=state->timeoutCallback;}
    if(!callback)return false;
    callback(backgroundFn);return true;
  };
  ::batbox::runtime::contextThread([state,options=std::move(options)]()mutable{
    try{
      auto result=state->shell->run(options);
      if(result.backgroundMissionId){
        const auto id=*result.backgroundMissionId;
        result=state->shell->wait(id,-1);
        result.backgroundMissionId=id;
        if(result.outputFilePath&&result.outputFileSize&&result.outputMissionId){
          std::lock_guard lock(state->outputMutex);
          state->outputInfo=CommandHandle::LargeOutputInfo{*result.outputFilePath,*result.outputFileSize,
            *result.outputMissionId};
        }
        result.outputFilePath.reset();result.outputFileSize.reset();result.outputMissionId.reset();
        result.backgroundedByUser=false;result.assistantAutoBackgrounded=false;
        result.assistantAutoBackgroundedOwnProperty=false;
      }
      state->promise.set_value(std::move(result));
    }
    catch(...){state->promise.set_exception(std::current_exception());}
    {std::lock_guard lock(state->timeoutMutex);
      state->workerDone=true;state->timeoutCallback={};}
    state->timeoutCv.notify_all();
    {std::lock_guard lock(state->startSignal->mutex);state->startSignal->finished=true;}
    state->startSignal->cv.notify_all();
  }).detach();
  {
    std::unique_lock lock(state->startSignal->mutex);
    state->startSignal->cv.wait(lock,[&]{return state->startSignal->started||
      state->startSignal->finished;});
  }
  return CommandHandle(std::move(state));
}
} // namespace batbox::shell
