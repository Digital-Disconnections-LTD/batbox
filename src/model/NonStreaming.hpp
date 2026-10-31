#pragma once

#include "retry/Retry.hpp"
#include <cstdint>

namespace batbox::model {

struct NonStreamingDependencies;
struct PreparedNonStreamingAttempt {
  Json params;
  double timeoutMs = 0;
};
PreparedNonStreamingAttempt prepareNonStreamingAttempt(double attempt,
  const retry::RetryContext& context,double timeoutMs,
  const NonStreamingDependencies& dependencies);

struct NonStreamingDependencies {
  retry::Dependencies retry;
  Json environment = Json::object();
  std::function<Json(const retry::RetryContext&)> paramsFromContext;
  std::function<void(const Json&)> captureRequest;
  std::function<void(double attempt, std::int64_t start, double maxOutputTokens)> onAttempt;
  std::function<Json(const Json& params, double timeoutMs, std::stop_token signal)> createMessage;
  std::function<std::string(const std::string&)> normalizeModelStringForAPI;
  std::function<void(const Json&)> yieldSystem;
  std::function<void(const std::string&,const std::string&)> logForDiagnosticsNoPII;
  std::function<std::int64_t()> nowMillis;
  std::function<Json(const retry::RetryOptions&, retry::Dependencies&)> withRetry;
  std::function<PreparedNonStreamingAttempt(double,const retry::RetryContext&,
    double,const NonStreamingDependencies&)> prepareAttemptSegment;
};

// Execute a non-streaming request and consume its result in order, using
// retry::withRetry for retry decisions.
Json executeNonStreamingRequest(const retry::RetryOptions& retryOptions,
                                NonStreamingDependencies dependencies);

} // namespace batbox::model
