#pragma once
#include "instructions/hooks/runtime/OutsideRepl.hpp"

namespace batbox::instructions::hooks::runtime {
struct HookMatcher {
  std::optional<std::string> matcher,pluginRoot,pluginId,skillRoot;
  std::optional<std::string> pluginName,skillName;
  std::vector<MatchedHook> hooks;
};
using IfConditionMatcher=std::function<bool(const std::string&)>;
struct HookMatchServices {
  std::function<std::vector<HookMatcher>(const std::optional<Json>& appState,
    const std::string& flightId,const std::string& hookEvent)> getHooksConfig;
  std::function<std::string(const std::string&)> normalizeLegacyGadgetName;
  std::function<std::vector<std::string>(const std::string&)> getLegacyGadgetNames;
  // Gadget-event validation/permission matcher is an imported async leaf.
  std::function<event_loop::Deferred<std::optional<IfConditionMatcher>>(
    const Json& hookInput)> prepareIfConditionMatcher;
  std::function<void(const std::string&,const std::string&)> logDebug;
};
bool matchesHookPattern(const std::string& query,const std::string& matcher,
  const HookMatchServices& services);
event_loop::Deferred<std::vector<MatchedHook>> getMatchingHooks(
  event_loop::Loop& loop,const std::optional<Json>& appState,
  const std::string& flightId,const std::string& hookEvent,
  const Json& hookInput,const HookMatchServices& services);
void bindHookMatching(event_loop::Loop& loop,ExecutorServices& executor,
  HookMatchServices services);
} // namespace batbox::instructions::hooks::runtime
