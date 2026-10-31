#include "settings/watcher/DetectorLifecycle.hpp"
#include "runtime/JsonValue.hpp"
#include <deque>
#include <iostream>

int main(){
  using namespace batbox;
  using namespace batbox::settings::watcher;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop owner;
    Json trace=Json::array();
    std::deque<std::function<void(
      std::optional<settings::persistence::WatchTargets>,std::exception_ptr)>>
      pending;
    std::vector<std::future<void>> initializations;
    bool mdmActive=false,watcherOpen=false,
      hasPendingDeletion=test.value("pendingDeletion",false);
    int nextWatcher=1;
    DetectorServices services;
    services.owner=owner.dispatcher();
    services.isRemoteMode=[&](){trace.push_back("remote");
      return test.value("remote",false);};
    services.startMdmPoll=[&](double){trace.push_back("mdmStart");
      mdmActive=true;};
    services.stopMdmPoll=[&](){
      if(mdmActive){trace.push_back("mdmStop");mdmActive=false;}};
    services.registerCleanup=[&](std::function<std::future<void>()>){
      trace.push_back("cleanupRegister");};
    services.getWatchTargets=[&](auto done){
      trace.push_back("targetsRequest");pending.push_back(std::move(done));};
    services.debug=[&](const std::string& text){trace.push_back({"log",text});};
    services.openWatcher=[&](const settings::persistence::WatchTargets& targets,
        const Options& options,const Events&){
      const int id=nextWatcher++;
      watcherOpen=true;
      trace.push_back({"watchOpen",targets.dirs,true,true,0,
        options.stabilityThresholdMs,options.pollIntervalMs,true,false,true});
      for(const auto& event:{"change","unlink","add"})
        trace.push_back({"on",event});
      return [&,id](std::function<void()> done){
        trace.push_back({"watchClose",id});watcherOpen=false;done();};
    };
    services.clearPendingDeletions=[&](){
      if(hasPendingDeletion){trace.push_back("timerCancel");
        hasPendingDeletion=false;}
    };
    services.clearInternalWrites=[&](){trace.push_back("internalClear");};
    services.clearSignal=[&](){trace.push_back("signalClear");};
    services.setDeletionGrace=[](double){};
    DetectorLifecycle detector(std::move(services));
    for(const auto& step:test["steps"]){
      const auto op=step["op"].get<std::string>();
      if(op=="init")initializations.push_back(detector.initialize());
      else if(op=="dispose")(void)detector.dispose();
      else if(op=="reset"){
        std::optional<TimingOverrides> overrides;
        if(step.contains("overrides")&&!step["overrides"].is_null()){
          overrides=TimingOverrides{};
          const auto& values=step["overrides"];
          if(values.contains("stabilityThreshold")&&
              !values["stabilityThreshold"].is_null())
            overrides->stabilityThreshold=values["stabilityThreshold"].get<double>();
          if(values.contains("pollInterval")&&
              !values["pollInterval"].is_null())
            overrides->pollInterval=values["pollInterval"].get<double>();
        }
        (void)detector.resetForTesting(overrides);
      }else if(op=="resolve"||op=="reject"){
        auto done=std::move(pending.front());pending.pop_front();
        if(op=="reject")done(std::nullopt,
          std::make_exception_ptr(std::runtime_error("stat failed")));
        else{
          const auto& value=step["targets"];
          settings::persistence::WatchTargets targets;
          targets.dirs=value["dirs"].get<std::vector<std::string>>();
          targets.settingsFiles=value["settingsFiles"].get<std::vector<std::string>>();
          if(!value["dropInDir"].is_null())
            targets.dropInDir=value["dropInDir"].get<std::string>();
          done(std::move(targets),nullptr);
        }
      }
      owner.runReady();
      for(auto it=initializations.begin();it!=initializations.end();){
        if(it->wait_for(std::chrono::milliseconds(0))!=
            std::future_status::ready){++it;continue;}
        try{it->get();}catch(const std::exception&){
          trace.push_back("initError");}
        it=initializations.erase(it);
      }
      trace.push_back({"state",detector.initialized(),detector.disposed(),
        watcherOpen,pending.size()});
    }
    std::cout<<value::stringify(trace)<<'\n';
  }
}
