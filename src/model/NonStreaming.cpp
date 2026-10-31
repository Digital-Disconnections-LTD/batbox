#include "model/NonStreaming.hpp"
#include "model/ModelWrappers.hpp"
#include "requests/assembly.hpp"
#include "transport/RetryAdapter.hpp"
#include <chrono>
#include <stdexcept>

namespace batbox::model {
namespace {
std::int64_t currentMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}
}

PreparedNonStreamingAttempt prepareNonStreamingAttempt(double attempt,
    const retry::RetryContext& context,double timeoutMs,
    const NonStreamingDependencies& dependencies) {
  const std::int64_t start = dependencies.nowMillis
    ? dependencies.nowMillis() : currentMillis();
  const Json retryParams = dependencies.paramsFromContext(context);
  dependencies.captureRequest(retryParams);
  dependencies.onAttempt(attempt,start,
    value::property(retryParams,"max_tokens").get<double>());
  Json adjustedParams=requests::adjustParamsForNonStreaming(retryParams,64000);
  adjustedParams["model"]=dependencies.normalizeModelStringForAPI(
    value::property(adjustedParams,"model").get<std::string>());
  return {std::move(adjustedParams),timeoutMs};
}

Json executeNonStreamingRequest(const retry::RetryOptions& retryOptions,
                                NonStreamingDependencies dependencies) {
  if (!dependencies.paramsFromContext || !dependencies.captureRequest ||
      !dependencies.onAttempt || !dependencies.createMessage ||
      !dependencies.normalizeModelStringForAPI || !dependencies.yieldSystem ||
      !dependencies.withRetry)
    throw std::invalid_argument("Non-streaming request dependencies are required");
  const double fallbackTimeoutMs = getNonstreamingFallbackTimeoutMs(dependencies.environment);
  dependencies.retry.yield = [&](const Json& message) {
    if (value::property(message, "type") == "system") dependencies.yieldSystem(message);
  };
  dependencies.retry.operation = [&](const Json&, double attempt, const retry::RetryContext& context) -> Json {
    auto prepared=dependencies.prepareAttemptSegment
      ? dependencies.prepareAttemptSegment(attempt,context,fallbackTimeoutMs,dependencies)
      : prepareNonStreamingAttempt(attempt,context,fallbackTimeoutMs,dependencies);
    try{return dependencies.createMessage(prepared.params,prepared.timeoutMs,retryOptions.signal);}
    catch(...){
      const auto error=std::current_exception();
      if(transport::errorRecordFromException(error).kind!=retry::ErrorRecord::Kind::Abort){
        if(!dependencies.logForDiagnosticsNoPII)
          throw std::invalid_argument("Non-streaming diagnostic import is required");
        dependencies.logForDiagnosticsNoPII("error","cli_nonstreaming_fallback_error");
      }
      std::rethrow_exception(error);
    }
  };
  return dependencies.withRetry(retryOptions, dependencies.retry);
}

} // namespace batbox::model
