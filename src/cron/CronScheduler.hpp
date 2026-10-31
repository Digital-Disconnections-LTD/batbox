#pragma once
#include "cron/CronJitter.hpp"
#include "event_loop/Deferred.hpp"
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace batbox::cron {
struct SchedulerTimer {
  virtual ~SchedulerTimer()=default;
  virtual void cancel()=0;
};
struct SchedulerWatcher {
  virtual ~SchedulerWatcher()=default;
  virtual void close()=0;
};
struct SchedulerOptions {
  std::function<void(const std::string&)> onFire;
  std::function<bool()> isLoading;
  bool assistantMode=false;
  std::function<void(const CronMissionRef&)> onFireMission;
  std::function<void(std::vector<CronMissionRef>&)> onMissed;
  std::optional<std::string> dir,lockIdentity;
  std::function<CronJitterConfig()> getJitterConfig;
  std::function<bool()> isKilled;
  std::function<bool(const CronMissionRef&)> filter;
};
struct SchedulerServices {
  std::function<double()> nowMs;
  std::function<event_loop::Deferred<event_loop::Unit>()> loadWatcher;
  std::function<event_loop::Deferred<std::vector<CronMissionRef>>(
    const std::optional<std::string>&)> readMissions;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::vector<std::string>&,const std::optional<std::string>&)>
    removeMissions;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::vector<std::string>&,double,
    const std::optional<std::string>&)> markFired;
  std::function<bool(const std::optional<std::string>&)> hasMissionsSync;
  std::function<std::string(const std::optional<std::string>&)> filePath;
  std::function<std::vector<CronMissionRef>()> getFlightMissions;
  std::function<void(const std::vector<std::string>&)> removeFlightMissions;
  std::function<bool()> getEnabled;
  std::function<void(bool)> setEnabled;
  std::function<event_loop::Deferred<bool>(const std::optional<std::string>&,
    const std::optional<std::string>&)> acquireLock;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::optional<std::string>&,const std::optional<std::string>&)>
    releaseLock;
  std::function<std::shared_ptr<SchedulerWatcher>(const std::string&,
    std::function<void()>,std::function<void()>,std::function<void()>)>
    watchFile;
  std::function<std::shared_ptr<SchedulerTimer>(double,
    std::function<void()>)> interval;
  std::function<void(const std::string&)> logDebug;
  std::function<std::string(double)> isoTime;
  std::function<std::string(const std::string&)> cronToHuman;
  std::function<std::string(double)> localDateTime;
};
bool isRecurringMissionAged(const CronMission& mission,double nowMs,
  double maxAgeMs);
std::string buildMissedMissionNotification(
  const std::vector<CronMissionRef>& missed,
  const std::function<std::string(const std::string&)>& cronToHuman,
  const std::function<std::string(double)>& localDateTime);

class CronScheduler:public std::enable_shared_from_this<CronScheduler> {
public:
  static std::shared_ptr<CronScheduler> create(event_loop::Loop& loop,
    SchedulerOptions options,SchedulerServices services);
  void start();
  void stop();
  std::optional<double> getNextFireTime()const;
  // Owner-loop entry points also serve deterministic scheduler fixtures.
  void check();
  void load(bool initial);
private:
  CronScheduler(event_loop::Loop& loop,SchedulerOptions options,
    SchedulerServices services);
  void enable();
  void beginAfterWatcherImport();
  void beginAfterLock(bool owner);
  void probeLock();
  void logFailure(const std::string& operation,std::exception_ptr error);
  event_loop::Loop& loop_;
  SchedulerOptions options_;
  SchedulerServices services_;
  std::vector<CronMissionRef> missions_;
  std::map<std::string,double> nextFireAt_;
  std::set<std::string> missedAsked_,inFlight_;
  std::shared_ptr<SchedulerTimer> enablePoll_,checkTimer_,lockProbeTimer_;
  std::shared_ptr<SchedulerWatcher> watcher_;
  bool stopped_=false,isOwner_=false;
};
}
