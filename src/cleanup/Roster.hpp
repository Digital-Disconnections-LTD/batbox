#pragma once
#include "event_loop/Deferred.hpp"
#include <functional>
#include <memory>

namespace batbox::cleanup {
using event_loop::Deferred;
using event_loop::Unit;
using CleanupFunction=std::function<Deferred<Unit>()>;
// Function identity is explicit because std::function has no equality. Reuse
// one shared CleanupFunction to match registering one JS function twice.
using CleanupIdentity=std::shared_ptr<CleanupFunction>;
class Roster {
public:
  explicit Roster(event_loop::Loop& loop);
  std::function<void()> registerCleanup(CleanupIdentity fn);
  std::function<void()> registerCleanup(CleanupFunction fn);
  Deferred<Unit> runCleanupFunctions();
  std::size_t size()const;
private:
  struct State;
  std::shared_ptr<State> state_;
};
} // namespace batbox::cleanup
