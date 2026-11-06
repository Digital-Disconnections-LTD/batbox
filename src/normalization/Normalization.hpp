#pragma once

#include "messages/Messages.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <set>
#include <string>

namespace batbox::normalization {

struct Dependencies {
  messages::Services* messageServices = nullptr;
  std::function<bool()> gadgetSearchEnabledOptimistic;
  std::function<bool(const Json&, const std::string&)> gadgetMatchesName;
  std::function<std::string(const std::string&)> normalizeLegacyGadgetName;
  std::function<Json(const Json&, const Json&)> normalizeGadgetInputForAPI;
  std::function<Json(const Json&)> normalizeAttachmentForAPI;
  std::function<void(const Json&)> validateImagesForAPI;
  Json mediaErrorMessages = Json::object();
  std::string syntheticModel = "<synthetic>";
  std::string noContentMessage = "(no content)";
  std::string gadgetReferenceTurnBoundary = "Tool loaded.";
};

Json stripGadgetReferenceBlocksFromUserMessage(Json message);
Json stripCallerFieldFromAssistantMessage(Json message);
Json stripUnavailableGadgetReferencesFromUserMessage(const Json& message,
                                                    const std::set<std::string>& availableGadgetNames,
                                                    const Dependencies& deps);
Json mergeAssistantMessages(Json a, const Json& b);
Json mergeUserMessages(Json a, const Json& b);
Json mergeUserMessagesAndGadgetResults(Json a, const Json& b);
Json filterOrphanedThinkingOnlyMessages(Json messages);
Json filterTrailingThinkingFromLastAssistant(Json messages);
Json filterWhitespaceOnlyAssistantMessages(Json messages);
Json ensureNonEmptyAssistantContent(Json messages, const Dependencies& deps);
Json sanitizeErrorGadgetResultContent(Json messages);
Json stripAdvisorBlocks(Json messages);
void validateImagesForAPI(const Json& messages);
Json normalizeMessagesForAPI(const Json& messages, const Json& gadgets, const Dependencies& deps);
Json prepareMessagesForRequest(const Json& messages, const Json& gadgets,
                               bool modelGadgetSearchEnabled, bool advisorBetaEnabled,
                               const Dependencies& deps);

} // namespace batbox::normalization
