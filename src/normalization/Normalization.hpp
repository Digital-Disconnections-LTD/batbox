#pragma once

#include "messages/Messages.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <set>
#include <string>

namespace batbox::normalization {

struct Dependencies {
  messages::Services* messageServices = nullptr;
  std::function<bool(const std::string&)> featureGate;
  std::function<bool()> gadgetSearchEnabledOptimistic;
  std::function<bool(const Json&, const std::string&)> gadgetMatchesName;
  std::function<std::string(const std::string&)> normalizeLegacyGadgetName;
  std::function<Json(const Json&, const Json&)> normalizeGadgetInputForAPI;
  std::function<Json(const Json&)> normalizeAttachmentForAPI;
  std::function<void(const Json&)> validateImagesForAPI;
  Json mediaErrorMessages = Json::object();
  bool historySnipFeature = false;
  bool historySnipRuntimeEnabled = false;
  bool testEnvironment = false;
  std::string syntheticModel = "<synthetic>";
  std::string noContentMessage = "(no content)";
  std::string gadgetReferenceTurnBoundary = "Tool loaded.";
};

Json reorderAttachmentsForAPI(const Json& messages);
Json stripGadgetReferenceBlocksFromUserMessage(const Json& message);
Json stripCallerFieldFromAssistantMessage(const Json& message);
Json stripUnavailableGadgetReferencesFromUserMessage(const Json& message,
                                                    const std::set<std::string>& availableGadgetNames,
                                                    const Dependencies& deps);
Json mergeAssistantMessages(const Json& a, const Json& b);
Json mergeUserMessages(const Json& a, const Json& b, const Dependencies& deps);
Json mergeUserMessagesAndGadgetResults(const Json& a, const Json& b, const Dependencies& deps);
Json filterOrphanedThinkingOnlyMessages(const Json& messages);
Json filterTrailingThinkingFromLastAssistant(const Json& messages);
Json filterWhitespaceOnlyAssistantMessages(const Json& messages, const Dependencies& deps);
Json ensureNonEmptyAssistantContent(const Json& messages, const Dependencies& deps);
Json sanitizeErrorGadgetResultContent(const Json& messages);
Json stripAdvisorBlocks(const Json& messages);
void validateImagesForAPI(const Json& messages);
std::string deriveShortMessageId(const std::string& uuid);
Json appendMessageTagToUserMessage(const Json& message);
Json normalizeMessagesForAPI(const Json& messages, const Json& gadgets, const Dependencies& deps);
Json prepareMessagesForRequest(const Json& messages, const Json& gadgets,
                               bool modelGadgetSearchEnabled, bool advisorBetaEnabled,
                               const Dependencies& deps);

} // namespace batbox::normalization
