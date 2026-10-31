#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace batbox::cli::remote {
struct SSEFrame {
  std::optional<std::string> event,id,data;
};
struct SSEParseResult {
  std::vector<SSEFrame> frames;
  std::string remaining;
};
SSEParseResult parseSSEFrames(std::string_view buffer);
}
