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
  settings.timeoutMs = std::isfinite(parsed) && parsed != 0 ? parsed : 90000;
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

StreamingFailureAction routeStreamingFailure(const StreamingFailureInput& input,
                                             bool& didFallBackToNonStreaming,
                                             const std::function<void()>& onStreamingFallback) {
  const auto action = streamingFailureAction(input);
  if (action == StreamingFailureAction::NonStreamingFallback) {
    didFallBackToNonStreaming = true;
    if (onStreamingFallback) onStreamingFallback();
  }
  return action;
}

bool is404StreamCreationError(bool didFallBackToNonStreaming,
                              bool isCannotRetryError,
                              bool originalIsAPIError,
                              int originalStatus) {
  return !didFallBackToNonStreaming && isCannotRetryError && originalIsAPIError && originalStatus == 404;
}

bool route404StreamCreationError(bool& didFallBackToNonStreaming,
                                 bool isCannotRetryError,
                                 bool originalIsAPIError,
                                 int originalStatus,
                                 const std::function<void()>& onStreamingFallback) {
  if (!is404StreamCreationError(didFallBackToNonStreaming, isCannotRetryError,
                                 originalIsAPIError, originalStatus)) return false;
  didFallBackToNonStreaming = true;
  if (onStreamingFallback) onStreamingFallback();
  return true;
}

std::string failedStreamRequestId(const std::optional<std::string>& requestId) {
  return requestId.value_or("unknown");
}

void StreamResources::setStream(bool controllerAlreadyAborted, std::function<void()> abort) {
  hasStream_ = true;
  controllerAlreadyAborted_ = controllerAlreadyAborted;
  abort_ = std::move(abort);
}

void StreamResources::setResponse(std::function<void()> cancelBody) {
  hasResponse_ = true;
  cancelBody_ = std::move(cancelBody);
}

void StreamResources::release() noexcept {
  // cleanupStream: check controller, abort if needed, ignore abort failure.
  if (hasStream_ && !controllerAlreadyAborted_) {
    try { if (abort_) abort_(); } catch (...) {}
  }
  hasStream_ = false;
  abort_ = {};
  // releaseStreamResources then cancels the Response body and clears it.
  if (hasResponse_) {
    try { if (cancelBody_) cancelBody_(); } catch (...) {}
    hasResponse_ = false;
    cancelBody_ = {};
  }
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
    const std::optional<std::string>& advisorModel,
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
  if (advisorModel && !advisorModel->empty()) assistant["advisorModel"] = *advisorModel;
  return assistant;
}

} // namespace batbox::model
