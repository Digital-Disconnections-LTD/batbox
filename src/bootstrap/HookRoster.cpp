#include "bootstrap/HookRoster.hpp"
#include "instructions/hooks/runtime/HookMatching.hpp"
#include <algorithm>
#include <utility>

namespace batbox::bootstrap {
void registerHookCallbacks(HookRosterState& state,
  const RegisteredHookMap& hooks){
  if(!state.registeredHooks)
    state.registeredHooks=std::make_shared<RegisteredHookMap>();
  for(const auto& [event,matchers]:hooks){
    auto found=std::find_if(state.registeredHooks->begin(),
      state.registeredHooks->end(),[&](const auto& item){
        return item.first==event;
      });
    if(found==state.registeredHooks->end()){
      state.registeredHooks->emplace_back(event,RegisteredHookList{});
      found=std::prev(state.registeredHooks->end());
    }
    found->second.insert(found->second.end(),matchers.begin(),
      matchers.end());
  }
}
std::shared_ptr<RegisteredHookMap> getRegisteredHooks(
  const HookRosterState& state){return state.registeredHooks;}
void clearRegisteredHooks(HookRosterState& state){
  state.registeredHooks.reset();
}
void clearRegisteredPluginHooks(HookRosterState& state){
  if(!state.registeredHooks)return;
  auto filtered=std::make_shared<RegisteredHookMap>();
  for(const auto& [event,matchers]:*state.registeredHooks){
    RegisteredHookList callbackHooks;
    for(const auto& matcher:matchers)
      if(!matcher->pluginRoot)callbackHooks.push_back(matcher);
    if(!callbackHooks.empty())
      filtered->emplace_back(event,std::move(callbackHooks));
  }
  state.registeredHooks=filtered->empty()?nullptr:std::move(filtered);
}
void resetSdkInitState(HookRosterState& hooks,
  FlightTransitions& transitions){
  transitions.initJsonSchema.reset();
  hooks.registeredHooks.reset();
}
}
