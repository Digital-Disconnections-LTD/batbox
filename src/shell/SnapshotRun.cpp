#include "shell/SnapshotRun.hpp"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdexcept>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace batbox::shell_snapshot {
namespace {
void require(bool ready,const char* name){
  if(!ready)throw std::invalid_argument(std::string("Snapshot service required: ")+name);
}
std::string shellType(const std::string& path){
  return path.find("zsh")!=std::string::npos?"zsh":
    path.find("bash")!=std::string::npos?"bash":"sh";
}
struct Execution {int code=1;std::string out,err;bool timedOut=false,tooLarge=false;};
bool makePipe(int (&fds)[2]){
#if defined(__linux__)
  return ::pipe2(fds,O_CLOEXEC)==0;
#else
  if(::pipe(fds)!=0)return false;
  for(const int fd:fds){const auto flags=::fcntl(fd,F_GETFD);
    if(flags<0||::fcntl(fd,F_SETFD,flags|FD_CLOEXEC)<0){
      ::close(fds[0]);::close(fds[1]);fds[0]=fds[1]=-1;return false;
    }}
  return true;
#endif
}
void drain(int& fd,std::string& output,bool& tooLarge){
  if(fd<0)return;
  char buffer[4096];
  for(;;){
    const auto count=::read(fd,buffer,sizeof(buffer));
    if(count>0){
      if(output.size()+static_cast<std::size_t>(count)>1024*1024){tooLarge=true;return;}
      output.append(buffer,static_cast<std::size_t>(count));continue;
    }
    if(count==0){::close(fd);fd=-1;return;}
    if(errno==EINTR)continue;
    if(errno==EAGAIN||errno==EWOULDBLOCK)return;
    ::close(fd);fd=-1;return;
  }
}
Execution runScript(const std::string& shellPath,const std::string& script,
  const std::map<std::string,std::string>& environment){
  int outPipe[2]{-1,-1},errPipe[2]{-1,-1};
  if(!makePipe(outPipe)||!makePipe(errPipe)){
    if(outPipe[0]>=0){::close(outPipe[0]);::close(outPipe[1]);}
    throw std::runtime_error("snapshot output pipe failed");
  }
  std::vector<std::string> values;
  for(const auto& [key,value]:environment)values.push_back(key+"="+value);
  std::vector<char*> envp;
  for(auto& value:values)envp.push_back(value.data());
  envp.push_back(nullptr);
  std::vector<std::string> args={shellPath,"-c","-l",script};
  std::vector<char*> argv;for(auto& arg:args)argv.push_back(arg.data());argv.push_back(nullptr);
  posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions,outPipe[1],STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&actions,errPipe[1],STDERR_FILENO);
  posix_spawn_file_actions_addclose(&actions,outPipe[0]);
  posix_spawn_file_actions_addclose(&actions,outPipe[1]);
  posix_spawn_file_actions_addclose(&actions,errPipe[0]);
  posix_spawn_file_actions_addclose(&actions,errPipe[1]);
  posix_spawnattr_t attr;posix_spawnattr_init(&attr);
  posix_spawnattr_setflags(&attr,POSIX_SPAWN_SETPGROUP);
  posix_spawnattr_setpgroup(&attr,0);
  pid_t pid=-1;
  const auto error=posix_spawn(&pid,shellPath.c_str(),&actions,&attr,argv.data(),envp.data());
  posix_spawnattr_destroy(&attr);posix_spawn_file_actions_destroy(&actions);
  ::close(outPipe[1]);::close(errPipe[1]);
  if(error){::close(outPipe[0]);::close(errPipe[0]);
    throw std::runtime_error("snapshot spawn failed: "+std::string(std::strerror(error)));}
  auto out=outPipe[0],err=errPipe[0];
  ::fcntl(out,F_SETFL,::fcntl(out,F_GETFL)|O_NONBLOCK);
  ::fcntl(err,F_SETFL,::fcntl(err,F_GETFL)|O_NONBLOCK);
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
  Execution result;
  bool exited=false,killed=false;
  int status=0;
  while(!exited||out>=0||err>=0){
    if(!exited){
      const auto waited=::waitpid(pid,&status,WNOHANG);
      if(waited==pid)exited=true;
      else if(waited<0&&errno!=EINTR)exited=true;
    }
    drain(out,result.out,result.tooLarge);
    drain(err,result.err,result.tooLarge);
    const auto now=std::chrono::steady_clock::now();
    if(!killed&&(result.tooLarge||now>=deadline)){
      result.timedOut=now>=deadline;
      ::kill(-pid,SIGKILL);killed=true;
    }
    if(exited&&out<0&&err<0)break;
    pollfd fds[2];nfds_t count=0;
    if(out>=0)fds[count++]={out,POLLIN|POLLHUP,0};
    if(err>=0)fds[count++]={err,POLLIN|POLLHUP,0};
    if(count)::poll(fds,count,20);
    else std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  if(!exited)do{const auto waited=::waitpid(pid,&status,0);
    if(waited==pid)break;
    if(waited<0&&errno!=EINTR)break;
  }while(true);
  if(out>=0)::close(out);
  if(err>=0)::close(err);
  result.code=WIFEXITED(status)?WEXITSTATUS(status):1;
  return result;
}
}
std::optional<std::string> createAndSaveSnapshot(const std::string& shellPath,
  const RunServices& services){
  require(bool(services.configHome),"configHome");
  require(bool(services.homeDirectory),"homeDirectory");
  require(bool(services.nowMillis),"nowMillis");
  require(bool(services.randomId),"randomId");
  require(bool(services.subprocessEnvironment),"subprocessEnvironment");
  require(bool(services.registerCleanup),"registerCleanup");
  require(bool(services.diagnostic),"diagnostic");
  try{
    const auto configFile=getConfigFile(shellPath,services.homeDirectory());
    const bool configExists=std::filesystem::exists(configFile);
    const auto dir=std::filesystem::path(services.configHome())/"shell-snapshots";
    const auto path=dir/("snapshot-"+shellType(shellPath)+"-"+
      std::to_string(services.nowMillis())+"-"+services.randomId()+".sh");
    std::filesystem::create_directories(dir);
    const auto script=getSnapshotScript(shellPath,path.string(),configExists,
      services.homeDirectory(),services.content);
    auto env=services.dontInheritEnvironment?std::map<std::string,std::string>{}:
      services.subprocessEnvironment();
    env["SHELL"]=shellPath;env["GIT_EDITOR"]="true";env["BATBOX"]="1";
    const auto result=runScript(shellPath,script,env);
    if(result.code!=0||result.timedOut||result.tooLarge){
      services.diagnostic("Shell snapshot creation failed: exit "+std::to_string(result.code)+
        (result.timedOut?" (timeout)":"")+(result.tooLarge?" (output limit)":"")+
        (result.err.empty()?"":"; stderr: "+result.err));
      return std::nullopt;
    }
    std::error_code error;
    const auto size=std::filesystem::file_size(path,error);
    if(error){services.diagnostic("Shell snapshot file missing after creation: "+path.string());
      return std::nullopt;}
    services.registerCleanup([saved=path.string()]{std::error_code ignored;
      std::filesystem::remove(saved,ignored);});
    (void)size;
    return path.string();
  }catch(const std::exception& error){
    services.diagnostic(std::string("Unexpected shell snapshot error: ")+error.what());
    return std::nullopt;
  }
}
} // namespace batbox::shell_snapshot
