#include "cron/CronScheduler.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::cron {
CronScheduler::CronScheduler(event_loop::Loop& loop,SchedulerOptions options,
  SchedulerServices services):loop_(loop),options_(std::move(options)),
  services_(std::move(services)){
  if(!options_.onFire||!options_.isLoading||!services_.nowMs||
     !services_.loadWatcher||!services_.readMissions||!services_.removeMissions||
     !services_.markFired||!services_.hasMissionsSync||!services_.filePath||
     !services_.acquireLock||!services_.releaseLock||!services_.watchFile||
     !services_.interval||!services_.logDebug||!services_.isoTime||
     !services_.cronToHuman||!services_.localDateTime)
    throw std::invalid_argument("Cron scheduler bindings required");
  if(!options_.dir&&(!services_.getFlightMissions||
      !services_.removeFlightMissions||!services_.getEnabled||
      !services_.setEnabled))
    throw std::invalid_argument("Cron scheduler bootstrap bindings required");
}
std::shared_ptr<CronScheduler> CronScheduler::create(event_loop::Loop& loop,
  SchedulerOptions options,SchedulerServices services){
  return std::shared_ptr<CronScheduler>(new CronScheduler(loop,
    std::move(options),std::move(services)));
}
void CronScheduler::logFailure(const std::string& operation,
  std::exception_ptr error){
  try{std::rethrow_exception(error);}
  catch(const std::exception& ex){services_.logDebug(
    "[ScheduledTasks] "+operation+": "+ex.what());}
  catch(...){services_.logDebug("[ScheduledTasks] "+operation);}
}
void CronScheduler::load(bool initial){
  auto loaded=services_.readMissions(options_.dir);
  std::weak_ptr<CronScheduler> weak=shared_from_this();
  loaded.then([weak,initial](const std::vector<CronMissionRef>* next,
    const std::exception_ptr& error){
    const auto self=weak.lock();
    if(!self||error||self->stopped_)return;
    self->missions_=*next;
    if(!initial)return;
    const double now=self->services_.nowMs();
    std::vector<CronMissionRef> missed;
    for(const auto& mission:findMissedMissions(*next,now)){
      if(mission->recurring.value_or(false)||
         self->missedAsked_.contains(mission->id)||
         (self->options_.filter&&!self->options_.filter(mission)))continue;
      missed.push_back(mission);
    }
    if(missed.empty())return;
    for(const auto& mission:missed){
      self->missedAsked_.insert(mission->id);
      self->nextFireAt_[mission->id]=
        std::numeric_limits<double>::infinity();
    }
    if(self->options_.onMissed)self->options_.onMissed(missed);
    else self->options_.onFire(buildMissedMissionNotification(missed,
      self->services_.cronToHuman,self->services_.localDateTime));
    std::vector<std::string> ids;
    for(const auto& mission:missed)ids.push_back(mission->id);
    auto removed=self->services_.removeMissions(ids,self->options_.dir);
    removed.then([weak](const event_loop::Unit*,
      const std::exception_ptr& removeError){
      if(const auto owner=weak.lock())
        if(removeError)owner->logFailure(
          "failed to remove missed tasks",removeError);
    });
    self->services_.logDebug("[ScheduledTasks] surfaced "+
      std::to_string(missed.size())+" missed one-shot task(s)");
  });
}
void CronScheduler::probeLock(){
  auto acquired=services_.acquireLock(options_.dir,options_.lockIdentity);
  std::weak_ptr<CronScheduler> weak=shared_from_this();
  acquired.then([weak](const bool* owned,const std::exception_ptr& error){
    const auto self=weak.lock();if(!self)return;
    if(error){self->logFailure("scheduler lock probe",error);return;}
    if(self->stopped_){
      if(*owned)(void)self->services_.releaseLock(self->options_.dir,
        self->options_.lockIdentity);
      return;
    }
    if(*owned){
      self->isOwner_=true;
      if(self->lockProbeTimer_)self->lockProbeTimer_->cancel();
      self->lockProbeTimer_.reset();
    }
  });
}
void CronScheduler::beginAfterLock(bool owner){
  isOwner_=owner;
  if(stopped_){
    if(isOwner_){isOwner_=false;
      (void)services_.releaseLock(options_.dir,options_.lockIdentity);}
    return;
  }
  std::weak_ptr<CronScheduler> weak=shared_from_this();
  if(!isOwner_)lockProbeTimer_=services_.interval(5000,[weak]{
    if(const auto self=weak.lock())self->probeLock();
  });
  load(true);
  watcher_=services_.watchFile(services_.filePath(options_.dir),
    [weak]{if(const auto self=weak.lock())self->load(false);},
    [weak]{if(const auto self=weak.lock())self->load(false);},
    [weak]{if(const auto self=weak.lock())if(!self->stopped_){
      self->missions_.clear();self->nextFireAt_.clear();}});
  checkTimer_=services_.interval(1000,[weak]{
    if(const auto self=weak.lock())self->check();
  });
}
void CronScheduler::beginAfterWatcherImport(){
  if(stopped_)return;
  auto acquired=services_.acquireLock(options_.dir,options_.lockIdentity);
  std::weak_ptr<CronScheduler> weak=shared_from_this();
  acquired.then([weak](const bool* owner,const std::exception_ptr& error){
    if(const auto self=weak.lock())self->beginAfterLock(error?false:*owner);
  });
}
void CronScheduler::enable(){
  if(stopped_)return;
  if(enablePoll_){enablePoll_->cancel();enablePoll_.reset();}
  auto imported=services_.loadWatcher();
  std::weak_ptr<CronScheduler> weak=shared_from_this();
  imported.then([weak](const event_loop::Unit*,
    const std::exception_ptr& error){
    if(const auto self=weak.lock()){
      if(error)return;
      self->beginAfterWatcherImport();
    }
  });
}
void CronScheduler::start(){
  stopped_=false;
  if(options_.dir){
    const auto hasMissions=services_.hasMissionsSync(options_.dir);
    services_.logDebug("[ScheduledTasks] scheduler start() — dir="+
      *options_.dir+", hasTasks="+
      (hasMissions?"true":"false"));
    enable();return;
  }
  const auto enabled=services_.getEnabled();
  const auto hasMissions=services_.hasMissionsSync(options_.dir);
  services_.logDebug("[ScheduledTasks] scheduler start() — enabled="+
    std::string(enabled?"true":"false")+", hasTasks="+
    (hasMissions?"true":"false"));
  if(!services_.getEnabled()&&
      (options_.assistantMode||services_.hasMissionsSync(options_.dir)))
    services_.setEnabled(true);
  if(services_.getEnabled()){enable();return;}
  std::weak_ptr<CronScheduler> weak=shared_from_this();
  enablePoll_=services_.interval(1000,[weak]{
    if(const auto self=weak.lock())
      if(self->services_.getEnabled())self->enable();
  });
}
void CronScheduler::stop(){
  stopped_=true;
  if(enablePoll_){enablePoll_->cancel();enablePoll_.reset();}
  if(checkTimer_){checkTimer_->cancel();checkTimer_.reset();}
  if(lockProbeTimer_){lockProbeTimer_->cancel();lockProbeTimer_.reset();}
  if(watcher_){watcher_->close();watcher_.reset();}
  if(isOwner_){isOwner_=false;
    (void)services_.releaseLock(options_.dir,options_.lockIdentity);}
}
}
