#include "model/QueryModel.hpp"
#include "model/LiveStreamAdapter.hpp"
#include "model/NonStreaming.hpp"
#include "model/Watchdog.hpp"
#include "model/StreamObservation.hpp"
#include "requests/RequestBuilder.hpp"
#include "services/api/batbox.hpp"
#include "transport/Errors.hpp"
#include "transport/RetryAdapter.hpp"
#include "utils/envUtils.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::model {
namespace {
const Json& field(const Json& value, const char* name) { return value::property(value, name); }
std::string stringField(const Json& value, const char* name) {
  const auto& part = field(value, name);
  return part.is_string() ? part.get<std::string>() : std::string{};
}
Json contextJson(const retry::RetryContext& context) {
  Json result = {{"model", context.model}, {"thinkingConfig", context.thinkingConfig}};
  if (context.maxTokensOverride) result["maxTokensOverride"] = *context.maxTokensOverride;
  if (context.fastMode) result["fastMode"] = *context.fastMode;
  return result;
}
void emitValue(const std::function<void(const streaming::MessageRef&)>& yield, Json value) {
  yield(std::make_shared<Json>(std::move(value)));
}
bool is529Exception(const std::exception_ptr& error) {
  return retry::is529Error(transport::errorRecordFromException(error));
}
std::string errorMessage(const std::exception_ptr& error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& caught){return caught.what();}
  catch(...){return "unknown";}
  return "unknown";
}
bool isAbortException(const std::exception_ptr& error) {
  try { std::rethrow_exception(error); }
  catch (const transport::APIUserAbortError&) { return true; }
  catch (const retry::CannotRetryError& e) {
    return e.originalError && e.originalError->kind == retry::ErrorRecord::Kind::Abort;
  }
  catch (const retry::RetryFailure& e) { return e.error.kind == retry::ErrorRecord::Kind::Abort; }
  catch (...) { return false; }
}
std::string errorModel(const std::exception_ptr& error, const std::string& defaultModel) {
  try { std::rethrow_exception(error); }
  catch (const retry::CannotRetryError& e) { return e.retryContext.model; }
  catch (...) { return defaultModel; }
}
api_errors::Failure failureForRendering(const std::exception_ptr& error) {
  try { std::rethrow_exception(error); }
  catch (const retry::CannotRetryError& cannot) {
    api_errors::Failure failure;
    if (cannot.originalError) {
      failure.error = *cannot.originalError;
      failure.isConnectionTimeout = failure.error.kind == retry::ErrorRecord::Kind::Connection &&
        failure.error.code == "ETIMEDOUT";
    } else failure.error = transport::errorRecordFromException(error);
    return failure;
  }
  catch (...) { return transport::apiFailureFromException(error); }
}
} // namespace

