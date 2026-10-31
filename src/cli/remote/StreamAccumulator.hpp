#pragma once
#include "runtime/JsonValue.hpp"
#include <map>
#include <memory>
#include <vector>

namespace batbox::cli::remote {
struct StreamAccumulator {
  using Chunks=std::shared_ptr<std::vector<std::string>>;
  std::map<std::string,std::map<std::string,Chunks>> byMessage;
  std::map<std::string,std::string> scopeToMessage;
};
StreamAccumulator createStreamAccumulator();
std::vector<Json> accumulateStreamEvents(const std::vector<Json>& buffer,
  StreamAccumulator& state);
void clearStreamAccumulatorForMessage(StreamAccumulator& state,
  const Json& assistant);
}
