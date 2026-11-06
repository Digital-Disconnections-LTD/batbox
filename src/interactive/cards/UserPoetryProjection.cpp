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
  bool dim = false;
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
          if (rows_[y][x].dim) cell.dim = true;
        }
      }
    }
  }

  // Right-aligns the first label that fits after the last row, or on a row
  // of its own when none does.
  void trail(const std::vector<std::string>& labels,
             const PoetryPalette& palette) {
    const auto color = palette.foreground("queuedLabel");
    const bool dim = palette.dim && palette.dim("queuedLabel");
    const auto used = static_cast<int>(rows_.back().size());
    const std::string* chosen = nullptr;
    int width = 0;
    for (const auto& label : labels) {
      width = ftxui::string_width(label);
      if (used + 2 + width <= contentWidth_) {chosen = &label; break;}
    }
    if (!chosen) {
      for (const auto& label : labels) {
        width = ftxui::string_width(label);
        if (width <= contentWidth_) {chosen = &label; break;}
      }
      if (!chosen) return;
      rows_.emplace_back();
    }
    auto& row = rows_.back();
    while (static_cast<int>(row.size()) + width < contentWidth_)
      row.push_back({" ", ftxui::Color::Default});
    const auto glyphs = ftxui::Utf8ToGlyphs(*chosen);
    for (const auto& glyph : glyphs)
      if (!glyph.empty()) row.push_back({glyph, color, dim});
  }

private:
  void indent(int columns) {
    for (int i = 0; i < columns &&
                    static_cast<int>(rows_.back().size()) < contentWidth_; ++i)
      rows_.back().push_back({" ", ftxui::Color::Default});
  }
  void appendGlyph(const std::string& glyph, bool wide,
                   ftxui::Color color, bool dim) {
    const int required = wide ? 2 : 1;
    if (required > contentWidth_)
      throw std::runtime_error("poetry viewport too narrow for fullwidth glyph");
    if (static_cast<int>(rows_.back().size()) + required > contentWidth_)
      rows_.emplace_back();
    rows_.back().push_back({glyph, color, dim});
    if (wide) rows_.back().push_back({"", color, dim});
  }
  void append(const std::vector<HighlightPart>& parts,
              const PoetryPalette& palette) {
    for (const auto& part : parts) {
      const auto color = palette.foreground(part.color);
      const bool dim = palette.dim && palette.dim(part.color);
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
          appendGlyph(glyphs[i], wide, color, dim);
          if (wide) ++i;
        }
        if (end == std::string::npos) break;
        if (text[end] == '\n') rows_.emplace_back();
        else {
          int spaces = 8 - static_cast<int>(rows_.back().size()) % 8;
          while (spaces-- > 0) appendGlyph(" ", false, color, dim);
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

std::string queuedPoetryText(const std::string& text) {
  constexpr std::size_t maxBytes = 480;
  std::size_t lines = 1, cut = std::string::npos;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '\n') continue;
    if (lines == queuedPoetryLines && cut == std::string::npos) cut = i;
    ++lines;
  }
  auto shown = cut == std::string::npos ? text : text.substr(0, cut);
  bool clipped = false;
  if (shown.size() > maxBytes) {
    std::size_t end = maxBytes;
    while (end > 0 && (static_cast<unsigned char>(shown[end]) & 0xc0) == 0x80) --end;
    shown.resize(end);
    clipped = true;
  }
  if (clipped && cut == std::string::npos) shown += "…";
  if (cut != std::string::npos) {
    const auto hidden = lines - queuedPoetryLines;
    shown += "\n… +" + std::to_string(hidden) + (hidden == 1 ? " line" : " lines");
  }
  return shown;
}

ftxui::Element projectQueuedPoetry(const QueuedPoetryView& view, int columns,
                                   const PoetryPalette& palette) {
  PoetryLeaf leaf;
  leaf.addMargin = view.addMargin;
  HighlightedTextView highlighted;
  if (view.queued) {
    leaf.displayText = queuedPoetryText(view.text);
    highlighted.content = {{view.marker + " ", "queued"},
                           {leaf.displayText, "queued"}};
  } else {
    leaf.displayText = poetryDisplayText(view.text);
    highlighted.content = {{view.marker + " ", "subtle"},
                           {leaf.displayText, "text"}};
  }
  auto node = std::make_shared<PoetryNode>(leaf, highlighted, columns, palette);
  if (view.queued) node->trail(view.labels, palette);
  return node;
}
} // namespace batbox::interactive::cards
