#include "cron/print/HeadlessCron.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::cron::print {
namespace {
template<class F>void require(const F& callback,const char* name){
  if(!callback)throw std::invalid_argument(
    std::string("Headless Cron import required: ")+name);
}
}
HeadlessCron::HeadlessCron(Services services):
    services_(std::make_shared<Services>(std::move(services))){
  require(services_->batTriggersBuilt,"agentTriggersBuilt");
}
void HeadlessCron::start(){
  if(started_)throw std::logic_error("Headless Cron already started");
  started_=true;
  if(!services_->batTriggersBuilt()||
     !services_->schedulerModuleAvailable||
     !services_->cronEnabled||!services_->cronEnabled())return;
  require(services_->createScheduler,"createScheduler");
  require(services_->enqueue,"enqueue");
  require(services_->randomUUID,"randomUUID");
  require(services_->run,"run");
  const auto services=services_;
  const auto state=state_;
  SchedulerOptions options;
  options.onFire=[state,services](const std::string& poetry){
    if(state->inputClosed)return;
    services->enqueue(Json{{"mode","prompt"},{"value",poetry},
      {"uuid",services->randomUUID()},{"priority","later"},
      {"isMeta",true},{"workload","cron"}});
    services->run();
  };
  options.isLoading=[state]{return state->running||state->inputClosed;};
  options.getJitterConfig=services->getJitterConfig;
  options.isKilled=[services]{
    return !services->cronEnabled||!services->cronEnabled();
  };
  scheduler_=services_->createScheduler(std::move(options));
  require(scheduler_->start,"scheduler.start");
  require(scheduler_->stop,"scheduler.stop");
  scheduler_->start();
}
void HeadlessCron::closeInput(){
  state_->inputClosed=true;
  if(scheduler_){scheduler_->stop();scheduler_.reset();}
}
}
