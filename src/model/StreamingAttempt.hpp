#pragma once

#include "retry/Retry.hpp"
#include "transport/Client.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::model {

struct OpenedStream {
  transport::Response response;
  std::optional<std::string> requestId;
  std::function<std::optional<Json>()> next;
  std::function<void()> abort;
};

struct StreamingAttemptState {
  double attemptNumber = 0;
  bool isFastModeRequest = false;
  std::int64_t start = 0;
  std::vector<std::int64_t> attemptStartTimes;
  double maxOutputTokens = 0;
  std::optional<std::string> clientRequestId;
  std::optional<std::string> streamRequestId;
  transport::Response streamResponse;
};

struct StreamingAttemptDependencies {
  std::function<std::int64_t()> nowMillis;
  std::function<Json(const retry::RetryContext&)> paramsFromContext;
  std::function<void(const Json&)> captureAPIRequest;
  std::function<std::string()> randomUUID;
  std::function<void(const std::string&)> queryCheckpoint;
  std::function<void(const std::string&)> headlessProfilerCheckpoint;
  bool hasBatId = false;
  bool firstPartyProvider = false;
  bool firstPartyBaseUrl = false;
  bool injectCorrelationId = false; // Production requests do not add correlation IDs.
  std::function<OpenedStream(const Json&, const std::optional<std::string>&)> openStream;
};

struct PreparedStreamingAttempt {
  Json requestParams;
  std::optional<std::string> clientRequestId;
};

// Statements before and after awaiting the transport response are separate
// segments. The request itself runs between them on a transport worker.
PreparedStreamingAttempt prepareStreamingAttempt(double attempt,
  const retry::RetryContext& context,StreamingAttemptState& state,
  const StreamingAttemptDependencies& deps);
OpenedStream finishStreamingAttempt(StreamingAttemptState& state,
  OpenedStream opened,const StreamingAttemptDependencies& deps);

OpenedStream executeStreamingAttempt(double attempt, const retry::RetryContext& context,
                                     StreamingAttemptState& state,
                                     const StreamingAttemptDependencies& deps);

} // namespace batbox::model
