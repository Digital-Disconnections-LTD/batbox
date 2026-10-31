#include "bootstrap/PlanFlightState.hpp"
#include "bootstrap/Identity.hpp"
#include <algorithm>

namespace batbox::bootstrap {
std::shared_ptr<PlanSlugStash> getPlanSlugStash(
  const IdentityState& state){return state.planSlugStash;}
std::shared_ptr<FlightCreatedTeams> getFlightCreatedTeams(
  const PlanFlightState& state){return state.flightCreatedTeams;}
void putPlanSlug(PlanSlugStash& stash,const std::string& key,
  const std::string& slug){
  for(auto& [existingKey,value]:stash)
    if(existingKey==key){value=slug;return;}
  stash.emplace_back(key,slug);
}
void addFlightCreatedTeam(FlightCreatedTeams& teams,
  const std::string& team){
  if(std::find(teams.begin(),teams.end(),team)==teams.end())
    teams.push_back(team);
}
}
