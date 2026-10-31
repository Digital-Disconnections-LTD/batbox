#include "model/StreamingAttempt.hpp"
#include <chrono>
#include <stdexcept>

namespace batbox::model {
namespace {
std::int64_t currentMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}
}

PreparedStreamingAttempt prepareStreamingAttempt(double attempt,
    const retry::RetryContext& context,StreamingAttemptState& state,
    const StreamingAttemptDependencies& deps) {
  if (!deps.paramsFromContext || !deps.captureAPIRequest ||
      (deps.injectCorrelationId && deps.firstPartyProvider && deps.firstPartyBaseUrl && !deps.randomUUID))
    throw std::invalid_argument("Streaming attempt dependencies are required");
  state.attemptNumber = attempt;
  state.isFastModeRequest = context.fastMode.value_or(false);
  state.start = deps.nowMillis ? deps.nowMillis() : currentMillis();
  state.attemptStartTimes.push_back(state.start);
  if(deps.queryCheckpoint)deps.queryCheckpoint("query_client_creation_end");
  const Json params = deps.paramsFromContext(context);
  deps.captureAPIRequest(params);
  state.maxOutputTokens = value::property(params, "max_tokens").get<double>();
  if(deps.queryCheckpoint)deps.queryCheckpoint("query_api_request_sent");
  if(!deps.hasBatId && deps.headlessProfilerCheckpoint)
    deps.headlessProfilerCheckpoint("api_request_sent");
  state.clientRequestId = deps.injectCorrelationId && deps.firstPartyProvider && deps.firstPartyBaseUrl
    ? std::optional<std::string>(deps.randomUUID()) : std::nullopt;
  Json streamingParams = params;
  streamingParams["stream"] = true;
  return {std::move(streamingParams),state.clientRequestId};
}

OpenedStream finishStreamingAttempt(StreamingAttemptState& state,
    OpenedStream opened,const StreamingAttemptDependencies& deps) {
  if(deps.queryCheckpoint)deps.queryCheckpoint("query_response_headers_received");
  state.streamRequestId = opened.requestId;
  state.streamResponse = opened.response;
  return opened;
}

OpenedStream executeStreamingAttempt(double attempt,
    const retry::RetryContext& context,StreamingAttemptState& state,
    const StreamingAttemptDependencies& deps) {
  if (!deps.openStream)
    throw std::invalid_argument("Streaming attempt dependencies are required");
  PreparedStreamingAttempt prepared=prepareStreamingAttempt(attempt,context,state,deps);
  return finishStreamingAttempt(state,
    deps.openStream(prepared.requestParams,prepared.clientRequestId),deps);
}

} // namespace batbox::model
