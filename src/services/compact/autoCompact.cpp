#include "services/compact/autoCompact.hpp"
#include <algorithm>
#include <cmath>
namespace batbox {
namespace {
std::string env(const AutoCompactDependencies& deps, const char* name) {
  auto it = deps.env.find(name);
  return it == deps.env.end() ? "" : it->second;
}
} // namespace
double getEffectiveContextWindowSize(const AutoCompactDependencies& deps) {
  const double reservedTokensForSummary = std::min(deps.maxOutputTokens, MAX_OUTPUT_TOKENS_FOR_SUMMARY);
  double contextWindow = deps.contextWindow;
  const auto autoCompactWindow = env(deps, "BATBOX_AUTO_COMPACT_WINDOW");
  if (!autoCompactWindow.empty()) {
    const double parsed = value::parseInt10(autoCompactWindow);
    if (!std::isnan(parsed) && parsed > 0) contextWindow = std::min(contextWindow, parsed);
  }
  return contextWindow - reservedTokensForSummary;
}
double getAutoCompactThreshold(const AutoCompactDependencies& deps) {
  const double effectiveContextWindow = getEffectiveContextWindowSize(deps);
  const double autocompactThreshold = effectiveContextWindow - AUTOCOMPACT_BUFFER_TOKENS;
  const auto envPercent = env(deps, "BATBOX_AUTOCOMPACT_PCT_OVERRIDE");
  if (!envPercent.empty()) {
    const double parsed = value::parseFloat(envPercent);
    if (!std::isnan(parsed) && parsed > 0 && parsed <= 100) {
      const double percentageThreshold = std::floor(effectiveContextWindow * (parsed / 100));
      return std::min(percentageThreshold, autocompactThreshold);
    }
  }
  return autocompactThreshold;
}
Json calculateTokenWarningState(double tokenUsage, const AutoCompactDependencies& deps) {
  const double autoCompactThreshold = getAutoCompactThreshold(deps);
  const double threshold = isAutoCompactEnabled(deps) ? autoCompactThreshold : getEffectiveContextWindowSize(deps);
  const double percentLeft = std::max(0.0, value::round(((threshold - tokenUsage) / threshold) * 100));
  const double warningThreshold = threshold - WARNING_THRESHOLD_BUFFER_TOKENS;
  const double errorThreshold = threshold - ERROR_THRESHOLD_BUFFER_TOKENS;
  const bool isAboveWarningThreshold = tokenUsage >= warningThreshold;
  const bool isAboveErrorThreshold = tokenUsage >= errorThreshold;
  const bool isAboveAutoCompactThreshold = isAutoCompactEnabled(deps) && tokenUsage >= autoCompactThreshold;
  const double actualContextWindow = getEffectiveContextWindowSize(deps);
  const double defaultBlockingLimit = actualContextWindow - MANUAL_COMPACT_BUFFER_TOKENS;
  const auto blockingLimitOverride = env(deps, "BATBOX_BLOCKING_LIMIT_OVERRIDE");
  const double parsedOverride = value::parseInt10(blockingLimitOverride);
  const double blockingLimit = !std::isnan(parsedOverride) && parsedOverride > 0 ? parsedOverride : defaultBlockingLimit;
  const bool isAtBlockingLimit = tokenUsage >= blockingLimit;
  return {{"percentLeft", percentLeft}, {"isAboveWarningThreshold", isAboveWarningThreshold},
    {"isAboveErrorThreshold", isAboveErrorThreshold}, {"isAboveAutoCompactThreshold", isAboveAutoCompactThreshold},
    {"isAtBlockingLimit", isAtBlockingLimit}};
}
bool isAutoCompactEnabled(const AutoCompactDependencies& deps) {
  if (isEnvTruthy(env(deps, "BATBOX_DISABLE_COMPACT"))) return false;
  if (isEnvTruthy(env(deps, "BATBOX_DISABLE_AUTO_COMPACT"))) return false;
  return deps.autoCompactEnabled;
}
} // namespace batbox
