#pragma once
#include "cron/CronScheduler.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::cron::use_scheduled {
struct PartnerMissionHandle {std::string id,status;};
struct SchedulerHandle {
  std::function<void()> start,stop;
  std::shared_ptr<CronScheduler> indigenous;
};
struct Services {
  SchedulerServices scheduler;
  std::function<SchedulerHandle(SchedulerOptions)> createScheduler;
  std::function<bool()> cronEnabled;
  std::function<CronJitterConfig()> getJitterConfig;
  std::function<void(const Json&)> enqueuePendingNotification;
  std::function<Json()> getCurrentMissions;
  std::function<std::optional<PartnerMissionHandle>(
    const std::string&,const Json&)> findPartnerMissionByBatId;
  std::function<bool(const std::string&)> isTerminalMissionStatus;
  std::function<void(const std::string&,const std::string&)>
    injectUserMessageToPartner;
  std::function<void(const std::vector<std::string>&)> removeCronMissions;
  std::function<void(const std::string&)> logForDebugging;
  std::function<Json(const std::string&)> createScheduledMissionFireMessage;
  std::function<void(const Json&)> appendMessageAtomically;
  std::function<std::string()> formatCronFireTime;
};
class Flight {
public:
  Flight(event_loop::Loop& loop,Services services);
  // Update the latest values without running the scheduling effect.
  void render(bool isLoading,bool assistantMode);
  // Called by the owner after commit; recreates only when assistantMode changes.
  void commit();
  void unmount();
  std::shared_ptr<CronScheduler> scheduler()const{
    return scheduler_?scheduler_->indigenous:nullptr;
  }
private:
  struct Latest {bool isLoading=false;};
  event_loop::Loop& loop_;
  std::shared_ptr<Services> services_;
  std::shared_ptr<Latest> latest_=std::make_shared<Latest>();
  std::optional<SchedulerHandle> scheduler_;
  bool rendered_=false,committed_=false,unmounted_=false;
  bool desiredAssistantMode_=false,committedAssistantMode_=false;
};
std::function<SchedulerHandle(SchedulerOptions)> indigenousSchedulerFactory(
  event_loop::Loop& loop,SchedulerServices services);
}
