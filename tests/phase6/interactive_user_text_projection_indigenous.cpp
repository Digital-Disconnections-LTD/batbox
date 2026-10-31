#include "interactive/cards/UserTextProjection.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>
using namespace batbox::interactive::cards;

int main() {
  const UserCardPalette palette{
    ftxui::Color::YellowLight, ftxui::Color::White,
    ftxui::Color::GrayDark, ftxui::Color::Blue,
    ftxui::Color::Cyan
  };
  auto bash = ftxui::Screen::Create(ftxui::Dimension::Fixed(24),
                                     ftxui::Dimension::Fixed(2));
  ftxui::Render(bash, projectBashInput({"echo hi", true}, palette));
  assert(bash.CellAt(0, 0).character.empty());
  assert(bash.CellAt(0, 1).character == "!");
  assert(bash.CellAt(1, 1).character == " ");
  assert(bash.CellAt(2, 1).character == "e");
  assert(bash.CellAt(0, 1).foreground_color == palette.bashBorder);
  assert(bash.CellAt(2, 1).foreground_color == palette.text);
  assert(bash.CellAt(2, 1).background_color == palette.bashBackground);
  auto command = ftxui::Screen::Create(ftxui::Dimension::Fixed(24),
                                        ftxui::Dimension::Fixed(1));
  ftxui::Render(command, projectCommand({"/inspect", false, false},
                                         palette, ">"));
  assert(command.CellAt(0, 0).character == ">");
  assert(command.CellAt(2, 0).character == "/");
  assert(command.CellAt(0, 0).foreground_color == palette.subtle);
  assert(command.CellAt(2, 0).background_color == palette.userBackground);
  std::cout << "2 native FTXUI user-card projection cases pass\n";
}
