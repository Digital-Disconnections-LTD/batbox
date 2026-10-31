#include "auth/storage/IndigenousSecurity.hpp"
#if defined(_WIN32)
#include <stdexcept>
namespace batbox::auth::storage {
MacKeychainOps indigenousMacKeychainOps(event_loop::Loop&,
    std::function<void(const std::string&,const std::string&)>,
    std::string){
  throw std::runtime_error("macOS keychain is unavailable on Windows");
}
}
#else
#include "locking/Async.hpp"
#include "runtime/JsonValue.hpp"
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <stdexcept>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <signal.h>

extern char **environ;

namespace batbox::auth::storage {
namespace {
[[noreturn]] void osError(const char* action,int code=errno){
  throw std::system_error(code,std::generic_category(),action);
}
void closeFd(int& fd){if(fd>=0){(void)::close(fd);fd=-1;}}
SecurityResult run(const std::string& executable,
    const std::vector<std::string>& args,
    const std::optional<std::string>& input){
  int outputPipe[2]={-1,-1},inputPipe[2]={-1,-1};
  if(::pipe(outputPipe)!=0)osError("open security output pipe");
  if(input&&::pipe(inputPipe)!=0){
    const auto code=errno;
    closeFd(outputPipe[0]);closeFd(outputPipe[1]);
    osError("open security input pipe",code);
  }
  posix_spawn_file_actions_t actions;
  posix_spawnattr_t attributes;
  bool actionsReady=false,attributesReady=false;
  pid_t child=-1;
  try{
    auto code=::posix_spawn_file_actions_init(&actions);
    if(code)osError("init security actions",code);
    actionsReady=true;
    code=::posix_spawnattr_init(&attributes);
    if(code)osError("init security attributes",code);
    attributesReady=true;
    code=::posix_spawnattr_setflags(&attributes,POSIX_SPAWN_SETPGROUP);
    if(code)osError("set security process group",code);
    code=::posix_spawnattr_setpgroup(&attributes,0);
    if(code)osError("set security process group",code);
    const auto add=[&](int result){
      if(result)osError("configure security pipe",result);};
    add(::posix_spawn_file_actions_adddup2(&actions,outputPipe[1],
      STDOUT_FILENO));
    add(::posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,
      "/dev/null",O_WRONLY,0));
    if(input)add(::posix_spawn_file_actions_adddup2(&actions,
      inputPipe[0],STDIN_FILENO));
    else add(::posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,
      "/dev/null",O_RDONLY,0));
    add(::posix_spawn_file_actions_addclose(&actions,outputPipe[0]));
    add(::posix_spawn_file_actions_addclose(&actions,outputPipe[1]));
    if(input){
      add(::posix_spawn_file_actions_addclose(&actions,inputPipe[0]));
      add(::posix_spawn_file_actions_addclose(&actions,inputPipe[1]));
    }
    std::vector<char*> argv;
    argv.reserve(args.size()+2);
    argv.push_back(const_cast<char*>(executable.c_str()));
    for(const auto& arg:args)argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    code=::posix_spawn(&child,executable.c_str(),&actions,&attributes,
      argv.data(),environ);
    if(code)osError("spawn security",code);
    ::posix_spawn_file_actions_destroy(&actions);actionsReady=false;
    ::posix_spawnattr_destroy(&attributes);attributesReady=false;
    closeFd(outputPipe[1]);closeFd(inputPipe[0]);
    if(input){
      std::size_t position=0;
      while(position<input->size()){
        const auto count=::write(inputPipe[1],input->data()+position,
          input->size()-position);
        if(count<0){if(errno==EINTR)continue;osError("write security stdin");}
        if(count==0)throw std::runtime_error("Short security stdin write");
        position+=static_cast<std::size_t>(count);
      }
      closeFd(inputPipe[1]);
    }
    const auto flags=::fcntl(outputPipe[0],F_GETFL);
    if(flags<0||::fcntl(outputPipe[0],F_SETFL,flags|O_NONBLOCK)<0)
      osError("set security output nonblocking");
    const auto deadline=std::chrono::steady_clock::now()+
      std::chrono::minutes(10);
    std::string stdoutText;
    bool exited=false;
    int status=0;
    while(!exited||outputPipe[0]>=0){
      if(!exited){
        const auto waited=::waitpid(child,&status,WNOHANG);
        if(waited==child)exited=true;
        else if(waited<0&&errno!=EINTR)osError("wait security process");
      }
      if(std::chrono::steady_clock::now()>=deadline){
        (void)::kill(-child,SIGKILL);(void)::kill(child,SIGKILL);
        throw std::runtime_error("Security process timed out");
      }
      if(outputPipe[0]<0)continue;
      pollfd descriptor{outputPipe[0],POLLIN|POLLHUP,0};
      const auto polled=::poll(&descriptor,1,20);
      if(polled<0&&errno!=EINTR)osError("poll security output");
      char buffer[8192];
      for(;;){
        const auto count=::read(outputPipe[0],buffer,sizeof(buffer));
        if(count>0){
          stdoutText.append(buffer,static_cast<std::size_t>(count));
          if(stdoutText.size()>1000000)
            throw std::runtime_error("Security output exceeded buffer");
          continue;
        }
        if(count==0){closeFd(outputPipe[0]);break;}
        if(errno==EINTR)continue;
        if(errno==EAGAIN||errno==EWOULDBLOCK)break;
        osError("read security output");
      }
    }
    return {WIFEXITED(status)?WEXITSTATUS(status):
      WIFSIGNALED(status)?128+WTERMSIG(status):-1,
      std::move(stdoutText)};
  }catch(...){
    if(child>0){(void)::kill(-child,SIGKILL);(void)::kill(child,SIGKILL);
      (void)::waitpid(child,nullptr,0);}
    if(actionsReady)::posix_spawn_file_actions_destroy(&actions);
    if(attributesReady)::posix_spawnattr_destroy(&attributes);
    closeFd(outputPipe[0]);closeFd(outputPipe[1]);
    closeFd(inputPipe[0]);closeFd(inputPipe[1]);
    throw;
  }
}
}
MacKeychainOps indigenousMacKeychainOps(event_loop::Loop& loop,
    std::function<void(const std::string&,const std::string&)> debug,
    std::string executable){
  if(!debug||executable.empty())
    throw std::invalid_argument("Missing native security binding");
  MacKeychainOps operations;
  operations.findSync=[executable](const std::string& username,
      const std::string& service){
    return value::trim(run(executable,{"find-generic-password","-a",
      username,"-w","-s",service},std::nullopt).stdoutText);
  };
  operations.findAsync=[&loop,executable](const std::string& username,
      const std::string& service){
    return locking::runJob<SecurityResult>(loop,
      [executable,username,service]{
        return run(executable,{"find-generic-password","-a",
          username,"-w","-s",service},std::nullopt);
      });
  };
  operations.executeSync=[executable](const std::vector<std::string>& args,
      const std::optional<std::string>& input){
    return run(executable,args,input);
  };
  operations.deleteSync=[executable](const std::string& username,
      const std::string& service){
    (void)run(executable,{"delete-generic-password","-a",username,
      "-s",service},std::nullopt);
  };
  operations.debug=std::move(debug);
  return operations;
}
}
#endif
