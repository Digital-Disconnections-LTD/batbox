#include "retry/Machine.hpp"
#include "retry/Internal.hpp"

namespace batbox::retry {

Json withRetryMachine(const RetryOptions& options,Dependencies& deps) {
  RetryMachine machine(options,deps);
  while(true) {
    const RetryStep step=machine.advance();
    switch(step.kind) {
      case RetryStep::Kind::RefreshOAuth:
        try {
          internal::require(deps.handleOAuth401Error,"handleOAuth401Error");
          deps.handleOAuth401Error(step.accessToken);
          machine.refreshedOAuth();
        } catch (...) {machine.failed(std::current_exception());}
        break;
      case RetryStep::Kind::GetClient:
        try {machine.receivedClient(deps.getClient());}
        catch (...) {machine.failed(std::current_exception());}
        break;
      case RetryStep::Kind::Operation:
        try {machine.receivedOperation(deps.operation(step.value,step.attempt,step.context));}
        catch (...) {machine.failed(std::current_exception());}
        break;
      case RetryStep::Kind::Yield:
        internal::require(deps.yield,"yield");
        deps.yield(step.value);
        machine.resumedYield();
        break;
      case RetryStep::Kind::Sleep:
        internal::checkAbort(options.signal);
        if(deps.sleep)deps.sleep(step.delayMs,options.signal);
        else sleepAbortResponsive(step.delayMs,options.signal);
        internal::checkAbort(options.signal);
        machine.completedSleep();
        break;
      case RetryStep::Kind::Complete:
        return step.value;
    }
  }
}

} // namespace batbox::retry
