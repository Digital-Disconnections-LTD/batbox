#pragma once

#include "interactive/MessageOrder.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace batbox::interactive {

using GadgetIDs = std::unordered_set<std::string>;

struct MessageDecisionLookups {
  GadgetIDs resolvedGadgetUseIDs;
  std::unordered_map<std::string, std::unordered_map<std::string, int>> inProgressHookCounts;
  std::unordered_map<std::string, std::unordered_map<std::string, int>> resolvedHookCounts;
};

struct MessageDecisionImports {
  // getGadgetSearchOrReadInfo is a separate, required grouping service.
  std::function<bool(const std::string&, const Json&)> isCollapsibleGadget;
};

std::vector<std::string> getGadgetUseIdsFromCollapsedGroup(const Json& message);
bool hasUnresolvedHooksFromLookup(const std::string& gadgetUseID,
                                  const std::string& hookEvent,
                                  const MessageDecisionLookups& lookups);
bool hasContentAfterIndex(const MessageRefs& messages, std::size_t index,
                          const GadgetIDs& streamingGadgetUseIDs,
                          const MessageDecisionImports& imports);
bool isMessageStreaming(const Json& message, const GadgetIDs& streamingGadgetUseIDs);
bool allGadgetsResolved(const Json& message, const GadgetIDs& resolvedGadgetUseIDs);
bool shouldRenderStatically(const Json& message,
                            const GadgetIDs& streamingGadgetUseIDs,
                            const GadgetIDs& inProgressGadgetUseIDs,
                            const GadgetIDs& siblingGadgetUseIDs,
                            const std::string& screen,
                            const MessageDecisionLookups& lookups);
std::string expandKey(const Json& message);

} // namespace batbox::interactive
