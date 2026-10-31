#include "runtime/AsyncLaunch.hpp"
#include "instructions/hooks/runtime/IndigenousCommandProcess.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdexcept>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace batbox::instructions::hooks::runtime {
namespace {
void closeFd(int& fd){if(fd>=0){::close(fd);fd=-1;}}
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
void makeNonblocking(int fd){
  const int flags=::fcntl(fd,F_GETFL);
  if(flags<0||::fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0)
    throw std::runtime_error(std::string("Hook pipe setup failed: ")+std::strerror(errno));
}
void readPipe(int& fd,std::string& raw,std::string& combined){
  if(fd<0)return;
  char bytes[8192];
  for(;;){
    const auto count=::read(fd,bytes,sizeof(bytes));
    if(count>0){raw.append(bytes,static_cast<std::size_t>(count));
      combined.append(bytes,static_cast<std::size_t>(count));continue;}
    if(count==0){closeFd(fd);return;}
    if(errno==EINTR)continue;
    if(errno==EAGAIN||errno==EWOULDBLOCK)return;
    throw std::runtime_error(std::string("Hook output read failed: ")+std::strerror(errno));
  }
}
CommandOutcome execute(IndigenousProcessRequest request){
  int in[2]{-1,-1},out[2]{-1,-1},err[2]{-1,-1};
  pid_t child=-1;
  try{
    for(const auto* value:{&request.shellPath,&request.command,&request.cwd,&request.input})
      if(value->find('\0')!=std::string::npos)
        throw std::invalid_argument("Hook process values cannot contain NUL");
    if(!makePipe(in)||!makePipe(out)||!makePipe(err))
      throw std::runtime_error("Cannot create hook process pipes");
    makeNonblocking(in[1]);makeNonblocking(out[0]);makeNonblocking(err[0]);
    posix_spawn_file_actions_t actions;::posix_spawn_file_actions_init(&actions);
    ::posix_spawn_file_actions_adddup2(&actions,in[0],STDIN_FILENO);
    ::posix_spawn_file_actions_adddup2(&actions,out[1],STDOUT_FILENO);
    ::posix_spawn_file_actions_adddup2(&actions,err[1],STDERR_FILENO);
    for(int fd:{in[0],in[1],out[0],out[1],err[0],err[1]})
      ::posix_spawn_file_actions_addclose(&actions,fd);
    const auto cwd=value::encodeUtf8(request.cwd);
    if(!cwd.empty())::posix_spawn_file_actions_addchdir_np(&actions,cwd.c_str());
    posix_spawnattr_t attributes;::posix_spawnattr_init(&attributes);
    ::posix_spawnattr_setflags(&attributes,POSIX_SPAWN_SETPGROUP);
    ::posix_spawnattr_setpgroup(&attributes,0);
    std::vector<std::string> envStrings;
    for(const auto& [name,value]:request.environment){
      if(name.find('\0')!=std::string::npos||name.find('=')!=std::string::npos||
        value.find('\0')!=std::string::npos)
        throw std::invalid_argument("Invalid hook environment entry");
      envStrings.push_back(value::encodeUtf8(name)+"="+value::encodeUtf8(value));
    }
    std::vector<char*> envp;
    for(auto& entry:envStrings)envp.push_back(entry.data());
    envp.push_back(nullptr);
    auto shell=value::encodeUtf8(request.shellPath);
    auto command=value::encodeUtf8(request.command);
    char option[]="-c";
    char* argv[]{shell.data(),option,command.data(),nullptr};
    const int spawnError=::posix_spawn(&child,shell.c_str(),&actions,
      &attributes,argv,envp.data());
    ::posix_spawnattr_destroy(&attributes);::posix_spawn_file_actions_destroy(&actions);
    closeFd(in[0]);closeFd(out[1]);closeFd(err[1]);
    if(spawnError){
      closeFd(in[1]);closeFd(out[0]);closeFd(err[0]);
      const auto message=std::string("Error occurred while executing hook command: ")+
        std::strerror(spawnError);
      return {"",message,message,1,false,false};
    }
    // Node's child.stdin.write(jsonInput + '\n', 'utf8') is a byte boundary.
    auto input=value::encodeUtf8(request.input)+"\n";
    sigset_t blocked;sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);
    ::pthread_sigmask(SIG_BLOCK,&blocked,nullptr);
    std::size_t written=0;bool reaped=false,killed=false;
    bool brokenInput=false;int status=0;
    std::string stdoutRaw,stderrRaw,combinedRaw;
    const auto started=std::chrono::steady_clock::now();
    while(in[1]>=0||out[0]>=0||err[0]>=0||!reaped){
      const auto elapsed=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-started).count();
      if(!killed&&(request.signal.stop_requested()||
        (request.timeoutMs>0&&elapsed>=request.timeoutMs))){
        ::kill(-child,SIGKILL);killed=true;closeFd(in[1]);
      }
      pollfd fds[3];nfds_t count=0;
      if(out[0]>=0)fds[count++]={out[0],POLLIN|POLLHUP,0};
      if(err[0]>=0)fds[count++]={err[0],POLLIN|POLLHUP,0};
      if(in[1]>=0)fds[count++]={in[1],POLLOUT|POLLHUP,0};
      const auto waited=::poll(fds,count,20);
      if(waited<0&&errno!=EINTR)
        throw std::runtime_error(std::string("Hook process poll failed: ")+std::strerror(errno));
      readPipe(out[0],stdoutRaw,combinedRaw);
      readPipe(err[0],stderrRaw,combinedRaw);
      if(in[1]>=0){
        if(written<input.size()){
          const auto amount=::write(in[1],input.data()+written,
            std::min<std::size_t>(8192,input.size()-written));
          if(amount>0)written+=static_cast<std::size_t>(amount);
          else if(amount<0&&errno!=EAGAIN&&errno!=EINTR){
            brokenInput=errno==EPIPE;closeFd(in[1]);
          }
        }
        if(written==input.size())closeFd(in[1]);
      }
      if(!reaped){
        const auto done=::waitpid(child,&status,WNOHANG);
        if(done==child||(done<0&&errno==ECHILD))reaped=true;
        else if(done<0&&errno!=EINTR)
          throw std::runtime_error(std::string("Hook process wait failed: ")+std::strerror(errno));
      }
    }
    if(brokenInput){
      const std::string message="Hook command closed stdin before hook input was fully written (EPIPE)";
      return {"",message,message,1,false,false};
    }
    const auto stdoutText=value::decodeUtf8(stdoutRaw);
    const auto stderrText=value::decodeUtf8(stderrRaw);
    const auto output=value::decodeUtf8(combinedRaw);
    const int code=WIFEXITED(status)?WEXITSTATUS(status):1;
    return {stdoutText,stderrText,output,code,request.signal.stop_requested(),false};
  }catch(...){
    if(child>0){::kill(-child,SIGKILL);
      while(::waitpid(child,nullptr,0)<0&&errno==EINTR){} }
    closeFd(in[0]);closeFd(in[1]);closeFd(out[0]);closeFd(out[1]);
    closeFd(err[0]);closeFd(err[1]);throw;
  }
}
}
event_loop::Deferred<CommandOutcome> spawnHookProcess(
  event_loop::Loop& loop,IndigenousProcessRequest request){
  if(!loop.isOwnerThread())throw std::logic_error("Hook process owner required");
  event_loop::Deferred<CommandOutcome> result(loop);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  const auto dispatcher=loop.dispatcher();
  try{
    ::batbox::runtime::contextThread([result,request=std::move(request),dispatcher,keep]()mutable{
      std::optional<CommandOutcome> value;std::exception_ptr error;
      try{value=execute(std::move(request));}catch(...){error=std::current_exception();}
      try{dispatcher.postMission([result,value=std::move(value),error,keep]{
        if(error)result.reject(error);else result.resolve(*value);
        keep->release();
      });}catch(...){keep->release();}
    }).detach();
  }catch(...){keep->release();result.reject(std::current_exception());}
  return result;
}
} // namespace batbox::instructions::hooks::runtime
