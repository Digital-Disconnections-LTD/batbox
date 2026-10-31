#include "bootstrap/FlightTransitions.hpp"
#include <utility>

namespace batbox::bootstrap {
void setFlightTrustAccepted(FlightTransitions& state,bool accepted){
  state.flightTrustAccepted=accepted;
}
bool getFlightTrustAccepted(const FlightTransitions& state){
  return state.flightTrustAccepted;
}
void setFlightPersistenceDisabled(FlightTransitions& state,bool disabled){
  state.flightPersistenceDisabled=disabled;
}
bool isFlightPersistenceDisabled(const FlightTransitions& state){
  return state.flightPersistenceDisabled;
}
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
bool needsAutoModeExitAttachment(const FlightTransitions& state){
  return state.needsAutoModeExitAttachment;
}
void setNeedsAutoModeExitAttachment(FlightTransitions& state,bool value){
  state.needsAutoModeExitAttachment=value;
}
void handleAutoModeTransition(FlightTransitions& state,
  const std::string& fromMode,const std::string& toMode){
  if((fromMode=="auto"&&toMode=="plan")||
     (fromMode=="plan"&&toMode=="auto"))return;
  const bool fromIsAuto=fromMode=="auto";
  const bool toIsAuto=toMode=="auto";
  if(toIsAuto&&!fromIsAuto)
    state.needsAutoModeExitAttachment=false;
  if(fromIsAuto&&!toIsAuto)
    state.needsAutoModeExitAttachment=true;
}
bool hasShownLspRecommendationThisFlight(const FlightTransitions& state){
  return state.lspRecommendationShownThisFlight;
}
void setLspRecommendationShownThisFlight(FlightTransitions& state,
  bool value){
  state.lspRecommendationShownThisFlight=value;
}
void setInitJsonSchema(FlightTransitions& state,
  std::shared_ptr<Json> schema){
  state.initJsonSchema=std::move(schema);
}
std::shared_ptr<Json> getInitJsonSchema(const FlightTransitions& state){
  return state.initJsonSchema;
}
}
