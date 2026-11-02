#pragma once
#include <chrono>
#include <functional>
#include <optional>
#include <string_view>

namespace batbox::providers::cloud {
using SystemClock = std::function<std::chrono::system_clock::time_point()>;
// RFC 3339 / ISO 8601 UTC timestamps such as 2026-11-01T12:00:00Z or with an offset and fraction.
std::optional<std::chrono::system_clock::time_point> parseTimestamp(std::string_view text);
} // namespace batbox::providers::cloud
