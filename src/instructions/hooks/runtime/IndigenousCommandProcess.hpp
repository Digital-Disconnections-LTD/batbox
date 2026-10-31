#pragma once
#include "instructions/hooks/runtime/OutsideRepl.hpp"
#include <map>

namespace batbox::instructions::hooks::runtime {
struct IndigenousProcessRequest {
  std::string shellPath,command,cwd,input,shellType="bash";
  std::map<std::string,std::string> environment;
  std::stop_token signal;
  double timeoutMs=0;
};
event_loop::Deferred<CommandOutcome> spawnHookProcess(
  event_loop::Loop& loop,IndigenousProcessRequest request);
} // namespace batbox::instructions::hooks::runtime
