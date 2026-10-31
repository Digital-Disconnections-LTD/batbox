#include "retry/Machine.hpp"

namespace batbox::retry {

Json withRetry(const RetryOptions& options,Dependencies& deps) {
  return withRetryMachine(options,deps);
}

} // namespace batbox::retry
