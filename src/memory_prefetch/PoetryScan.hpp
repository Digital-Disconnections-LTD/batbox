#pragma once
#include "runtime/JsonValue.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace batbox::memory_prefetch {
struct SurfacedMemories {
  std::vector<std::string> paths;
  std::size_t totalBytes=0;
};
SurfacedMemories collectSurfacedMemories(const Json& messages);
std::vector<std::string> collectRecentSuccessfulGadgets(const Json& messages,
  std::size_t lastUserIndex);
std::vector<std::string> extractBatMentions(const std::string& content);
} // namespace batbox::memory_prefetch
