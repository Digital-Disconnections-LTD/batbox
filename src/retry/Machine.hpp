#pragma once

#include "retry/Retry.hpp"
#include <exception>

namespace batbox::retry {

struct RetryStep {
  enum class Kind { RefreshOAuth, GetClient, Operation, Yield, Sleep, Complete };
  Kind kind;
  Json value;
  double attempt = 0;
  double delayMs = 0;
  RetryContext context;
  std::string accessToken;
  RetryStep(Kind kind,Json value=Json(),double attempt=0,double delayMs=0,
            RetryContext context={},std::string accessToken={})
      : kind(kind),value(std::move(value)),attempt(attempt),delayMs(delayMs),
        context(std::move(context)),accessToken(std::move(accessToken)) {}
};

// Retry state transitions between await and yield points.
// A driver performs the requested awaited action, then resumes this machine.
class RetryMachine {
public:
  RetryMachine(const RetryOptions& options, Dependencies& deps);
  RetryStep advance();
  void refreshedOAuth();
  void receivedClient(Json client);
  void receivedOperation(Json result);
  void resumedYield();
  void completedSleep();
  void failed(std::exception_ptr error);
  std::stop_token signal() const { return options_.signal; }
private:
  enum class Phase {
    Start, NeedClient, NeedOperation, HandleError, PersistentLoop,
    ReadyPersistentSleep, ReadyOrdinarySleep, WaitingOAuth, WaitingClient,
    WaitingOperation, WaitingYield, WaitingSleep, Finished
  };
  RetryOptions options_;
  Dependencies& deps_;
  double maxRetries_ = 0;
  RetryContext context_;
  std::optional<Json> client_;
  int consecutive529Errors_ = 0;
  std::optional<ErrorRecord> lastError_;
  std::optional<ErrorRecord> pendingError_;
  double persistentAttempt_ = 0;
  double attempt_ = 1;
  double delayMs_ = 0;
  double remainingMs_ = 0;
  double sleepChunkMs_ = 0;
  double reportedAttempt_ = 0;
  bool wasFastModeActive_ = false;
  bool persistent_ = false;
  bool pendingYieldPersistent_ = false;
  Phase phase_ = Phase::Start;
  Json result_;
  void recordFailure(std::exception_ptr error);
};

// Direct synchronous composition of the retry transition machine.
Json withRetryMachine(const RetryOptions& options,Dependencies& deps);

} // namespace batbox::retry
