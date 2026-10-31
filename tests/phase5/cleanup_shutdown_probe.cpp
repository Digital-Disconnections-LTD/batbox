#include "cleanup/Shutdown.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

using namespace batbox;
namespace {
struct Case {std::string name;int code;bool cleanupReject=false,cleanupPending=false,
  hookReject=false,reentrant=false;std::optional<std::string> final;
  bool sync=false,importFailure=false;};
Json run(const Case& item){
  event_loop::Loop loop;Json logs=Json::array();
  std::map<int,std::function<void()>> timers;
  cleanup::ShutdownServices services;
  services.loadHooks=[&]{
    if(item.importFailure)throw std::runtime_error("load failed");
    logs.push_back("loadHooks");
    event_loop::Deferred<cleanup::FlightEndHooks> result(loop);
    cleanup::FlightEndHooks hooks;
    hooks.getTimeoutMs=[&]{logs.push_back("hookBudget");return 1500.0;};
    hooks.execute=[&](cleanup::HookCall call){
      logs.push_back("hooks:"+call.reason+":"+value::numberToString(call.timeoutMs));
      event_loop::Deferred<event_loop::Unit> pending(loop);
      if(item.hookReject)pending.reject(std::make_exception_ptr(std::runtime_error("hook failed")));
      else pending.resolve(event_loop::Unit{});
      return pending;
    };
    result.resolve(hooks);return result;
  };
  services.setTimeout=[&](double delay,std::function<void()> callback){
    const int ms=static_cast<int>(delay);
    logs.push_back((ms==1500?"signal:":"timer:")+std::to_string(ms));
    timers[ms]=std::move(callback);
    return cleanup::ShutdownTimer{
      [&,ms]{if(ms!=1500)logs.push_back("clear:"+std::to_string(ms));timers.erase(ms);},
      [&,ms]{if(ms!=1500)logs.push_back("unref:"+std::to_string(ms));}
    };
  };
  services.setExitCode=[&](int code){logs.push_back("exitCode:"+std::to_string(code));};
  services.cleanupTerminalModes=[&]{logs.push_back("terminal");};
  services.printResumeHint=[&]{logs.push_back("hint");};
  event_loop::Deferred<event_loop::Unit> cleanupGate(loop);
  services.runCleanupFunctions=[&]{
    logs.push_back("cleanup");
    if(item.cleanupPending)return cleanupGate;
    event_loop::Deferred<event_loop::Unit> result(loop);
    if(item.cleanupReject)result.reject(std::make_exception_ptr(std::runtime_error("cleanup failed")));
    else result.resolve(event_loop::Unit{});
    return result;
  };
  services.profileReport=[&]{logs.push_back("profile");};
  services.writeStderr=[&](const std::string& value){
    auto text=value;
    std::string escaped;
    for(char ch:text)escaped+=ch=='\n'?"\\n":std::string(1,ch);
    logs.push_back("write:2:"+escaped);
  };
  services.debugFailure=[&](std::exception_ptr error){
    try{std::rethrow_exception(error);}catch(const std::exception& e){
      logs.push_back("debug:Graceful shutdown failed: Error: "+std::string(e.what()));
    }
  };
  services.resetResumeHint=[]{ };
  services.forceExit=[&](int code){logs.push_back("exit:"+std::to_string(code));};
  cleanup::Shutdown shutdown(loop,services);
  cleanup::ShutdownOptions options;options.finalMessage=item.final;
  if(item.sync){
    shutdown.gracefulShutdownSync(item.code,"other",options);
    loop.runReady();return Json{{"name",item.name},{"logs",logs}};
  }
  auto pending=shutdown.gracefulShutdown(item.code,"other",options);
  pending.then([&](const event_loop::Unit*,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);logs.push_back("resolved");
  });
  if(item.reentrant){auto second=shutdown.gracefulShutdown(99,"other");
    second.then([&](const event_loop::Unit*,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);logs.push_back("reentrant-resolved");});}
  loop.runReady();
  if(item.cleanupPending){
    const auto found=timers.find(2000);if(found==timers.end())throw std::runtime_error("cleanup timer absent");
    auto callback=found->second;callback();loop.runReady();
  }
  return Json{{"name",item.name},{"logs",logs}};
}
}
int main(){
  const std::vector<Case> cases={{"normal",7,false,false,false,false,"done"},
    {"cleanup-rejection",2,true,false},{"cleanup-timeout",1,false,true},
    {"hook-rejection",3,false,false,true},{"reentrant",4,false,true,false,true},
    {"sync-import-failure",5,false,false,false,false,{},true,true}};
  Json out=Json::array();for(const auto& item:cases)out.push_back(run(item));
  std::cout<<value::stringify(out)<<'\n';
}
