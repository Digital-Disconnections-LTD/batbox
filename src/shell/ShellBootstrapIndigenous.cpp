#include "shell/ShellBootstrap.hpp"
#include "runtime/Encoding.hpp"
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char** environ;
namespace batbox::shell {
bool indigenousIsExecutable(const std::string& shellPath){
  if(shellPath.empty())return false;
  const auto encoded=value::encodeUtf8(shellPath);
  if(::access(encoded.c_str(),X_OK)==0)return true;
  // Fallback: execute `shellPath --version` with stdio ignored and a
  // one-second timeout when X_OK fails (Nix/virtual filesystems can do this).
  posix_spawn_file_actions_t actions;
  if(posix_spawn_file_actions_init(&actions)!=0)return false;
  const int nullFd=::open("/dev/null",O_RDWR|O_CLOEXEC);
  if(nullFd<0){posix_spawn_file_actions_destroy(&actions);return false;}
  for(const auto fd:{0,1,2})posix_spawn_file_actions_adddup2(&actions,nullFd,fd);
  posix_spawn_file_actions_addclose(&actions,nullFd);
  char* argv[]={const_cast<char*>(encoded.c_str()),const_cast<char*>("--version"),nullptr};
  pid_t pid=-1;
  const int spawnError=posix_spawnp(&pid,encoded.c_str(),&actions,nullptr,argv,environ);
  posix_spawn_file_actions_destroy(&actions);::close(nullFd);
  if(spawnError!=0)return false;
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
  int status=0;
  while(true){
    const auto waited=::waitpid(pid,&status,WNOHANG);
    if(waited==pid)return WIFEXITED(status)&&WEXITSTATUS(status)==0;
    if(waited<0){if(errno==EINTR)continue;return false;}
    if(std::chrono::steady_clock::now()>=deadline){
      ::kill(pid,SIGKILL);
      while(::waitpid(pid,&status,0)<0&&errno==EINTR){}
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}
std::function<event_loop::Deferred<std::optional<std::string>>(const std::string&)>
indigenousWhich(event_loop::Loop& loop,const std::string& pathEnvironment){
  return [&loop,pathEnvironment](const std::string& executable){
    event_loop::Deferred<std::optional<std::string>> result(loop);
    if(executable.find('/')!=std::string::npos){result.resolve(std::nullopt);return result;}
    std::size_t start=0;
    while(start<=pathEnvironment.size()){
      const auto end=pathEnvironment.find(':',start);
      const auto dir=pathEnvironment.substr(start,
        end==std::string::npos?std::string::npos:end-start);
      const auto candidate=(dir.empty()?std::string("."):dir)+"/"+executable;
      const auto encoded=value::encodeUtf8(candidate);
      if(::access(encoded.c_str(),X_OK)==0){result.resolve(candidate);return result;}
      if(end==std::string::npos)break;
      start=end+1;
    }
    result.resolve(std::nullopt);return result;
  };
}
} // namespace batbox::shell
