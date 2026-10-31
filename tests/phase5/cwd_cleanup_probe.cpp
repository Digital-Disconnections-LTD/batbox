#include "cleanup/Roster.hpp"
#include "cwd/Override.hpp"
#include "cwd/SetCwd.hpp"
#include "cwd/State.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Unicode.hpp"
#include <cerrno>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <vector>

using namespace batbox;
namespace {
Json cwdTrace(){
  cwd::State state{"/original","/original","/global"};
  event_loop::Loop loop;
  Json events=Json::array();
  events.push_back("initial:"+cwd::pwd(state)+":"+cwd::getCwd(state));
  cwd::runWithCwdOverride("/outer",[&]{
    events.push_back("outer:"+cwd::pwd(state));
    cwd::runWithCwdOverride("/inner",[&]{events.push_back("inner:"+cwd::getCwd(state));});
    events.push_back("restored:"+cwd::pwd(state));
    loop.queueMicrotask(cwd::bindCurrentCwd([&]{events.push_back("async:"+cwd::getCwd(state));}));
  });
  events.push_back("after:"+cwd::pwd(state));loop.runReady();
  auto getter=[]{throw std::runtime_error("cwd unavailable");return std::string();};
  try{(void)cwd::pwd(getter);}catch(const std::exception& error){
    events.push_back(std::string("pwd-error:")+error.what());
  }
  events.push_back("fallback:"+cwd::getCwd(getter,[]{return std::string("/original");}));
  cwd::runWithCwdOverride("/override",[&]{events.push_back("override-fallback:"+
    cwd::getCwd(getter,[]{return std::string("/original");}));});
  return events;
}
Json stateTrace(){
  cwd::State state{"/initial","/initial","/initial"};
  cwd::setOriginalCwd(state,"e\xcc\x81");
  cwd::setProjectRoot(state,"a\xcc\x8a");
  cwd::setCwdState(state,"o\xcc\x88");
  Json out=Json::array();
  out.push_back(cwd::getOriginalCwd(state));out.push_back(cwd::getProjectRoot(state));
  out.push_back(cwd::getCwdState(state));
  cwd::setOriginalCwd(state,"/moved");
  out.push_back(cwd::getOriginalCwd(state));out.push_back(cwd::getProjectRoot(state));
  return out;
}
Json setCwdTrace(){
  cwd::State state{"/original","/original","/global"};Json events=Json::array();
  cwd::SetCwdServices services;
  services.fsCwd=[]{return std::string("/base");};
  services.realpathSync=[&](const std::string& path){
    events.push_back("realpath:"+path);
    if(path=="/missing"||path=="/base/missing")
      throw std::system_error(ENOENT,std::generic_category(),"not found");
    if(path=="/forbidden")throw std::runtime_error("denied");
    return std::string("/physical/")+value::normalizeNFC(path.substr(1));
  };
  const std::vector<std::pair<std::string,std::optional<std::string>>> cases={
    {"work",{}},{"../next","/alternative/sub"},{"/absolute",{}},
    {"missing",{}},{"/forbidden",{}},{"e\xcc\x81",{}}};
  for(const auto& [path,relative]:cases){
    try{cwd::setCwd(state,path,relative,services);
      events.push_back("set:"+state.cwd);events.push_back("ok:"+state.cwd);}
    catch(const std::exception& error){events.push_back(std::string("error:")+error.what());}
  }
  return events;
}
std::string errorMessage(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}catch(const std::exception& e){return e.what();}
}
Json rosterTrace(){
  Json result=Json::array();
  {
    event_loop::Loop loop;cleanup::Roster roster(loop);Json events=Json::array();
    event_loop::Deferred<event_loop::Unit> gate(loop);
    auto a=std::make_shared<cleanup::CleanupFunction>([&]{
      events.push_back("call:a");event_loop::Deferred<event_loop::Unit> p(loop);
      p.resolve(event_loop::Unit{});return p;});
    auto b=std::make_shared<cleanup::CleanupFunction>([&]{events.push_back("call:b");return gate;});
    auto unregister=roster.registerCleanup(a);roster.registerCleanup(a);roster.registerCleanup(b);
    auto pending=roster.runCleanupFunctions();
    pending.then([&](const event_loop::Unit*,const std::exception_ptr& error){
      if(error)events.push_back("rejected:"+errorMessage(error));else events.push_back("resolved");});
    events.push_back("after-call");unregister();events.push_back("after-unregister");
    gate.resolve(event_loop::Unit{});loop.run();events.push_back("after-await");
    result.push_back({{"name","snapshot-dedup"},{"events",events}});
  }
  {
    event_loop::Loop loop;cleanup::Roster roster(loop);Json events=Json::array();
    roster.registerCleanup([&]{events.push_back("call:a");event_loop::Deferred<event_loop::Unit> p(loop);
      p.resolve(event_loop::Unit{});return p;});
    roster.registerCleanup([&]() -> event_loop::Deferred<event_loop::Unit> {
      events.push_back("call:b");throw std::runtime_error("sync failure");});
    roster.registerCleanup([&]{events.push_back("call:c");event_loop::Deferred<event_loop::Unit> p(loop);
      p.resolve(event_loop::Unit{});return p;});
    auto pending=roster.runCleanupFunctions();
    pending.then([&](const event_loop::Unit*,const std::exception_ptr& error){
      if(error)events.push_back("rejected:"+errorMessage(error));else events.push_back("resolved");});
    events.push_back("after-call");loop.run();events.push_back("after-await");
    result.push_back({{"name","sync-throw"},{"events",events}});
  }
  {
    event_loop::Loop loop;cleanup::Roster roster(loop);Json events=Json::array();
    event_loop::Deferred<event_loop::Unit> gate(loop);
    roster.registerCleanup([&]{events.push_back("call:a");return gate;});
    roster.registerCleanup([&]{events.push_back("call:b");event_loop::Deferred<event_loop::Unit> p(loop);
      p.reject(std::make_exception_ptr(std::runtime_error("async failure")));return p;});
    auto pending=roster.runCleanupFunctions();
    pending.then([&](const event_loop::Unit*,const std::exception_ptr& error){
      if(error)events.push_back("rejected:"+errorMessage(error));else events.push_back("resolved");});
    events.push_back("after-call");loop.runReady();events.push_back("after-await");
    gate.resolve(event_loop::Unit{});loop.runReady();
    result.push_back({{"name","async-reject"},{"events",events}});
  }
  return result;
}
}
int main(){
  std::cout<<value::stringify(Json{{"cwd",cwdTrace()},{"state",stateTrace()},{"setCwd",setCwdTrace()},
    {"registry",rosterTrace()}})<<'\n';
}
