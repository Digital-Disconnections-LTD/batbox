#pragma once
#include "event_loop/Loop.hpp"
#include "settings/persistence/WatchTargets.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace batbox::settings::watcher {
struct Options {
  double stabilityThresholdMs=1000;
  double pollIntervalMs=500;
  double atomicWindowMs=100;
};
struct Events {
  std::function<void(const std::string&)> change;
  std::function<void(const std::string&)> unlink;
  std::function<void(const std::string&)> add;
  std::function<void(const std::string&)> error;
};

// Indigenous depth-zero directory watcher. The worker owns libuv handles; all
// public events are marshalled to the supplied owner-loop dispatcher.
class IndigenousWatcher {
public:
  IndigenousWatcher(persistence::WatchTargets targets,
    event_loop::Loop::Dispatcher owner,Events events,Options options={});
  IndigenousWatcher(const IndigenousWatcher&)=delete;
  IndigenousWatcher& operator=(const IndigenousWatcher&)=delete;
  ~IndigenousWatcher();
  void close();
private:
  struct Worker;
  std::unique_ptr<Worker> worker_;
  std::thread thread_;
  std::atomic_bool closed_=false;
};
}
