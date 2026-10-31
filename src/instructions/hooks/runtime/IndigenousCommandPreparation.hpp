#pragma once
#include "instructions/hooks/runtime/IndigenousCommandFull.hpp"
#include "instructions/hooks/runtime/IndigenousCommandProcess.hpp"

namespace batbox::instructions::hooks::runtime {
event_loop::Deferred<IndigenousProcessRequest> prepareIndigenousHookCommand(
  event_loop::Loop& loop,const IndigenousCommandConfig& config,
  const FullCommandRequest& request);
} // namespace batbox::instructions::hooks::runtime
