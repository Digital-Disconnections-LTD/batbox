#include "runtime/AsyncLaunch.hpp"
#include "instructions/hooks/runtime/IndigenousManagedHookProcess.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include <cerrno>
#include <atomic>
#include <chrono>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <mutex>
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
void checkNul(const std::string& value){
  if(value.find('\0')!=std::string::npos)
    throw std::invalid_argument("Hook process values cannot contain NUL");
}
void makeNonblocking(int fd){
  const int flags=::fcntl(fd,F_GETFL);
  if(flags<0||::fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0)
    throw std::runtime_error(std::string("Hook pipe setup failed: ")+std::strerror(errno));
}
std::size_t completeUtf8Bytes(const std::string& raw){
  const auto size=raw.size();if(size==0)return 0;
  std::size_t start=size;
  while(start>0&&size-start<4&&
    (static_cast<unsigned char>(raw[start-1])&0xc0)==0x80)--start;
  if(start==0)return size;
  const auto lead=static_cast<unsigned char>(raw[start-1]);
  const std::size_t needed=lead>=0xf0&&lead<=0xf4?4:
    lead>=0xe0&&lead<=0xef?3:lead>=0xc2&&lead<=0xdf?2:1;
  return size-(start-1)<needed?start-1:size;
}
}
struct IndigenousManagedHookProcess::State {
  explicit State(event_loop::Loop& owner):loop(owner),dispatcher(owner.dispatcher()),
    result(owner),closed(owner),keep(owner.hold()){}
  event_loop::Loop& loop;
  event_loop::Loop::Dispatcher dispatcher;
  event_loop::Deferred<int> result;
  event_loop::Deferred<int> closed;
  event_loop::Loop::KeepAlive keep;
  pid_t pid=-1;
  std::string status="running",stdoutRaw,stderrRaw,stdoutDelivered,stderrDelivered;
  std::optional<std::string> stdinFailure;
  std::function<void(const std::string&)> stdoutListener,stderrListener;
  bool cleaned=false;
  std::atomic_bool killedByOwner=false;
  std::mutex inputMutex;
  std::deque<std::string> inputQueue;
  std::size_t inputOffset=0;
  bool closeAfterInput=true,destroyInput=false,stdinClosed=false;
  void deliver(std::string bytes,bool stdoutPipe){
    if(cleaned)return;
    auto& raw=stdoutPipe?stdoutRaw:stderrRaw;
    auto& delivered=stdoutPipe?stdoutDelivered:stderrDelivered;
    auto& listener=stdoutPipe?stdoutListener:stderrListener;
    raw+=bytes;
    const auto complete=completeUtf8Bytes(raw);
    const auto decoded=value::decodeUtf8(raw.substr(0,complete));
    const auto delta=decoded.substr(delivered.size());
    delivered=decoded;
    if(listener&&!delta.empty())listener(delta);
  }
  void finish(int resultCode,int closeCode,bool killed,
    std::optional<std::string> inputError){
    auto flush=[this](bool stdoutPipe){
      if(cleaned)return;
      auto& raw=stdoutPipe?stdoutRaw:stderrRaw;
      auto& delivered=stdoutPipe?stdoutDelivered:stderrDelivered;
      auto& listener=stdoutPipe?stdoutListener:stderrListener;
      const auto decoded=value::decodeUtf8(raw);
      const auto delta=decoded.substr(delivered.size());delivered=decoded;
      if(listener&&!delta.empty())listener(delta);
    };
    flush(true);flush(false);
    stdinFailure=std::move(inputError);
    status=killed||status=="killed"?"killed":"completed";
    result.resolve(resultCode);closed.resolve(closeCode);
    keep.release();
  }
  void fail(std::exception_ptr error){
    status="killed";result.reject(error);closed.reject(error);keep.release();
  }
};
long IndigenousManagedHookProcess::pid()const{return state_->pid;}
std::string IndigenousManagedHookProcess::status()const{return state_->status;}
bool IndigenousManagedHookProcess::background(const std::string&)const{
  if(state_->status!="running")return false;
  state_->status="backgrounded";return true;
}
void IndigenousManagedHookProcess::kill()const{
  if(state_->status=="completed"||state_->status=="killed")return;
  state_->killedByOwner.store(true,std::memory_order_release);
  ::kill(-state_->pid,SIGKILL);state_->status="killed";
}
void IndigenousManagedHookProcess::cleanup()const{
  if(!state_->dispatcher.isOwnerThread())throw std::logic_error("Hook cleanup owner required");
  state_->cleaned=true;
  state_->stdoutRaw.clear();state_->stderrRaw.clear();
  state_->stdoutDelivered.clear();state_->stderrDelivered.clear();
  state_->stdoutListener={};state_->stderrListener={};
}
void IndigenousManagedHookProcess::writeStdin(const std::string& text)const{
  if(!state_->dispatcher.isOwnerThread())throw std::logic_error("Hook stdin owner required");
  std::lock_guard lock(state_->inputMutex);
  if(state_->stdinClosed||state_->destroyInput||state_->closeAfterInput)
    throw std::runtime_error("Hook stdin is closed");
  state_->inputQueue.push_back(value::encodeUtf8(text));
}
void IndigenousManagedHookProcess::destroyStdin()const{
  if(!state_->dispatcher.isOwnerThread())throw std::logic_error("Hook stdin owner required");
  std::lock_guard lock(state_->inputMutex);
  state_->destroyInput=true;state_->inputQueue.clear();
}
event_loop::Deferred<std::string> IndigenousManagedHookProcess::getStdout()const{
  event_loop::Deferred<std::string> value(state_->loop);
  value.resolve(state_->stdoutDelivered);return value;
}
std::string IndigenousManagedHookProcess::getStderr()const{return state_->stderrDelivered;}
event_loop::Deferred<int> IndigenousManagedHookProcess::resultCode()const{return state_->result;}
event_loop::Deferred<int> IndigenousManagedHookProcess::closeCode()const{return state_->closed;}
std::optional<std::string> IndigenousManagedHookProcess::stdinError()const{
  return state_->stdinFailure;}
