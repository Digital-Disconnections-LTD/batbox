#pragma once
#include "event_loop/Deferred.hpp"
#include "shell/CommandHandle.hpp"

namespace batbox::shell {
// Bridge the managed child completion onto the owner event loop. The worker
// waits on the existing Indigenous future; the observer runs as an
// owner microtask. A referenced KeepAlive covers the worker wait.
event_loop::Deferred<ExecResult> resultOnLoop(event_loop::Loop& loop,
  std::shared_future<ExecResult> result);
event_loop::Deferred<ExecResult> resultOnLoop(event_loop::Loop& loop,
  const CommandHandle& handle);
} // namespace batbox::shell
