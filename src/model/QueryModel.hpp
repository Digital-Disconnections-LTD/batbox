#pragma once

#include "model/Preflight.hpp"
#include "model/StreamingAttempt.hpp"
#include "model/Lifecycle.hpp"
#include "model/Completion.hpp"
#include "model/NonStreaming.hpp"
#include "model/PostStream.hpp"
#include "api_errors/ApiErrors.hpp"
#include "streaming/Reducer.hpp"
#include "transport/Client.hpp"
#include <exception>
#include <memory>
#include <stop_token>

namespace batbox::model {
class StreamWatchdog;
class StreamObservation;
// Indigenous AsyncGenerator.return() completion at a suspended model yield.
struct GeneratorClosed {};

struct QueryServices {
  // Local writer for debugging stash behavior.
  std::function<void(const std::string&,const std::string&)> logForDebugging;
  std::function<void(const std::string&,const std::string&)> logForDiagnosticsNoPII;
  std::function<std::function<void()>(double,std::function<void()>)>
    scheduleWatchdogTimer;
  std::function<void(const std::string&)> queryCheckpoint;
  std::function<void(const std::string&)> headlessProfilerCheckpoint;
  std::function<void()> endQueryProfile;
  std::function<void(const std::string&)> startFlightActivity;
  std::function<void(const std::string&)> stopFlightActivity;
  // Account for one successful model request, with
  // retry time included in the first value and the final attempt in the second.
  std::function<void(double,double)> addToTotalDurationState;
  PostStreamServices postStream;
  std::function<void(const PostStreamInputs&,const PostStreamServices&)>
    processPostStreamSegment;
  PreflightServices preflight;
  requests::BuilderDependencies builder;
  requests::WireProtocol wire;
  requests::BuilderHeaders headers;
  // Indigenous asynchronous boundaries for complete synchronous segments.
  // The default implementation invokes these functions directly;
  // an owner-loop driver may schedule each complete segment on its owner.
  std::function<PreflightState(const PreflightInputs&, const PreflightServices&)>
    preparePreflightSegment;
  std::function<Json(const Json&, requests::RequestCapture&,
    const requests::BuilderDependencies&, const requests::WireProtocol&,
    const requests::BuilderHeaders&)> buildParamsSegment;
  std::function<void(OpenedStream&,streaming::Reducer&,
    const std::function<void(const streaming::MessageRef&)>&,
    StreamWatchdog*,const std::function<void()>&,StreamObservation*)> consumeStreamSegment;
  std::function<void(const CompletionState&,const CompletionServices&)>
    completeRequestSegment;
  std::function<PreparedStreamingAttempt(double,const retry::RetryContext&,
    StreamingAttemptState&,const StreamingAttemptDependencies&)>
    prepareStreamingAttemptSegment;
  std::function<OpenedStream(StreamingAttemptState&,OpenedStream,
    const StreamingAttemptDependencies&)>
    finishStreamingAttemptSegment;
  std::function<Json(const retry::RetryOptions&,retry::Dependencies&)>
    withRetrySegment;
  std::function<PreparedNonStreamingAttempt(double,const retry::RetryContext&,
    double,const NonStreamingDependencies&)>
    prepareNonStreamingAttemptSegment;
  std::function<Json(const std::function<Json()>&)> makeFallbackMessageSegment;
  std::function<FallbackAccounting(const std::function<FallbackAccounting()>&)>
    accountFallbackSegment;
  std::function<std::optional<Json>(const std::function<std::optional<Json>()>&)>
    processErrorSegment;
  std::function<void(const retry::ErrorRecord&)> extractQuotaStatusFromError;
  retry::Dependencies retry;
  streaming::ReducerConfig reducer;
  std::function<std::shared_ptr<transport::Client>()> createClient;
  std::function<Json(const Json&)> normalizeFallbackContent;
  std::function<Json(const api_errors::Failure&, const std::string&)> getAssistantMessageFromError;
  std::function<void()> onStreamingFallback;
  std::function<void()> markGadgetsSentToAPIState;
  std::function<bool()> hasBatContext;
  std::function<void(const std::string&)> setLastMainRequestId;
  std::function<std::int64_t()> nowMillis;
  Json environment = Json::object();
  std::stop_token signal;
  bool disableFallbackFeature = false;
  bool stashedMicrocompactFeature = false;
};

struct QueryResult {
  std::vector<streaming::MessageRef> newMessages;
  Json usage;
  Json stopReason = nullptr;
  double costUSD = 0;
  bool didFallBackToNonStreaming = false;
  std::optional<std::string> streamRequestId;
  double attemptNumber = 0;
};

// Synchronous indigenous async-generator bridge. The yield callback is called for
// each emitted object and may retain shared assistant message pointers.
QueryResult queryModel(const PreflightInputs& input, QueryServices services,
                       const std::function<void(const streaming::MessageRef&)>& yield);

} // namespace batbox::model
