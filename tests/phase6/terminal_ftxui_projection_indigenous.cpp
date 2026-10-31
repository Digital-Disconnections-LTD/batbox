#include "terminal_ftxui/FrameProjection.hpp"
#include "terminal_ftxui/StyleProjection.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

namespace ts = batbox::terminal_screen;
namespace tr = batbox::terminal_renderer;
namespace tf = batbox::terminal_ftxui;
namespace ansi = batbox::ansi_tokenize;

static ansi::CodeRef code(std::string value) {
  return std::make_shared<ansi::AnsiCode>(ansi::AnsiCode{
      "ansi", std::move(value), "\x1b[0m"});
}

int main() {
  ftxui::Cell color;
  tf::projectStyle({code("\x1b[38;2;0;12;200m")}, color);
  assert(color.foreground_color == ftxui::Color::RGB(0, 12, 200));
  tf::projectStyle({code("\x1b[48:2::1:2:3m")}, color);
  assert(color.background_color == ftxui::Color::RGB(1, 2, 3));
  tf::projectStyle({code("\x1b[38;5;201m")}, color);
  assert(color.foreground_color ==
         ftxui::Color(static_cast<ftxui::Color::Palette256>(201)));
  color.character = "keep";
  color.hyperlink = 7;
  tf::projectStyle({code("\x1b[0m"), code("\x1b[4:2m")}, color);
  assert(color.character == "keep" && color.hyperlink == 7);
  assert(color.underlined_double && !color.underlined);
  bool unsupported = false;
  try { tf::projectStyle({code("\x1b[53m")}, color); }
  catch (const tf::ProjectionError&) { unsupported = true; }
  assert(unsupported);
  unsupported = false;
  try { tf::projectStyle({code("\x1b[4:3m")}, color); }
  catch (const tf::ProjectionError&) { unsupported = true; }
  assert(unsupported);

  const auto styles = std::make_shared<ts::StylePool>();
  const auto chars = std::make_shared<ts::CharPool>();
  const auto links = std::make_shared<ts::HyperlinkPool>();
  auto screen = ts::createScreen(4, 2, styles, chars, links, {});
  const auto boldRed = styles->intern(std::make_shared<ansi::Codes>(
      ansi::Codes{code("\x1b[1m"), code("\x1b[31m")}));
  ts::setCellAt(*screen, 0, 0, {"A", boldRed, ts::CellWidth::Narrow, {}});
  ts::setCellAt(*screen, 1, 0, {"界", 0, ts::CellWidth::Wide, {}});
  ts::setCellAt(*screen, 3, 0,
                {"B", 0, ts::CellWidth::Narrow, "https://example.test"});
  tr::Frame frame;
  frame.screen = screen;
  frame.viewport = {4, 2};
  frame.cursor = {3, 0, true};
  ftxui::Screen drawn(4, 2);
  ftxui::Render(drawn, tf::projectFrame(frame, styles));
  assert(drawn.CellAt(0, 0).character == "A");
  assert(drawn.CellAt(0, 0).bold);
  assert(drawn.CellAt(0, 0).foreground_color == ftxui::Color::Red);
  assert(drawn.CellAt(1, 0).character == "界");
  assert(drawn.CellAt(2, 0).character.empty());
  assert(drawn.CellAt(3, 0).character == "B");
  assert(drawn.Hyperlink(drawn.CellAt(3, 0).hyperlink) ==
         "https://example.test");
  assert(drawn.cursor().x == 3 && drawn.cursor().y == 0);
  assert(drawn.cursor().shape == ftxui::Screen::Cursor::Block);
  const auto terminalBytes = drawn.ToString();
  assert(terminalBytes.find("界") != std::string::npos);
  assert(terminalBytes.find("B") != std::string::npos);

  frame.cursor.visible = false;
  ftxui::Screen hidden(4, 2);
  ftxui::Render(hidden, tf::projectFrame(frame, styles));
  assert(hidden.cursor().shape == ftxui::Screen::Cursor::Hidden);

  bool clippedWide = false;
  try {
    ftxui::Screen clipped(2, 2);
    ftxui::Render(clipped, tf::projectFrame(frame, styles));
  } catch (const tf::ProjectionError&) { clippedWide = true; }
  assert(clippedWide);
  std::cout << "10 native FTXUI frame/style projection checks pass\n";
}
