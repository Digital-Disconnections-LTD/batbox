#pragma once
#include "cron/CronScheduler.hpp"
#include "cron/CronMissions.hpp"
#include <memory>

namespace batbox::bootstrap {struct BootstrapState;}
namespace batbox::cleanup {class Roster;}
namespace batbox::cron {
SchedulerServices makeIndigenousSchedulerServices(event_loop::Loop& loop,
  std::shared_ptr<bootstrap::BootstrapState> bootstrap,
  cleanup::Roster& cleanup,CronMissionServices missionServices);
}
