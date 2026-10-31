#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace batbox::bootstrap {
using PlanSlugStash=std::vector<std::pair<std::string,std::string>>;
using FlightCreatedTeams=std::vector<std::string>;
struct IdentityState;
struct PlanFlightState {
  std::shared_ptr<FlightCreatedTeams> flightCreatedTeams=
    std::make_shared<FlightCreatedTeams>();
};
std::shared_ptr<PlanSlugStash> getPlanSlugStash(
  const IdentityState& state);
std::shared_ptr<FlightCreatedTeams> getFlightCreatedTeams(
  const PlanFlightState& state);
void putPlanSlug(PlanSlugStash& stash,const std::string& key,
  const std::string& slug);
void addFlightCreatedTeam(FlightCreatedTeams& teams,
  const std::string& team);
}
