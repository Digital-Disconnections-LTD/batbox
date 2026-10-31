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
  const auto measured = width.stringWidth(Json(std::string(text)));
  if (!std::isfinite(measured) || measured < 0 ||
      measured > std::numeric_limits<int>::max() ||
      std::trunc(measured) != measured)
    throw std::runtime_error("Invalid terminal grapheme width");
  return static_cast<int>(measured);
}

std::vector<std::string> glyphCells(const intl::Segmenter& segmenter,
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
      if (!result.empty()) {
        auto last = result.end();
        do { --last; } while (last != result.begin() && last->empty());
        if (!last->empty()) *last += grapheme;
      }
      continue;
    }
    if (cells > 2)
      throw std::runtime_error("Grapheme exceeds two terminal cells");
    result.push_back(grapheme);
    if (cells == 2) result.emplace_back();
  }
  return result;
}
} // namespace

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
      return glyphCells(*segmenter, *width, text);
    }});
}
} // namespace batbox::terminal_ftxui
