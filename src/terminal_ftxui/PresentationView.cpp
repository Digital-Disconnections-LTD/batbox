#include "terminal_ftxui/PresentationView.hpp"
#include "interactive/TranscriptRows.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/screen/screen.hpp"
#include <algorithm>
#include <memory>
#include <utility>

namespace batbox::terminal_ftxui {
using Kind = interactive::TranscriptRow::Kind;

namespace {
InlinePalette& inlinePalette() {
  static InlinePalette palette;
  return palette;
}
} // namespace

void setInlinePalette(const InlinePalette& palette) { inlinePalette() = palette; }

ftxui::Color colorFor(Kind kind) {
  const auto& palette = inlinePalette();
  switch (kind) {
    case Kind::User: return palette.user;
    case Kind::Assistant: return palette.assistant;
    case Kind::Reasoning: return palette.reasoning;
    case Kind::Gadget: return palette.gadget;
    case Kind::GadgetResult: return palette.gadgetResult;
    case Kind::Progress: return palette.progress;
    case Kind::System: return palette.system;
    case Kind::Error: return palette.error;
  }
  return ftxui::Color::Default;
}

namespace {

class PresentationNode final : public ftxui::Node {
public:
  PresentationNode(PresentationLayout layout, ftxui::Dimensions dimensions)
      : layout_(std::move(layout)), dimensions_(dimensions) {}

  void ComputeRequirement() override {
    requirement_ = {};
    requirement_.min_x = dimensions_.dimx;
    requirement_.min_y = dimensions_.dimy;
  }

  void Render(ftxui::Screen& screen) override {
    const auto height = std::min(screen.dimy(),
        static_cast<int>(layout_.visibleLines.size()));
    for (int y = 0; y < height; ++y) {
      const auto& line = layout_.visibleLines[y];
      const auto width = std::min(screen.dimx(),
          static_cast<int>(line.cells.size()));
      for (int x = 0; x < screen.dimx(); ++x) {
        auto& cell = screen.CellAt(x, y);
        if (x < width) cell.character = line.cells[x];
        if (x < width || line.selected) {
          cell.foreground_color = colorFor(line.kind);
          cell.inverted = line.selected;
        }
      }
    }
    if (layout_.caretVisible && layout_.caretX >= 0 &&
        layout_.caretY >= 0 && layout_.caretX < screen.dimx() &&
        layout_.caretY < screen.dimy())
      screen.SetCursor({layout_.caretX, layout_.caretY,
          ftxui::Screen::Cursor::Bar});
    else screen.SetCursor({0, 0, ftxui::Screen::Cursor::Hidden});
  }

private:
  PresentationLayout layout_;
  ftxui::Dimensions dimensions_;
};
} // namespace

ftxui::Element renderPresentation(const interactive::PresentationState& state,
                                  ftxui::Dimensions dimensions) {
  return std::make_shared<PresentationNode>(
      layoutPresentation(state, dimensions.dimx, dimensions.dimy), dimensions);
}

ftxui::Element renderPresentation(const interactive::PresentationState& state,
                                  ftxui::Dimensions dimensions,
                                  const std::unordered_set<std::string>& excluded) {
  return std::make_shared<PresentationNode>(
      layoutPresentation(state, dimensions.dimx, dimensions.dimy, &excluded), dimensions);
}

std::string drainPresentationPlain(const interactive::PresentationState& state,
                                   PlainCursor& cursor) {
  std::string output;
  for (const auto& message : state.messages) {
    if (!message || cursor.emittedIds.contains(message.get())) continue;
    interactive::PresentationState one;
    one.messages.push_back(message);
    interactive::rebuildTranscriptRows(one);
    for (const auto& row : one.transcriptRows) {
      output += row.text;
      if (output.empty() || output.back() != '\n') output += '\n';
    }
    cursor.emitted.push_back(message);
    cursor.emittedIds.insert(message.get());
  }
  return output;
}
} // namespace batbox::terminal_ftxui
