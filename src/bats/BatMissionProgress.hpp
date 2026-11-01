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
// The last non-empty line of an assistant message's text, trimmed and clipped
// to maxChars characters. Empty when the message carries no text.
std::string lastAssistantTextLine(const Json& message,std::size_t maxChars=120);
ActivityResolver createActivityDescriptionResolver(const gadgets::Gadgets& gadgets);
} // namespace batbox::bats
