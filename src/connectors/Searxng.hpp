#pragma once
#include "runtime/JsonValue.hpp"
#include "connectors/Abort.hpp"
#include <chrono>
#include <string>
namespace batbox {
// SearXNG search connector with cancellation and request timeouts.
class Searxng {
  std::string endpoint;
public:
  explicit Searxng(std::string endpoint = "http://127.0.0.1:8888/search");
  Json search(const Json& input, const std::string& gadgetUseId,
    std::chrono::milliseconds timeout = std::chrono::minutes(2),std::stop_token signal={}) const;
};
} // namespace batbox
