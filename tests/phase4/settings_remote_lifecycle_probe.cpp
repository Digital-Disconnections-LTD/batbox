#include "settings/remote/Lifecycle.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  using namespace batbox::settings::remote;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array(),values=Json::array();
    SyncStashServices stashServices;
    stashServices.configHome=[](){return "/config";};
    stashServices.readFileSync=[](const std::string&)->std::string{
      throw std::runtime_error("ENOENT");};
    stashServices.resetMergedSettingsStash=[](){};
    SyncStash stash(stashServices);
    EligibilityServices eligibilityServices;
    eligibilityServices.provider=[&](){return test.value("eligible",true)
      ?std::string("firstParty"):std::string("other");};
    eligibilityServices.isPrimaryBaseUrl=[](){return true;};
    eligibilityServices.entrypoint=[](){return std::string{};};
    eligibilityServices.oauthTokens=[](){return Json{
      {"accessToken","token"},{"subscriptionType",nullptr}};};
    eligibilityServices.apiKeyWithoutHelper=[](){
      return std::optional<std::string>{};};
    Eligibility eligibility(stash,eligibilityServices);
    if(test.contains("cached")&&!test["cached"].is_null())
      stash.setFlightStash(test["cached"]);
    std::size_t fetch=0;
    std::function<void()> tick;
    std::function<void()> timeout;
    LifecycleServices services;
    services.eligibility=&eligibility;services.stash=&stash;
    services.fetchAndLoad=[&](){
      trace.push_back({"fetch"});
      event_loop::Deferred<std::optional<Json>> done(loop);
      std::optional<Json> next;
      if(test.contains("fetchResults")&&fetch<test["fetchResults"].size()){
        const auto& value=test["fetchResults"][fetch++];
        if(!value.is_null()){next=value;stash.setFlightStash(next);}
      }
      done.resolve(next);return done;
    };
    services.unlinkSettings=[&](const std::string& path){
      trace.push_back({"unlink",path});
      event_loop::Deferred<event_loop::Unit> done(loop);done.resolve({});
      return done;
    };
    services.debug=[&](const std::string& value){
      trace.push_back({"debug",value});};
    services.notifyPolicySettings=[&](){
      trace.push_back({"notify","policySettings"});};
    services.setTimeout=[&](double delay,std::function<void()> callback){
      trace.push_back({"timeoutSet",delay});timeout=std::move(callback);
    };
    services.setInterval=[&](double delay,std::function<void()> callback,
        bool unref){
      trace.push_back({"interval",delay});tick=std::move(callback);
      if(unref)trace.push_back({"unref"});
      return [&](){trace.push_back({"clearInterval"});tick={};};
    };
    services.registerCleanup=[&](auto){trace.push_back({"cleanup"});};
    Lifecycle lifecycle(loop,services);
    const auto await=[&](event_loop::Deferred<event_loop::Unit> promise){
      bool done=false;
      promise.then([&](const event_loop::Unit*,
          const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);
        done=true;
      });
      for(int i=0;i<100&&!done;i++)loop.runReady();
      if(!done)throw std::runtime_error("Lifecycle did not settle");
    };
    for(const auto& action:test["actions"]){
      const auto op=action.get<std::string>();
      if(op=="init")lifecycle.initializeRemoteManagedSettingsLoadingPromise();
      else if(op=="load")await(lifecycle.loadRemoteManagedSettings());
      else if(op=="wait")await(lifecycle.waitForRemoteManagedSettingsToLoad());
      else if(op=="refresh")await(lifecycle.refreshRemoteManagedSettings());
      else if(op=="poll")await(lifecycle.pollRemoteSettings());
      else if(op=="startPoll")lifecycle.startBackgroundPolling();
      else if(op=="stopPoll")lifecycle.stopBackgroundPolling();
      else if(op=="tick"&&tick){tick();loop.runReady();}
      else if(op=="timeout"&&timeout){timeout();loop.runReady();}
      else if(op=="eligible")values.push_back(
        lifecycle.isEligibleForRemoteManagedSettings());
    }
    const auto current=stash.getRemoteManagedSettingsSyncFromStash();
    std::cout<<value::stringify(Json{{"values",values},
      {"session",current?*current:Json(nullptr)},
      {"trace",trace}})<<'\n';
  }
}
