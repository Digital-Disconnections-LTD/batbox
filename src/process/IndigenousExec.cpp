#include "runtime/AsyncLaunch.hpp"
#include "process/ExecFileNoThrow.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <memory>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdexcept>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char** environ;
namespace batbox::process {
namespace {
void closeFd(int& fd){if(fd>=0){::close(fd);fd=-1;}}
void pipeCloexec(int fds[2]){
#if defined(__linux__)
  if(::pipe2(fds,O_CLOEXEC)==0)return;
#else
  if(::pipe(fds)==0){
    if(::fcntl(fds[0],F_SETFD,FD_CLOEXEC)==0&&
       ::fcntl(fds[1],F_SETFD,FD_CLOEXEC)==0)return;
    const int saved=errno;
    closeFd(fds[0]);closeFd(fds[1]);errno=saved;
  }
#endif
  throw std::runtime_error(std::string("Cannot create process pipe: ")+
    std::strerror(errno));
}
void nonblocking(int fd){
  const int flags=::fcntl(fd,F_GETFL,0);
  if(flags<0||::fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0)
    throw std::runtime_error(std::string("Cannot set process pipe nonblocking: ")+std::strerror(errno));
}
std::string signalName(int signal){
  switch(signal){case SIGTERM:return "SIGTERM";case SIGKILL:return "SIGKILL";
    case SIGINT:return "SIGINT";case SIGPIPE:return "SIGPIPE";
    default:return "SIG"+std::to_string(signal);}
}
void stripFinalNewline(std::string& text){
  if(!text.empty()&&text.back()=='\n'){
    text.pop_back();if(!text.empty()&&text.back()=='\r')text.pop_back();
  }
}
std::vector<std::string> childEnv(const ExecOptions& options){
  std::map<std::string,std::string> env;
  for(char** variable=environ;*variable;++variable){
    std::string item=*variable;const auto equal=item.find('=');
    if(equal!=std::string::npos)env[item.substr(0,equal)]=item.substr(equal+1);
  }
  if(options.env)for(const auto& [key,value]:*options.env)
    env[value::encodeUtf8(key)]=value::encodeUtf8(value);
  std::vector<std::string> result;result.reserve(env.size());
  for(const auto& [key,value]:env)result.push_back(key+"="+value);
  return result;
}
void drain(int& fd,std::string& text,const std::optional<std::size_t>& maxBuffer,bool& tooMuch){
  if(fd<0)return;
  char bytes[8192];
  for(;;){
    const auto count=::read(fd,bytes,sizeof(bytes));
    if(count>0){
      const auto amount=static_cast<std::size_t>(count);
      if(maxBuffer&&text.size()+amount>*maxBuffer){
        text.append(bytes,std::min(amount,*maxBuffer>text.size()?*maxBuffer-text.size():0));
        tooMuch=true;
      }else text.append(bytes,amount);
      continue;
    }
    if(count==0){closeFd(fd);return;}
    if(errno==EINTR)continue;
    if(errno==EAGAIN||errno==EWOULDBLOCK)return;
    closeFd(fd);throw std::runtime_error("Process output read failed");
  }
}
ExecaResult run(ExecaRequest request){
  if(request.file.empty())throw std::invalid_argument("Process executable required");
  auto requireNoNul=[](const std::string& value){
    if(value.find('\0')!=std::string::npos)
      throw std::invalid_argument("Process argv, cwd and environment cannot contain NUL");
  };
  requireNoNul(request.file);
  for(const auto& arg:request.args)requireNoNul(arg);
  if(request.options.cwd)requireNoNul(*request.options.cwd);
  if(request.options.env)for(const auto& [key,value]:*request.options.env){
    requireNoNul(key);requireNoNul(value);
    if(key.find('=')!=std::string::npos)
      throw std::invalid_argument("Process environment key cannot contain equals");
  }
  if(request.options.shell&&value::truthy(*request.options.shell))
    throw std::invalid_argument("Native argv process adapter does not implement shell option");
  int out[2]{-1,-1},err[2]{-1,-1},in[2]{-1,-1},nullFd=-1;
  pid_t child=-1;
  try{
    const auto executable=value::encodeUtf8(request.file);
    const auto cwdBytes=request.options.cwd?
      std::optional(value::encodeUtf8(*request.options.cwd)):std::nullopt;
    const auto inputBytes=request.options.input?
      std::optional(value::encodeUtf8(*request.options.input)):std::nullopt;
    pipeCloexec(out);pipeCloexec(err);
    const bool inputPipe=request.options.input.has_value()||request.options.stdin=="pipe";
    if(inputPipe)pipeCloexec(in);
    else if(request.options.stdin!="inherit"){
      nullFd=::open("/dev/null",O_RDONLY|O_CLOEXEC);
      if(nullFd<0)throw std::runtime_error("Cannot open process null input");
    }
    nonblocking(out[0]);nonblocking(err[0]);if(in[1]>=0)nonblocking(in[1]);
    posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions,out[1],STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions,err[1],STDERR_FILENO);
    if(in[0]>=0)posix_spawn_file_actions_adddup2(&actions,in[0],STDIN_FILENO);
    else if(nullFd>=0)posix_spawn_file_actions_adddup2(&actions,nullFd,STDIN_FILENO);
    for(int fd:{out[0],out[1],err[0],err[1],in[0],in[1],nullFd})
      if(fd>=0)posix_spawn_file_actions_addclose(&actions,fd);
    if(cwdBytes&&!cwdBytes->empty())
      posix_spawn_file_actions_addchdir_np(&actions,cwdBytes->c_str());
    posix_spawnattr_t attributes;posix_spawnattr_init(&attributes);
    posix_spawnattr_setflags(&attributes,POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attributes,0);
    std::vector<std::string> arguments;arguments.push_back(executable);
    for(const auto& arg:request.args)arguments.push_back(value::encodeUtf8(arg));
    std::vector<char*> argv;for(auto& arg:arguments)argv.push_back(arg.data());argv.push_back(nullptr);
    auto environment=childEnv(request.options);
    std::vector<char*> envp;for(auto& entry:environment)envp.push_back(entry.data());envp.push_back(nullptr);
    int spawnError=0;
    const auto pathOverride=request.options.env?
      request.options.env->find("PATH"):std::map<std::string,std::string>::const_iterator{};
    if(executable.find('/')==std::string::npos&&request.options.env&&
      pathOverride!=request.options.env->end()){
      const auto search=value::encodeUtf8(pathOverride->second);
      bool denied=false;spawnError=ENOENT;
      std::size_t start=0;
      for(;;){
        const auto end=search.find(':',start);
        const auto directory=search.substr(start,end==std::string::npos?
          std::string::npos:end-start);
        const auto candidate=(directory.empty()?std::string("."):directory)+"/"+executable;
        const int attempt=::posix_spawn(&child,candidate.c_str(),&actions,
          &attributes,argv.data(),envp.data());
        if(attempt==0){spawnError=0;break;}
        if(attempt==EACCES)denied=true;
        else if(attempt!=ENOENT&&attempt!=ENOTDIR){spawnError=attempt;break;}
        if(end==std::string::npos){spawnError=denied?EACCES:ENOENT;break;}
        start=end+1;
      }
    }else spawnError=::posix_spawnp(&child,executable.c_str(),&actions,
      &attributes,argv.data(),envp.data());
    posix_spawnattr_destroy(&attributes);posix_spawn_file_actions_destroy(&actions);
    closeFd(out[1]);closeFd(err[1]);closeFd(in[0]);closeFd(nullFd);
    if(spawnError){
      closeFd(out[0]);closeFd(err[0]);closeFd(in[1]);
      return {true,{},"","",std::string("Command failed to start: ")+std::strerror(spawnError),{}};
    }
    if(in[1]>=0&&(!inputBytes||inputBytes->empty()))closeFd(in[1]);
    ExecaResult result;
    const bool hasDeadline=request.options.timeout&&std::isfinite(*request.options.timeout)&&
      *request.options.timeout>0;
    const auto started=std::chrono::steady_clock::now();
    const auto deadline=started+std::chrono::milliseconds(hasDeadline?
      static_cast<std::int64_t>(*request.options.timeout):0);
    std::size_t written=0;bool reaped=false,killed=false,timedOut=false,aborted=false,tooMuch=false;
    int status=0;
    std::optional<std::size_t> maxBuffer;
    if(request.options.maxBuffer&&std::isfinite(*request.options.maxBuffer)&&
      *request.options.maxBuffer>=0)maxBuffer=static_cast<std::size_t>(*request.options.maxBuffer);
    // This worker exits after the process completes. Keep SIGPIPE blocked for
    // its lifetime so a child closing stdin cannot terminate the host.
    sigset_t blocked;sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);
    pthread_sigmask(SIG_BLOCK,&blocked,nullptr);
    while(out[0]>=0||err[0]>=0||in[1]>=0||!reaped){
      const auto now=std::chrono::steady_clock::now();
      if(!killed&&(request.options.abortSignal.stop_requested()||
        (hasDeadline&&now>=deadline)||tooMuch)){
        aborted=request.options.abortSignal.stop_requested();
        timedOut=!aborted&&hasDeadline&&now>=deadline;
        ::kill(-child,SIGKILL);killed=true;closeFd(in[1]);
      }
      pollfd fds[3];nfds_t count=0;
      if(out[0]>=0)fds[count++]={out[0],POLLIN|POLLHUP,0};
      if(err[0]>=0)fds[count++]={err[0],POLLIN|POLLHUP,0};
      if(in[1]>=0)fds[count++]={in[1],POLLOUT|POLLHUP,0};
      const int waited=::poll(fds,count,25);
      if(waited<0&&errno!=EINTR)throw std::runtime_error("Process pipe poll failed");
      drain(out[0],result.stdoutText,maxBuffer,tooMuch);
      drain(err[0],result.stderrText,maxBuffer,tooMuch);
      if(in[1]>=0){
        const auto& input=*inputBytes;
        const auto remaining=input.size()-written;
        if(remaining){
          const auto amount=::write(in[1],input.data()+written,std::min<std::size_t>(remaining,8192));
          if(amount>0)written+=static_cast<std::size_t>(amount);
          else if(amount<0&&errno!=EAGAIN&&errno!=EINTR)closeFd(in[1]);
        }
        if(written==input.size())closeFd(in[1]);
      }
      if(!reaped){
        const auto done=::waitpid(child,&status,WNOHANG);
        if(done==child||(done<0&&errno==ECHILD))reaped=true;
        else if(done<0&&errno!=EINTR)throw std::runtime_error("Process wait failed");
      }
    }
    if(!reaped)while(::waitpid(child,&status,0)<0&&errno==EINTR){}
    result.stdoutText=value::decodeUtf8(result.stdoutText);
    result.stderrText=value::decodeUtf8(result.stderrText);
    stripFinalNewline(result.stdoutText);stripFinalNewline(result.stderrText);
    result.exitCode=WIFEXITED(status)?std::optional<int>(WEXITSTATUS(status)):std::nullopt;
    if(timedOut)result.shortMessage="Command timed out";
    else if(aborted)result.shortMessage="Command was aborted";
    else if(tooMuch)result.shortMessage="Command output exceeded maxBuffer";
    else if(WIFSIGNALED(status))result.signal=signalName(WTERMSIG(status));
    else if(result.exitCode&&*result.exitCode!=0)
      result.shortMessage="Command failed with exit code "+std::to_string(*result.exitCode);
    result.failed=timedOut||aborted||tooMuch||!result.exitCode||*result.exitCode!=0;
    return result;
  }catch(...){
    if(child>0){::kill(-child,SIGKILL);while(::waitpid(child,nullptr,0)<0&&errno==EINTR){}}
    closeFd(out[0]);closeFd(out[1]);closeFd(err[0]);closeFd(err[1]);
    closeFd(in[0]);closeFd(in[1]);closeFd(nullFd);
    throw;
  }
}
}
std::function<event_loop::Deferred<ExecaResult>(ExecaRequest)> indigenousExeca(
  event_loop::Loop& loop){
  return [&loop](ExecaRequest request){
    if(!loop.isOwnerThread())throw std::logic_error("native execa owner required");
    event_loop::Deferred<ExecaResult> output(loop);
    auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
    const auto dispatcher=loop.dispatcher();
    try{
      ::batbox::runtime::contextThread([output,request=std::move(request),keep,dispatcher]()mutable{
        std::optional<ExecaResult> result;std::exception_ptr error;
        try{result=run(std::move(request));}catch(...){error=std::current_exception();}
        try{dispatcher.postMission([output,result=std::move(result),error,keep]{
          if(error)output.reject(error);else output.resolve(*result);
          keep->release();
        });}catch(...){keep->release();}
      }).detach();
    }catch(...){keep->release();output.reject(std::current_exception());}
    return output;
  };
}
} // namespace batbox::process
