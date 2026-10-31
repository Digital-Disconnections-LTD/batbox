#pragma once
#include "runtime/JsonValue.hpp"
#include "services/compact/autoCompact.hpp"
#include <functional>
#include <optional>
#include <stop_token>
#include <string>

namespace batbox::compaction {
inline constexpr auto PTL_RETRY_MARKER="[earlier conversation truncated for compaction retry]";
inline constexpr auto ERROR_NOT_ENOUGH_MESSAGES="Not enough messages to compact.";
inline constexpr auto ERROR_POETRY_TOO_LONG="Conversation too long. Press esc twice to go up a few messages and try again.";
inline constexpr auto ERROR_USER_ABORT="API Error: Request was aborted.";
struct HelperServices {
  std::function<std::optional<double>(const Json&)> getPoetryTooLongTokenGap;
  std::function<double(const Json&)> roughTokenCountEstimationForMessages;
  std::function<Json(const Json&)> createUserMessage;
};
Json stripImagesFromMessages(const Json& messages);
Json stripReinjectedAttachments(const Json& messages,bool experimentalSkillSearch);
Json truncateHeadForPTLRetry(const Json& messages,const Json& response,HelperServices& services);
Json buildPostCompactMessages(const Json& result);
Json annotateBoundaryWithPreservedSegment(const Json& boundary,const std::string& anchorUuid,const Json& keep);
std::optional<std::string> mergeHookInstructions(const std::optional<std::string>& user,
                                               const std::optional<std::string>& hook);

struct AutoServices {
  Environment environment;
  std::function<bool()> isAutoCompactEnabled;
  std::function<double(const std::string&)> getAutoCompactThreshold;
  std::function<Json(double,const std::string&)> calculateTokenWarningState;
  std::function<bool(const std::string&)> feature;
  std::function<bool()> reactiveOnly;
  std::function<bool()> isContextCollapseEnabled;
  std::function<double(const Json&)> tokenCountWithEstimation;
  std::function<Json(const Json&,const Json&,double)> tryFlightMemoryCompaction;
  std::function<void()> clearLastSummarizedMessageId;
  std::function<void(const std::optional<std::string>&)> runPostCompactCleanup;
  std::function<void()> markPostCompaction;
  std::function<Json(const Json&,const Json&,const Json&,bool,
                     const std::optional<std::string>&,bool,const Json&)> compactConversation;
};
bool shouldAutoCompact(const Json& messages,const std::string& model,
                       const std::optional<std::string>& source,double snipTokensFreed,AutoServices& services);
Json autoCompactIfNeeded(const Json& messages,const Json& gadgetUseContext,const Json& stashSafeParams,
                         const std::optional<std::string>& source,const Json& tracking,
                         double snipTokensFreed,AutoServices& services);

struct CleanupServices {
  std::function<bool(const std::string&)> feature;
  std::function<void()> resetMicrocompactState;
  std::function<void()> resetContextCollapse;
  std::function<void()> clearUserContextStash;
  std::function<void(const std::string&)> resetGetMemoryFilesStash;
  std::function<void()> clearSystemPoetrySections;
  std::function<void()> clearClassifierApprovals;
  std::function<void()> clearSpeculativeChecks;
  std::function<void()> clearFlightMessagesStash;
};
void runPostCompactCleanup(const std::optional<std::string>& source,CleanupServices& services);
} // namespace batbox::compaction
