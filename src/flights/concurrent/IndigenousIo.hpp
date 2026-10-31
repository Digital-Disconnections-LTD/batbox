#pragma once
#include "flights/concurrent/Roster.hpp"
#include "fs_operations/NodeFsAsyncIndigenous.hpp"
#include "cleanup/Roster.hpp"

namespace batbox::flights::concurrent {
// Supplies real POSIX async filesystem imports, JSON/runtime conversion, cleanup,
// clock and process liveness. Bootstrap state/env/feature imports remain explicit.
Services bindIndigenousIo(Services services,
  std::shared_ptr<fs_operations::IndigenousAsyncFsHost> host,cleanup::Roster& cleanup);
}
