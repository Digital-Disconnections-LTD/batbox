#include "retry/Machine.hpp"
#include "retry/Internal.hpp"
#include <algorithm>

namespace batbox::retry {
namespace {
constexpr double FLOOR_OUTPUT_TOKENS = 3000;
constexpr int MAX_529_RETRIES = 3;
constexpr double PERSISTENT_MAX_BACKOFF_MS = 5 * 60 * 1000;
constexpr double PERSISTENT_RESET_CAP_MS = 6 * 60 * 60 * 1000;
constexpr double HEARTBEAT_INTERVAL_MS = 30000;
constexpr double DEFAULT_FAST_MODE_FALLBACK_HOLD_MS = 30 * 60 * 1000;
constexpr double SHORT_RETRY_THRESHOLD_MS = 20 * 1000;
constexpr double MIN_COOLDOWN_MS = 10 * 60 * 1000;
constexpr const char* REPEATED_529_ERROR_MESSAGE = "Repeated 529 Overloaded errors";
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
          (lastError_ && lastError_->isAPIError() && lastError_->status==401) ||
          (lastError_ && internal::isOAuthTokenRevokedError(*lastError_)) ||
          (lastError_ && internal::isBedrockAuthError(*lastError_,deps_)) ||
          (lastError_ && internal::isVertexAuthError(*lastError_,deps_)) || stale;
        if(fresh) {
          if(lastError_ && ((lastError_->isAPIError() && lastError_->status==401) ||
              internal::isOAuthTokenRevokedError(*lastError_))) {
            internal::require(deps_.getOAuthAccessToken,"getOAuthAccessToken");
            auto token=deps_.getOAuthAccessToken();
            if(token && !token->empty()) {
              phase_=Phase::WaitingOAuth;
              return {RetryStep::Kind::RefreshOAuth,Json(),attempt_,0,context_,*token};
            }
          }
          phase_=Phase::NeedClient;
        } else phase_=Phase::NeedOperation;
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
        if(wasFastModeActive_ && !isPersistentRetryEnabled(deps_) && error.isAPIError() &&
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
            persistent_=false;
            phase_=Phase::ReadyOrdinarySleep;
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
        if(is529Error(error) && !shouldRetry529(options_.querySource,deps_.bashClassifierFeature))
          throw CannotRetryError(error,context_);
        if(is529Error(error) &&
            (!internal::envString(deps_,"BATBOX_FALLBACK_FOR_ALL_PRIMARY_MODELS").empty() ||
             ([&]{internal::require(deps_.isSubscriber,"isSubscriber");
               if(deps_.isSubscriber())return false;
               internal::require(deps_.isDefaultPremiumModel,"isDefaultPremiumModel");
               return deps_.isDefaultPremiumModel(options_.model);})())) {
          ++consecutive529Errors_;
          if(consecutive529Errors_>=MAX_529_RETRIES) {
            if(options_.fallbackModel && !options_.fallbackModel->empty())
              throw FallbackTriggeredError(options_.model,*options_.fallbackModel);
            if(internal::envString(deps_,"BATBOX_USER_TYPE")=="external" &&
                internal::envString(deps_,"BATBOX_IS_SANDBOX").empty() && !isPersistentRetryEnabled(deps_)) {
              ErrorRecord repeated;repeated.message=REPEATED_529_ERROR_MESSAGE;
              throw CannotRetryError(repeated,context_);
            }
          }
        }
        persistent_=isPersistentRetryEnabled(deps_) && internal::isTransientCapacityError(error);
        if(attempt_>maxRetries_ && !persistent_)throw CannotRetryError(error,context_);
        const bool cloud=internal::handleAwsCredentialError(error,deps_) ||
          internal::handleGcpCredentialError(error,deps_);
        if(!cloud && (!error.isAPIError() || !shouldRetry(error,deps_)))
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
        if(persistent_ && error.isAPIError() && error.status==429) {
          ++persistentAttempt_;
          const auto reset=internal::getRateLimitResetDelayMs(error,deps_);
          delayMs_=reset?*reset:std::min(getRetryDelay(persistentAttempt_,retryAfter,PERSISTENT_MAX_BACKOFF_MS,deps_),PERSISTENT_RESET_CAP_MS);
        } else if(persistent_) {
          ++persistentAttempt_;
          delayMs_=std::min(getRetryDelay(persistentAttempt_,retryAfter,PERSISTENT_MAX_BACKOFF_MS,deps_),PERSISTENT_RESET_CAP_MS);
        } else delayMs_=getRetryDelay(attempt_,retryAfter,32000,deps_);
        reportedAttempt_=persistent_?persistentAttempt_:attempt_;
        if(persistent_) {
          remainingMs_=delayMs_;
          phase_=Phase::PersistentLoop;
        } else if(error.isAPIError()) {
          internal::require(deps_.createSystemAPIErrorMessage,"createSystemAPIErrorMessage");
          phase_=Phase::WaitingYield;
          pendingYieldPersistent_=false;
          return {RetryStep::Kind::Yield,
            deps_.createSystemAPIErrorMessage(error,delayMs_,attempt_,maxRetries_)};
        } else phase_=Phase::ReadyOrdinarySleep;
        break;
      }
      case Phase::PersistentLoop:
        if(remainingMs_<=0) {
          if(attempt_>=maxRetries_)attempt_=maxRetries_;
          ++attempt_;phase_=Phase::Start;break;
        }
        internal::checkAbort(options_.signal);
        if(pendingError_->isAPIError()) {
          internal::require(deps_.createSystemAPIErrorMessage,"createSystemAPIErrorMessage");
          phase_=Phase::WaitingYield;
          pendingYieldPersistent_=true;
          return {RetryStep::Kind::Yield,
            deps_.createSystemAPIErrorMessage(*pendingError_,remainingMs_,reportedAttempt_,maxRetries_)};
        }
        phase_=Phase::ReadyPersistentSleep;
        break;
      case Phase::ReadyPersistentSleep:
        sleepChunkMs_=std::min(remainingMs_,HEARTBEAT_INTERVAL_MS);
        phase_=Phase::WaitingSleep;
        return {RetryStep::Kind::Sleep,Json(),0,sleepChunkMs_};
      case Phase::ReadyOrdinarySleep:
        phase_=Phase::WaitingSleep;
        return {RetryStep::Kind::Sleep,Json(),0,delayMs_};
      case Phase::Finished:
        return {RetryStep::Kind::Complete,result_};
      default:
        throw std::logic_error("Retry machine resumed before an awaited action settled");
    }
  }
}

void RetryMachine::refreshedOAuth() {
  if(phase_!=Phase::WaitingOAuth)throw std::logic_error("OAuth refresh was not pending");
  phase_=Phase::NeedClient;
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
  phase_=pendingYieldPersistent_?Phase::ReadyPersistentSleep:Phase::ReadyOrdinarySleep;
}
void RetryMachine::completedSleep() {
  if(phase_!=Phase::WaitingSleep)throw std::logic_error("Retry sleep was not pending");
  if(persistent_) {
    remainingMs_-=sleepChunkMs_;phase_=Phase::PersistentLoop;
  } else {++attempt_;phase_=Phase::Start;}
}
void RetryMachine::failed(std::exception_ptr error) {
  if(phase_!=Phase::WaitingOAuth && phase_!=Phase::WaitingClient &&
      phase_!=Phase::WaitingOperation)
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
