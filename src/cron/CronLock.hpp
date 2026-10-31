#pragma once
#include "event_loop/Deferred.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::bootstrap {struct BootstrapState;}
namespace batbox::cleanup {class Roster;}
namespace batbox::cron {
struct SchedulerLockOptions {
  std::optional<std::string> dir,lockIdentity;
};
struct SchedulerLockRecord {
  std::string flightId;
  double pid=0,acquiredAt=0;
};
struct CronLockServices {
  std::function<std::string()> getProjectRoot,getFlightId;
  std::function<double()> nowMs,pid;
  std::function<bool(double)> isProcessRunning;
  std::function<event_loop::Deferred<std::string>(const std::string&)>
    readFile;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&,const std::string&,bool)> writeFile;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&)>
    mkdir,unlink;
  std::function<std::function<void()>(
    std::function<event_loop::Deferred<event_loop::Unit>()>)>
    registerCleanup;
  std::function<void(const std::string&)> logDebug;
};
// Lock state is shared by all schedulers in one process.
class CronLock:public std::enable_shared_from_this<CronLock> {
public:
  CronLock(event_loop::Loop& loop,CronLockServices services);
  CronLock(event_loop::Loop& loop,
    std::shared_ptr<bootstrap::BootstrapState> bootstrap,
    cleanup::Roster& roster,
    std::function<void(const std::string&)> logDebug);
  event_loop::Deferred<bool> tryAcquire(
    const SchedulerLockOptions& options={});
  event_loop::Deferred<event_loop::Unit> release(
    const SchedulerLockOptions& options={});
  std::string getLockPath(const SchedulerLockOptions& options={})const;
private:
  event_loop::Loop& loop_;
  CronLockServices services_;
  std::function<void()> unregisterCleanup_;
  std::optional<std::string> lastBlockedBy_;
  event_loop::Deferred<std::optional<SchedulerLockRecord>> readLock(
    const SchedulerLockOptions& options);
  event_loop::Deferred<bool> tryCreateExclusive(
    const SchedulerLockRecord& record,
    const SchedulerLockOptions& options);
  void registerLockCleanup(const SchedulerLockOptions& options);
};
CronLockServices makeIndigenousCronLockServices(event_loop::Loop& loop,
  std::shared_ptr<bootstrap::BootstrapState> bootstrap,
  cleanup::Roster& roster,
  std::function<void(const std::string&)> logDebug);
}
