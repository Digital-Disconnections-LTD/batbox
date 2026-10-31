#include "runtime/AsyncLaunch.hpp"
#include "auth/helper/IndigenousCommand.hpp"
#if defined(_WIN32)
#include "process/ExecFileNoThrow.hpp"
#include "runtime/IndigenousUnicode.hpp"
#include <cmath>
#include <cstdlib>
#include <stdexcept>
namespace batbox::auth::helper {
event_loop::Deferred<CommandResult> indigenousExecuteShell(
    event_loop::Loop& loop,const std::string& command,double timeoutMs){
  if(!std::isfinite(timeoutMs)||timeoutMs<0)
    throw std::invalid_argument("Invalid helper timeout");
  event_loop::Deferred<CommandResult> output(loop);
  process::ExecOptions options;options.timeout=timeoutMs;
  options.windowsVerbatimArguments=true;
  const auto configured=runtime::environmentUtf8("COMSPEC");
  const auto executable=configured.empty()?std::string("cmd.exe"):configured;
  // cmd /s removes this outer pair. CRT argv escaping would insert literal
  // backslashes before the command's quotes and break quoted helper paths.
  process::indigenousExeca(loop)({executable,{"/d","/s","/c","\""+command+"\""},options})
    .then([output](const process::ExecaResult* value,
        const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      output.resolve(CommandResult{value->failed,
        value->shortMessage=="Command timed out",
        value->exitCode.value_or(-1),value->stdoutText,value->stderrText});
    });
  return output;
}
}
#else
#include <cerrno>
#include <chrono>
#include <cmath>
#include <fcntl.h>
#include <memory>
#include <optional>
#include <poll.h>
#include <spawn.h>
#include <stdexcept>
#include <string>
#include <system_error>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <signal.h>

extern char **environ;

namespace batbox::auth::helper {
namespace {
[[noreturn]] void osError(const char* action,int code=errno){
  throw std::system_error(code,std::generic_category(),action);
}
void closeFd(int& fd){if(fd>=0){::close(fd);fd=-1;}}
void nonblocking(int fd){
  const auto flags=::fcntl(fd,F_GETFL);
  if(flags<0||::fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0)
    osError("set helper pipe nonblocking");
}
void drain(int& fd,std::string& output){
  if(fd<0)return;
  char bytes[8192];
  for(;;){
    const auto count=::read(fd,bytes,sizeof(bytes));
    if(count>0){output.append(bytes,static_cast<std::size_t>(count));continue;}
    if(count==0){closeFd(fd);return;}
    if(errno==EINTR)continue;
    if(errno==EAGAIN||errno==EWOULDBLOCK)return;
    osError("read helper pipe");
  }
}
CommandResult run(const std::string& command,double timeoutMs){
  int out[2]={-1,-1},err[2]={-1,-1};
  if(::pipe(out)!=0)osError("open helper stdout pipe");
  if(::pipe(err)!=0){const auto code=errno;
    closeFd(out[0]);closeFd(out[1]);osError("open helper stderr pipe",code);}
  posix_spawn_file_actions_t actions;
  posix_spawnattr_t attr;
  bool actionsReady=false,attrReady=false;
  pid_t child=-1;
  try{
    auto code=::posix_spawn_file_actions_init(&actions);
    if(code)osError("init helper file actions",code);
    actionsReady=true;
    code=::posix_spawnattr_init(&attr);
    if(code)osError("init helper spawn attrs",code);
    attrReady=true;
    code=::posix_spawnattr_setflags(&attr,POSIX_SPAWN_SETPGROUP);
    if(code)osError("set helper process group",code);
    code=::posix_spawnattr_setpgroup(&attr,0);
    if(code)osError("set helper process group",code);
    const auto add=[&](int value){
      if(value)osError("configure helper pipe",value);};
    add(::posix_spawn_file_actions_adddup2(&actions,out[1],STDOUT_FILENO));
    add(::posix_spawn_file_actions_adddup2(&actions,err[1],STDERR_FILENO));
    add(::posix_spawn_file_actions_addclose(&actions,out[0]));
    add(::posix_spawn_file_actions_addclose(&actions,err[0]));
    add(::posix_spawn_file_actions_addclose(&actions,out[1]));
    add(::posix_spawn_file_actions_addclose(&actions,err[1]));
    const char* argv[]={"sh","-c",command.c_str(),nullptr};
    code=::posix_spawn(&child,"/bin/sh",&actions,&attr,
      const_cast<char* const*>(argv),environ);
    if(code)osError("spawn API key helper",code);
    ::posix_spawn_file_actions_destroy(&actions);actionsReady=false;
    ::posix_spawnattr_destroy(&attr);attrReady=false;
    closeFd(out[1]);closeFd(err[1]);
    nonblocking(out[0]);nonblocking(err[0]);
    const auto deadline=std::chrono::steady_clock::now()+
      std::chrono::milliseconds(static_cast<std::int64_t>(timeoutMs));
    bool timedOut=false,exited=false;
    std::optional<std::chrono::steady_clock::time_point> killTime;
    int status=0;
    CommandResult result;
    while(!exited||out[0]>=0||err[0]>=0){
      if(!exited){
        const auto waited=::waitpid(child,&status,WNOHANG);
        if(waited==child)exited=true;
        else if(waited<0&&errno!=EINTR)osError("wait API key helper");
      }
      if(!timedOut&&std::chrono::steady_clock::now()>=deadline){
        timedOut=true;
        killTime=std::chrono::steady_clock::now();
        (void)::kill(-child,SIGKILL);
        (void)::kill(child,SIGKILL);
      }
      if(killTime&&std::chrono::steady_clock::now()-*killTime>
          std::chrono::seconds(1)){
        closeFd(out[0]);closeFd(err[0]);
      }
      pollfd fds[2]={{out[0],POLLIN|POLLHUP,0},
        {err[0],POLLIN|POLLHUP,0}};
      const auto count=::poll(fds,2,10);
      if(count<0&&errno!=EINTR)osError("poll helper output");
      drain(out[0],result.stdoutText);
      drain(err[0],result.stderrText);
    }
    result.timedOut=timedOut;
    result.exitCode=WIFEXITED(status)?WEXITSTATUS(status):
      WIFSIGNALED(status)?128+WTERMSIG(status):-1;
    result.failed=timedOut||result.exitCode!=0;
    return result;
  }catch(...){
    if(child>0){(void)::kill(-child,SIGKILL);
      (void)::kill(child,SIGKILL);(void)::waitpid(child,nullptr,0);}
    if(actionsReady)::posix_spawn_file_actions_destroy(&actions);
    if(attrReady)::posix_spawnattr_destroy(&attr);
    closeFd(out[0]);closeFd(out[1]);
    closeFd(err[0]);closeFd(err[1]);
    throw;
  }
}
}
event_loop::Deferred<CommandResult> indigenousExecuteShell(
    event_loop::Loop& loop,const std::string& command,double timeoutMs){
  if(!std::isfinite(timeoutMs)||timeoutMs<0)
    throw std::invalid_argument("Invalid helper timeout");
  event_loop::Deferred<CommandResult> output(loop);
  auto hold=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  ::batbox::runtime::contextThread([output,hold,command,timeoutMs]{
    try{output.resolve(run(command,timeoutMs));}
    catch(...){output.reject(std::current_exception());}
  }).detach();
  return output;
}
}
#endif
