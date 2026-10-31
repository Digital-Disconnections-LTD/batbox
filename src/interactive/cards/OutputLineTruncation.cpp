#include "interactive/cards/OutputLineCard.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace batbox::interactive::cards {
namespace {
bool space(char16_t c) {
  return (c >= 9 && c <= 13) || c == 0x20 || c == 0xa0 || c == 0x1680 ||
    (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 ||
    c == 0x202f || c == 0x205f || c == 0x3000 || c == 0xfeff;
}
std::string trimEnd(const std::string& text) {
  auto units = value::utf16CodeUnits(text);
  while (!units.empty() && space(units.back())) units.pop_back();
  return value::fromUtf16(units);
}
std::vector<std::string> splitLines(const std::string& content) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start <= content.size()) {
    const auto end = content.find('\n', start);
    lines.push_back(content.substr(start, end - start));
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return lines;
}
std::string join(const std::vector<std::string>& lines,
                 std::size_t count) {
  std::string result;
  for (std::size_t i = 0; i < std::min(lines.size(), count); ++i) {
    if (i) result.push_back('\n');
    result += lines[i];
  }
  return trimEnd(result);
}
struct WrappedText {
  std::string aboveTheFold;
  long remainingLines = 0;
};
WrappedText wrapText(const std::string& text, int width,
                     const OutputLineImports& imports) {
  if (!imports.stringWidth || !imports.sliceAnsi)
    throw std::logic_error("Output width and ANSI slice services required");
  std::vector<std::string> wrapped;
  for (const auto& line : splitLines(text)) {
    const auto visibleWidth = imports.stringWidth(line);
    if (visibleWidth <= width) wrapped.push_back(trimEnd(line));
    else for (double position = 0; position < visibleWidth; position += width)
      wrapped.push_back(trimEnd(imports.sliceAnsi(line,
          position, position + width)));
  }
  const long remaining = static_cast<long>(wrapped.size()) - 3;
  if (remaining == 1) return {join(wrapped, 4), 0};
  return {join(wrapped, 3), std::max(0L, remaining)};
}
} // namespace

std::string renderTruncatedContent(const std::string& content,
                                    int terminalWidth, bool suppressExpandHint,
                                    const OutputLineImports& imports) {
  const auto trimmed = trimEnd(content);
  if (trimmed.empty()) return "";
  const int wrapWidth = std::max(terminalWidth - 10, 10);
  const auto maxChars = static_cast<std::size_t>(3 * wrapWidth * 4);
  const auto units = value::utf16CodeUnits(trimmed);
  const bool preTruncated = units.size() > maxChars;
  const auto contentForWrapping = preTruncated ?
      value::fromUtf16(units.substr(0, maxChars)) : trimmed;
  const auto wrapped = wrapText(contentForWrapping, wrapWidth, imports);
  const long estimatedRemaining = preTruncated ?
      std::max(wrapped.remainingLines,
        static_cast<long>(std::ceil(static_cast<double>(units.size()) /
                                     wrapWidth)) - 3) : wrapped.remainingLines;
  if (estimatedRemaining <= 0) return wrapped.aboveTheFold;
  if (!imports.dim || (!suppressExpandHint && !imports.expandHint))
    throw std::logic_error("Output dim and expand hint services required");
  const std::string suffix = "… +" + std::to_string(estimatedRemaining) +
      " lines" + (suppressExpandHint ? "" : " " + imports.expandHint());
  const auto dimmed = imports.dim(suffix);
  return wrapped.aboveTheFold.empty() ? dimmed :
      wrapped.aboveTheFold + "\n" + dimmed;
}
} // namespace batbox::interactive::cards
