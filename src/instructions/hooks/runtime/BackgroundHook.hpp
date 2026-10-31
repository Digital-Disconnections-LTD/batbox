#pragma once
#include "instructions/hooks/runtime/AsyncHookRoster.hpp"

namespace batbox::instructions::hooks::runtime {
struct BackgroundHookRequest {
  std::string processId,hookId,hookEvent,hookName,command;
  std::optional<std::string> pluginId;
  Json asyncResponse;
  bool asyncRewake=false;
  std::shared_ptr<AsyncHookProcess> process;
  std::function<bool(const std::string&)> background;
};
struct BackgroundHookServices {
  std::function<event_loop::Deferred<event_loop::Unit>()> yieldToIo;
  std::function<void(const AsyncHookResponseEvent&)> emitHookResponse;
  std::function<std::string(const std::string&)> wrapInSystemReminder;
  std::function<void(const std::string&,const std::string&)> enqueuePendingNotification;
  std::function<void(std::exception_ptr)> onDetachedError;
};
bool executeInBackground(event_loop::Loop& loop,AsyncHookRoster& roster,
  BackgroundHookRequest request,const BackgroundHookServices& services);
} // namespace batbox::instructions::hooks::runtime
