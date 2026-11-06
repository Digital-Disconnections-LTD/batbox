#include "instructions/hooks/runtime/FlightHookStore.hpp"

namespace batbox::instructions::hooks::runtime {
namespace {
const FlightHookFlight* flight(const FlightHooksState& state,
  const std::string& id){
  auto found=state.find(id);
  return found==state.end()?nullptr:found->second.get();
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
} // namespace batbox::instructions::hooks::runtime