QueryResult queryModel(const PreflightInputs& input, QueryServices services,
                       const std::function<void(const streaming::MessageRef&)>& yield) {
  if (!yield || !services.createClient || !services.normalizeFallbackContent ||
      !services.getAssistantMessageFromError || !services.reducer.randomUUID ||
      !services.reducer.timestamp || !services.reducer.calculateUSDCost ||
      !services.reducer.addToTotalFlightCost)
    throw std::invalid_argument("Query model dependencies are required");
  if(!services.preflight.queryCheckpoint)
    services.preflight.queryCheckpoint=services.queryCheckpoint;
  QueryResult result;
  result.usage = EMPTY_USAGE;
  auto preflight = services.preparePreflightSegment
    ? services.preparePreflightSegment(input, services.preflight)
    : prepareQueryModel(input, services.preflight);
  if (preflight.offSwitch) {
    emitValue(yield, preflight.offSwitchMessage);
    return result;
  }
  if (services.addToTotalDurationState && !services.nowMillis)
    throw std::invalid_argument("API duration clock is required");
  const auto startIncludingRetries=services.nowMillis
    ?services.nowMillis():std::int64_t{};
  const std::string model = stringField(input.options, "model");
  const std::string querySource = stringField(input.options, "querySource");
  auto paramsFromContext = [&](const retry::RetryContext& context) {
    const Json retryContext=contextJson(context);
    return services.buildParamsSegment
      ? services.buildParamsSegment(retryContext, preflight.capture,
          services.builder, services.wire, services.headers)
      : requests::buildParamsFromContext(retryContext, preflight.capture,
          services.builder, services.wire, services.headers);
  };
  // Build the initial request before retry so the builder's ordered
  // stash-marker side effects occur before the first attempt.
  retry::RetryContext initialContext;
  initialContext.model = model;
  initialContext.thinkingConfig = input.thinkingConfig;
  (void)paramsFromContext(initialContext);

  StreamingAttemptState attemptState;
  std::shared_ptr<transport::Client> currentClient;
  std::shared_ptr<OpenedStream> opened;
  std::optional<Json> fallbackMessage;
  bool completed = false;
  bool finalized = false;
  auto captureRequest = [](const Json&) { /* Request content is not recorded. */ };
  auto fallback = [&](int initial529, const std::optional<std::string>& requestId) {
    NonStreamingDependencies nonstream;
    nonstream.environment = services.environment;
    nonstream.retry = services.retry;
    nonstream.nowMillis = services.nowMillis;
    nonstream.paramsFromContext = paramsFromContext;
    nonstream.captureRequest = captureRequest;
    nonstream.onAttempt = [&](double attempt, std::int64_t, double tokens) {
      attemptState.attemptNumber = attempt;
      attemptState.maxOutputTokens = tokens;
    };
    nonstream.logForDiagnosticsNoPII=services.logForDiagnosticsNoPII;
    nonstream.normalizeModelStringForAPI = services.preflight.normalizeModelStringForAPI;
    nonstream.yieldSystem = [&](const Json& value) { emitValue(yield, value); };
    nonstream.retry.getClient = [&]() -> Json {
      currentClient = services.createClient();
      return Json::object();
    };
    nonstream.createMessage = [&](const Json& params, double timeoutMs, std::stop_token stop) -> Json {
      try { return currentClient->request(params, false, {}, stop, {},
        std::chrono::milliseconds(static_cast<std::int64_t>(timeoutMs))).body; }
      catch (const transport::APIError&) { throw retry::RetryFailure(transport::errorRecordFromException(std::current_exception())); }
    };
    nonstream.withRetry = [&](const retry::RetryOptions& options, retry::Dependencies& deps) {
      return services.withRetrySegment
        ? services.withRetrySegment(options,deps)
        : retry::withRetry(options, deps);
    };
    nonstream.prepareAttemptSegment=services.prepareNonStreamingAttemptSegment;
    retry::RetryOptions retryOptions;
    retryOptions.model = model;
    retryOptions.thinkingConfig = input.thinkingConfig;
    retryOptions.signal = services.signal;
    retryOptions.querySource = querySource;
    retryOptions.initialConsecutive529Errors = initial529;
    const auto& fallbackModel = field(input.options, "fallbackModel");
    if (fallbackModel.is_string()) retryOptions.fallbackModel = fallbackModel.get<std::string>();
    if (preflight.isFastMode) retryOptions.fastMode = true;
    Json response = executeNonStreamingRequest(retryOptions, std::move(nonstream));
    auto construct=[&]{return makeFallbackAssistantMessage(response,
      services.normalizeFallbackContent,requestId,preflight.advisorModel,
      services.reducer.randomUUID,services.reducer.timestamp);};
    Json message=services.makeFallbackMessageSegment
      ? services.makeFallbackMessageSegment(construct) : construct();
    fallbackMessage = message;
    auto ref = std::make_shared<Json>(std::move(message));
    result.newMessages.push_back(ref);
    yield(ref);
  };
  auto finalize = [&] {
    if(finalized)return;
    finalized=true;
    if(!services.stopFlightActivity)
      throw std::invalid_argument("Stop session activity import is required");
    services.stopFlightActivity("api_call");
    if (opened && opened->abort) opened->abort();
    if (fallbackMessage) {
      auto account=[&]{return accountFallbackMessage(*fallbackMessage,result.costUSD,
        preflight.resolvedModel,model,services.reducer.calculateUSDCost,
        services.reducer.addToTotalFlightCost,preflight.stashedMCEnabled);};
      const auto accounted=services.accountFallbackSegment
        ? services.accountFallbackSegment(account) : account();
      result.usage = accounted.usage;
      result.stopReason = accounted.stopReason;
      result.costUSD = accounted.costUSD;
    }
  };

  try {
    try {
      if(services.queryCheckpoint)
        services.queryCheckpoint("query_client_creation_start");
      auto retryDeps = services.retry;
      retryDeps.getClient = [&]() -> Json {
        try { currentClient = services.createClient(); return Json::object(); }
        catch (const transport::APIError&) { throw retry::RetryFailure(transport::errorRecordFromException(std::current_exception())); }
      };
      retryDeps.yield = [&](const Json& value) { emitValue(yield, value); };
      retryDeps.operation = [&](const Json&, double attempt, const retry::RetryContext& context) -> Json {
        result.attemptNumber = attempt;
        StreamingAttemptDependencies attemptDeps;
        attemptDeps.nowMillis = services.nowMillis;
        attemptDeps.queryCheckpoint=services.queryCheckpoint;
        attemptDeps.headlessProfilerCheckpoint=services.headlessProfilerCheckpoint;
        attemptDeps.hasBatId=value::truthy(field(input.options,"agentId"));
        attemptDeps.paramsFromContext = paramsFromContext;
        attemptDeps.captureAPIRequest = captureRequest;
        attemptDeps.firstPartyProvider = preflight.capture.provider == "firstParty";
        attemptDeps.firstPartyBaseUrl = false;
        attemptDeps.injectCorrelationId = false;
        attemptDeps.openStream = [&](const Json& params, const std::optional<std::string>& id) {
          return openLiveStream(*currentClient, params, services.signal, id);
        };
        try {
          if (services.prepareStreamingAttemptSegment ||
              services.finishStreamingAttemptSegment) {
            const auto prepared=services.prepareStreamingAttemptSegment
              ? services.prepareStreamingAttemptSegment(attempt,context,
                  attemptState,attemptDeps)
              : prepareStreamingAttempt(attempt,context,attemptState,attemptDeps);
            auto response=attemptDeps.openStream(prepared.requestParams,
              prepared.clientRequestId);
            opened=std::make_shared<OpenedStream>(
              services.finishStreamingAttemptSegment
                ? services.finishStreamingAttemptSegment(attemptState,std::move(response),attemptDeps)
                : finishStreamingAttempt(attemptState,std::move(response),attemptDeps));
          } else {
            opened = std::make_shared<OpenedStream>(
              executeStreamingAttempt(attempt, context, attemptState, attemptDeps));
          }
        } catch (const transport::APIError&) { throw retry::RetryFailure(transport::errorRecordFromException(std::current_exception())); }
        return Json{{"controller", true}};
      };
      retry::RetryOptions retryOptions;
      retryOptions.model = model;
      retryOptions.thinkingConfig = input.thinkingConfig;
      retryOptions.signal = services.signal;
      retryOptions.querySource = querySource;
      const auto& fallbackModel = field(input.options, "fallbackModel");
      if (fallbackModel.is_string()) retryOptions.fallbackModel = fallbackModel.get<std::string>();
      if (preflight.isFastMode) retryOptions.fastMode = true;
      (void)(services.withRetrySegment
        ? services.withRetrySegment(retryOptions,retryDeps)
        : retry::withRetry(retryOptions,retryDeps));
      if (!opened) throw std::runtime_error("Streaming attempt returned without a stream");
      result.attemptNumber = attemptState.attemptNumber;
      result.streamRequestId = attemptState.streamRequestId;
      services.reducer.model = model;
      services.reducer.resolvedModel = preflight.resolvedModel;
      services.reducer.requestId = result.streamRequestId.value_or("");
      services.reducer.advisorModel = preflight.advisorModel.value_or("");
      services.reducer.maxOutputTokens = static_cast<int>(attemptState.maxOutputTokens);
      services.reducer.startMillis = attemptState.start;
      services.reducer.stashedMicrocompact = preflight.stashedMCEnabled;
      streaming::Reducer reducer(services.reducer);
      StreamObservation observation(services.nowMillis,services.logForDebugging,
        services.logForDiagnosticsNoPII);
      const auto idle=watchdogSettings(services.environment);
      StreamWatchdog watchdog(idle,opened->abort,
        [&]{
          if(services.logForDebugging)services.logForDebugging(
            "Streaming idle warning: no chunks received for "+
              value::numberToString(idle.warningMs/1000)+"s","warn");
          if(services.logForDiagnosticsNoPII)services.logForDiagnosticsNoPII(
            "warn","cli_streaming_idle_warning");
        },
        [&]{
          if(services.logForDebugging)services.logForDebugging(
            "Streaming idle timeout: no chunks received for "+
              value::numberToString(idle.timeoutMs/1000)+"s, aborting stream","error");
          if(services.logForDiagnosticsNoPII)services.logForDiagnosticsNoPII(
            "error","cli_streaming_idle_timeout");
        },services.scheduleWatchdogTimer);
      watchdog.reset();
      auto onFirstChunk=[&]{
        if(services.logForDebugging)
          services.logForDebugging("Stream started - received first chunk","debug");
        if(services.queryCheckpoint)
          services.queryCheckpoint("query_first_chunk_received");
        if(!value::truthy(field(input.options,"agentId")) &&
            services.headlessProfilerCheckpoint)
          services.headlessProfilerCheckpoint("first_chunk");
        if(services.endQueryProfile)services.endQueryProfile();
      };
      if(!services.startFlightActivity)
        throw std::invalid_argument("Start session activity import is required");
      services.startFlightActivity("api_call");
      try {
        if(services.consumeStreamSegment)
          services.consumeStreamSegment(*opened,reducer,yield,&watchdog,onFirstChunk,&observation);
        else consumeLiveStream(*opened,reducer,yield,&watchdog,onFirstChunk,&observation);
      } catch (const GeneratorClosed&) {
        finalize();
        return result;
      } catch (...) {
        for (const auto& message : reducer.messages()) result.newMessages.push_back(message);
        result.usage = reducer.usage();
        result.stopReason = reducer.stopReason();
        result.costUSD = reducer.costUSD();
        const auto streamingError = std::current_exception();
        if(watchdog.aborted())observation.watchdogErrorExit();
        const bool apiAbort = isAbortException(streamingError);
        if (apiAbort && services.signal.stop_requested()) {
          if(services.logForDebugging)services.logForDebugging(
            "Streaming aborted by user: "+errorMessage(streamingError),"debug");
          std::rethrow_exception(streamingError);
        }
        if (apiAbort){
          if(services.logForDebugging)services.logForDebugging(
            "Streaming timeout (SDK abort): "+errorMessage(streamingError),"error");
          throw transport::APIConnectionTimeoutError("Request timed out");
        }
        StreamingFailureInput failure;
        failure.disableFallbackEnvironment = isEnvTruthy(field(services.environment,"BATBOX_DISABLE_NONSTREAMING_FALLBACK"));
        failure.disableFallbackFeature = services.disableFallbackFeature;
        const auto action=streamingFailureAction(failure);
        if (action == StreamingFailureAction::Propagate){
          if(services.logForDebugging)services.logForDebugging(
            "Error streaming (non-streaming fallback disabled): "+
              errorMessage(streamingError),"error");
          std::rethrow_exception(streamingError);
        }
        if(services.logForDebugging)services.logForDebugging(
          "Error streaming, falling back to non-streaming mode: "+
            errorMessage(streamingError),"error");
        result.didFallBackToNonStreaming=true;
        if(services.onStreamingFallback)services.onStreamingFallback();
        fallback(is529Exception(streamingError) ? 1 : 0, result.streamRequestId);
      }
      if (!result.didFallBackToNonStreaming) {
        PostStreamInputs completedStream{input.options,reducer.usage(),
          input.messages,result.streamRequestId,&attemptState.streamResponse.headers};
        if(services.processPostStreamSegment)
          services.processPostStreamSegment(completedStream,services.postStream);
        else processCompletedStream(completedStream,services.postStream);
        for (const auto& message : reducer.messages()) result.newMessages.push_back(message);
        result.usage = reducer.usage();
        result.stopReason = reducer.stopReason();
        result.costUSD = reducer.costUSD();
      }
      completed = true;
    } catch (const retry::FallbackTriggeredError&) {
      throw;
    } catch (...) {
      auto error = std::current_exception();
      bool handled404 = false;
      try {
        std::rethrow_exception(error);
      } catch (const retry::CannotRetryError& cannot) {
        const auto& original = cannot.originalError;
        const bool api = original && original->isAPIError();
        handled404 = is404StreamCreationError(result.didFallBackToNonStreaming, true,
          api, original && original->status ? static_cast<int>(*original->status) : 0);
        if (handled404) {
          if(services.logForDebugging)services.logForDebugging(
            "Streaming endpoint returned 404, falling back to non-streaming mode","warn");
          result.didFallBackToNonStreaming=true;
          if(services.onStreamingFallback)services.onStreamingFallback();
          try {
            fallback(0, result.streamRequestId);
            completed = true;
          } catch (const retry::FallbackTriggeredError&) { throw; }
          catch (...) {
            error = std::current_exception();
            if(services.logForDebugging)services.logForDebugging(
              "Non-streaming fallback also failed: "+errorMessage(error),"error");
          }
        }
      } catch (...) {}
      if (!completed) {
        if(!handled404&&services.logForDebugging)services.logForDebugging(
          "Error in API request: "+errorMessage(error),"error");
        auto processError=[&]() -> std::optional<Json> {
          const auto failure=failureForRendering(error);
          if(failure.error.isAPIError()) {
            if(!services.extractQuotaStatusFromError)
              throw std::invalid_argument("Quota-error import is required");
            services.extractQuotaStatusFromError(failure.error);
          }
          if(isAbortException(error))return std::nullopt;
          return services.getAssistantMessageFromError(failure,errorModel(error,model));
        };
        auto errorMessage=services.processErrorSegment
          ? services.processErrorSegment(processError) : processError();
        if(errorMessage)emitValue(yield,*errorMessage);
        finalize();
        return result;
      }
    }
  } catch (...) {
    finalize();
    throw;
  }
  finalize();
  if (completed) {
    CompletionState state{preflight.stashedMCEnabled,result.streamRequestId,&input.options};
    CompletionServices completion;
    completion.stashedMicrocompactFeature=services.preflight.stashedMicrocompactFeature;
    if (!completion.stashedMicrocompactFeature)
      completion.stashedMicrocompactFeature=[enabled=services.stashedMicrocompactFeature]{
        return enabled;
      };
    completion.markGadgetsSentToAPIState=services.markGadgetsSentToAPIState;
    completion.hasBatContext=services.hasBatContext;
    completion.setLastMainRequestId=services.setLastMainRequestId;
    if (services.completeRequestSegment)
      services.completeRequestSegment(state,completion);
    else completeQueryModel(state,completion);
    if (services.addToTotalDurationState){
      const auto finalAttempt=services.nowMillis()-attemptState.start;
      const auto includingRetries=services.nowMillis()-startIncludingRetries;
      services.addToTotalDurationState(
        static_cast<double>(includingRetries),
        static_cast<double>(finalAttempt));
    }
  }
  return result;
}

} // namespace batbox::model
