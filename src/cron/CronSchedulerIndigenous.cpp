#include "cron/CronSchedulerIndigenous.hpp"
#include "cron/CronHuman.hpp"
#include "cron/locale/NotificationDate.hpp"
#include "cron/CronLock.hpp"
#include "bootstrap/BootstrapState.hpp"
#include "runtime/Platform.hpp"
#include "settings/watcher/IndigenousWatcher.hpp"
#include <atomic>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace batbox::cron {
namespace {
class IndigenousTimer final:public SchedulerTimer,
  public std::enable_shared_from_this<IndigenousTimer> {
public:
  IndigenousTimer(event_loop::Loop& loop,double delay,
    std::function<void()> callback):loop_(loop),delay_(delay),
    callback_(std::move(callback)){}
  void start(){schedule();}
  void cancel()override{active_=false;timer_.cancel();}
private:
  void schedule(){
    std::weak_ptr<IndigenousTimer> weak=shared_from_this();
    timer_=loop_.setTimeout(delay_,[weak]{
      if(const auto self=weak.lock()){
        if(!self->active_)return;
        self->callback_();
        if(self->active_)self->schedule();
      }
    },false);
  }
  event_loop::Loop& loop_;
  double delay_;
  std::function<void()> callback_;
  event_loop::Loop::TimerHandle timer_;
  bool active_=true;
};
class IndigenousFileWatcher final:public SchedulerWatcher {
public:
  IndigenousFileWatcher(const std::string& path,event_loop::Loop& loop,
    std::function<void()> add,std::function<void()> change,
    std::function<void()> unlink,
    const std::function<void(const std::string&)>& log){
    settings::persistence::WatchTargets targets;
    targets.dirs.push_back(std::filesystem::path(path).parent_path().string());
    targets.settingsFiles.push_back(path);
    settings::watcher::Events events;
    events.add=[add=std::move(add)](const std::string&){add();};
    events.change=[change=std::move(change)](const std::string&){change();};
    events.unlink=[unlink=std::move(unlink)](const std::string&){unlink();};
    events.error=[log](const std::string& error){
      log("[ScheduledTasks] watcher error: "+error);
    };
    settings::watcher::Options options;
    options.stabilityThresholdMs=300;
    options.pollIntervalMs=50;
    options.atomicWindowMs=100;
    watcher_=std::make_unique<settings::watcher::IndigenousWatcher>(
      std::move(targets),loop.dispatcher(),std::move(events),options);
  }
  void close()override{if(watcher_)watcher_->close();}
private:
  std::unique_ptr<settings::watcher::IndigenousWatcher> watcher_;
};
}
SchedulerServices makeIndigenousSchedulerServices(event_loop::Loop& loop,
  std::shared_ptr<bootstrap::BootstrapState> bootstrap,
  cleanup::Roster& cleanup,CronMissionServices missionServices){
  if(!bootstrap||!missionServices.logDebug)
    throw std::invalid_argument("Cron scheduler native bindings required");
  const auto lock=std::make_shared<CronLock>(loop,bootstrap,cleanup,
    missionServices.logDebug);
  SchedulerServices services;
  services.nowMs=[]{return static_cast<double>(platform::nowMillis());};
  services.loadWatcher=[&loop]{
    event_loop::Deferred<event_loop::Unit> done(loop);
    done.resolve({});return done;
  };
  services.readMissions=[&loop,missionServices](const auto& dir){
    event_loop::Deferred<std::vector<CronMissionRef>> out(loop);
    auto original=readCronMissions(loop,dir,missionServices);
    original.then([out](const std::vector<CronMission>* values,
      const std::exception_ptr& error){
      if(error){out.reject(error);return;}
      std::vector<CronMissionRef> refs;
      refs.reserve(values->size());
      for(const auto& value:*values)
        refs.push_back(std::make_shared<CronMission>(value));
      out.resolve(std::move(refs));
    });
    return out;
  };
  services.removeMissions=[&loop,missionServices](const auto& ids,const auto& dir){
    return removeCronMissions(loop,ids,dir,missionServices);
  };
  services.markFired=[&loop,missionServices](const auto& ids,double now,
    const auto& dir){return markCronMissionsFired(loop,ids,now,dir,missionServices);};
  services.hasMissionsSync=[missionServices](const auto& dir){
    return hasCronMissionsSync(dir,missionServices);
  };
  services.filePath=[missionServices](const auto& dir){
    return getCronFilePath(dir,missionServices);
  };
  services.getFlightMissions=[bootstrap]{
    std::vector<CronMissionRef> refs;
    for(const auto& mission:*bootstrap::getFlightCronMissions(bootstrap->cron)){
      auto copy=std::make_shared<CronMission>();
      copy->id=mission->id;copy->cron=mission->cron;copy->poetry=mission->poetry;
      copy->createdAt=mission->createdAt;copy->recurring=mission->recurring;
      copy->batId=mission->batId;refs.push_back(std::move(copy));
    }
    return refs;
  };
  services.removeFlightMissions=[bootstrap](const auto& ids){
    (void)bootstrap::removeFlightCronMissions(bootstrap->cron,ids);
  };
  services.getEnabled=[bootstrap]{
    return bootstrap::getScheduledMissionsEnabled(bootstrap->flags);
  };
  services.setEnabled=[bootstrap](bool value){
    bootstrap::setScheduledMissionsEnabled(bootstrap->flags,value);
  };
  services.acquireLock=[lock](const auto& dir,const auto& identity){
    return lock->tryAcquire({dir,identity});
  };
  services.releaseLock=[lock](const auto& dir,const auto& identity){
    return lock->release({dir,identity});
  };
  services.logDebug=missionServices.logDebug;
  services.watchFile=[&loop,log=missionServices.logDebug](const auto& path,
    auto add,auto change,auto unlink){
    return std::static_pointer_cast<SchedulerWatcher>(
      std::make_shared<IndigenousFileWatcher>(path,loop,std::move(add),
        std::move(change),std::move(unlink),log));
  };
  services.interval=[&loop](double ms,std::function<void()> callback){
    auto timer=std::make_shared<IndigenousTimer>(loop,ms,
      std::move(callback));timer->start();
    return std::static_pointer_cast<SchedulerTimer>(timer);
  };
  services.isoTime=[](double ms){return platform::isoTimestamp(ms);};
  const auto human=indigenousCronHumanServices();
  services.cronToHuman=[human](const std::string& expression){
    return cronToHuman(expression,false,human);
  };
  services.localDateTime=locale::indigenousNotificationDateFormatter();
  return services;
}
}
