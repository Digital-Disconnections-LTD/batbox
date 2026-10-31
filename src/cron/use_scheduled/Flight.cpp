#include "cron/use_scheduled/Flight.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::cron::use_scheduled {
namespace {
template<class F>void require(const F& callback,const char* name){
  if(!callback)throw std::invalid_argument(
    std::string("Scheduled-task import required: ")+name);
}
void enqueueForLead(const std::shared_ptr<Services>& services,
    const std::string& poetry){
  services->enqueuePendingNotification(Json{{"value",poetry},
    {"mode","prompt"},{"priority","later"},{"isMeta",true},
    {"workload","cron"}});
}
}
Flight::Flight(event_loop::Loop& loop,Services services):loop_(loop),
    services_(std::make_shared<Services>(std::move(services))){
  require(services_->cronEnabled,"cronEnabled");
  require(services_->createScheduler,"createScheduler");
  require(services_->getJitterConfig,"getJitterConfig");
  require(services_->enqueuePendingNotification,"enqueuePendingNotification");
  require(services_->getCurrentMissions,"getCurrentTasks");
  require(services_->findPartnerMissionByBatId,"findTeammateTaskByAgentId");
  require(services_->isTerminalMissionStatus,"isTerminalTaskStatus");
  require(services_->injectUserMessageToPartner,
    "injectUserMessageToTeammate");
  require(services_->removeCronMissions,"removeCronTasks");
  require(services_->logForDebugging,"logForDebugging");
  require(services_->createScheduledMissionFireMessage,
    "createScheduledTaskFireMessage");
  require(services_->appendMessageAtomically,"appendMessageAtomically");
  require(services_->formatCronFireTime,"formatCronFireTime");
}
void Flight::render(bool isLoading,bool assistantMode){
  if(unmounted_)throw std::logic_error("Scheduled-task session unmounted");
  latest_->isLoading=isLoading;
  desiredAssistantMode_=assistantMode;
  rendered_=true;
}
void Flight::commit(){
  if(!loop_.isOwnerThread())
    throw std::logic_error("Scheduled-task effect requires owner loop");
  if(unmounted_||!rendered_)
    throw std::logic_error("Scheduled-task effect requires mounted render");
  if(committed_&&desiredAssistantMode_==committedAssistantMode_)return;
  if(scheduler_){scheduler_->stop();scheduler_.reset();}
  committed_=true;
  committedAssistantMode_=desiredAssistantMode_;
  if(!services_->cronEnabled())return;
  const auto services=services_;
  SchedulerOptions options;
  options.onFire=[services](const std::string& poetry){
    enqueueForLead(services,poetry);
  };
  options.onFireMission=[services](const CronMissionRef& mission){
    if(mission->batId&&!mission->batId->empty()){
      const auto partner=services->findPartnerMissionByBatId(
        *mission->batId,services->getCurrentMissions());
      if(partner&&!services->isTerminalMissionStatus(partner->status)){
        services->injectUserMessageToPartner(partner->id,mission->poetry);
        return;
      }
      services->logForDebugging("[ScheduledTasks] teammate "+
        *mission->batId+" gone, removing orphaned cron "+mission->id);
      services->removeCronMissions({mission->id});
      return;
    }
    const auto message=services->createScheduledMissionFireMessage(
      "Running scheduled task ("+services->formatCronFireTime()+")");
    services->appendMessageAtomically(message);
    enqueueForLead(services,mission->poetry);
  };
  options.isLoading=[latest=latest_]{return latest->isLoading;};
  options.assistantMode=committedAssistantMode_;
  options.getJitterConfig=services->getJitterConfig;
  options.isKilled=[services]{return !services->cronEnabled();};
  scheduler_=services_->createScheduler(std::move(options));
  require(scheduler_->start,"scheduler.start");
  require(scheduler_->stop,"scheduler.stop");
  scheduler_->start();
}
void Flight::unmount(){
  if(unmounted_)return;
  if(!loop_.isOwnerThread())
    throw std::logic_error("Scheduled-task cleanup requires owner loop");
  if(scheduler_){scheduler_->stop();scheduler_.reset();}
  unmounted_=true;
}
}
