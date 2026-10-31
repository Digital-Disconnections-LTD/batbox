#include "bootstrap/PlanFlightState.hpp"
#include "bootstrap/Identity.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    bootstrap::PlanFlightState state;
    bootstrap::IdentityState identity;
    identity.flightId="session-1";
    const auto stash=bootstrap::getPlanSlugStash(identity);
    const auto teams=bootstrap::getFlightCreatedTeams(state);
    auto snap=[&](){
      Json entries=Json::array();
      for(const auto& [key,value]:*bootstrap::getPlanSlugStash(identity))
        entries.push_back(Json::array({key,value}));
      Json names=Json::array();
      for(const auto& name:*bootstrap::getFlightCreatedTeams(state))
        names.push_back(name);
      return Json{{"cache",entries},{"sameCache",
        bootstrap::getPlanSlugStash(identity)==stash},{"teams",names},
        {"sameTeams",bootstrap::getFlightCreatedTeams(state)==teams}};
    };
    Json trace=Json::array();trace.push_back(snap());
    if(test.contains("ops"))for(const auto& op:test["ops"]){
      const auto kind=op["kind"].get<std::string>();
      if(kind=="slug")bootstrap::putPlanSlug(*bootstrap::getPlanSlugStash(
        identity),op["key"].get<std::string>(),op["value"].get<std::string>());
      else if(kind=="team")bootstrap::addFlightCreatedTeam(
        *bootstrap::getFlightCreatedTeams(state),
        op["value"].get<std::string>());
      else if(kind=="clearSlugs")bootstrap::getPlanSlugStash(identity)->clear();
      else if(kind=="clearTeams")
        bootstrap::getFlightCreatedTeams(state)->clear();
      else if(kind=="switch")bootstrap::switchFlight(identity,
        op["id"].get<std::string>());
      else throw std::runtime_error("Unknown plan-session operation");
      trace.push_back(snap());
    }
    std::cout<<value::stringify(trace)<<'\n';
  }
}
