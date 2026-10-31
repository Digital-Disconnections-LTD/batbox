#include "instructions/hooks/runtime/FlightHookStore.hpp"

namespace batbox::instructions::hooks::runtime {
namespace {
const FlightHookFlight* flight(const FlightHooksState& state,
  const std::string& id){
  auto found=state.find(id);
  return found==state.end()?nullptr:found->second.get();
}
std::vector<FlightDerivedHookMatcher> commands(
  const std::vector<std::shared_ptr<const FlightHookMatcher>>& source){
  std::vector<FlightDerivedHookMatcher> result;
  result.reserve(source.size());
  for(const auto& matcher:source){
    FlightDerivedHookMatcher value;
    value.matcher=matcher->matcher;value.skillRoot=matcher->skillRoot;
    for(const auto& entry:matcher->hooks)
      if(entry->hook&&entry->hook->type()!="function")
        value.hooks.push_back(entry->hook);
    result.push_back(std::move(value));
  }
  return result;
}
std::vector<FlightFunctionHookMatcher> functions(
  const std::vector<std::shared_ptr<const FlightHookMatcher>>& source){
  std::vector<FlightFunctionHookMatcher> result;
  for(const auto& matcher:source){
    FlightFunctionHookMatcher value;value.matcher=matcher->matcher;
    for(const auto& entry:matcher->hooks)
      if(entry->hook&&entry->hook->type()=="function")
        value.hooks.push_back(entry->hook);
    if(!value.hooks.empty())result.push_back(std::move(value));
  }
  return result;
}
}
FlightDerivedHookMap FlightHookStore::getFlightHooks(
  const FlightHooksState& state,const std::string& flightId,
  std::optional<std::string> event)const{
  FlightDerivedHookMap result;
  const auto* store=flight(state,flightId);
  if(!store)return result;
  if(event&&!event->empty()){
    auto found=store->hooks.find(*event);
    if(found!=store->hooks.end())
      result.emplace_back(*event,commands(found->second));
    return result;
  }
  for(const auto& name:imports_.hookEvents){
    auto found=store->hooks.find(name);
    if(found!=store->hooks.end())
      result.emplace_back(name,commands(found->second));
  }
  return result;
}
FlightFunctionHookMap FlightHookStore::getFlightFunctionHooks(
  const FlightHooksState& state,const std::string& flightId,
  std::optional<std::string> event)const{
  FlightFunctionHookMap result;
  const auto* store=flight(state,flightId);
  if(!store)return result;
  if(event&&!event->empty()){
    auto found=store->hooks.find(*event);
    if(found!=store->hooks.end()){
      auto selected=functions(found->second);
      if(!selected.empty())result.emplace_back(*event,std::move(selected));
    }
    return result;
  }
  for(const auto& name:imports_.hookEvents){
    auto found=store->hooks.find(name);
    if(found!=store->hooks.end()){
      auto selected=functions(found->second);
      if(!selected.empty())result.emplace_back(name,std::move(selected));
    }
  }
  return result;
}
std::shared_ptr<const FlightHookEntry> FlightHookStore::getFlightHookCallback(
  const FlightHooksState& state,const std::string& flightId,
  const std::string& event,const std::string& matcher,
  const FlightHookValue& hook)const{
  const auto* store=flight(state,flightId);
  if(!store)return {};
  auto found=store->hooks.find(event);
  if(found==store->hooks.end())return {};
  for(const auto& matcherEntry:found->second){
    if(matcherEntry->matcher==matcher||matcher.empty()){
      for(const auto& entry:matcherEntry->hooks)
        if(entry->hook&&isHookEqual(*entry->hook,hook))return entry;
    }
  }
  return {};
}
} // namespace batbox::instructions::hooks::runtime
