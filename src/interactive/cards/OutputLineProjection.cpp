#include "interactive/cards/OutputLineProjection.hpp"
#include "ansi_tokenize/AnsiTokenize.hpp"
#include "terminal_ftxui/StyleProjection.hpp"
#include "ftxui/screen/screen.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace batbox::interactive::cards {
namespace {
struct VisualCell {
  std::string glyph;
  ansi_tokenize::Codes codes;
  bool prefix = false;
};
using VisualRow = std::vector<VisualCell>;

class OutputLineNode final : public ftxui::Node {
public:
  OutputLineNode(const OutputLineCard& line, int columns,
                 const OutputLinePalette& palette, bool singleLine)
      : columns_(columns), palette_(palette), role_(line.colorRole),
        singleLine_(singleLine) {
    if (columns < 6)
      throw std::invalid_argument("Output line viewport too narrow");
    rows_.emplace_back();
    indent(true);
    const auto chars = ansi_tokenize::styledCharsFromTokens(
        ansi_tokenize::tokenize(line.formatted));
    for (const auto& character : chars) {
      if (character.value == "\n") {
        rows_.emplace_back();
        indent(false);
      } else if (character.value == "\t") {
        int spaces = 8 - static_cast<int>(rows_.back().size()) % 8;
        while (spaces-- > 0) append({" ", character.styles, false}, false);
      } else if (!character.value.empty()) {
        append({character.value, character.styles, false}, character.fullWidth);
      }
    }
  }

  void ComputeRequirement() override {
    requirement_ = {};
    requirement_.min_x = columns_;
    requirement_.min_y = singleLine_ ? 1 : static_cast<int>(rows_.size());
  }

  void Render(ftxui::Screen& screen) override {
    const auto limit = std::min(singleLine_ ? 1 : static_cast<int>(rows_.size()),
                                box_.y_max - box_.y_min + 1);
    for (int y = 0; y < limit; ++y) {
      const auto screenY = box_.y_min + y;
      if (screenY < 0 || screenY >= screen.dimy()) continue;
      for (int x = 0; x < static_cast<int>(rows_[y].size()); ++x) {
        const auto screenX = box_.x_min + x;
        if (screenX < 0 || screenX >= screen.dimx()) continue;
        auto& cell = screen.CellAt(screenX, screenY);
        const auto& visual = rows_[y][x];
        cell.character = visual.glyph;
        cell.foreground_color = visual.prefix ? palette_.prefix :
          role_ == "error" ? palette_.error :
          role_ == "warning" ? palette_.warning : palette_.text;
        if (!visual.prefix) {
          terminal_ftxui::projectStyle(visual.codes, cell);
          for (const auto& code : visual.codes) {
            if (!code || !code->code.starts_with("\x1b]8;")) continue;
            const auto begin = code->code.find(";;");
            if (begin == std::string::npos) continue;
            const auto end = code->code.find_first_of("\x07\x1b", begin + 2);
            if (end == std::string::npos) continue;
            const auto url = code->code.substr(begin + 2, end - begin - 2);
            cell.hyperlink = url.empty() ? 0 : screen.RegisterHyperlink(url);
          }
        }
      }
    }
  }

private:
  void indent(bool first) {
    rows_.back().push_back({" ", {}, true});
    rows_.back().push_back({" ", {}, true});
    rows_.back().push_back({first ? "⎿" : " ", {}, true});
    rows_.back().push_back({" ", {}, true});
    rows_.back().push_back({" ", {}, true});
  }
  void append(VisualCell cell, bool wide) {
    if (wide && columns_ < 7)
      throw std::invalid_argument("Output viewport too narrow for fullwidth glyph");
    if (static_cast<int>(rows_.back().size()) + (wide ? 2 : 1) > columns_) {
      rows_.emplace_back();
      indent(false);
    }
    rows_.back().push_back(std::move(cell));
    if (wide) rows_.back().push_back({"", rows_.back().back().codes, false});
  }
  int columns_ = 0;
  OutputLinePalette palette_;
  std::optional<std::string> role_;
  bool singleLine_ = false;
  std::vector<VisualRow> rows_;
};
} // namespace

ftxui::Element projectOutputLine(const OutputLineCard& line, int columns,
                                 const OutputLinePalette& palette,
                                 bool singleLine) {
  return std::make_shared<OutputLineNode>(line, columns, palette, singleLine);
}
} // namespace batbox::interactive::cards
