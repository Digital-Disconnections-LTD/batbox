#pragma once

#include "retry/Retry.hpp"
#include <exception>

namespace batbox::retry {

struct RetryStep {
  enum class Kind { GetClient, Operation, Yield, Sleep, Complete };
  Kind kind;
  Json value;
  double attempt = 0;
  double delayMs = 0;
  RetryContext context;
  RetryStep(Kind kind,Json value=Json(),double attempt=0,double delayMs=0,
            RetryContext context={})
      : kind(kind),value(std::move(value)),attempt(attempt),delayMs(delayMs),
        context(std::move(context)) {}
};

// Retry state transitions between await and yield points.
// A driver performs the requested awaited action, then resumes this machine.
class RetryMachine {
public:
  RetryMachine(const RetryOptions& options, Dependencies& deps);
  RetryStep advance();
  void receivedClient(Json client);
  void receivedOperation(Json result);
  void resumedYield();
  void completedSleep();
  void failed(std::exception_ptr error);
  std::stop_token signal() const { return options_.signal; }
private:
  enum class Phase {
    Start, NeedClient, NeedOperation, HandleError, ReadySleep, WaitingClient,
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
  double attempt_ = 1;
  double delayMs_ = 0;
  bool wasFastModeActive_ = false;
  Phase phase_ = Phase::Start;
  Json result_;
  void recordFailure(std::exception_ptr error);
};

// Direct synchronous composition of the retry transition machine.
Json withRetryMachine(const RetryOptions& options,Dependencies& deps);

} // namespace batbox::retry
