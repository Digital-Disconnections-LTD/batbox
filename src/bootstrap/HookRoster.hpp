#pragma once
#include "bootstrap/FlightTransitions.hpp"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace batbox::instructions::hooks::runtime {struct HookMatcher;}
namespace batbox::bootstrap {
using RegisteredHookList=std::vector<std::shared_ptr<
  instructions::hooks::runtime::HookMatcher>>;
using RegisteredHookMap=std::vector<std::pair<std::string,
  RegisteredHookList>>;
struct HookRosterState {
  std::shared_ptr<RegisteredHookMap> registeredHooks;
};
void registerHookCallbacks(HookRosterState& state,
  const RegisteredHookMap& hooks);
std::shared_ptr<RegisteredHookMap> getRegisteredHooks(
  const HookRosterState& state);
void clearRegisteredHooks(HookRosterState& state);
void clearRegisteredPluginHooks(HookRosterState& state);
void resetSdkInitState(HookRosterState& hooks,
  FlightTransitions& transitions);
}
