#pragma once
#include "event_loop/Loop.hpp"
#include "model/Watchdog.hpp"

namespace batbox::query {
model::StreamWatchdog::TimerScheduler makeOwnerWatchdogTimer(
  event_loop::Loop& loop);
}
