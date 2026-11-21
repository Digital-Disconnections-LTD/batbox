#pragma once
#include <string>

namespace batbox::bootstrap {
struct FlightTransitions {
  bool hasExitedPlanMode=false;
  bool needsPlanModeExitAttachment=false;
};
inline bool hasExitedPlanModeInFlight(const FlightTransitions& state){
  return state.hasExitedPlanMode;
}
inline void setHasExitedPlanMode(FlightTransitions& state,bool value){
  state.hasExitedPlanMode=value;
}
inline bool needsPlanModeExitAttachment(const FlightTransitions& state){
  return state.needsPlanModeExitAttachment;
}
inline void setNeedsPlanModeExitAttachment(FlightTransitions& state,bool value){
  state.needsPlanModeExitAttachment=value;
}
inline void handlePlanModeTransition(FlightTransitions& state,
  const std::string& fromMode,const std::string& toMode){
  if(toMode=="plan"&&fromMode!="plan")
    state.needsPlanModeExitAttachment=false;
  if(fromMode=="plan"&&toMode!="plan")
    state.needsPlanModeExitAttachment=true;
}
}
