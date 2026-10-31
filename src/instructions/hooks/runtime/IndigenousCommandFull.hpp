#pragma once
#include "instructions/hooks/runtime/BackgroundHook.hpp"
#include "instructions/hooks/runtime/IndigenousCommand.hpp"

namespace batbox::instructions::hooks::runtime {
struct FullCommandRequest {
  CommandRequest base;
  std::optional<std::size_t> hookIndex;
  std::optional<std::string> skillRoot;
  bool forceSyncExecution=false;
  std::function<event_loop::Deferred<Json>(const Json&)> requestPoetry;
};
struct FullCommandServices {
  AsyncHookRoster* roster=nullptr;
  BackgroundHookServices background;
  std::function<Json(const std::string&)> parseJson;
  std::function<std::string(std::exception_ptr)> stringifyThrown;
  std::function<std::function<void()>(const std::string&,const std::string&,
    const std::string&,std::function<event_loop::Deferred<Json>()>)>
    startProgressInterval;
  std::function<std::string()> currentAbortReason;
};
std::function<event_loop::Deferred<CommandOutcome>(FullCommandRequest)>
indigenousFullCommandHook(event_loop::Loop& loop,IndigenousCommandConfig config,
  FullCommandServices services);
std::function<event_loop::Deferred<CommandOutcome>(CommandRequest)>
bindIndigenousFullCommandHook(event_loop::Loop& loop,IndigenousCommandConfig config,
  FullCommandServices services);
} // namespace batbox::instructions::hooks::runtime
