#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>

namespace batbox::transport {
struct SdkRetryAuth {
  bool usedTokenStash=false,didRefreshFor401=false;
  std::function<void()> invalidateTokenStash;
};
struct SdkRetryClock {
  std::function<double()> random,nowMillis;
  std::function<double(const std::string&)> parseDate;
};
bool sdkShouldRetry(long status,const Json& headers,SdkRetryAuth& auth);
double sdkDefaultRetryDelay(int remaining,int maximum,double random);
double sdkRetryDelay(const Json& headers,int remaining,int maximum,
  const SdkRetryClock& clock);
SdkRetryClock indigenousSdkRetryClock();
} // namespace batbox::transport
