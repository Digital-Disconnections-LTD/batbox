#pragma once
// Stand-in for transport::WireClient until the shared interface lands; same surface.
#include "runtime/JsonValue.hpp"
#include "transport/Client.hpp"
#include <chrono>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>

namespace batbox::providers::cloud {
class WireClient {
public:
  using OnEvent = std::function<void(const Json&)>;
  using OnResponse = std::function<void(const transport::Response&)>;
  using Timeout = std::optional<std::chrono::milliseconds>;
  virtual ~WireClient() = default;
  virtual transport::Response request(const Json& anthropicBody, bool stream, const OnEvent& onEvent = {},
                                      std::stop_token stop = {}, const OnResponse& onResponse = {},
                                      Timeout timeout = std::nullopt) = 0;
  virtual Json listModels() = 0;
  virtual std::string kind() const = 0;
};
} // namespace batbox::providers::cloud
