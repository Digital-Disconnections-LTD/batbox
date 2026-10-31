#include "cleanup/StashThrottles.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace batbox::cleanup {
namespace {
constexpr double oneDayMs=24.0*60.0*60.0*1000.0;
void requireCommon(const StashThrottleServices& s){
  if(!s.configHome||!s.nowIso||!s.nowMs||!s.statMtimeMs||!s.writeFile||
    !s.lock||!s.unlock||!s.debug||!s.logError||!s.joinPath)
    throw std::invalid_argument("cleanup throttle imports required");
}
struct StashRun:std::enable_shared_from_this<StashRun>{
  event_loop::Loop& loop;StashThrottleServices services;
  event_loop::Deferred<event_loop::Unit> result;
  std::string marker,stashPath;
  double startTime=0;
  StashApi api;StashEntryStream stream;
  std::vector<StashEntry> entries;
  std::vector<std::string> keysToRemove;
  bool ending=false;
  StashRun(event_loop::Loop& owner,StashThrottleServices imports):loop(owner),
    services(std::move(imports)),result(owner){}
  void start(){
    marker=services.joinPath(services.configHome(),".npm-cache-cleanup");
    auto self=shared_from_this();
    services.statMtimeMs(marker).then([self](const double* mtime,
      const std::exception_ptr& error){
      if(!error&&self->services.nowMs()-*mtime<oneDayMs){
        self->services.debug("npm cache cleanup: skipping, ran recently");
        self->result.resolve(event_loop::Unit{});return;
      }
      self->acquire();
    });
  }
  void acquire(){
    auto self=shared_from_this();
    services.lock(marker).then([self](const event_loop::Unit*,
      const std::exception_ptr& error){
      if(error){self->services.debug("npm cache cleanup: skipping, lock held");
        self->result.resolve(event_loop::Unit{});return;}
      self->services.debug("npm cache cleanup: starting");
      self->stashPath=self->services.joinPath(
        self->services.joinPath(self->services.homeDir(),".npm"),"_cacache");
      self->startTime=self->services.nowMs();self->load();
    });
  }
  void load(){
    auto self=shared_from_this();
    try{services.loadStashApi().then([self](const StashApi* value,
      const std::exception_ptr& error){
      if(error){self->fail(error);return;}
      try{self->api=*value;self->stream=self->api.listStream(self->stashPath);
        if(!self->stream.next)throw std::invalid_argument("cache stream next required");
        self->readNext();}
      catch(...){self->fail(std::current_exception());}
    });}catch(...){fail(std::current_exception());}
  }
  void readNext(){
    auto self=shared_from_this();
    try{stream.next().then([self](const std::optional<StashEntry>* entry,
      const std::exception_ptr& error){
      if(error){self->fail(error);return;}
      if(!*entry){self->selectRemovals();return;}
      if((*entry)->key.find(self->services.packageKeySubstring)!=std::string::npos)
        self->entries.push_back(**entry);
      self->readNext();
    });}catch(...){fail(std::current_exception());}
  }
  void selectRemovals(){
    std::vector<std::pair<std::string,std::vector<StashEntry>>> groups;
    for(const auto& entry:entries){
      const auto at=entry.key.rfind('@');
      const auto name=at!=std::string::npos&&at>0?entry.key.substr(0,at):entry.key;
      auto found=std::find_if(groups.begin(),groups.end(),
        [&](const auto& group){return group.first==name;});
      if(found==groups.end()){groups.push_back({name,{}});found=std::prev(groups.end());}
      found->second.push_back(entry);
    }
    const auto cutoff=startTime-oneDayMs;
    for(auto& [name,group]:groups){
      (void)name;
      std::stable_sort(group.begin(),group.end(),[](const StashEntry& a,
        const StashEntry& b){return a.timeMs>b.timeMs;});
      for(std::size_t index=0;index<group.size();++index){
        if(group[index].timeMs<cutoff||index>=5)
          keysToRemove.push_back(group[index].key);
      }
    }
    removeAll();
  }
  void removeAll(){
    if(keysToRemove.empty()){writeMarker();return;}
    auto self=shared_from_this();
    auto remaining=std::make_shared<std::size_t>(keysToRemove.size());
    // Promise.all starts every remove call in map order before it observes
    // any already-settled rejection.
    try{for(const auto& key:keysToRemove){
      api.removeEntry(stashPath,key).then([self,remaining](const event_loop::Unit*,
        const std::exception_ptr& error){
        if(error){self->fail(error);return;}
        if(--*remaining==0&&!self->ending)self->writeMarker();
      });
    }}catch(...){fail(std::current_exception());}
  }
  void writeMarker(){
    auto self=shared_from_this();
    try{services.writeFile(marker,services.nowIso()).then([self](const event_loop::Unit*,
      const std::exception_ptr& error){
      if(error){self->fail(error);return;}
      try{
        const auto duration=self->services.nowMs()-self->startTime;
        if(!self->keysToRemove.empty())
          self->services.debug("npm cache cleanup: Removed "+
            std::to_string(self->keysToRemove.size())+" old "+
            self->services.packageDebugLabel+" entries in "+
            value::numberToString(duration)+"ms");
        else self->services.debug("npm cache cleanup: completed in "+
          value::numberToString(duration)+"ms");
        self->unlock();
      }catch(...){self->fail(std::current_exception());}
    });}catch(...){fail(std::current_exception());}
  }
  void fail(std::exception_ptr error){
    if(ending)return;
    try{services.logError(error);}catch(...){}
    unlock();
  }
  void unlock(){
    if(ending)return;
    ending=true;
    auto self=shared_from_this();
    try{services.unlock(marker).then([self](const event_loop::Unit*,
      const std::exception_ptr&){self->result.resolve(event_loop::Unit{});});}
    catch(...){result.resolve(event_loop::Unit{});}
  }
};
struct VersionRun:std::enable_shared_from_this<VersionRun>{
  event_loop::Loop& loop;StashThrottleServices services;
  event_loop::Deferred<event_loop::Unit> result;
  std::string marker;
  VersionRun(event_loop::Loop& owner,StashThrottleServices imports):loop(owner),
    services(std::move(imports)),result(owner){}
  void start(){
    marker=services.joinPath(services.configHome(),".version-cleanup");
    auto self=shared_from_this();
    services.statMtimeMs(marker).then([self](const double* mtime,
      const std::exception_ptr& error){
      if(!error&&self->services.nowMs()-*mtime<oneDayMs){
        self->services.debug("version cleanup: skipping, ran recently");
        self->result.resolve(event_loop::Unit{});return;
      }
      self->acquire();
    });
  }
  void acquire(){
    auto self=shared_from_this();
    services.lock(marker).then([self](const event_loop::Unit*,
      const std::exception_ptr& error){
      if(error){self->services.debug("version cleanup: skipping, lock held");
        self->result.resolve(event_loop::Unit{});return;}
      self->services.debug("version cleanup: starting (throttled)");
      self->runCleanup();
    });
  }
  void runCleanup(){
    auto self=shared_from_this();
    try{services.cleanupOldVersions().then([self](const event_loop::Unit*,
      const std::exception_ptr& error){
      if(error){self->fail(error);return;}
      self->writeMarker();
    });}catch(...){fail(std::current_exception());}
  }
  void writeMarker(){
    auto self=shared_from_this();
    try{services.writeFile(marker,services.nowIso()).then([self](const event_loop::Unit*,
      const std::exception_ptr& error){
      if(error)self->fail(error);else self->unlock();
    });}catch(...){fail(std::current_exception());}
  }
  void fail(std::exception_ptr error){
    try{services.logError(error);}catch(...){}
    unlock();
  }
  void unlock(){
    auto self=shared_from_this();
    try{services.unlock(marker).then([self](const event_loop::Unit*,
      const std::exception_ptr&){self->result.resolve(event_loop::Unit{});});}
    catch(...){result.resolve(event_loop::Unit{});}
  }
};
}
event_loop::Deferred<event_loop::Unit> cleanupPackageStashThrottled(
  event_loop::Loop& loop,const StashThrottleServices& services){
  if(!loop.isOwnerThread())throw std::logic_error("package cleanup requires owner");
  requireCommon(services);
  if(!services.homeDir||!services.loadStashApi||services.packageKeySubstring.empty()||
    services.packageDebugLabel.empty())
    throw std::invalid_argument("cache package imports required");
  auto run=std::make_shared<StashRun>(loop,services);
  try{run->start();}catch(...){run->result.reject(std::current_exception());}
  return run->result;
}
event_loop::Deferred<event_loop::Unit> cleanupOldVersionsThrottled(
  event_loop::Loop& loop,const StashThrottleServices& services){
  if(!loop.isOwnerThread())throw std::logic_error("version cleanup requires owner");
  requireCommon(services);
  if(!services.cleanupOldVersions)
    throw std::invalid_argument("old versions cleanup import required");
  auto run=std::make_shared<VersionRun>(loop,services);
  try{run->start();}catch(...){run->result.reject(std::current_exception());}
  return run->result;
}
} // namespace batbox::cleanup
