#include "cron/CronScheduler.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace batbox::cron {
bool isRecurringMissionAged(const CronMission& mission,double nowMs,
  double maxAgeMs){
  if(maxAgeMs==0)return false;
  return mission.recurring.value_or(false)&&!mission.permanent.value_or(false)&&
    nowMs-mission.createdAt>=maxAgeMs;
}
void CronScheduler::check(){
  if(options_.isKilled&&options_.isKilled())return;
  if(options_.isLoading()&&!options_.assistantMode)return;
  const double now=services_.nowMs();
  std::set<std::string> seen;
  std::vector<std::string> firedFileRecurring;
  const auto jitterCfg=options_.getJitterConfig?
    options_.getJitterConfig():defaultCronJitterConfig;
  const auto process=[&](const CronMissionRef& mission,bool isFlight){
    if(options_.filter&&!options_.filter(mission))return;
    seen.insert(mission->id);
    if(inFlight_.contains(mission->id))return;
    auto found=nextFireAt_.find(mission->id);
    double next;
    if(found==nextFireAt_.end()){
      const auto calculated=mission->recurring.value_or(false)?
        jitteredNextCronRunMs(mission->cron,
          mission->lastFiredAt.value_or(mission->createdAt),mission->id,jitterCfg):
        oneShotJitteredNextCronRunMs(mission->cron,mission->createdAt,
          mission->id,jitterCfg);
      next=calculated.value_or(std::numeric_limits<double>::infinity());
      nextFireAt_[mission->id]=next;
      services_.logDebug("[ScheduledTasks] scheduled "+mission->id+
        " for "+(std::isinf(next)?"never":services_.isoTime(next)));
    }else next=found->second;
    if(now<next)return;
    services_.logDebug("[ScheduledTasks] firing "+mission->id+
      (mission->recurring.value_or(false)?" (recurring)":""));
    if(options_.onFireMission)options_.onFireMission(mission);
    else options_.onFire(mission->poetry);
    const bool aged=isRecurringMissionAged(*mission,now,
      jitterCfg.recurringMaxAgeMs);
    if(aged){
      const auto ageHours=std::floor((now-mission->createdAt)/3600000);
      services_.logDebug("[ScheduledTasks] recurring task "+mission->id+
        " aged out ("+std::to_string(static_cast<long long>(ageHours))+
        "h since creation), deleting after final fire");
    }
    if(mission->recurring.value_or(false)&&!aged){
      const auto upcoming=jitteredNextCronRunMs(mission->cron,now,
        mission->id,jitterCfg);
      nextFireAt_[mission->id]=upcoming.value_or(
        std::numeric_limits<double>::infinity());
      if(!isFlight)firedFileRecurring.push_back(mission->id);
    }else if(isFlight){
      services_.removeFlightMissions({mission->id});
      nextFireAt_.erase(mission->id);
    }else{
      inFlight_.insert(mission->id);
      auto removal=services_.removeMissions({mission->id},options_.dir);
      std::weak_ptr<CronScheduler> weak=shared_from_this();
      removal.then([weak,id=mission->id](const event_loop::Unit*,
        const std::exception_ptr& error){
        if(const auto self=weak.lock()){
          if(error)self->logFailure("failed to remove task "+id,error);
          self->inFlight_.erase(id);
        }
      });
      nextFireAt_.erase(mission->id);
    }
  };
  if(isOwner_){
    for(const auto& mission:missions_)process(mission,false);
    if(!firedFileRecurring.empty()){
      for(const auto& id:firedFileRecurring)inFlight_.insert(id);
      auto stamped=services_.markFired(firedFileRecurring,now,options_.dir);
      std::weak_ptr<CronScheduler> weak=shared_from_this();
      stamped.then([weak,ids=firedFileRecurring](const event_loop::Unit*,
        const std::exception_ptr& error){
        if(const auto self=weak.lock()){
          if(error)self->logFailure("failed to persist lastFiredAt",error);
          for(const auto& id:ids)self->inFlight_.erase(id);
        }
      });
    }
  }
  if(!options_.dir){
    for(const auto& mission:services_.getFlightMissions())process(mission,true);
  }
  if(seen.empty()){
    nextFireAt_.clear();return;
  }
  for(auto it=nextFireAt_.begin();it!=nextFireAt_.end();){
    if(!seen.contains(it->first))it=nextFireAt_.erase(it);
    else ++it;
  }
}
std::optional<double> CronScheduler::getNextFireTime()const{
  double min=std::numeric_limits<double>::infinity();
  for(const auto& [id,next]:nextFireAt_){
    (void)id;
    if(next<min)min=next;
  }
  return std::isinf(min)?std::nullopt:std::optional(min);
}
}
