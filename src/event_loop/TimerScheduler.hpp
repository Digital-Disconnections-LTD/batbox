#pragma once
#include "event_loop/Loop.hpp"
#include "timers/Timers.hpp"

namespace batbox::event_loop {
// Binds sleep and timeout scheduling to the owner loop.
// schedule must be called on the loop owner; cancel may cross threads.
class TimerScheduler final:public timers::TimerScheduler {
public:
  explicit TimerScheduler(Loop& loop):loop_(loop){}
  std::shared_ptr<timers::TimerHandle> schedule(double ms,Callback callback,bool unref) override;
private:
  Loop& loop_;
};
} // namespace batbox::event_loop
