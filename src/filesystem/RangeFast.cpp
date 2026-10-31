#include "filesystem/Range.hpp"
#include <limits>
#include <vector>

namespace batbox::filesystem {

ReadFileRangeResult readFileInRangeFast(const std::string& raw, double mtimeMs,
  std::size_t offset, std::optional<std::size_t> maxLines,
  std::optional<std::size_t> truncateAtBytes) {
  const std::size_t endLine = maxLines && *maxLines <= std::numeric_limits<std::size_t>::max() - offset
    ? offset + *maxLines : std::numeric_limits<std::size_t>::max();
  const std::string text = raw.rfind("\xef\xbb\xbf", 0) == 0 ? raw.substr(3) : raw;
  std::vector<std::string> selected;
  std::size_t lineIndex = 0, start = 0, selectedBytes = 0;
  bool truncated = false;
  const auto tryPush = [&](std::string line) {
    if (truncateAtBytes) {
      const std::size_t nextBytes = selectedBytes + (selected.empty() ? 0 : 1) + line.size();
      if (nextBytes > *truncateAtBytes) { truncated = true; return; }
      selectedBytes = nextBytes;
    }
    selected.push_back(std::move(line));
  };
  std::size_t newline;
  while ((newline = text.find('\n', start)) != std::string::npos) {
    if (lineIndex >= offset && lineIndex < endLine && !truncated) {
      std::string line = text.substr(start, newline - start);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      tryPush(std::move(line));
    }
    ++lineIndex;
    start = newline + 1;
  }
  if (lineIndex >= offset && lineIndex < endLine && !truncated) {
    std::string line = text.substr(start);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    tryPush(std::move(line));
  }
  ++lineIndex;
  std::string content;
  for (const auto& line : selected) {
    if (!content.empty() || &line != &selected.front()) content += '\n';
    content += line;
  }
  return {content, selected.size(), lineIndex, text.size(), content.size(), mtimeMs, truncated};
}

} // namespace batbox::filesystem
