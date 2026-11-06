#pragma once
#include "application/Config.hpp"
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>

namespace batbox::providers {
// An endpoint's request limits with defaults filled in (docs/request-limits.md).
// Durations of 0 mean no limit.
struct RequestSettings {
  // A server on this machine or the local network, which in practice serves
  // one request at a time and pays minutes to reread a long prompt.
  bool local=false;
  // Resend the whole prompt without streaming after a failed stream.
  bool nonStreamingFallback=true;
  std::chrono::milliseconds requestTimeout{0},streamIdleTimeout{0},
    connectTimeout{0},nonStreamingTimeout{0};
  // Unset: the retry policy's own default and caps.
  std::optional<std::size_t> maxRetries;
};
// localhost, loopback, private, link-local and carrier-grade NAT addresses,
// single-label host names and .local/.lan/.home.arpa/.internal/.ts.net names.
bool localNetworkUrl(const std::string& url);
RequestSettings requestSettings(const application::InferenceProfile& profile);
} // namespace batbox::providers
