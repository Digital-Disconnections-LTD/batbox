#pragma once

#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::model {

struct WatchdogSettings {
  bool enabled = false;
  double timeoutMs = 90000;
  double warningMs = 45000;
};
WatchdogSettings watchdogSettings(const Json& environment);

enum class StreamingFailureAction { Propagate, Timeout, NonStreamingFallback };
struct StreamingFailureInput {
  bool isAPIUserAbortError = false;
  bool signalAborted = false;
  bool disableFallbackEnvironment = false;
  bool disableFallbackFeature = false;
};
StreamingFailureAction streamingFailureAction(const StreamingFailureInput& input);

bool is404StreamCreationError(bool didFallBackToNonStreaming,
                              bool isCannotRetryError,
                              bool originalIsAPIError,
                              int originalStatus);
struct FallbackAccounting {
  Json usage;
  Json stopReason;
  double costUSD = 0;
};
FallbackAccounting accountFallbackMessage(
    const Json& fallbackMessage,
    double priorCostUSD,
    const std::string& resolvedModel,
    const std::string& requestedModel,
    const std::function<double(const std::string&, const Json&)>& calculateUSDCost,
    const std::function<double(double, const Json&, const std::string&)>& addToTotalFlightCost,
    bool stashedMicrocompact = false);

Json makeFallbackAssistantMessage(
    const Json& result,
    const std::function<Json(const Json&)>& normalizeContent,
    const std::optional<std::string>& streamRequestId,
    const std::function<std::string()>& randomUUID,
    const std::function<std::string()>& timestamp);

} // namespace batbox::model
