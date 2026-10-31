#pragma once
#include "instructions/hooks/runtime/HookAbort.hpp"
#include "instructions/hooks/runtime/OutsideRepl.hpp"

namespace batbox::instructions::hooks::runtime {
struct CommandLeafServices {
  std::function<bool()> shouldDisableAllHooksIncludingManaged,
    shouldSkipHookDueToTrust,shouldAllowManagedHooksOnly;
  std::function<std::optional<Json>()> getPolicySettings,getSettings;
  std::function<std::string()> randomUUID;
  std::function<event_loop::Deferred<CommandOutcome>(CommandRequest)>
    execCommandHook;
  std::function<void(const std::string&,const std::string&)> logForDebugging;
  std::function<std::string(std::exception_ptr)> stringifyThrown;
};
event_loop::Deferred<std::optional<std::string>> executeStatusLineCommand(
  event_loop::Loop& loop,const CommandLeafServices& services,
  const Json& statusLineInput,std::optional<std::stop_token> signal=std::nullopt,
  double timeoutMs=5000,bool logResult=false);
event_loop::Deferred<std::vector<std::string>> executeFileSuggestionCommand(
  event_loop::Loop& loop,const CommandLeafServices& services,
  const Json& fileSuggestionInput,std::optional<std::stop_token> signal=std::nullopt,
  double timeoutMs=5000);
} // namespace batbox::instructions::hooks::runtime
