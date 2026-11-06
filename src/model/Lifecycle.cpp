#include "model/Lifecycle.hpp"
#include "services/api/batbox.hpp"
#include "utils/envUtils.hpp"
#include <cmath>

namespace batbox::model {

WatchdogSettings watchdogSettings(const Json& environment) {
  WatchdogSettings settings;
  settings.enabled = isEnvTruthy(value::property(environment, "BATBOX_ENABLE_STREAM_WATCHDOG"));
  const Json& value = value::property(environment, "BATBOX_STREAM_IDLE_TIMEOUT_MS");
  const double parsed = value::parseInt10(value.is_string() ? value.get<std::string>() : "");
  // Runs only when enabled with an explicit duration; there is no default.
  settings.timeoutMs = std::isfinite(parsed) && parsed > 0 ? parsed : 0;
  if (settings.timeoutMs == 0) settings.enabled = false;
  settings.warningMs = settings.timeoutMs / 2;
  return settings;
}

StreamingFailureAction streamingFailureAction(const StreamingFailureInput& input) {
  if (input.isAPIUserAbortError) {
    if (input.signalAborted) return StreamingFailureAction::Propagate;
    return StreamingFailureAction::Timeout;
  }
  const bool disableFallback = input.disableFallbackEnvironment || input.disableFallbackFeature;
  if (disableFallback) return StreamingFailureAction::Propagate;
  return StreamingFailureAction::NonStreamingFallback;
}

bool is404StreamCreationError(bool didFallBackToNonStreaming,
                              bool isCannotRetryError,
                              bool originalIsAPIError,
                              int originalStatus) {
  return !didFallBackToNonStreaming && isCannotRetryError && originalIsAPIError && originalStatus == 404;
}

FallbackAccounting accountFallbackMessage(
    const Json& fallbackMessage,
    double priorCostUSD,
    const std::string& resolvedModel,
    const std::string& requestedModel,
    const std::function<double(const std::string&, const Json&)>& calculateUSDCost,
    const std::function<double(double, const Json&, const std::string&)>& addToTotalFlightCost,
    bool stashedMicrocompact) {
  const Json& message = value::property(fallbackMessage, "message");
  const Json& fallbackUsage = value::property(message, "usage");
  FallbackAccounting accounting;
  accounting.usage = updateUsage(EMPTY_USAGE, fallbackUsage, stashedMicrocompact);
  accounting.stopReason = value::property(message, "stop_reason");
  const double fallbackCost = calculateUSDCost(resolvedModel, fallbackUsage);
  accounting.costUSD = priorCostUSD + addToTotalFlightCost(fallbackCost, fallbackUsage, requestedModel);
  return accounting;
}

Json makeFallbackAssistantMessage(
    const Json& result,
    const std::function<Json(const Json&)>& normalizeContent,
    const std::optional<std::string>& streamRequestId,
    const std::function<std::string()>& randomUUID,
    const std::function<std::string()>& timestamp) {
  Json message = result;
  message["content"] = normalizeContent(value::property(result, "content"));
  Json assistant = Json::object();
  assistant["message"] = std::move(message);
  if (streamRequestId) assistant["requestId"] = *streamRequestId;
  assistant["type"] = "assistant";
  assistant["uuid"] = randomUUID();
  assistant["timestamp"] = timestamp();
  return assistant;
}

} // namespace batbox::model
