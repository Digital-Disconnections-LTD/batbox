#pragma once
#include "application/Config.hpp"
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>

namespace batbox::providers {
// Defaults for every endpoint (docs/request-limits.md). 0 means no limit.
inline constexpr std::chrono::milliseconds kDefaultRequestTimeout{0};
inline constexpr std::chrono::milliseconds kDefaultStreamIdleTimeout=std::chrono::hours(6);
inline constexpr std::chrono::milliseconds kDefaultConnectTimeout=std::chrono::minutes(2);
inline constexpr std::chrono::milliseconds kDefaultNonStreamingTimeout{0};
// An endpoint's request limits with defaults filled in (docs/request-limits.md).
// Durations of 0 mean no limit.
struct RequestSettings {
  // A server on this machine or the local network, which in practice serves
  // one request at a time and pays minutes to reread a long prompt. It sets
  // retry counts and memory recall, never a time limit.
  bool local=false;
  // Resend the whole prompt without streaming after a failed stream.
  bool nonStreamingFallback=false;
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