void IndigenousManagedHookProcess::onStdout(
  std::function<void(const std::string&)> callback)const{
  state_->stdoutListener=std::move(callback);
}
void IndigenousManagedHookProcess::onStderr(
  std::function<void(const std::string&)> callback)const{
  state_->stderrListener=std::move(callback);
}
std::shared_ptr<AsyncHookProcess> IndigenousManagedHookProcess::rosterProcess()const{
  auto process=std::make_shared<AsyncHookProcess>();auto self=*this;
  process->getStdout=[self]{return self.getStdout();};
  process->getStderr=[self]{return self.getStderr();};
  process->resultCode=[self]{return self.resultCode();};
  process->status=[self]{return self.status();};
  process->cleanup=[self]{self.cleanup();};
  process->kill=[self]{self.kill();};
  return process;
}
namespace {
void runIndigenousHookPipeWorker(std::shared_ptr<IndigenousManagedHookProcess::State> state,
  IndigenousProcessRequest request,int inputFd,int outputFd,int errorFd,
  std::function<std::string()> currentAbortReason){
  auto dispatcher=state->dispatcher;
  ::batbox::runtime::contextThread([state=std::move(state),dispatcher,request=std::move(request),
    inputFd,outputFd,errorFd,currentAbortReason=std::move(currentAbortReason)]()mutable{
    auto started=std::chrono::steady_clock::now();
    bool reaped=false,killed=false,timedOut=false;
    int waitStatus=0;std::optional<std::string> inputError;
    sigset_t blocked;sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);
    ::pthread_sigmask(SIG_BLOCK,&blocked,nullptr);
    try{
      while(inputFd>=0||outputFd>=0||errorFd>=0||!reaped){
        const auto elapsed=std::chrono::duration<double,std::milli>(
          std::chrono::steady_clock::now()-started).count();
        const bool abortRequested=request.signal.stop_requested()&&
          (!currentAbortReason||currentAbortReason()!="interrupt");
        const bool timeoutReached=request.timeoutMs>0&&elapsed>=request.timeoutMs;
        if(!killed&&(abortRequested||timeoutReached)){
          ::kill(-state->pid,SIGKILL);killed=true;closeFd(inputFd);
          timedOut=timeoutReached&&!abortRequested;
        }
        bool hasInput=false,destroyInput=false,closeInput=false;
        {
          std::lock_guard lock(state->inputMutex);
          hasInput=!state->inputQueue.empty();
          destroyInput=state->destroyInput;
          closeInput=state->closeAfterInput&&!hasInput;
        }
        if(inputFd>=0&&(destroyInput||closeInput||reaped)){
          closeFd(inputFd);
          std::lock_guard lock(state->inputMutex);state->stdinClosed=true;
        }
        pollfd fds[3];nfds_t count=0;
        if(outputFd>=0)fds[count++]={outputFd,POLLIN|POLLHUP,0};
        if(errorFd>=0)fds[count++]={errorFd,POLLIN|POLLHUP,0};
        if(inputFd>=0&&hasInput)fds[count++]={inputFd,POLLOUT|POLLHUP,0};
        if(::poll(fds,count,20)<0&&errno!=EINTR)
          throw std::runtime_error(std::string("Hook process poll failed: ")+
            std::strerror(errno));
        auto read=[&](int& fd,bool stdoutPipe){
          if(fd<0)return;
          char bytes[8192];
          for(;;){
            const auto n=::read(fd,bytes,sizeof(bytes));
            if(n>0){
              dispatcher.postMission([state,chunk=std::string(bytes,n),stdoutPipe]{
                state->deliver(chunk,stdoutPipe);});
              continue;
            }
            if(n==0){closeFd(fd);return;}
            if(errno==EINTR)continue;
            if(errno==EAGAIN||errno==EWOULDBLOCK)return;
            throw std::runtime_error(std::string("Hook output read failed: ")+
              std::strerror(errno));
          }
        };
        read(outputFd,true);read(errorFd,false);
        if(inputFd>=0){
          std::lock_guard lock(state->inputMutex);
          if(!state->inputQueue.empty()){
            auto& text=state->inputQueue.front();
            const auto amount=::write(inputFd,text.data()+state->inputOffset,
              std::min<std::size_t>(8192,text.size()-state->inputOffset));
            if(amount>0){
              state->inputOffset+=static_cast<std::size_t>(amount);
              if(state->inputOffset==text.size()){
                state->inputQueue.pop_front();state->inputOffset=0;
              }
            }else if(amount<0&&errno!=EAGAIN&&errno!=EINTR){
              inputError=errno==EPIPE?"EPIPE":std::strerror(errno);
              closeFd(inputFd);state->stdinClosed=true;
            }
          }
        }
        if(!reaped){
          const auto done=::waitpid(state->pid,&waitStatus,WNOHANG);
          if(done==state->pid||(done<0&&errno==ECHILD))reaped=true;
          else if(done<0&&errno!=EINTR)
            throw std::runtime_error(std::string("Hook process wait failed: ")+
              std::strerror(errno));
        }
      }
      const int closeCode=WIFEXITED(waitStatus)?WEXITSTATUS(waitStatus):1;
      const bool ownerKilled=state->killedByOwner.load(std::memory_order_acquire);
      const int resultCode=timedOut?143:killed||ownerKilled?137:closeCode;
      dispatcher.postMission([state,resultCode,closeCode,killed,ownerKilled,
        inputError=std::move(inputError)]()mutable{
        state->finish(resultCode,closeCode,killed||ownerKilled,
          std::move(inputError));});
    }catch(...){
      auto error=std::current_exception();
      ::kill(-state->pid,SIGKILL);
      while(::waitpid(state->pid,nullptr,0)<0&&errno==EINTR){}
      try{dispatcher.postMission([state,error]{state->fail(error);});}catch(...){}
    }
    closeFd(inputFd);closeFd(outputFd);closeFd(errorFd);
    {std::lock_guard lock(state->inputMutex);state->stdinClosed=true;}
  }).detach();
}
}
IndigenousManagedHookProcess startIndigenousManagedHookProcess(
  event_loop::Loop& loop,IndigenousProcessRequest request,bool keepStdinOpen,
  std::function<std::string()> currentAbortReason){
  if(!loop.isOwnerThread())throw std::logic_error("Hook process owner required");
  checkNul(request.shellPath);checkNul(request.command);
  checkNul(request.cwd);checkNul(request.input);
  int input[2]{-1,-1},output[2]{-1,-1},errors[2]{-1,-1};
  pid_t child=-1;
  try{
    if(!makePipe(input)||!makePipe(output)||!makePipe(errors))
      throw std::runtime_error("Cannot create hook process pipes");
    makeNonblocking(input[1]);makeNonblocking(output[0]);makeNonblocking(errors[0]);
    posix_spawn_file_actions_t actions;
    if(::posix_spawn_file_actions_init(&actions))
      throw std::runtime_error("Hook spawn actions failed");
    ::posix_spawn_file_actions_adddup2(&actions,input[0],STDIN_FILENO);
    ::posix_spawn_file_actions_adddup2(&actions,output[1],STDOUT_FILENO);
    ::posix_spawn_file_actions_adddup2(&actions,errors[1],STDERR_FILENO);
    for(int fd:{input[0],input[1],output[0],output[1],errors[0],errors[1]})
      ::posix_spawn_file_actions_addclose(&actions,fd);
    const auto cwd=value::encodeUtf8(request.cwd);
    if(!cwd.empty())::posix_spawn_file_actions_addchdir_np(&actions,cwd.c_str());
    posix_spawnattr_t attrs;::posix_spawnattr_init(&attrs);
    ::posix_spawnattr_setflags(&attrs,POSIX_SPAWN_SETPGROUP);
    ::posix_spawnattr_setpgroup(&attrs,0);
    std::vector<std::string> environment;
    for(const auto& [name,value]:request.environment){
      if(name.find('=')!=std::string::npos)throw std::invalid_argument("Invalid hook environment entry");
      checkNul(name);checkNul(value);
      environment.push_back(value::encodeUtf8(name)+"="+value::encodeUtf8(value));
    }
    std::vector<char*> envp;
    for(auto& entry:environment)envp.push_back(entry.data());
    envp.push_back(nullptr);
    auto shell=value::encodeUtf8(request.shellPath);
    auto command=value::encodeUtf8(request.command);
    char option[]="-c";char* argv[]{shell.data(),option,command.data(),nullptr};
    const int spawnError=::posix_spawn(&child,shell.c_str(),&actions,&attrs,
      argv,envp.data());
    ::posix_spawnattr_destroy(&attrs);::posix_spawn_file_actions_destroy(&actions);
    if(spawnError)throw std::runtime_error(std::string("Hook spawn failed: ")+
      std::strerror(spawnError));
    closeFd(input[0]);closeFd(output[1]);closeFd(errors[1]);
    auto state=std::make_shared<IndigenousManagedHookProcess::State>(loop);
    state->pid=child;
    state->closeAfterInput=!keepStdinOpen;
    state->inputQueue.push_back(value::encodeUtf8(request.input)+"\n");
    runIndigenousHookPipeWorker(state,std::move(request),input[1],output[0],errors[0],
      std::move(currentAbortReason));
    return IndigenousManagedHookProcess(state);
  }catch(...){
    if(child>0){::kill(-child,SIGKILL);while(::waitpid(child,nullptr,0)<0&&errno==EINTR){}}
    closeFd(input[0]);closeFd(input[1]);closeFd(output[0]);closeFd(output[1]);
    closeFd(errors[0]);closeFd(errors[1]);throw;
  }
}
} // namespace batbox::instructions::hooks::runtime
