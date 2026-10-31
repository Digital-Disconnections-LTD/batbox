#pragma once
#include "locking/ProperLock.hpp"

namespace batbox::locking {
struct CleanupCallbacks {
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&)> lock,unlock;
};
CleanupCallbacks makeCleanupCallbacks(event_loop::Loop& loop,Manager& manager);
}
