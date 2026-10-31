#pragma once
#include "bats/AsyncLifecycle.hpp"

namespace batbox::bats {
struct BatMissionProgressImports {
  std::string syntheticOutputGadgetName;
  std::function<Json(const std::string& gadgetName,const Json& input,
    const gadgets::Gadgets&)> getGadgetSearchOrReadInfo;
};
Json createProgressTracker();
double getTokenCountFromTracker(const Json& tracker);
void updateProgressFromMessage(Json& tracker,const Json& message,
  const ActivityResolver& resolveActivityDescription,
  const gadgets::Gadgets& gadgets,BatMissionProgressImports& imports);
Json getProgressUpdate(const Json& tracker);
ActivityResolver createActivityDescriptionResolver(const gadgets::Gadgets& gadgets);
} // namespace batbox::bats
