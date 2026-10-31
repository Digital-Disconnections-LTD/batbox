#include "retry/Internal.hpp"
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace batbox::retry {
void sleepAbortResponsive(double ms, std::stop_token signal) {
  internal::checkAbort(signal);
  if (ms <= 0 || !std::isfinite(ms)) return;
  std::condition_variable_any cv;
  std::mutex mutex;
  std::unique_lock lock(mutex);
  cv.wait_for(lock, signal, std::chrono::duration<double, std::milli>(ms), [] { return false; });
  internal::checkAbort(signal);
}
} // namespace batbox::retry
