#include "bootstrap/FlightTransitions.hpp"

namespace batbox::bootstrap {
bool hasExitedPlanModeInFlight(const FlightTransitions& state){
  return state.hasExitedPlanMode;
}
void setHasExitedPlanMode(FlightTransitions& state,bool value){
  state.hasExitedPlanMode=value;
}
bool needsPlanModeExitAttachment(const FlightTransitions& state){
  return state.needsPlanModeExitAttachment;
}
void setNeedsPlanModeExitAttachment(FlightTransitions& state,bool value){
  state.needsPlanModeExitAttachment=value;
}
void handlePlanModeTransition(FlightTransitions& state,
  const std::string& fromMode,const std::string& toMode){
  if(toMode=="plan"&&fromMode!="plan")
    state.needsPlanModeExitAttachment=false;
  if(fromMode=="plan"&&toMode!="plan")
    state.needsPlanModeExitAttachment=true;
}
}
