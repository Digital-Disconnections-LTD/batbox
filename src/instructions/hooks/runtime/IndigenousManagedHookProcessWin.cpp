#include "instructions/hooks/runtime/IndigenousManagedHookProcess.hpp"
#include "process/IndigenousJsonlProcess.hpp"
#include "runtime/AsyncLaunch.hpp"
#include "runtime/Encoding.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace batbox::instructions::hooks::runtime {
namespace {
void checkNul(const std::string& value){
  if(value.find('\0')!=std::string::npos)
    throw std::invalid_argument("Hook process values cannot contain NUL");
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
  long pid=0;
  std::shared_ptr<process::IndigenousJsonlProcess> child;
  std::string status="running",stdoutRaw,stderrRaw,stdoutDelivered,stderrDelivered;
  std::optional<std::string> stdinFailure;
  std::function<void(const std::string&)> stdoutListener,stderrListener;
  bool cleaned=false;
  std::atomic_bool killedByOwner=false;
  std::mutex inputMutex;
  std::deque<std::string> inputQueue;
  bool closeAfterInput=true,destroyInput=false,stdinClosed=false;
  void deliver(std::string bytes,bool stdoutPipe){
    if(cleaned)return;
    auto& raw=stdoutPipe?stdoutRaw:stderrRaw;
    auto& delivered=stdoutPipe?stdoutDelivered:stderrDelivered;
    auto& listener=stdoutPipe?stdoutListener:stderrListener;
    raw+=bytes;
    const auto complete=completeUtf8Bytes(raw);
    const auto decoded=value::decodeUtf8(raw.substr(0,complete));
    const auto delta=decoded.substr(delivered.size());delivered=decoded;
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
    child.reset();
    result.resolve(resultCode);closed.resolve(closeCode);keep.release();
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
  if(state_->child)state_->child->kill();
  state_->status="killed";
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
std::optional<std::string> IndigenousManagedHookProcess::stdinError()const{return state_->stdinFailure;}
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
IndigenousManagedHookProcess startIndigenousManagedHookProcess(
    event_loop::Loop& loop,IndigenousProcessRequest request,bool keepStdinOpen,
    std::function<std::string()> currentAbortReason){
  if(!loop.isOwnerThread())throw std::logic_error("Hook process owner required");
  checkNul(request.shellPath);checkNul(request.command);
  checkNul(request.cwd);checkNul(request.input);
  for(const auto& [name,value]:request.environment){
    if(name.find('=')!=std::string::npos)throw std::invalid_argument("Invalid hook environment entry");
    checkNul(name);checkNul(value);
  }
  auto state=std::make_shared<IndigenousManagedHookProcess::State>(loop);
  state->closeAfterInput=!keepStdinOpen;
  state->inputQueue.push_back(value::encodeUtf8(request.input)+"\n");
  process::IndigenousChildOptions options;
  options.lineMode=false;options.inheritEnvironment=false;
  if(!request.cwd.empty())options.cwd=value::encodeUtf8(request.cwd);
  const auto dispatcher=state->dispatcher;
  std::weak_ptr<IndigenousManagedHookProcess::State> weak=state;
  options.onData=[weak,dispatcher](const std::string& bytes,bool stdoutPipe){
    auto state=weak.lock();if(!state)return;
    try{dispatcher.postMission([state,bytes,stdoutPipe]{state->deliver(bytes,stdoutPipe);});}
    catch(...){}
  };
  std::vector<std::string> argv{value::encodeUtf8(request.shellPath)};
  if(request.shellType=="powershell")
    argv.insert(argv.end(),{"-NoProfile","-NonInteractive","-Command"});
  else argv.push_back("-c");
  argv.push_back(value::encodeUtf8(request.command));
  state->child=std::make_shared<process::IndigenousJsonlProcess>(
    argv,request.environment,std::move(options));
  state->pid=state->child->pid();
  ::batbox::runtime::contextThread([state,request=std::move(request),
      currentAbortReason=std::move(currentAbortReason)]()mutable{
    bool killed=false,timedOut=false,stdinClosed=false;
    std::optional<std::string> inputError;int closeCode=1;
    const auto started=std::chrono::steady_clock::now();
    try{
      for(;;){
        const auto elapsed=std::chrono::duration<double,std::milli>(
          std::chrono::steady_clock::now()-started).count();
        const bool abortRequested=request.signal.stop_requested()&&
          (!currentAbortReason||currentAbortReason()!="interrupt");
        const bool timeoutReached=request.timeoutMs>0&&elapsed>=request.timeoutMs;
        if(!killed&&(abortRequested||timeoutReached)){
          state->child->kill();killed=true;
          timedOut=timeoutReached&&!abortRequested;
        }
        std::string input;
        bool shouldClose=false;
        {
          std::lock_guard lock(state->inputMutex);
          if(!state->inputQueue.empty()){
            input=std::move(state->inputQueue.front());state->inputQueue.pop_front();
          }
          shouldClose=state->destroyInput||
            (state->closeAfterInput&&state->inputQueue.empty()&&input.empty());
        }
        if(!input.empty()&&!killed){
          const auto writeDeadline=request.timeoutMs>0?
            started+std::chrono::milliseconds(static_cast<long long>(request.timeoutMs)):
            std::chrono::steady_clock::time_point::max();
          try{state->child->writeLine(input,writeDeadline);}
          catch(const std::exception& error){
            if(std::string(error.what())=="Child stdin write timed out"){
              killed=true;timedOut=true;break;
            }
            inputError=error.what();shouldClose=true;
          }
        }
        if(shouldClose&&!stdinClosed){
          state->child->closeStdin();stdinClosed=true;
          std::lock_guard lock(state->inputMutex);state->stdinClosed=true;
        }
        try{
          closeCode=state->child->waitForExit(std::chrono::steady_clock::now()+
            std::chrono::milliseconds(25));
          break;
        }catch(const std::exception& error){
          if(std::string(error.what())!="Child process wait timed out")throw;
        }
      }
      const bool ownerKilled=state->killedByOwner.load(std::memory_order_acquire);
      const int resultCode=timedOut?143:killed||ownerKilled?137:closeCode;
      state->child->close();
      state->dispatcher.postMission([state,resultCode,closeCode,killed,ownerKilled,
          inputError=std::move(inputError)]()mutable{
        state->finish(resultCode,closeCode,killed||ownerKilled,std::move(inputError));
      });
    }catch(...){
      const auto error=std::current_exception();state->child->kill();state->child->close();
      try{state->dispatcher.postMission([state,error]{state->fail(error);});}catch(...){}
    }
    {std::lock_guard lock(state->inputMutex);state->stdinClosed=true;}
  }).detach();
  return IndigenousManagedHookProcess(state);
}
} // namespace batbox::instructions::hooks::runtime
