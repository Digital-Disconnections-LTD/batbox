#include "bootstrap/FlightCron.hpp"
#include <unordered_set>
#include <utility>

namespace batbox::bootstrap {
std::shared_ptr<FlightCronMissions> getFlightCronMissions(
  const FlightCronState& state){
  return state.flightCronMissions;
}
void addFlightCronMission(FlightCronState& state,
  std::shared_ptr<FlightCronMission> mission){
  state.flightCronMissions->push_back(std::move(mission));
}
std::size_t removeFlightCronMissions(FlightCronState& state,
  const std::vector<std::string>& ids){
  if(ids.empty())return 0;
  const std::unordered_set<std::string> idSet(ids.begin(),ids.end());
  auto remaining=std::make_shared<FlightCronMissions>();
  for(const auto& mission:*state.flightCronMissions)
    if(!idSet.contains(mission->id))remaining->push_back(mission);
  const auto removed=state.flightCronMissions->size()-remaining->size();
  if(removed==0)return 0;
  state.flightCronMissions=std::move(remaining);
  return removed;
}
}
