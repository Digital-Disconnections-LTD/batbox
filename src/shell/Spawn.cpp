#include "runtime/AsyncLaunch.hpp"
#include "shell/ManagedShellInternal.hpp"
#include "shell/BashProvider.hpp"
#include "shell/ShellCwdCompletion.hpp"
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <vector>
#include <unistd.h>
extern char** environ;

namespace batbox::shell {
void terminateGroup(ShellProcessId pid){if(pid>0)::kill(-pid,SIGKILL);}
namespace {
std::map<std::string,std::string> childEnv(const FlightConfig& config){
  std::map<std::string,std::string> env;
  for(char** p=environ;*p;++p){std::string item=*p;const auto equals=item.find('=');
    if(equals!=std::string::npos)env[item.substr(0,equals)]=item.substr(equals+1);}
  for(const auto& [key,value]:config.environmentOverrides)env[key]=value;
  env["SHELL"]=config.shellPath;env["GIT_EDITOR"]="true";env["BATBOX"]="1";
  return env;
}
bool safeMissionId(const std::string& id){
  if(id.empty())return false;
  for(char c:id)if(!std::isalnum(static_cast<unsigned char>(c))&&c!='-'&&c!='_')return false;
  return true;
}
}
std::shared_ptr<ManagedShell::Job> ManagedShell::spawn(const RunOptions& options,
  const std::string& missionId,const std::string& cwd){
  if(!safeMissionId(missionId))throw std::invalid_argument("invalid shell task ID");
  std::filesystem::create_directories(config_.outputDir);
  auto job=std::make_shared<Job>();job->missionId=missionId;job->command=options.command;
  job->description=options.description.empty()?options.command:options.description;
  job->outputPath=outputPath(missionId);job->cwdPath=config_.outputDir+"/"+missionId+"-cwd";
  job->startingCwd=cwd;job->started=std::chrono::steady_clock::now();
  std::string script;
  std::vector<std::string> args;
  if(config_.provider){
    if(!options.preparedCommandString||!options.preparedCwdFilePath)
      throw std::invalid_argument("provider command must be prepared before spawn");
    script=*options.preparedCommandString;job->cwdPath=*options.preparedCwdFilePath;
  }else{
    script="eval "+quotePosix(options.command)+"; batbox_status=$?; "
      "if [ \"$batbox_status\" -eq 0 ]; then pwd -P >| "+quotePosix(job->cwdPath)+"; fi; exit \"$batbox_status\"";
  }
  if(options.useSandbox){
    if(!options.sandboxTmpDir||!options.sandboxWrap||!options.logDebug)
      throw std::invalid_argument("sandbox wrapping, temp directory, and diagnostic services required");
    script=options.sandboxWrap(script);
    if(::mkdir(options.sandboxTmpDir->c_str(),0700)!=0)
      options.logDebug("Failed to create "+*options.sandboxTmpDir+
        " directory: "+std::strerror(errno));
  }
  if(config_.provider){
    args.push_back(config_.provider->shellPath());
    for(const auto& arg:config_.provider->getSpawnArgs(script))args.push_back(arg);
  }else args={config_.shellPath,"-c","-l",script};
  auto env=childEnv(config_);std::vector<std::string> envStrings;std::vector<char*> envp;
  if(config_.provider)for(const auto& [key,value]:config_.provider->getEnvironmentOverrides(options.command))
    env[key]=value;
  for(const auto& [key,value]:env)envStrings.push_back(key+"="+value);
  for(auto& entry:envStrings)envp.push_back(entry.data());
  envp.push_back(nullptr);
  std::vector<char*> argv;
  for(auto& arg:args)argv.push_back(arg.data());
  argv.push_back(nullptr);
  const int output=::open(job->outputPath.c_str(),O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW|O_CLOEXEC,0600);
  if(output<0)throw std::runtime_error("open shell output failed: "+std::string(std::strerror(errno)));
  const int nullInput=::open("/dev/null",O_RDONLY|O_CLOEXEC);
  if(nullInput<0){::close(output);throw std::runtime_error("open /dev/null failed");}
  posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions,nullInput,STDIN_FILENO);
  posix_spawn_file_actions_adddup2(&actions,output,STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions,output,STDERR_FILENO);
  posix_spawn_file_actions_addclose(&actions,nullInput);
  posix_spawn_file_actions_addclose(&actions,output);
  posix_spawn_file_actions_addchdir_np(&actions,cwd.c_str());
  posix_spawnattr_t attr;posix_spawnattr_init(&attr);
  posix_spawnattr_setflags(&attr,POSIX_SPAWN_SETPGROUP);
  posix_spawnattr_setpgroup(&attr,0);
  const int error=posix_spawn(&job->pid,config_.shellPath.c_str(),&actions,&attr,argv.data(),envp.data());
  posix_spawnattr_destroy(&attr);posix_spawn_file_actions_destroy(&actions);
  ::close(output);::close(nullInput);
  if(error!=0)throw std::runtime_error("spawn shell failed: "+std::string(std::strerror(error)));
  auto completion=config_.onBackgroundComplete;
  auto completionError=config_.onBackgroundError;
  auto cwdCompletion=config_.cwdCompletion;
  const bool sandbox=options.useSandbox,preventCwdChanges=options.preventCwdChanges;
  const auto maxInline=config_.maxInlineOutputBytes;
  ::batbox::runtime::contextThread([job,completion,completionError,cwdCompletion,sandbox,preventCwdChanges,maxInline]{
    int status=0;pid_t waited;
    do{waited=::waitpid(job->pid,&status,0);}while(waited<0&&errno==EINTR);
    ExecResult notification;
    {
      std::lock_guard lock(job->mutex);
      job->completed=true;
      job->code=job->timedOut?143:job->killed?137:
        waited<0?1:WIFEXITED(status)?WEXITSTATUS(status):
        WIFSIGNALED(status)&&WTERMSIG(status)==SIGTERM?144:1;
      notification.code=job->code;notification.backgroundMissionId=job->background?std::optional(job->missionId):std::nullopt;
      notification.interrupted=job->code==137;
    }
    if(notification.backgroundMissionId){
      if(cwdCompletion){
        try{completeShellCwd(&notification,preventCwdChanges,sandbox,
          job->cwdPath,job->startingCwd,*cwdCompletion);}
        catch(...){try{cwdCompletion->onFailure(std::current_exception());}catch(...){}}
      }else{std::error_code error;std::filesystem::remove(job->cwdPath,error);}
    }
    {std::lock_guard lock(job->mutex);job->postExitReady=true;}
    job->cv.notify_all();
    if(completion&&notification.backgroundMissionId){
      try{
        std::size_t totalBytes=0;
        notification.stdoutText=readOutputPrefix(job->outputPath,maxInline,totalBytes);
        if(totalBytes>maxInline){notification.outputFilePath=job->outputPath;
          notification.outputFileSize=totalBytes;notification.outputMissionId=job->missionId;}
      }catch(const std::exception& error){
        notification.stdoutText="<bash output unavailable: output file "+job->outputPath+
          " could not be read. "+error.what()+">";
      }
      try{completion(job->missionId,notification);}
      catch(...){
        const auto failure=std::current_exception();
        {std::lock_guard lock(job->mutex);job->completionFailure=failure;}
        if(completionError)try{completionError(job->missionId,failure);}catch(...){}
      }
    }
    {std::lock_guard lock(job->mutex);job->callbackDone=true;}
    job->cv.notify_all();
  }).detach();
  return job;
}
} // namespace batbox::shell
