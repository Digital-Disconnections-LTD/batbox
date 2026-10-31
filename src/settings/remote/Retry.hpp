#pragma once
#include "settings/remote/FetchTypes.hpp"

namespace batbox::settings::remote {
struct RetryServices {
  std::function<event_loop::Deferred<FetchResult>(
    const std::optional<std::string>&)> fetchOne;
  std::function<double(int)> getRetryDelay;
  std::function<event_loop::Deferred<event_loop::Unit>(double)> sleep;
  std::function<void(const std::string&)> debug;
  int maxRetries=5;
};
event_loop::Deferred<FetchResult> fetchWithRetry(event_loop::Loop& loop,
  const std::optional<std::string>& stashedChecksum,
  const RetryServices& services);
}
