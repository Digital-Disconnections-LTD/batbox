#pragma once
#include "event_loop/Loop.hpp"
#include <functional>
#include <memory>
#include <optional>

namespace batbox::lodash_throttle {
struct Timer {
  std::function<void()> cancel;
};
struct Services {
  std::function<double()> now;
  std::function<Timer(double,std::function<void()>)> setTimeout;
};
Services indigenousServices(event_loop::Loop& loop);

// lodash-es throttle/debounce behavior: void callback, no arguments, leading
// and trailing enabled, and maxWait equal to wait.
class Throttle:public std::enable_shared_from_this<Throttle> {
public:
  Throttle(std::function<void()> callback,double wait,Services services);
  void call();
  void cancel();
  void flush();
private:
  void invokeFunc(double time);
  void leadingEdge(double time);
  double remainingWait(double time)const;
  bool shouldInvoke(double time)const;
  void timerExpired();
  void trailingEdge(double time);
  void schedule(double delay);
  std::function<void()> callback_;
  Services services_;
  double wait_=0,maxWait_=0,lastInvokeTime_=0;
  bool lastArgs_=false;
  std::optional<double> lastCallTime_;
  std::optional<Timer> timerId_;
};
using ThrottleRef=std::shared_ptr<Throttle>;
} // namespace batbox::lodash_throttle
