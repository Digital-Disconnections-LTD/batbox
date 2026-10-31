#include "terminal_ftxui/FrameProjection.hpp"
#include "terminal_ftxui/StyleProjection.hpp"
#include "runtime/Encoding.hpp"
#include "ftxui/screen/string.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace batbox::terminal_ftxui {
namespace {

int dimension(double value) {
  if (!std::isfinite(value) || value < 0 || value > std::numeric_limits<int>::max() ||
      std::trunc(value) != value)
    throw ProjectionError("Invalid frame dimension");
  return static_cast<int>(value);
}

class FrameNode final : public ftxui::Node {
public:
  FrameNode(terminal_renderer::Frame frame,
            std::shared_ptr<terminal_screen::StylePool> styles)
      : frame_(std::move(frame)), styles_(std::move(styles)) {
    if (!frame_.screen || !styles_)
      throw ProjectionError("Frame screen and style pool required");
    width_ = dimension(frame_.screen->width);
    height_ = dimension(frame_.screen->height);
  }

  void ComputeRequirement() override {
    requirement_ = {};
    requirement_.min_x = width_;
    requirement_.min_y = height_;
  }

  void Render(ftxui::Screen& target) override {
    const int width = std::min(width_, target.dimx());
    const int height = std::min(height_, target.dimy());
    std::unordered_map<std::int32_t, ftxui::Cell> styleStash;
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const auto source = terminal_screen::cellAt(*frame_.screen, x, y);
        if (!source) throw ProjectionError("Frame cell missing inside bounds");
        auto stashed = styleStash.find(source->styleId);
        if (stashed == styleStash.end()) {
          ftxui::Cell styled;
          projectStyle(*styles_->get(source->styleId), styled);
          stashed = styleStash.emplace(source->styleId, std::move(styled)).first;
        }
        auto cell = stashed->second;
        switch (source->width) {
          case terminal_screen::CellWidth::Narrow:
            cell.character = value::encodeUtf8(source->character);
            break;
          case terminal_screen::CellWidth::Wide:
            cell.character = value::encodeUtf8(source->character);
            if (x + 1 >= width || ftxui::string_width(cell.character) != 2)
              throw ProjectionError("Fullwidth frame glyph cannot fit FTXUI cells");
            break;
          case terminal_screen::CellWidth::SpacerTail:
          case terminal_screen::CellWidth::SpacerHead:
            cell.character.clear();
            break;
        }
        if (source->hyperlink && !source->hyperlink->empty()) {
          const auto link = value::encodeUtf8(*source->hyperlink);
          cell.hyperlink = target.RegisterHyperlink(link);
          if (cell.hyperlink == 0)
            throw ProjectionError("FTXUI hyperlink table is full");
        }
        target.CellAt(x, y) = std::move(cell);
      }
    }
    const auto& cursor = frame_.cursor;
    if (cursor.visible && std::isfinite(cursor.x) && std::isfinite(cursor.y) &&
        cursor.x >= 0 && cursor.y >= 0 && cursor.x < target.dimx() &&
        cursor.y < target.dimy())
      target.SetCursor(ftxui::Screen::Cursor{
          static_cast<int>(cursor.x), static_cast<int>(cursor.y),
          ftxui::Screen::Cursor::Block});
    else
      target.SetCursor(ftxui::Screen::Cursor{0, 0,
          ftxui::Screen::Cursor::Hidden});
  }

private:
  terminal_renderer::Frame frame_;
  std::shared_ptr<terminal_screen::StylePool> styles_;
  int width_ = 0;
  int height_ = 0;
};
} // namespace

ftxui::Element projectFrame(const terminal_renderer::Frame& frame,
                            std::shared_ptr<terminal_screen::StylePool> styles) {
  return std::make_shared<FrameNode>(frame, std::move(styles));
}
} // namespace batbox::terminal_ftxui
