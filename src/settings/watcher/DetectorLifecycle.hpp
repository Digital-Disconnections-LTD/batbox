#pragma once
#include "event_loop/Loop.hpp"
#include "settings/persistence/WatchTargets.hpp"
#include "settings/watcher/IndigenousWatcher.hpp"
#include <functional>
#include <future>
#include <optional>
#include <atomic>
#include <memory>

namespace batbox::settings::watcher {
struct TimingOverrides {
  std::optional<double> stabilityThreshold,pollInterval,mdmPollInterval,
    deletionGrace;
};
using WatcherClose=std::function<void(std::function<void()>)>;
struct DetectorServices {
  event_loop::Loop::Dispatcher owner;
  std::function<bool()> isRemoteMode;
  std::function<void(double)> startMdmPoll;
  std::function<void()> stopMdmPoll;
  std::function<void(std::function<std::future<void>()>)> registerCleanup;
  // Completion must eventually run; the detector queues the continuation as an
  // owner-loop microtask.
  std::function<void(std::function<void(
    std::optional<persistence::WatchTargets>,std::exception_ptr)>)>
      getWatchTargets;
  std::function<WatcherClose(const persistence::WatchTargets&,
    const Options&,const Events&)> openWatcher;
  Events events;
  std::function<void(const std::string&)> debug;
  std::function<void()> clearPendingDeletions;
  std::function<void()> clearInternalWrites;
  std::function<void()> clearSignal;
  std::function<void(double)> setDeletionGrace;
};

class DetectorLifecycle {
public:
  explicit DetectorLifecycle(DetectorServices services);
  ~DetectorLifecycle();
  std::future<void> initialize();
  std::future<void> dispose();
  std::future<void> resetForTesting(
    std::optional<TimingOverrides> overrides=std::nullopt);
  bool initialized()const{return initialized_;}
  bool disposed()const{return disposed_;}
private:
  DetectorServices services_;
  std::optional<WatcherClose> watcherClose_;
  std::optional<TimingOverrides> overrides_;
  bool initialized_=false,disposed_=false;
  std::shared_ptr<std::atomic_bool> alive_=
    std::make_shared<std::atomic_bool>(true);
  static std::future<void> readyFuture();
  std::future<void> closeWatcher();
};

// Production libuv binding for openWatcher. The indigenous watcher closes and
// joins its worker before calling completion.
WatcherClose openIndigenousWatcher(const persistence::WatchTargets& targets,
  const event_loop::Loop::Dispatcher& owner,const Events& events,
  const Options& options);
std::function<void(std::function<void(
  std::optional<persistence::WatchTargets>,std::exception_ptr)>)>
    bindIndigenousWatchTargets(Paths& paths,
      const event_loop::Loop::Dispatcher& owner);
}
