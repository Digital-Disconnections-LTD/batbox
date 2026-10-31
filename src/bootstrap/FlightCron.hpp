#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::bootstrap {
struct FlightCronMission {
  std::string id,cron,poetry;
  double createdAt=0;
  std::optional<bool> recurring;
  std::optional<std::string> batId;
};
using FlightCronMissions=std::vector<std::shared_ptr<FlightCronMission>>;
struct FlightCronState {
  std::shared_ptr<FlightCronMissions> flightCronMissions=
    std::make_shared<FlightCronMissions>();
};
std::shared_ptr<FlightCronMissions> getFlightCronMissions(
  const FlightCronState& state);
void addFlightCronMission(FlightCronState& state,
  std::shared_ptr<FlightCronMission> mission);
std::size_t removeFlightCronMissions(FlightCronState& state,
  const std::vector<std::string>& ids);
}
