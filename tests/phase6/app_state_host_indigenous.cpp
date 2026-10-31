#include "app_state/Host.hpp"
#include <cassert>
#include <thread>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  auto initial=std::make_shared<app_state::AppState>();
  initial->fields=std::make_shared<value::SharedRecord>();
  initial->permissionContext=std::make_shared<value::SharedRecord>();
  initial->permissionContext->set("mode","default");
  initial->fields->set("toolPermissionContext",
    initial->permissionContext->snapshot());
  initial->fields->set("mainLoopModel",nullptr);
  initial->fields->set("expandedView","none");
  initial->fields->set("verbose",false);
  initial->fields->set("settings",Json::object());
  Json config={{"verbose",false}};
  std::vector<std::string> trace;
  app_state::AppStateChangeServices services;
  services.isPrivilegedUserType=[] {return false;};
  services.getGlobalConfig=[&]{trace.push_back("getConfig");return config;};
  services.saveGlobalConfig=[&](const std::function<Json(const Json&)>& update){
    config=update(config);trace.push_back("saveConfig");};
  auto host=std::make_shared<app_state::Host>(loop,initial,services);
  auto listener=std::make_shared<app_state::AppStateStore::Listener>([&]{
    trace.push_back("listener");});
  auto unsubscribe=host->subscribe(listener);
  auto get=host->snapshotGetter();
  auto set=host->typedSetter();
  assert(get().at("verbose")==false);
  set([](const app_state::AppStateRef& previous){
    auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
    next->fields->set("verbose",true);
    return next;
  });
  assert((trace==std::vector<std::string>{"getConfig","saveConfig","listener"}));
  assert(get().at("verbose")==true);
  unsubscribe();
  bool offOwner=false;
  std::thread worker([&]{
    try{(void)host->getSnapshot();}
    catch(const std::logic_error&){offOwner=true;}
  });
  worker.join();
  assert(offOwner);
  host.reset();
  bool expired=false;
  try{(void)get();}catch(const std::logic_error&){expired=true;}
  assert(expired);
}
