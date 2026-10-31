#include "cleanup/IndigenousSignalBridge.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <csignal>
#include <map>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace batbox::cleanup {
namespace {
std::atomic_bool bridgeActive=false;
std::mutex handlerMutex;
std::function<void(int)> activeHandler;
BOOL WINAPI consoleHandler(DWORD event){
  int number=0;
  if(event==CTRL_C_EVENT)number=SIGINT;
  else if(event==CTRL_BREAK_EVENT||event==CTRL_CLOSE_EVENT||
    event==CTRL_LOGOFF_EVENT||event==CTRL_SHUTDOWN_EVENT)number=SIGTERM;
  if(!number)return FALSE;
  std::function<void(int)> handler;
  {std::lock_guard lock(handlerMutex);handler=activeHandler;}
  if(!handler)return FALSE;
  try{handler(number);}catch(...){}
  return TRUE;
}
}
struct IndigenousSignalBridge::State {
  event_loop::Loop::Dispatcher dispatcher;
  std::map<int,std::vector<std::function<void()>>> callbacks;
  explicit State(event_loop::Loop& loop):dispatcher(loop.dispatcher()){}
};
IndigenousSignalBridge::IndigenousSignalBridge(event_loop::Loop& loop):
  state_(std::make_shared<State>(loop)){
  if(!loop.isOwnerThread())throw std::logic_error("signal bridge requires loop owner");
  if(bridgeActive.exchange(true))throw std::logic_error("one native signal bridge allowed");
  const auto weak=std::weak_ptr<State>(state_);
  {std::lock_guard lock(handlerMutex);
    activeHandler=[weak](int number){
      const auto state=weak.lock();if(!state)return;
      try{state->dispatcher.postMission([weak,number]{
        const auto state=weak.lock();if(!state)return;
        const auto found=state->callbacks.find(number);
        if(found==state->callbacks.end())return;
        for(const auto& callback:found->second)callback();
      });}catch(...){}
    };
  }
  if(!::SetConsoleCtrlHandler(consoleHandler,TRUE)){
    std::lock_guard lock(handlerMutex);activeHandler={};bridgeActive=false;
    throw std::runtime_error("console control handler installation failed");
  }
}
IndigenousSignalBridge::~IndigenousSignalBridge(){
  if(!state_)return;
  {std::lock_guard lock(handlerMutex);activeHandler={};}
  (void)::SetConsoleCtrlHandler(consoleHandler,FALSE);
  bridgeActive=false;
}
void IndigenousSignalBridge::onSignal(int number,std::function<void()> callback){
  if(!state_->dispatcher.isOwnerThread())throw std::logic_error("signal registration requires owner");
  if(!callback)throw std::invalid_argument("signal callback required");
  if(number!=SIGINT&&number!=SIGTERM)
    throw std::invalid_argument("unsupported Windows console signal");
  state_->callbacks[number].push_back(std::move(callback));
}
} // namespace batbox::cleanup
#else
#include <atomic>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <map>
#include <poll.h>
#include <stdexcept>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace batbox::cleanup {
namespace {
volatile sig_atomic_t signalWriteFd=-1;
std::atomic_bool bridgeActive=false;
extern "C" void signalHandler(int number){
  const int saved=errno;
  const int fd=signalWriteFd;
  if(fd>=0){const unsigned char byte=static_cast<unsigned char>(number);
    (void)::write(fd,&byte,1);}
  errno=saved;
}
}
struct IndigenousSignalBridge::State {
  event_loop::Loop::Dispatcher dispatcher;
  int readFd=-1,writeFd=-1;
  std::map<int,std::vector<std::function<void()>>> callbacks;
  std::map<int,struct sigaction> previous;
  std::jthread reader;
  explicit State(event_loop::Loop& loop):dispatcher(loop.dispatcher()){}
};
IndigenousSignalBridge::IndigenousSignalBridge(event_loop::Loop& loop):
  state_(std::make_shared<State>(loop)){
  if(!loop.isOwnerThread())throw std::logic_error("signal bridge requires loop owner");
  if(bridgeActive.exchange(true))throw std::logic_error("one native signal bridge allowed");
  int pipeFds[2]{-1,-1};
#if defined(__APPLE__)
  const bool pipeReady=::pipe(pipeFds)==0;
  bool flagsReady=pipeReady;
  if(pipeReady)for(int fd:pipeFds){
    const auto flags=::fcntl(fd,F_GETFL);
    if(flags<0||::fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0||
       ::fcntl(fd,F_SETFD,FD_CLOEXEC)<0)flagsReady=false;
  }
  if(!flagsReady){
    if(pipeFds[0]>=0)::close(pipeFds[0]);
    if(pipeFds[1]>=0)::close(pipeFds[1]);
#else
  if(::pipe2(pipeFds,O_CLOEXEC|O_NONBLOCK)!=0){
#endif
    bridgeActive=false;
    throw std::runtime_error("signal self-pipe creation failed");}
  state_->readFd=pipeFds[0];state_->writeFd=pipeFds[1];
  signalWriteFd=state_->writeFd;
  const auto weak=std::weak_ptr<State>(state_);
  state_->reader=std::jthread([weak,fd=state_->readFd](std::stop_token stopped){
    while(!stopped.stop_requested()){
      pollfd item{fd,POLLIN,0};
      const int ready=::poll(&item,1,100);
      if(ready<0){if(errno==EINTR)continue;break;}
      if(ready==0)continue;
      unsigned char bytes[64];
      const auto count=::read(fd,bytes,sizeof(bytes));
      if(count<0){if(errno==EINTR||errno==EAGAIN)continue;break;}
      if(count==0)break;
      for(ssize_t i=0;i<count;++i){
        if(auto state=weak.lock()){
          try{state->dispatcher.postMission([weak,number=static_cast<int>(bytes[i])]{
            const auto state=weak.lock();if(!state)return;
            const auto found=state->callbacks.find(number);
            if(found==state->callbacks.end())return;
            for(const auto& callback:found->second)callback();
          });}catch(...){return;}
        }
      }
    }
  });
}
IndigenousSignalBridge::~IndigenousSignalBridge(){
  if(!state_)return;
  for(const auto& [number,previous]:state_->previous)
    ::sigaction(number,&previous,nullptr);
  signalWriteFd=-1;
  state_->reader.request_stop();
  if(state_->reader.joinable())state_->reader.join();
  if(state_->readFd>=0)::close(state_->readFd);
  if(state_->writeFd>=0)::close(state_->writeFd);
  bridgeActive=false;
}
void IndigenousSignalBridge::onSignal(int number,std::function<void()> callback){
  if(!state_->dispatcher.isOwnerThread())throw std::logic_error("signal registration requires owner");
  if(!callback)throw std::invalid_argument("signal callback required");
  if(number<=0||number>=256)throw std::invalid_argument("signal number out of pipe range");
  if(!state_->previous.contains(number)){
    struct sigaction action{},prior{};
    action.sa_handler=signalHandler;sigemptyset(&action.sa_mask);action.sa_flags=SA_RESTART;
    if(::sigaction(number,&action,&prior)!=0)throw std::runtime_error("sigaction failed");
    state_->previous[number]=prior;
  }
  state_->callbacks[number].push_back(std::move(callback));
}
} // namespace batbox::cleanup
#endif
