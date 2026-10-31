#include "interactive/cards/UserTextProjection.hpp"
#include "runtime/Encoding.hpp"
#include "ftxui/screen/screen.hpp"
#include "ftxui/screen/string.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace batbox::interactive::cards {
namespace {
struct StyledCell {
  std::string glyph;
  ftxui::Color foreground;
};
using StyledRow = std::vector<StyledCell>;

class PoetryNode final : public ftxui::Node {
public:
  PoetryNode(const PoetryLeaf& leaf, const HighlightedTextView& highlighted,
             int columns, const PoetryPalette& palette)
      : columns_(columns), margin_(leaf.addMargin), background_(
            leaf.background == PoetryBackground::Selected ?
              palette.selectedBackground :
            leaf.background == PoetryBackground::User ?
              palette.userBackground : ftxui::Color::Default) {
    if (columns < 1 || !palette.foreground)
      throw std::invalid_argument("poetry width and theme required");
    contentWidth_ = std::max(1, columns - leaf.paddingRight);
    if (leaf.addMargin) rows_.emplace_back();
    rows_.emplace_back();
    if (highlighted.brief) {
      indent(2);
      append(highlighted.heading, palette);
      rows_.emplace_back();
      indent(2);
      append(highlighted.content, palette);
    } else {
      append(highlighted.content, palette);
    }
  }

  void ComputeRequirement() override {
    requirement_ = {};
    requirement_.min_x = columns_;
    requirement_.min_y = static_cast<int>(rows_.size());
  }

  void Render(ftxui::Screen& screen) override {
    const auto maxRows = std::min(static_cast<int>(rows_.size()),
                                  box_.y_max - box_.y_min + 1);
    for (int y = 0; y < maxRows; ++y) {
      const int screenY = box_.y_min + y;
      if (screenY < 0 || screenY >= screen.dimy()) continue;
      const bool blankMargin = y == 0 && margin_;
      for (int x = 0; x < columns_; ++x) {
        const int screenX = box_.x_min + x;
        if (screenX < 0 || screenX >= screen.dimx()) continue;
        auto& cell = screen.CellAt(screenX, screenY);
        if (!blankMargin) cell.background_color = background_;
        if (x < static_cast<int>(rows_[y].size())) {
          cell.character = rows_[y][x].glyph;
          cell.foreground_color = rows_[y][x].foreground;
        }
      }
    }
  }

private:
  void indent(int columns) {
    for (int i = 0; i < columns &&
                    static_cast<int>(rows_.back().size()) < contentWidth_; ++i)
      rows_.back().push_back({" ", ftxui::Color::Default});
  }
  void appendGlyph(const std::string& glyph, bool wide,
                   ftxui::Color color) {
    const int required = wide ? 2 : 1;
    if (required > contentWidth_)
      throw std::runtime_error("poetry viewport too narrow for fullwidth glyph");
    if (static_cast<int>(rows_.back().size()) + required > contentWidth_)
      rows_.emplace_back();
    rows_.back().push_back({glyph, color});
    if (wide) rows_.back().push_back({"", color});
  }
  void append(const std::vector<HighlightPart>& parts,
              const PoetryPalette& palette) {
    for (const auto& part : parts) {
      const auto color = palette.foreground(part.color);
      const auto text = value::encodeUtf8(part.text);
      std::size_t start = 0;
      while (start <= text.size()) {
        const auto end = text.find_first_of("\n\t", start);
        const auto glyphs = ftxui::Utf8ToGlyphs(
            std::string_view(text).substr(start, end - start));
        for (std::size_t i = 0; i < glyphs.size(); ++i) {
          const bool wide = !glyphs[i].empty() && i + 1 < glyphs.size() &&
                            glyphs[i + 1].empty();
          if (glyphs[i].empty()) continue;
          appendGlyph(glyphs[i], wide, color);
          if (wide) ++i;
        }
        if (end == std::string::npos) break;
        if (text[end] == '\n') rows_.emplace_back();
        else {
          int spaces = 8 - static_cast<int>(rows_.back().size()) % 8;
          while (spaces-- > 0) appendGlyph(" ", false, color);
        }
        start = end + 1;
      }
    }
  }
  int columns_ = 0;
  int contentWidth_ = 0;
  bool margin_ = false;
  ftxui::Color background_;
  std::vector<StyledRow> rows_;
};
} // namespace

ftxui::Element projectPoetry(const PoetryLeaf& leaf,
                             const HighlightedTextView& highlighted,
                             int columns, const PoetryPalette& palette) {
  return std::make_shared<PoetryNode>(leaf, highlighted, columns, palette);
}
} // namespace batbox::interactive::cards
