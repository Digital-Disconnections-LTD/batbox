#pragma once
#include "runtime/JsonValue.hpp"
#include <memory>
#include <string>

namespace batbox::bootstrap {
struct FlightTransitions {
  bool flightTrustAccepted=false;
  bool flightPersistenceDisabled=false;
  bool hasExitedPlanMode=false;
  bool needsPlanModeExitAttachment=false;
  bool needsAutoModeExitAttachment=false;
  bool lspRecommendationShownThisFlight=false;
  std::shared_ptr<Json> initJsonSchema;
};
void setFlightTrustAccepted(FlightTransitions& state,bool accepted);
bool getFlightTrustAccepted(const FlightTransitions& state);
void setFlightPersistenceDisabled(FlightTransitions& state,bool disabled);
bool isFlightPersistenceDisabled(const FlightTransitions& state);
bool hasExitedPlanModeInFlight(const FlightTransitions& state);
void setHasExitedPlanMode(FlightTransitions& state,bool value);
bool needsPlanModeExitAttachment(const FlightTransitions& state);
void setNeedsPlanModeExitAttachment(FlightTransitions& state,bool value);
void handlePlanModeTransition(FlightTransitions& state,
  const std::string& fromMode,const std::string& toMode);
bool needsAutoModeExitAttachment(const FlightTransitions& state);
void setNeedsAutoModeExitAttachment(FlightTransitions& state,bool value);
void handleAutoModeTransition(FlightTransitions& state,
  const std::string& fromMode,const std::string& toMode);
bool hasShownLspRecommendationThisFlight(const FlightTransitions& state);
void setLspRecommendationShownThisFlight(FlightTransitions& state,
  bool value);
void setInitJsonSchema(FlightTransitions& state,
  std::shared_ptr<Json> schema);
std::shared_ptr<Json> getInitJsonSchema(const FlightTransitions& state);
}
