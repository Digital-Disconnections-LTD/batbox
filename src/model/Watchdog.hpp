#pragma once

#include "model/Lifecycle.hpp"
#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace batbox::model {

// Active timer for the stream idle watchdog. It runs independently of
// the HTTP producer and generator consumer, including while a yield is paused.
class StreamWatchdog {
 public:
  using TimerScheduler=std::function<std::function<void()>(double,std::function<void()>)>;
  StreamWatchdog(WatchdogSettings settings, std::function<void()> abort,
    std::function<void()> warning={},std::function<void()> timeout={},
    TimerScheduler scheduler={});
  ~StreamWatchdog();
  StreamWatchdog(const StreamWatchdog&) = delete;
  StreamWatchdog& operator=(const StreamWatchdog&) = delete;
  void reset();
  void clear() noexcept;
  bool aborted() const;
 private:
  void run();
  void fireWarning(std::uint64_t generation);
  void fireTimeout(std::uint64_t generation);
  WatchdogSettings settings_;
  std::function<void()> abort_;
  std::function<void()> warning_,timeout_;
  TimerScheduler scheduler_;
  std::function<void()> cancelWarning_,cancelTimeout_;
  mutable std::mutex mutex_;
  std::condition_variable changed_;
  std::thread timer_;
  std::chrono::steady_clock::time_point deadline_;
  std::chrono::steady_clock::time_point warningDeadline_;
  std::uint64_t generation_ = 0;
  bool armed_ = false;
  bool stopped_ = false;
  bool aborted_ = false;
  bool warned_ = false;
};

} // namespace batbox::model
