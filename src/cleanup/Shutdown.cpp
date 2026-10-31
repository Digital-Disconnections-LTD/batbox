#include "cleanup/Shutdown.hpp"
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace batbox::cleanup {
struct Shutdown::State {
  event_loop::Loop& loop;
  ShutdownServices services;
  bool inProgress=false;
  std::optional<ShutdownTimer> failsafe;
  std::optional<event_loop::Deferred<event_loop::Unit>> pending;
  State(event_loop::Loop& owner,ShutdownServices imports):loop(owner),services(std::move(imports)){}
};
namespace {
void require(const ShutdownServices& services){
  if(!services.loadHooks||!services.setTimeout||!services.setExitCode||
    !services.cleanupTerminalModes||!services.printResumeHint||
    !services.runCleanupFunctions||!services.profileReport||
    !services.writeStderr||!services.debugFailure||
    !services.resetResumeHint||!services.forceExit)
    throw std::invalid_argument("graceful shutdown imports required");
}
}
Shutdown::Shutdown(event_loop::Loop& loop,ShutdownServices services):
  state_(std::make_shared<State>(loop,std::move(services))){require(state_->services);}
bool Shutdown::isShuttingDown()const{return state_->inProgress;}
void Shutdown::resetShutdownState(){
  if(!state_->loop.isOwnerThread())throw std::logic_error("shutdown reset requires owner");
  state_->inProgress=false;
  state_->services.resetResumeHint();
  if(state_->failsafe){state_->failsafe->cancel();state_->failsafe.reset();}
  state_->pending.reset();
}
std::optional<event_loop::Deferred<event_loop::Unit>> Shutdown::pendingShutdown()const{
  return state_->pending;
}
event_loop::Deferred<event_loop::Unit> Shutdown::gracefulShutdown(int code,
  const std::string& reason,ShutdownOptions options){
  auto state=state_;
  if(!state->loop.isOwnerThread())throw std::logic_error("shutdown requires owner");
  event_loop::Deferred<event_loop::Unit> result(state->loop);
  if(state->inProgress){result.resolve(event_loop::Unit{});return result;}
  state->inProgress=true;
  // Await hook loading before arming the failsafe or
  // setting process.exitCode. Hook loading is a required owner-loop dependency.
  state->loop.queueMicrotask([state,result,code,reason,options]{
    try{state->services.loadHooks().then([state,result,code,reason,options](
    const FlightEndHooks* loaded,const std::exception_ptr& importError){
    if(importError){result.reject(importError);return;}
    try{
      if(!loaded->getTimeoutMs||!loaded->execute)
        throw std::invalid_argument("SessionEnd hook imports required");
      const auto hooks=*loaded;
      const auto timeout=hooks.getTimeoutMs();
      state->failsafe=state->services.setTimeout(std::max(5000.0,timeout+3500.0),
        [state,code]{
          state->services.cleanupTerminalModes();
          state->services.printResumeHint();
          if(state->failsafe){state->failsafe->cancel();state->failsafe.reset();}
          state->services.forceExit(code);
        });
      state->failsafe->unref();
      state->services.setExitCode(code);
      state->services.cleanupTerminalModes();
      state->services.printResumeHint();

      struct CleanupGate {bool completed=false;std::optional<ShutdownTimer> timeout;};
      auto gate=std::make_shared<CleanupGate>();
      auto afterCleanup=[state,result,code,reason,options,hooks,timeout,gate]{
        if(gate->completed)return;
        gate->completed=true;
        if(gate->timeout){gate->timeout->cancel();gate->timeout.reset();}
        auto afterHooks=[state,result,code,options]{
          try{state->services.profileReport();}catch(...){}
          if(options.finalMessage&&!options.finalMessage->empty()){
            try{state->services.writeStderr(*options.finalMessage+"\n");}catch(...){}
          }
          if(state->failsafe){state->failsafe->cancel();state->failsafe.reset();}
          try{state->services.forceExit(code);result.resolve(event_loop::Unit{});}
          catch(...){result.reject(std::current_exception());}
        };
        auto signal=std::make_shared<std::stop_source>();
        auto abortTimer=std::make_shared<ShutdownTimer>(state->services.setTimeout(timeout,
          [signal]{signal->request_stop();}));
        abortTimer->unref();
        try{hooks.execute(HookCall{reason,options,signal->get_token(),timeout}).then(
          [afterHooks,abortTimer,signal](const event_loop::Unit*,const std::exception_ptr&){
            abortTimer->cancel();afterHooks();
          });}
        catch(...){abortTimer->cancel();afterHooks();}
      };
      std::optional<event_loop::Deferred<event_loop::Unit>> cleanup;
      try{cleanup=state->services.runCleanupFunctions();}
      catch(...){} // Roster cleanup failures do not prevent shutdown.
      gate->timeout=state->services.setTimeout(2000,[afterCleanup]{afterCleanup();});
      if(cleanup)cleanup->then([afterCleanup](const event_loop::Unit*,
        const std::exception_ptr&){afterCleanup();});
      else state->loop.queueMicrotask(afterCleanup);
    }catch(...){result.reject(std::current_exception());}
    });}
    catch(...){result.reject(std::current_exception());}
  });
  return result;
}
void Shutdown::gracefulShutdownSync(int code,const std::string& reason,
  ShutdownOptions options){
  auto state=state_;
  if(!state->loop.isOwnerThread())throw std::logic_error("shutdown requires owner");
  state->services.setExitCode(code);
  event_loop::Deferred<event_loop::Unit> settled(state->loop);
  state->pending=settled;
  gracefulShutdown(code,reason,std::move(options)).then([state,settled,code](
    const event_loop::Unit*,const std::exception_ptr& error){
    if(error){
      try{
        state->services.debugFailure(error);
        state->services.cleanupTerminalModes();
        state->services.printResumeHint();
        state->services.forceExit(code);
      }catch(...){} // Catch failures from the final cleanup reaction.
    }
    settled.resolve(event_loop::Unit{});
  });
}
ShutdownTimer indigenousShutdownTimer(event_loop::Loop& loop,double delayMs,
  std::function<void()> callback){
  auto timer=loop.setTimeout(delayMs,std::move(callback));
  return {[timer]{timer.cancel();},[timer]{timer.unref();}};
}
} // namespace batbox::cleanup
