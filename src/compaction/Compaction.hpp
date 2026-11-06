#pragma once
#include "runtime/JsonValue.hpp"
#include "services/compact/autoCompact.hpp"
#include <functional>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>

namespace batbox::compaction {
inline constexpr auto PTL_RETRY_MARKER="[earlier conversation truncated for compaction retry]";
inline constexpr auto ERROR_NOT_ENOUGH_MESSAGES="Not enough messages to compact.";
inline constexpr auto ERROR_POETRY_TOO_LONG="Conversation too long. Press esc twice to go up a few messages and try again.";
inline constexpr auto ERROR_USER_ABORT="API Error: Request was aborted.";
// A summary request that timed out. Resending the whole conversation at once
// is unlikely to go better, so callers stop instead of falling back.
struct CompactionTimeout:std::runtime_error {using std::runtime_error::runtime_error;};
// True for a timeout error message, as the API error renderer writes it.
bool isTimeoutText(const std::string& text);
struct HelperServices {
  std::function<std::optional<double>(const Json&)> getPoetryTooLongTokenGap;
  std::function<double(const Json&)> roughTokenCountEstimationForMessages;
  std::function<Json(const Json&)> createUserMessage;
};
Json stripImagesFromMessages(const Json& messages);
Json truncateHeadForPTLRetry(const Json& messages,const Json& response,HelperServices& services);
Json buildPostCompactMessages(const Json& result);
std::optional<std::string> mergeHookInstructions(const std::optional<std::string>& user,
                                               const std::optional<std::string>& hook);

struct AutoServices {
  Environment environment;
  // Failed automatic compactions in one submission before it stops trying.
  double failureLimit=3;
  // Why an automatic compaction failed; timedOut also ends this submission's attempts.
  std::function<void(const std::string& reason,bool timedOut)> reportFailure;
  std::function<bool()> isAutoCompactEnabled;
  std::function<double(const std::string&)> getAutoCompactThreshold;
  std::function<Json(double,const std::string&)> calculateTokenWarningState;
  std::function<double(const Json&)> tokenCountWithEstimation;
  std::function<void(const std::optional<std::string>&)> runPostCompactCleanup;
  std::function<Json(const Json&,const Json&,const Json&,bool,
                     const std::optional<std::string>&,bool,const Json&)> compactConversation;
};
bool shouldAutoCompact(const Json& messages,const std::string& model,
                       const std::optional<std::string>& source,AutoServices& services);
Json autoCompactIfNeeded(const Json& messages,const Json& gadgetUseContext,const Json& stashSafeParams,
                         const std::optional<std::string>& source,const Json& tracking,
                         AutoServices& services);

struct CleanupServices {
  std::function<void()> resetMicrocompactState;
  std::function<void()> clearUserContextStash;
  std::function<void(const std::string&)> resetGetMemoryFilesStash;
  std::function<void()> clearSystemPoetrySections;
  std::function<void()> clearClassifierApprovals;
  std::function<void()> clearSpeculativeChecks;
  std::function<void()> clearFlightMessagesStash;
};
void runPostCompactCleanup(const std::optional<std::string>& source,CleanupServices& services);
} // namespace batbox::compaction
