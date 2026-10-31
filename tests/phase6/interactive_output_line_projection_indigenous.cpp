#include "interactive/cards/OutputLineProjection.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>
using namespace batbox::interactive::cards;

int main() {
  const OutputLinePalette palette{
    ftxui::Color::GrayDark, ftxui::Color::White,
    ftxui::Color::RedLight, ftxui::Color::YellowLight};
  auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(14),
                                      ftxui::Dimension::Fixed(2));
  const OutputLineCard line{"\x1b[31mR\x1b[0m\n🙂X", "warning"};
  ftxui::Render(screen, projectOutputLine(line, 14, palette));
  assert(screen.CellAt(2, 0).character == "⎿");
  assert(screen.CellAt(5, 0).character == "R");
  assert(screen.CellAt(5, 0).foreground_color == ftxui::Color::Red);
  assert(screen.CellAt(2, 1).character == " ");
  assert(screen.CellAt(5, 1).character == "🙂");
  assert(screen.CellAt(6, 1).character.empty());
  assert(screen.CellAt(7, 1).character == "X");
  auto linkScreen = ftxui::Screen::Create(ftxui::Dimension::Fixed(14),
                                          ftxui::Dimension::Fixed(1));
  const OutputLineCard link{"\x1b]8;;https://x.test\x07X\x1b]8;;\x07", {}};
  ftxui::Render(linkScreen, projectOutputLine(link, 14, palette));
  assert(linkScreen.CellAt(5, 0).character == "X");
  assert(linkScreen.CellAt(5, 0).hyperlink != 0);
  assert(linkScreen.Hyperlink(linkScreen.CellAt(5, 0).hyperlink) ==
         "https://x.test");
  std::cout << "2 native FTXUI ANSI output-line projection cases pass\n";
}
