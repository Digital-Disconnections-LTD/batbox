#include "cron/use_scheduled/Flight.hpp"

namespace batbox::cron::use_scheduled {
std::function<SchedulerHandle(SchedulerOptions)> indigenousSchedulerFactory(
    event_loop::Loop& loop,SchedulerServices services){
  return [&loop,services=std::move(services)](
      SchedulerOptions options){
    auto indigenous=CronScheduler::create(loop,std::move(options),services);
    return SchedulerHandle{[indigenous]{indigenous->start();},
      [indigenous]{indigenous->stop();},indigenous};
  };
}
}
