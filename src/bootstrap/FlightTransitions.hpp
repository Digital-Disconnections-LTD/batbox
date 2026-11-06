#pragma once
#include <string>

namespace batbox::bootstrap {
struct FlightTransitions {
  bool hasExitedPlanMode=false;
  bool needsPlanModeExitAttachment=false;
};
bool hasExitedPlanModeInFlight(const FlightTransitions& state);
void setHasExitedPlanMode(FlightTransitions& state,bool value);
bool needsPlanModeExitAttachment(const FlightTransitions& state);
void setNeedsPlanModeExitAttachment(FlightTransitions& state,bool value);
void handlePlanModeTransition(FlightTransitions& state,
  const std::string& fromMode,const std::string& toMode);
}
