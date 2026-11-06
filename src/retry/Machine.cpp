#include "retry/Machine.hpp"
#include "retry/Internal.hpp"
#include <algorithm>

namespace batbox::retry {
namespace {
constexpr double FLOOR_OUTPUT_TOKENS = 3000;
constexpr int MAX_529_RETRIES = 3;
constexpr double DEFAULT_FAST_MODE_FALLBACK_HOLD_MS = 30 * 60 * 1000;
constexpr double SHORT_RETRY_THRESHOLD_MS = 20 * 1000;
constexpr double MIN_COOLDOWN_MS = 10 * 60 * 1000;
std::string num(double value) { return value::stringify(Json(value)); }
}

RetryMachine::RetryMachine(const RetryOptions& options,Dependencies& deps)
    : options_(options),deps_(deps),maxRetries_(internal::getMaxRetries(options,deps)),
      consecutive529Errors_(options.initialConsecutive529Errors) {
  internal::require(deps_.getClient,"getClient");
  internal::require(deps_.operation,"operation");
  internal::require(deps_.isFastModeEnabled,"isFastModeEnabled");
  context_.model=options_.model;
  context_.thinkingConfig=options_.thinkingConfig;
  if(deps_.isFastModeEnabled())context_.fastMode=options_.fastMode;
}

RetryStep RetryMachine::advance() {
  while(true) {
    switch(phase_) {
      case Phase::Start: {
        if(attempt_>maxRetries_+1)
          throw CannotRetryError(lastError_,context_);
        internal::checkAbort(options_.signal);
        wasFastModeActive_=false;
        if(deps_.isFastModeEnabled() && context_.fastMode.value_or(false)) {
          internal::require(deps_.isFastModeCooldown,"isFastModeCooldown");
          wasFastModeActive_=!deps_.isFastModeCooldown();
        }
        try {
        const bool stale=lastError_ && internal::isStaleConnectionError(*lastError_);
        if(stale && deps_.disableKeepAliveOnStaleConnection) {
          internal::require(deps_.logForDebugging,"logForDebugging");
          deps_.logForDebugging("Stale connection (ECONNRESET/EPIPE) — disabling keep-alive for retry");
          internal::require(deps_.disableKeepAlive,"disableKeepAlive");
          deps_.disableKeepAlive();
        }
        const bool fresh=!client_ || client_->is_null() ||
          (lastError_ && lastError_->isAPIError() && lastError_->status==401) || stale;
        phase_=fresh?Phase::NeedClient:Phase::NeedOperation;
        } catch (...) {recordFailure(std::current_exception());}
        break;
      }
      case Phase::NeedClient:
        phase_=Phase::WaitingClient;
        return {RetryStep::Kind::GetClient};
      case Phase::NeedOperation:
        phase_=Phase::WaitingOperation;
        return {RetryStep::Kind::Operation,*client_,attempt_,0,context_};
      case Phase::HandleError: {
        const ErrorRecord error=*pendingError_;
        lastError_=error;
        internal::require(deps_.logForDebugging,"logForDebugging");
        deps_.logForDebugging("API error (attempt " + num(attempt_) + "/" + num(maxRetries_+1) + "): " +
          (error.isAPIError() ? (error.status ? num(*error.status) : "undefined") + " " + error.message : error.message));
        if(wasFastModeActive_ && error.isAPIError() &&
            (error.status==429 || is529Error(error))) {
          const auto overageReason=internal::headerGet(error.headers,deps_.wire.overageDisabledReason);
          if(overageReason) {
            internal::require(deps_.handleFastModeOverageRejection,"handleFastModeOverageRejection");
            deps_.handleFastModeOverageRejection(*overageReason);
            context_.fastMode=false;
            ++attempt_;phase_=Phase::Start;break;
          }
          const auto retryAfterMs=internal::getRetryAfterMs(error,deps_);
          if(retryAfterMs && *retryAfterMs<SHORT_RETRY_THRESHOLD_MS) {
            delayMs_=*retryAfterMs;
            phase_=Phase::ReadySleep;
            break;
          }
          const double cooldownMs=std::max(retryAfterMs.value_or(DEFAULT_FAST_MODE_FALLBACK_HOLD_MS),MIN_COOLDOWN_MS);
          const std::string reason=is529Error(error)?"overloaded":"rate_limit";
          internal::require(deps_.nowMs,"nowMs");
          internal::require(deps_.triggerFastModeCooldown,"triggerFastModeCooldown");
          deps_.triggerFastModeCooldown(deps_.nowMs()+cooldownMs,reason);
          if(deps_.isFastModeEnabled())context_.fastMode=false;
          ++attempt_;phase_=Phase::Start;break;
        }
        if(wasFastModeActive_ && internal::isFastModeNotEnabledError(error)) {
          internal::require(deps_.handleFastModeRejectedByAPI,"handleFastModeRejectedByAPI");
          deps_.handleFastModeRejectedByAPI();
          context_.fastMode=false;
          ++attempt_;phase_=Phase::Start;break;
        }
        if(is529Error(error) && !shouldRetry529(options_.querySource))
          throw CannotRetryError(error,context_);
        if(is529Error(error) &&
            ([&]{internal::require(deps_.isDefaultPremiumModel,"isDefaultPremiumModel");
              return deps_.isDefaultPremiumModel(options_.model);})()) {
          ++consecutive529Errors_;
          if(consecutive529Errors_>=MAX_529_RETRIES &&
              options_.fallbackModel && !options_.fallbackModel->empty())
            throw FallbackTriggeredError(options_.model,*options_.fallbackModel);
        }
        if(attempt_>maxRetries_)throw CannotRetryError(error,context_);
        if(!error.isAPIError() || !shouldRetry(error,deps_))
          throw CannotRetryError(error,context_);
        if(error.isAPIError()) {
          const auto overflow=parseMaxTokensContextOverflowError(error,deps_);
          if(overflow) {
            const double available=std::max(0.0,overflow->contextLimit-overflow->inputTokens-1000);
            if(available<FLOOR_OUTPUT_TOKENS) {
              internal::require(deps_.logError,"logError");
              deps_.logError("availableContext " + num(available) + " is less than FLOOR_OUTPUT_TOKENS " + num(FLOOR_OUTPUT_TOKENS));
              throw RetryFailure(error);
            }
            const double required=(internal::stringField(context_.thinkingConfig,"type")=="enabled" ?
              internal::field(context_.thinkingConfig,"budgetTokens").get<double>() : 0)+1;
            context_.maxTokensOverride=std::max({FLOOR_OUTPUT_TOKENS,available,required});
            ++attempt_;phase_=Phase::Start;break;
          }
        }
        const auto retryAfter=getRetryAfter(error,deps_.wire);
        delayMs_=getRetryDelay(attempt_,retryAfter,32000,deps_);
        if(deps_.maxRetryDelayMs)delayMs_=std::min(delayMs_,*deps_.maxRetryDelayMs);
        if(error.isAPIError()) {
          internal::require(deps_.createSystemAPIErrorMessage,"createSystemAPIErrorMessage");
          phase_=Phase::WaitingYield;
          return {RetryStep::Kind::Yield,
            deps_.createSystemAPIErrorMessage(error,delayMs_,attempt_,maxRetries_)};
        } else phase_=Phase::ReadySleep;
        break;
      }
      case Phase::ReadySleep:
        phase_=Phase::WaitingSleep;
        return {RetryStep::Kind::Sleep,Json(),0,delayMs_};
      case Phase::Finished:
        return {RetryStep::Kind::Complete,result_};
      default:
        throw std::logic_error("Retry machine resumed before an awaited action settled");
    }
  }
}

void RetryMachine::receivedClient(Json client) {
  if(phase_!=Phase::WaitingClient)throw std::logic_error("Client creation was not pending");
  client_=std::move(client);phase_=Phase::NeedOperation;
}
void RetryMachine::receivedOperation(Json result) {
  if(phase_!=Phase::WaitingOperation)throw std::logic_error("Retry operation was not pending");
  result_=std::move(result);phase_=Phase::Finished;
}
void RetryMachine::resumedYield() {
  if(phase_!=Phase::WaitingYield)throw std::logic_error("Retry yield was not pending");
  phase_=Phase::ReadySleep;
}
void RetryMachine::completedSleep() {
  if(phase_!=Phase::WaitingSleep)throw std::logic_error("Retry sleep was not pending");
  ++attempt_;phase_=Phase::Start;
}
void RetryMachine::failed(std::exception_ptr error) {
  if(phase_!=Phase::WaitingClient && phase_!=Phase::WaitingOperation)
    throw std::logic_error("Retry failure arrived without an awaited action");
  recordFailure(error);
}
void RetryMachine::recordFailure(std::exception_ptr error) {
  try {std::rethrow_exception(error);}
  catch(const RetryFailure& failure){pendingError_=failure.error;}
  catch(const std::logic_error&){throw;}
  catch(const std::exception& failure){
    ErrorRecord other;other.kind=ErrorRecord::Kind::Other;other.message=failure.what();
    pendingError_=std::move(other);
  }
  phase_=Phase::HandleError;
}

} // namespace batbox::retry
