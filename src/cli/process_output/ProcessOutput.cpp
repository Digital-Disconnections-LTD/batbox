#include "cli/process_output/ProcessOutput.hpp"
#include <cerrno>
#ifndef _WIN32
#include <csignal>
#include <fcntl.h>
#include <deque>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <algorithm>
#include <deque>
#endif
#include <stdexcept>
#include <system_error>

namespace batbox::cli::process_output {
namespace {
#ifndef _WIN32
class BlockSigpipe {
public:
  BlockSigpipe(){
    sigemptyset(&set_);sigaddset(&set_,SIGPIPE);
    const int error=pthread_sigmask(SIG_BLOCK,&set_,&previous_);
    if(error)throw std::system_error(error,std::generic_category(),
                                     "block SIGPIPE");
    sigset_t pending{};
    sigpending(&pending);
    alreadyPending_=sigismember(&pending,SIGPIPE)==1;
  }
  ~BlockSigpipe(){pthread_sigmask(SIG_SETMASK,&previous_,nullptr);}
  void consume(){
    if(alreadyPending_||sigismember(&previous_,SIGPIPE)==1)return;
#ifdef __APPLE__
    sigset_t pending{};
    if(sigpending(&pending)==0&&sigismember(&pending,SIGPIPE)==1){
      int signal=0;(void)sigwait(&set_,&signal);
    }
#else
    const timespec zero{0,0};
    while(sigtimedwait(&set_,nullptr,&zero)<0&&errno==EINTR){}
#endif
  }
private:
  sigset_t set_{},previous_{};
  bool alreadyPending_=false;
};
#endif
void writeAll(int fd,const std::string& data){
#ifdef _WIN32
  const auto raw=::_get_osfhandle(fd);
  if(raw==-1)throw std::system_error(errno,std::generic_category(),"stdout handle");
  const HANDLE output=reinterpret_cast<HANDLE>(raw);
  for(std::size_t offset=0;offset<data.size();){
    const auto wanted=static_cast<DWORD>(std::min<std::size_t>(
      data.size()-offset,0xffffffffu));
    DWORD count=0;
    if(!::WriteFile(output,data.data()+offset,wanted,&count,nullptr)){
      const auto code=::GetLastError();
      if(code==ERROR_BROKEN_PIPE||code==ERROR_NO_DATA)
        throw std::system_error(EPIPE,std::generic_category(),"write output");
      throw std::system_error(static_cast<int>(code),std::system_category(),"write output");
    }
    if(count==0)throw std::runtime_error("Output write made no progress");
    offset+=count;
  }
#else
#ifdef __APPLE__
  // Darwin may deliver a pipe signal to another thread. Suppress it on this
  // descriptor while the serialized output channel writes, then restore it.
  const int previousPipeSignal=::fcntl(fd,F_GETNOSIGPIPE);
  if(previousPipeSignal<0||::fcntl(fd,F_SETNOSIGPIPE,1)<0)
    throw std::system_error(errno,std::generic_category(),"suppress output SIGPIPE");
  struct RestorePipeSignal {
    int fd,previous;
    ~RestorePipeSignal(){(void)::fcntl(fd,F_SETNOSIGPIPE,previous);}
  } restorePipeSignal{fd,previousPipeSignal};
#else
  BlockSigpipe block;
#endif
  for(std::size_t offset=0;offset<data.size();){
    const auto count=::write(fd,data.data()+offset,data.size()-offset);
    if(count>0){offset+=static_cast<std::size_t>(count);continue;}
    if(count==0)throw std::runtime_error("Output write made no progress");
    if(errno==EINTR)continue;
    const int error=errno;
#ifndef __APPLE__
    if(error==EPIPE)block.consume();
#endif
    throw std::system_error(error,std::generic_category(),"write output");
  }
#endif
}
}
struct OutputChannel::State {
  State(event_loop::Loop& owner,
      std::shared_ptr<fs_operations::IndigenousAsyncFsHost> workers,int descriptor):
    loop(owner),host(std::move(workers)),fd(descriptor){}
  event_loop::Loop& loop;
  std::shared_ptr<fs_operations::IndigenousAsyncFsHost> host;
  int fd;
  std::deque<std::string> queue;
  bool writing=false,destroyed=false,handleEpipe=false;
};
namespace {
void pump(const std::shared_ptr<OutputChannel::State>& state){
  if(state->writing||state->destroyed||state->queue.empty())return;
  auto chunk=std::move(state->queue.front());state->queue.pop_front();
  state->writing=true;
  auto pending=state->host->submit<event_loop::Unit>(
    [fd=state->fd,chunk=std::move(chunk)]{
      writeAll(fd,chunk);return event_loop::Unit{};
    });
  pending.then([state](const event_loop::Unit*,const std::exception_ptr& error){
    state->writing=false;
    if(error){
      try{std::rethrow_exception(error);}
      catch(const std::system_error& failure){
        if(failure.code().value()==EPIPE&&state->handleEpipe){
          state->destroyed=true;state->queue.clear();return;
        }
        throw;
      }
    }
    pump(state);
  });
}
}
OutputChannel::OutputChannel(event_loop::Loop& loop,
    std::shared_ptr<fs_operations::IndigenousAsyncFsHost> host,int fd):
  state_(std::make_shared<State>(loop,std::move(host),fd)){
  if(!state_->host)throw std::invalid_argument("Process output worker required");
}
void OutputChannel::registerEpipeHandler(){
  if(!state_->loop.isOwnerThread())throw std::logic_error("Output owner required");
  state_->handleEpipe=true;
}
void OutputChannel::writeOut(const std::string& data){
  if(!state_->loop.isOwnerThread())throw std::logic_error("Output owner required");
  if(state_->destroyed)return;
  state_->queue.push_back(data);
  pump(state_);
}
bool OutputChannel::destroyed()const{return state_->destroyed;}
void ProcessOutput::registerProcessOutputErrorHandlers(){
  stdout_.registerEpipeHandler();
  stderr_.registerEpipeHandler();
}
void ProcessOutput::writeToStdout(const std::string& data){
  stdout_.writeOut(data);
}
void ProcessOutput::writeToStderr(const std::string& data){
  stderr_.writeOut(data);
}
}
