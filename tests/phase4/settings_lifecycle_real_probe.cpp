#include <unistd.h>
#include "settings/watcher/DetectorLifecycle.hpp"
#include "settings/persistence/ChangeEvents.hpp"
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace std::chrono_literals;
void need(bool okay,const char* message){
  if(!okay)throw std::runtime_error(message);
}
void write(const std::string& path,const std::string& content){
  std::ofstream file(path,std::ios::binary|std::ios::trunc);
  if(!file)throw std::runtime_error("Cannot write settings fixture");
  file<<content;
}
}
int main(){
  try{
    using namespace batbox;
    using namespace batbox::settings;
    using namespace batbox::settings::persistence;
    using namespace batbox::settings::watcher;
    char directory[]="/tmp/batbox-settings-life-XXXXXX";
    need(::mkdtemp(directory)!=nullptr,"mkdtemp failed");
    const auto root=std::filesystem::path(directory);
    std::filesystem::create_directories(root/"cfg");
    std::filesystem::create_directories(root/"project");
    std::filesystem::create_directories(root/"managed"/"managed-settings.d");
    PathServices pathServices;
    pathServices.processCwd=root.string();
    pathServices.originalCwd=(root/"project").string();
    pathServices.configHome=(root/"cfg").string();
    pathServices.platform="linux";pathServices.internalUser=true;
    pathServices.managedPathOverride=(root/"managed").string();
    Paths paths(pathServices);
    const auto userPath=*paths.getSettingsFilePathForSource(Source::User);
    write(userPath,"old");
    event_loop::Loop owner;
    auto now=[](){return std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now().time_since_epoch()).count();};
    InternalWrites internal(now);
    Json trace=Json::array();
    int unlinkSeen=0;
    ChangeEventServices eventServices;
    eventServices.paths=bindWatchPaths(paths);
    eventServices.internalWrites=&internal;
    eventServices.debug=[](const std::string&){};
    eventServices.executeConfigChangeHooks=[&](const std::string& source,
        const std::string&,auto done){
      trace.push_back("hook:"+source);
      owner.queueMicrotask([done=std::move(done)](){done({});});
    };
    eventServices.setTimeout=[&](double ms,auto callback){
      auto handle=owner.setTimeout(ms,std::move(callback));
      return [handle](){handle.cancel();};
    };
    eventServices.resetSettingsStash=[&](){trace.push_back("reset");};
    eventServices.deletionGraceMs=200;
    ChangeEvents change(std::move(eventServices));
    change.subscribe([&](Source source){
      trace.push_back("emit:"+sourceKey(source));});
    DetectorServices services;
    services.owner=owner.dispatcher();
    services.isRemoteMode=[](){return false;};
    services.startMdmPoll=[](double){};
    services.stopMdmPoll=[](){};
    services.registerCleanup=[](std::function<std::future<void>()>){};
    services.getWatchTargets=bindIndigenousWatchTargets(paths,owner.dispatcher());
    services.openWatcher=[&](const WatchTargets& targets,
        const Options& options,const Events& events){
      return openIndigenousWatcher(targets,owner.dispatcher(),events,options);
    };
    services.events.change=[&](const std::string& path){change.handleChange(path);};
    services.events.add=[&](const std::string& path){change.handleAdd(path);};
    services.events.unlink=[&](const std::string& path){
      ++unlinkSeen;change.handleDelete(path);};
    services.events.error=[](const std::string& error){
      throw std::runtime_error(error);};
    services.debug=[](const std::string&){};
    services.clearPendingDeletions=[&](){change.clearPendingDeletions();};
    services.clearInternalWrites=[&](){internal.clearInternalWrites();};
    services.clearSignal=[&](){change.clearListeners();};
    services.setDeletionGrace=[&](double ms){change.setDeletionGrace(ms);};
    DetectorLifecycle lifecycle(std::move(services));
    (void)lifecycle.resetForTesting(TimingOverrides{80,20,std::nullopt,200});
    auto init=lifecycle.initialize();
    auto pump=[&](int ms){
      const auto end=std::chrono::steady_clock::now()+
        std::chrono::milliseconds(ms);
      while(std::chrono::steady_clock::now()<end){
        owner.runReady();std::this_thread::sleep_for(10ms);
      }
      owner.runReady();
    };
    pump(50);
    need(init.wait_for(0ms)==std::future_status::ready,
      "Native detector initialize did not settle");
    init.get();
    need(lifecycle.initialized(),"Native detector not initialized");
    auto expectNotification=[&](std::size_t before){
      for(int i=0;i<200;i++){
        owner.runReady();
        if(trace.size()>=before+3)return;
        std::this_thread::sleep_for(10ms);
      }
      throw std::runtime_error("Missing settings notification");
    };
    auto expectOrder=[&](std::size_t start){
      need(trace[start]=="hook:user_settings","Hook did not run first");
      need(trace[start+1]=="reset","Cache was not reset second");
      need(trace[start+2]=="emit:userSettings","Listener was not last");
    };
    std::size_t before=trace.size();
    write(userPath,"new-content");expectNotification(before);expectOrder(before);
    internal.markInternalWrite(userPath);
    write(userPath,"self-write-content");
    pump(160);need(trace.size()==before+3,"Internal write notified listeners");
    before=trace.size();
    const auto temp=(root/"cfg"/"replacement.tmp").string();
    write(temp,"atomic-replacement");
    std::filesystem::rename(temp,userPath);
    expectNotification(before);expectOrder(before);
    before=trace.size();std::filesystem::remove(userPath);
    for(int i=0;i<100&&unlinkSeen==0;i++){
      owner.runReady();std::this_thread::sleep_for(10ms);
    }
    need(unlinkSeen>0,"Native deletion was not observed");
    write(userPath,"recreated-before-grace");
    expectNotification(before);expectOrder(before);
    pump(250);
    need(trace.size()==before+3,"Pending deletion was not cancelled");
    auto closed=lifecycle.dispose();
    need(closed.wait_for(0ms)==std::future_status::ready,
      "Watcher close did not settle");closed.get();
    before=trace.size();write(userPath,"after-close");pump(160);
    need(trace.size()==before,"Disposed detector emitted change");
    std::filesystem::remove_all(root);
    std::cout<<value::stringify({{"notifications",before/3},
      {"ownerOrdering",true},{"internalSuppression",true},
      {"disposedSuppression",true}})<<'\n';
  }catch(const std::exception& error){
    std::cerr<<error.what()<<'\n';return 1;
  }
}
