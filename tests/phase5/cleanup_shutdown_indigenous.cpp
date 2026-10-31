#include "cleanup/Roster.hpp"
#include "cleanup/Shutdown.hpp"
#include <cassert>
#include <string>
#include <vector>

using namespace batbox;
int main(){
  event_loop::Loop loop;cleanup::Roster roster(loop);
  std::vector<std::string> events;
  roster.registerCleanup([&]{
    events.push_back("cleanup");
    event_loop::Deferred<event_loop::Unit> result(loop);
    loop.queueMicrotask([result]{result.resolve(event_loop::Unit{});});
    return result;
  });
  cleanup::ShutdownServices services;
  services.loadHooks=[&]{
    event_loop::Deferred<cleanup::FlightEndHooks> result(loop);
    cleanup::FlightEndHooks hooks;
    hooks.getTimeoutMs=[]{return 100.0;};
    hooks.execute=[&](cleanup::HookCall call){
      assert(call.timeoutMs==100&&call.reason=="other"&&!call.signal.stop_requested());
      events.push_back("hook");
      event_loop::Deferred<event_loop::Unit> done(loop);done.resolve(event_loop::Unit{});return done;
    };
    result.resolve(hooks);return result;
  };
  services.setTimeout=[&](double delay,std::function<void()> callback){
    return cleanup::indigenousShutdownTimer(loop,delay,std::move(callback));
  };
  services.setExitCode=[&](int code){assert(code==8);events.push_back("code");};
  services.cleanupTerminalModes=[&]{events.push_back("terminal");};
  services.printResumeHint=[&]{events.push_back("hint");};
  services.runCleanupFunctions=[&]{return roster.runCleanupFunctions();};
  services.profileReport=[&]{events.push_back("profile");};
  services.writeStderr=[&](const std::string& value){
    assert(value=="finished\n");events.push_back("message");
  };
  services.debugFailure=[](std::exception_ptr){assert(false);};
  services.resetResumeHint=[&]{events.push_back("reset-hint");};
  services.forceExit=[&](int code){assert(code==8);events.push_back("exit");};
  cleanup::Shutdown shutdown(loop,services);
  cleanup::ShutdownOptions options;options.finalMessage="finished";
  auto pending=shutdown.gracefulShutdown(8,"other",options);
  bool settled=false;
  pending.then([&](const event_loop::Unit*,const std::exception_ptr& error){
    assert(!error);settled=true;
  });
  loop.run();
  assert(settled&&shutdown.isShuttingDown());
  assert((events==std::vector<std::string>{"code","terminal","hint","cleanup","hook",
    "profile","message","exit"}));
  shutdown.resetShutdownState();
  assert(!shutdown.isShuttingDown()&&events.back()=="reset-hint");
}
