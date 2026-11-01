#include "terminal_ftxui/IndigenousStringMetrics.hpp"
#include "ftxui/screen/string.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace batbox::terminal_ftxui {
namespace {
int widthOf(const text_width::StringWidth& width, std::string_view text) {
  const auto measured = width.stringWidthUtf8(text);
  if (!std::isfinite(measured) || measured < 0 ||
      measured > std::numeric_limits<int>::max() ||
      std::trunc(measured) != measured)
    throw std::runtime_error("Invalid terminal grapheme width");
  return static_cast<int>(measured);
}

void appendZeroWidth(std::vector<std::string>& result, std::string_view grapheme) {
  if (result.empty()) return;
  auto last = result.end();
  do { --last; } while (last != result.begin() && last->empty());
  if (!last->empty()) *last += grapheme;
}
} // namespace

std::vector<std::string> segmentedGlyphCells(const intl::Segmenter& segmenter,
                                             const text_width::StringWidth& width,
                                             std::string_view text) {
  std::vector<std::string> result;
  auto cursor = segmenter.segment(std::string(text));
  while (const auto next = cursor.next()) {
    const auto& grapheme = next->text;
    if (grapheme.empty()) continue;
    if (grapheme.size() == 1 &&
        static_cast<unsigned char>(grapheme.front()) < 0x20 &&
        grapheme.front() != '\n') continue;
    const int cells = widthOf(width, grapheme);
    if (cells == 0) {
      appendZeroWidth(result, grapheme);
      continue;
    }
    if (cells > 2)
      throw std::runtime_error("Grapheme exceeds two terminal cells");
    result.push_back(grapheme);
    if (cells == 2) result.emplace_back();
  }
  return result;
}

std::vector<std::string> indigenousGlyphCells(const intl::Segmenter& segmenter,
                                              const text_width::StringWidth& width,
                                              std::string_view text) {
  for (const unsigned char byte : text)
    if (byte >= 0x7f) return segmentedGlyphCells(segmenter, width, text);
  // Below DEL every byte is its own grapheme except CR LF, which forms one.
  // Printable bytes take one cell; LF and CR LF have no width and join the
  // previous glyph; other lone control bytes are dropped.
  std::vector<std::string> result;
  result.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    const auto byte = static_cast<unsigned char>(text[i]);
    if (byte == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
      appendZeroWidth(result, text.substr(i, 2));
      ++i;
    } else if (byte == '\n') {
      appendZeroWidth(result, text.substr(i, 1));
    } else if (byte >= 0x20) {
      result.emplace_back(1, static_cast<char>(byte));
    }
  }
  return result;
}

void installIndigenousStringMetrics(std::shared_ptr<intl::Stash> intl,
                                std::shared_ptr<text_width::StringWidth> width) {
  if (!intl || !width)
    throw std::invalid_argument("Terminal grapheme and width providers required");
  auto segmenter = intl->getGraphemeSegmenter();
  if (!segmenter || !segmenter->segment)
    throw std::invalid_argument("Terminal grapheme segmenter required");
  ftxui::SetStringMetrics({
    [width](std::string_view text) { return widthOf(*width, text); },
    [width, segmenter](std::string_view text) {
      return indigenousGlyphCells(*segmenter, *width, text);
    }});
}
} // namespace batbox::terminal_ftxui
