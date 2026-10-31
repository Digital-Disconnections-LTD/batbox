#include "gadgets/bash/RunShellCommand.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <stdexcept>

namespace batbox::bash_gadget {
namespace {
using Clock=std::chrono::steady_clock;
struct State {
  std::mutex mutex,lifecycle;
  std::condition_variable cv;
  std::string fullOutput,lastProgressOutput;
  std::size_t totalLines=0,totalBytes=0;
  std::string backgroundId,foregroundId;
  bool assistantAutoBackgrounded=false,progressReady=false,closed=false;
};
shell::ExecResult backgroundResult(const std::string& id,bool assistant,bool byUser,
  bool assistantOwnProperty=true){
  shell::ExecResult result;result.backgroundMissionId=id;
  result.assistantAutoBackgrounded=assistant;result.backgroundedByUser=byUser;
  result.assistantAutoBackgroundedOwnProperty=assistantOwnProperty&&!byUser;
  return result;
}
void require(bool value,const char* name){
  if(!value)throw std::invalid_argument(std::string("runShellCommand callback required: ")+name);
}
} // namespace
shell::ExecResult runShellCommand(std::shared_ptr<shell::ManagedShell> shell,
  shell::MissionOutputPoller& poller,shell::RunOptions options,
  const RunShellSettings& settings,const RunShellCallbacks& callbacks){
  require(bool(shell),"shell");
  require(bool(callbacks.spawnBackgroundMission),"spawnBackgroundTask");
  require(bool(callbacks.registerForeground),"registerForeground");
  require(bool(callbacks.backgroundExistingForegroundMission),"backgroundExistingForegroundTask");
  require(bool(callbacks.unregisterForeground),"unregisterForeground");
  require(bool(callbacks.markMissionNotified),"markTaskNotified");
  require(bool(callbacks.yieldProgress),"yieldProgress");
  if(settings.showBackgroundUI)require(bool(callbacks.showBackgroundHint),"showBackgroundHint");
  auto state=std::make_shared<State>();
  options.runInBackground=false;
  options.onProgress={};
  options.autoBackgroundEnabled=!settings.backgroundMissionsDisabled;
  options.managedAssistantBackgroundEnabled=false;
  const auto handle=shell::startManagedCommand(std::move(shell),std::move(options),true);
  const auto resultPromise=handle.result();
  const auto started=Clock::now();
  auto startBackgrounding=[state,handle,callbacks](
    const std::function<bool(const std::string&)>& backgroundFn={}){
    std::lock_guard lifecycle(state->lifecycle);
    {std::lock_guard lock(state->mutex);if(state->closed)return;}
    std::string foreground;
    {std::lock_guard lock(state->mutex);foreground=state->foregroundId;}
    if(!foreground.empty()){
      if(!callbacks.backgroundExistingForegroundMission(foreground,handle))return;
      {std::lock_guard lock(state->mutex);state->backgroundId=foreground;}
      if(backgroundFn)backgroundFn(foreground);
      return; // The foreground path waits for the next progress poll.
    }
    const auto id=callbacks.spawnBackgroundMission(handle);
    {std::lock_guard lock(state->mutex);state->backgroundId=id;state->progressReady=true;}
    state->cv.notify_all(); // Wake the waiter after the background transition.
    if(backgroundFn)backgroundFn(id);
  };
  // Register the timeout handler before checking explicit background mode.
  handle.onTimeout([startBackgrounding](const auto& backgroundFn){
    startBackgrounding(backgroundFn);
  });
  if(settings.runInBackground&&!settings.backgroundMissionsDisabled){
    const auto id=callbacks.spawnBackgroundMission(handle);
    {std::lock_guard lock(state->mutex);state->closed=true;}
    return backgroundResult(id,false,false,false);
  }
  const auto deadline=started+std::chrono::duration_cast<Clock::duration>(
    std::chrono::duration<double,std::milli>(settings.progressThresholdMs));
  while(resultPromise.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready&&
        Clock::now()<deadline){
    std::unique_lock lock(state->mutex);
    state->cv.wait_until(lock,std::min(deadline,Clock::now()+std::chrono::milliseconds(25)));
  }
  if(resultPromise.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
    {std::lock_guard lock(state->mutex);state->closed=true;}
    auto result=resultPromise.get();handle.cleanup();return result;
  }
  {
    std::lock_guard lock(state->mutex);
    if(!state->backgroundId.empty()){
      state->closed=true;
      return backgroundResult(state->backgroundId,state->assistantAutoBackgrounded,false);
    }
  }
  const auto missionId=handle.missionId();
  poller.registerOutput(missionId,handle.outputPath(),[state](const shell::PollSample& sample){
    std::lock_guard lock(state->mutex);
    if(state->closed)return;
    state->lastProgressOutput=sample.lastLines;
    state->fullOutput=sample.allLines;
    state->totalLines=sample.totalLines;
    state->totalBytes=sample.isIncomplete?sample.totalBytes:0;
    state->progressReady=true;state->cv.notify_all();
  });
  poller.startPolling(missionId);
  struct PollGuard {shell::MissionOutputPoller& poller;std::string id;
    ~PollGuard(){poller.stopPolling(id);poller.unregisterOutput(id);}} guard{poller,missionId};
  try{
    for(;;){
      {
        std::unique_lock lock(state->mutex);
        state->cv.wait_for(lock,std::chrono::milliseconds(25),[&]{return state->progressReady;});
      }
      if(auto error=poller.takeCallbackFailure(missionId))std::rethrow_exception(error);
      // Promise.race resolves a completed command ahead of a later poll tick.
      if(resultPromise.wait_for(std::chrono::milliseconds(0))==std::future_status::ready){
        auto result=resultPromise.get();
        std::string foreground;
        {std::lock_guard lock(state->mutex);foreground=state->foregroundId;state->closed=true;}
        if(result.backgroundMissionId){
          callbacks.markMissionNotified(*result.backgroundMissionId);
          result.backgroundMissionId.reset();
          if(const auto info=handle.largeOutputInfo()){
            result.outputFilePath=info->path;result.outputFileSize=info->size;
            result.outputMissionId=info->missionId;
          }
          handle.cleanup();return result;
        }
        if(!foreground.empty())callbacks.unregisterForeground(foreground);
        handle.cleanup();return result;
      }
      std::string background,foreground;
      shell::Progress progress;
      bool ready=false,assistant=false;
      {
        std::lock_guard lock(state->mutex);
        ready=state->progressReady;state->progressReady=false;
        background=state->backgroundId;foreground=state->foregroundId;
        assistant=state->assistantAutoBackgrounded;
        progress.lastLines=state->lastProgressOutput;
        progress.fullOutput=state->fullOutput;
        progress.totalLines=state->totalLines;progress.totalBytes=state->totalBytes;
      }
      if(!background.empty()){
        std::lock_guard lock(state->mutex);state->closed=true;
        return backgroundResult(background,assistant,false);
      }
      if(!foreground.empty()&&handle.status()==shell::CommandStatus::Backgrounded){
        std::lock_guard lock(state->mutex);state->closed=true;
        return backgroundResult(foreground,false,true);
      }
      const auto elapsed=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
      if(settings.assistantMode&&settings.isMainThread&&!settings.backgroundMissionsDisabled&&
         elapsed>=settings.assistantBlockingBudgetMs&&
         handle.status()==shell::CommandStatus::Running){
        {std::lock_guard lock(state->mutex);state->assistantAutoBackgrounded=true;}
        startBackgrounding();
        continue;
      }
      if(!ready)continue;
      progress.elapsedTimeSeconds=std::floor(elapsed/1000.0);
      progress.missionId=missionId;progress.timeoutMs=settings.explicitTimeoutMs;
      if(!settings.backgroundMissionsDisabled&&settings.showBackgroundUI&&
         progress.elapsedTimeSeconds>=settings.progressThresholdMs/1000.0){
        std::lock_guard lifecycle(state->lifecycle);
        bool hasForeground=false;
        {std::lock_guard lock(state->mutex);hasForeground=!state->foregroundId.empty();}
        if(!hasForeground){
          const auto id=callbacks.registerForeground(handle);
          std::lock_guard lock(state->mutex);state->foregroundId=id;
        }
        callbacks.showBackgroundHint();
      }
      callbacks.yieldProgress(progress);
    }
  }catch(...){
    {std::lock_guard lock(state->mutex);state->closed=true;}
    if(handle.status()==shell::CommandStatus::Running)handle.kill();
    throw;
  }
}
} // namespace batbox::bash_gadget
