#include "runtime/AsyncLaunch.hpp"
#include "shell/ManagedShellInternal.hpp"
#include "shell/BashProvider.hpp"
#include "shell/RandomSalt.hpp"
#include "shell/FormatDuration.hpp"
#include "shell/ShellCwdCompletion.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>

namespace batbox::shell {
namespace {
bool backgroundJob(const std::shared_ptr<ManagedShell::Job>& job,const FlightConfig& config,bool byUser=false){
  {
    std::lock_guard lock(job->mutex);
    if(job->completed||job->background)return false;
    job->background=true;job->backgroundedByUser=byUser;
  }
  job->cv.notify_all();
  std::weak_ptr<ManagedShell::Job> weak=job;
  ::batbox::runtime::contextThread([weak,limit=config.maxBackgroundOutputBytes]{
    while(auto job=weak.lock()){
      {
        std::lock_guard lock(job->mutex);
        if(job->completed||!job->background)return;
      }
      std::error_code sizeError;
      const auto size=std::filesystem::file_size(runtime::pathFromUtf8(job->outputPath),sizeError);
      if(!sizeError&&size>limit){
        {
          std::lock_guard lock(job->mutex);
          if(job->completed)return;
          job->killedForSize=true;job->killed=true;
        }
        terminateGroup(job->pid);return;
      }
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
  }).detach();
  return true;
}
std::string tailLines(const std::string& content,std::size_t lines){
  std::size_t at=content.size();for(std::size_t i=0;i<lines&&at>0;i++){
    auto found=content.rfind('\n',at-1);if(found==std::string::npos)return content;
    at=found;
  }
  return content.substr(at==0?0:at+1);
}
std::size_t countLines(const std::string& content){
  return static_cast<std::size_t>(std::count(content.begin(),content.end(),'\n'))+1;
}
}
ManagedShell::ManagedShell(FlightConfig config):config_(std::move(config)),cwd_(config_.originalCwd){
  if(config_.shellPath.empty())throw std::invalid_argument("shellPath required");
  if(config_.originalCwd.empty())throw std::invalid_argument("originalCwd required");
  if(config_.outputDir.empty())throw std::invalid_argument("outputDir required");
  if(!config_.generateMissionId)throw std::invalid_argument("generateTaskId required");
  if(!config_.splitCommand)throw std::invalid_argument("splitCommand required");
  if(config_.provider&&config_.provider->shellPath()!=config_.shellPath)
    throw std::invalid_argument("provider shellPath differs from session shellPath");
  if(config_.cwdCompletion&&!config_.cwdCompletion->onFailure)
    throw std::invalid_argument("cwd completion failure diagnostic required");
  if(config_.maxInlineOutputBytes==0)throw std::invalid_argument("maxInlineOutputBytes must be positive");
}
void ManagedShell::beginShutdown(){
  std::lock_guard lock(mutex_);shuttingDown_=true;
}
std::vector<std::string> ManagedShell::activeJobIds()const{
  std::vector<std::string> ids;
  std::lock_guard lock(mutex_);
  for(const auto& [id,job]:jobs_){
    std::lock_guard jobLock(job->mutex);
    if(!job->callbackDone)ids.push_back(id);
  }
  return ids;
}
void ManagedShell::waitForShutdown(const std::string& missionId){
  std::shared_ptr<Job> job;
  {
    std::lock_guard lock(mutex_);
    const auto it=jobs_.find(missionId);
    if(it==jobs_.end())return;
    job=it->second;
  }
  std::unique_lock lock(job->mutex);
  job->cv.wait(lock,[&]{return job->callbackDone;});
}
std::string ManagedShell::cwd()const{std::lock_guard lock(mutex_);return cwd_;}
std::string ManagedShell::allocateMissionId()const{return config_.generateMissionId();}
std::optional<CommandStatus> ManagedShell::status(const std::string& id)const{
  std::shared_ptr<Job> job;
  {std::lock_guard lock(mutex_);const auto it=jobs_.find(id);if(it==jobs_.end())return std::nullopt;
    job=it->second;}
  std::lock_guard lock(job->mutex);
  if(job->killed)return CommandStatus::Killed;
  if(job->completed)return CommandStatus::Completed;
  if(job->background)return CommandStatus::Backgrounded;
  return CommandStatus::Running;
}
void ManagedShell::setCwd(const std::string& path){
  std::error_code error;const auto canonical=std::filesystem::canonical(runtime::pathFromUtf8(path),error);
  if(error||!std::filesystem::is_directory(canonical))throw std::invalid_argument("invalid shell cwd: "+path);
  std::lock_guard lock(mutex_);cwd_=runtime::pathToUtf8(canonical);
}
std::string ManagedShell::outputPath(const std::string& id)const{return config_.outputDir+"/"+id+".output";}
std::string ManagedShell::readOutput(const std::string& id)const{
  std::size_t size=0;
  try{return readOutputPrefix(outputPath(id),config_.maxInlineOutputBytes,size);}
  catch(const std::exception& error){return "<bash output unavailable: output file "+outputPath(id)+
    " could not be read. "+error.what()+">";}
}
std::exception_ptr ManagedShell::backgroundCallbackFailure(const std::string& id)const{
  std::shared_ptr<Job> job;
  {std::lock_guard lock(mutex_);auto it=jobs_.find(id);
    if(it==jobs_.end())throw std::out_of_range("Unknown shell task ID: "+id);
    job=it->second;}
  std::lock_guard lock(job->mutex);return job->completionFailure;
}
ExecResult ManagedShell::resultFor(const std::shared_ptr<Job>& job,bool backgroundResult){
  ExecResult result;
  if(backgroundResult){std::lock_guard lock(job->mutex);
    result.backgroundMissionId=job->missionId;result.backgroundedByUser=job->backgroundedByUser;return result;}
  {
    std::lock_guard lock(job->mutex);
    result.code=job->code;result.interrupted=job->code==137;
    if(job->killedForSize)result.stderrText="Background command killed: output file exceeded 5GB";
    else if(job->timedOut)result.stderrText="Command timed out after "+
      formatDuration(job->timeoutMs);
  }
  try{
    std::size_t size=0;
    result.stdoutText=readOutputPrefix(job->outputPath,config_.maxInlineOutputBytes,size);
    if(size>config_.maxInlineOutputBytes){result.outputFilePath=job->outputPath;
      result.outputFileSize=size;result.outputMissionId=job->missionId;}
  }catch(const std::exception& e){
    result.stdoutText="<bash output unavailable: output file "+job->outputPath+
      " could not be read. "+e.what()+">";
  }
  return result;
}
void ManagedShell::updateCwd(const std::shared_ptr<Job>& job,const RunOptions& options,
  const ExecResult& result){
  if(config_.cwdCompletion){
    try{completeShellCwd(&result,options.preventCwdChanges,options.useSandbox,
      job->cwdPath,job->startingCwd,*config_.cwdCompletion);}
    catch(...){try{config_.cwdCompletion->onFailure(std::current_exception());}catch(...){}}
    return;
  }
  if(!options.preventCwdChanges){
    std::ifstream stream(runtime::pathFromUtf8(job->cwdPath));
    if(stream){std::string newCwd;std::getline(stream,newCwd);stream.close();
      std::error_code error;const auto canonical=std::filesystem::canonical(runtime::pathFromUtf8(newCwd),error);
      if(!error){std::lock_guard lock(mutex_);cwd_=runtime::pathToUtf8(canonical);}}
  }
  std::error_code error;std::filesystem::remove(runtime::pathFromUtf8(job->cwdPath),error);
}
ExecResult ManagedShell::run(const RunOptions& options){
  {
    std::lock_guard lock(mutex_);
    if(shuttingDown_){
      ExecResult result;result.code=145;result.interrupted=true;
      result.stderrText="Command aborted during shutdown";
      result.preSpawnError=result.stderrText;return result;
    }
  }
  RunOptions effective=options;
  if(config_.provider&&!effective.preparedCommandString){
    if(effective.useSandbox&&!effective.sandboxTmpDir)
      throw std::invalid_argument("sandbox temp directory required");
    const auto execId=config_.generateExecId?config_.generateExecId():randomPlaceholderSalt().substr(0,4);
    const auto built=config_.provider->buildExecCommand(effective.command,
      {execId,effective.sandboxTmpDir,effective.useSandbox});
    effective.preparedCommandString=built.commandString;
    effective.preparedCwdFilePath=built.cwdFilePath;
  }
  const auto missionId=options.reservedMissionId?*options.reservedMissionId:config_.generateMissionId();
  std::string startingCwd=cwd();
  std::error_code error;
  (void)std::filesystem::canonical(runtime::pathFromUtf8(startingCwd),error);
  if(error){
    const auto fallback=config_.originalCwd;
    error.clear();(void)std::filesystem::canonical(runtime::pathFromUtf8(fallback),error);
    if(!error){
      std::lock_guard lock(mutex_);cwd_=fallback;startingCwd=fallback;
    }else{
      const auto message="Working directory \""+startingCwd+
        "\" no longer exists. Please restart Batbox from an existing directory.";
      ExecResult failure;failure.code=1;failure.stderrText=message;failure.preSpawnError=message;return failure;
    }
  }
  if(options.signal.stop_requested()){
    ExecResult result;result.stderrText="Command aborted before execution";
    result.code=145;result.interrupted=true;return result;
  }
  // Classify the command before creating a child. A
  // parser exception must not leave a live process outside this run loop.
  const bool immediateBackground=options.runInBackground&&!config_.backgroundMissionsDisabled;
  bool autoBackground=false;
  if(!immediateBackground&&!config_.backgroundMissionsDisabled&&options.autoBackgroundEnabled)
    autoBackground=isAutobackgroundingAllowed(options.command,config_.splitCommand);
  std::shared_ptr<Job> job;
  try{job=spawn(effective,missionId,startingCwd);}
  catch(const std::exception& e){ExecResult result;result.stderrText=e.what();
    result.code=126;result.interrupted=true;result.preSpawnError=e.what();return result;}
  if(options.signal.stop_possible()){
    std::weak_ptr<Job> weak=job;
    std::function<void()> cancel=[weak,reason=options.currentAbortReason]{
      if(auto current=weak.lock()){
        try{if(reason&&reason()=="interrupt")return;}catch(...){}
        std::lock_guard lock(current->mutex);
        if(current->completed)return;
        current->cancelledBySignal=true;current->killed=true;
        terminateGroup(current->pid);current->cv.notify_all();
      }
    };
    job->signalCallback=std::make_unique<std::stop_callback<std::function<void()>>>(
      options.signal,std::move(cancel));
  }
  bool reject=false;
  {
    std::lock_guard lock(mutex_);
    reject=shuttingDown_;
    if(!reject)jobs_[missionId]=job;
  }
  if(reject){
    {
      std::lock_guard lock(job->mutex);
      job->killed=true;terminateGroup(job->pid);
    }
    std::unique_lock lock(job->mutex);
    job->cv.wait(lock,[&]{return job->postExitReady;});
    lock.unlock();
    std::error_code ignored;
    std::filesystem::remove(runtime::pathFromUtf8(job->outputPath),ignored);
    std::filesystem::remove(runtime::pathFromUtf8(job->cwdPath),ignored);
    ExecResult result;result.code=145;result.interrupted=true;
    result.stderrText="Command aborted during shutdown";
    result.preSpawnError=result.stderrText;return result;
  }
  if(options.startSignal){
    {std::lock_guard lock(options.startSignal->mutex);options.startSignal->started=true;}
    options.startSignal->cv.notify_all();
  }
  if(immediateBackground&&backgroundJob(job,config_))
    return resultFor(job,true);
  if(immediateBackground)
    autoBackground=isAutobackgroundingAllowed(options.command,config_.splitCommand);
  const double timeout=options.timeoutMs.value_or(config_.defaultTimeoutMs);
  {std::lock_guard lock(job->mutex);job->timeoutMs=timeout;}
  auto nextProgress=job->started+std::chrono::seconds(2);
  bool timeoutNotified=false;
  std::exception_ptr timeoutFailure;
  for(;;){
    std::unique_lock lock(job->mutex);
    if(job->completed)break;
    if(job->background){lock.unlock();return resultFor(job,true);}
    const auto now=std::chrono::steady_clock::now();
    const auto elapsed=std::chrono::duration<double,std::milli>(now-job->started).count();
    if(elapsed>=timeout&&!(autoBackground&&options.onTimeout&&timeoutNotified)){
      if(autoBackground&&options.onTimeout){
        timeoutNotified=true;lock.unlock();
        bool attached=false;
        try{attached=options.onTimeout([&](const std::string& id){
          return id==missionId&&backgroundJob(job,config_);});}
        catch(...){timeoutFailure=std::current_exception();terminateGroup(job->pid);}
        bool isBackground=false;
        {std::lock_guard stateLock(job->mutex);isBackground=job->background;
          if(!attached&&!timeoutFailure&&!job->timedOut){job->timedOut=true;job->killed=true;
            terminateGroup(job->pid);}}
        if(isBackground)return resultFor(job,true);
        continue;
      }
      if(autoBackground){lock.unlock();if(backgroundJob(job,config_))return resultFor(job,true);
        continue;}
      if(!job->timedOut){job->timedOut=true;job->killed=true;terminateGroup(job->pid);}
    }
    if(config_.assistantAutoBackgroundEnabled&&options.managedAssistantBackgroundEnabled&&
       options.isMainThread&&
       elapsed>=config_.assistantBlockingBudgetMs&&!config_.backgroundMissionsDisabled){
      lock.unlock();if(backgroundJob(job,config_)){auto result=resultFor(job,true);
        result.assistantAutoBackgrounded=true;return result;}
      continue;
    }
    if(options.onProgress&&now>=nextProgress){
      lock.unlock();
      try{
        std::size_t size=0;const auto content=readOutputPrefix(job->outputPath,config_.maxInlineOutputBytes,size);
        Progress progress;progress.lastLines=tailLines(content,5);progress.fullOutput=tailLines(content,100);
        progress.elapsedTimeSeconds=std::floor(elapsed/1000);progress.totalLines=countLines(content);
        progress.totalBytes=size;progress.missionId=missionId;if(options.timeoutMs)progress.timeoutMs=timeout;
        options.onProgress(progress);
      }catch(...){}
      nextProgress=now+std::chrono::seconds(1);
      continue;
    }
    job->cv.wait_for(lock,std::chrono::milliseconds(50));
  }
  auto result=resultFor(job,false);
  updateCwd(job,options,result);
  if(!result.outputFilePath){std::filesystem::remove(runtime::pathFromUtf8(job->outputPath),error);}
  {std::lock_guard lock(mutex_);jobs_.erase(missionId);}
  if(timeoutFailure)std::rethrow_exception(timeoutFailure);
  return result;
}
ExecResult ManagedShell::wait(const std::string& missionId,double maxWaitMs){
  std::shared_ptr<Job> job;
  {std::lock_guard lock(mutex_);auto it=jobs_.find(missionId);
    if(it==jobs_.end())throw std::out_of_range("Unknown shell task ID: "+missionId);
    job=it->second;}
  std::unique_lock lock(job->mutex);
  if(maxWaitMs<0)job->cv.wait(lock,[&]{return job->postExitReady;});
  else job->cv.wait_for(lock,std::chrono::duration<double,std::milli>(maxWaitMs),[&]{return job->postExitReady;});
  const bool completed=job->postExitReady;lock.unlock();
  return resultFor(job,!completed);
}
ExecResult ManagedShell::stop(const std::string& missionId){
  std::shared_ptr<Job> job;
  {std::lock_guard lock(mutex_);auto it=jobs_.find(missionId);
    if(it==jobs_.end())throw std::out_of_range("Unknown shell task ID: "+missionId);
    job=it->second;}
  {
    std::lock_guard lock(job->mutex);
    if(!job->completed){job->killed=true;terminateGroup(job->pid);}
  }
  std::unique_lock lock(job->mutex);job->cv.wait(lock,[&]{return job->postExitReady;});lock.unlock();
  return resultFor(job,false);
}
bool ManagedShell::requestKill(const std::string& missionId){
  std::shared_ptr<Job> job;
  {std::lock_guard lock(mutex_);const auto it=jobs_.find(missionId);
    if(it==jobs_.end())return false;
    job=it->second;}
  std::lock_guard lock(job->mutex);
  if(job->completed)return false;
  job->killed=true;terminateGroup(job->pid);job->cv.notify_all();return true;
}
bool ManagedShell::background(const std::string& missionId,bool byUser){
  std::shared_ptr<Job> job;
  {std::lock_guard lock(mutex_);auto it=jobs_.find(missionId);
    if(it==jobs_.end())return false;
    job=it->second;}
  if(config_.backgroundMissionsDisabled)return false;
  return backgroundJob(job,config_,byUser);
}
bool ManagedShell::cleanup(const std::string& missionId){
  std::shared_ptr<Job> job;
  {std::lock_guard lock(mutex_);auto it=jobs_.find(missionId);
    if(it==jobs_.end())return false;
    job=it->second;
    std::lock_guard jobLock(job->mutex);if(!job->completed)return false;
    jobs_.erase(it);}
  std::error_code error;std::filesystem::remove(runtime::pathFromUtf8(job->outputPath),error);
  std::filesystem::remove(runtime::pathFromUtf8(job->cwdPath),error);return true;
}
bool ManagedShell::release(const std::string& missionId){
  std::lock_guard lock(mutex_);const auto it=jobs_.find(missionId);
  if(it==jobs_.end())return false;
  {std::lock_guard jobLock(it->second->mutex);if(!it->second->completed)return false;}
  jobs_.erase(it);return true;
}
} // namespace batbox::shell
