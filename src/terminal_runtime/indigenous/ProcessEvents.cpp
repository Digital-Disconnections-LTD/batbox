#include "terminal_runtime/indigenous/ProcessEvents.hpp"
#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace batbox::terminal_runtime::indigenous {
namespace {
std::vector<std::function<void()>>& exitHooks(){
  static std::vector<std::function<void()>> hooks;
  return hooks;
}
void runExitHooks(){
  const auto hooks=exitHooks();
  for(const auto& hook:hooks)hook();
}
void ensureExitHook(){
  static std::once_flag installed;
  std::call_once(installed,[]{
    if(std::atexit(runExitHooks)!=0)
      throw std::runtime_error("Process exit registration failed");
  });
}
void removeLast(std::vector<ListenerRef>& listeners,
    const ListenerRef& listener){
  const auto found=std::find(listeners.rbegin(),listeners.rend(),listener);
  if(found!=listeners.rend())listeners.erase(std::next(found).base());
}
}
struct ProcessEvents::State:std::enable_shared_from_this<State> {
  struct ExitEntry {ListenerRef listener;bool alwaysLast=false,active=true;};
  event_loop::Loop::Dispatcher loop;
  std::shared_ptr<cleanup::IndigenousSignalBridge> signals;
  std::vector<std::shared_ptr<ExitEntry>> exitListeners;
  std::vector<ListenerRef> resizeListeners,continueListeners;
  bool exitHookRegistered=false,resizeHookRegistered=false,
    continueHookRegistered=false,exited=false;
  State(event_loop::Loop& owner,
      std::shared_ptr<cleanup::IndigenousSignalBridge> bridge):
      loop(owner.dispatcher()),signals(std::move(bridge)){}
  void requireOwner()const{
    if(!loop.isOwnerThread())
      throw std::logic_error("Process events require owner loop");
  }
  void emitResize(){
    requireOwner();
    const auto listeners=resizeListeners;
    for(const auto& listener:listeners)listener->callback();
  }
  void emitContinue(){
    requireOwner();
    const auto listeners=continueListeners;
    for(const auto& listener:listeners)listener->callback();
  }
  void emitExit(){
    if(exited)return;
    exited=true;
    const auto listeners=exitListeners;
    for(const bool last:{false,true})
      for(const auto& entry:listeners)
        if(entry->active&&entry->alwaysLast==last)
          entry->listener->callback();
  }
  std::function<void()> onExit(ListenerRef listener,bool alwaysLast){
    requireOwner();
    if(!listener||!listener->callback)
      throw std::invalid_argument("Exit listener required");
    if(!exitHookRegistered){
      auto& hooks=exitHooks();
      ensureExitHook();
      auto weak=weak_from_this();
      hooks.push_back([weak]{
        if(const auto state=weak.lock())state->emitExit();});
      exitHookRegistered=true;
    }
    auto entry=std::make_shared<ExitEntry>(ExitEntry{
      std::move(listener),alwaysLast,true});
    exitListeners.push_back(entry);
    return [entry]{entry->active=false;};
  }
  void onResize(ListenerRef listener){
    requireOwner();
    if(!listener||!listener->callback)
      throw std::invalid_argument("Resize listener required");
    if(!resizeHookRegistered){
      auto weak=weak_from_this();
#ifndef _WIN32
      signals->onSignal(SIGWINCH,[weak]{
        if(const auto state=weak.lock())state->emitResize();});
#else
      (void)weak; // FTXUI's Windows console input path owns resize delivery.
#endif
      resizeHookRegistered=true;
    }
    resizeListeners.push_back(std::move(listener));
  }
  void onContinue(ListenerRef listener){
    requireOwner();
    if(!listener||!listener->callback)
      throw std::invalid_argument("Continue listener required");
    if(!continueHookRegistered){
      auto weak=weak_from_this();
#ifndef _WIN32
      signals->onSignal(SIGCONT,[weak]{
        if(const auto state=weak.lock())state->emitContinue();});
#else
      (void)weak; // Windows has no SIGCONT transition.
#endif
      continueHookRegistered=true;
    }
    continueListeners.push_back(std::move(listener));
  }
};
ProcessEvents::ProcessEvents(event_loop::Loop& loop,
    std::shared_ptr<cleanup::IndigenousSignalBridge> signals){
  if(!loop.isOwnerThread()||!signals)
    throw std::invalid_argument("Process signal owner required");
  state_=std::make_shared<State>(loop,std::move(signals));
}
ProcessLifecycleServices ProcessEvents::services()const{
  auto state=state_;
  ProcessLifecycleServices imports;
  imports.onExit=[state](ListenerRef listener,bool alwaysLast){
    return state->onExit(std::move(listener),alwaysLast);};
  imports.onStdoutResize=[state](ListenerRef listener){
    state->onResize(std::move(listener));};
  imports.offStdoutResize=[state](ListenerRef listener){
    state->requireOwner();removeLast(state->resizeListeners,listener);};
  imports.onSigcont=[state](ListenerRef listener){
    state->onContinue(std::move(listener));};
  imports.offSigcont=[state](ListenerRef listener){
    state->requireOwner();removeLast(state->continueListeners,listener);};
  return imports;
}
void ProcessEvents::emitExit()const{state_->emitExit();}
} // namespace batbox::terminal_runtime::indigenous
