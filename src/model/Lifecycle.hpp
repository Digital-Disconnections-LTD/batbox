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
StreamingFailureAction routeStreamingFailure(const StreamingFailureInput& input,
                                             bool& didFallBackToNonStreaming,
                                             const std::function<void()>& onStreamingFallback = {});

bool is404StreamCreationError(bool didFallBackToNonStreaming,
                              bool isCannotRetryError,
                              bool originalIsAPIError,
                              int originalStatus);
bool route404StreamCreationError(bool& didFallBackToNonStreaming,
                                 bool isCannotRetryError,
                                 bool originalIsAPIError,
                                 int originalStatus,
                                 const std::function<void()>& onStreamingFallback = {});
std::string failedStreamRequestId(const std::optional<std::string>& requestId);

// Indigenous ownership for stream cleanup and resource release.
// The transport owns the callbacks and response body. All releases are idempotent.
class StreamResources {
 public:
  void setStream(bool controllerAlreadyAborted, std::function<void()> abort);
  void setResponse(std::function<void()> cancelBody);
  void release() noexcept;
  bool hasStream() const { return hasStream_; }
  bool hasResponse() const { return hasResponse_; }
 private:
  bool hasStream_ = false;
  bool hasResponse_ = false;
  bool controllerAlreadyAborted_ = false;
  std::function<void()> abort_;
  std::function<void()> cancelBody_;
};

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
    const std::optional<std::string>& advisorModel,
    const std::function<std::string()>& randomUUID,
    const std::function<std::string()>& timestamp);

} // namespace batbox::model
