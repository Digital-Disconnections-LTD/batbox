#include "interactive/cards/BashResultProjection.hpp"
#include "interactive/cards/UserBashOutputProjection.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>
using namespace batbox::interactive::cards;

int main() {
  BashResultProjectionImports imports;
  imports.columns = 40;
  imports.outputPalette = {ftxui::Color::GrayDark, ftxui::Color::White,
    ftxui::Color::RedLight, ftxui::Color::YellowLight};
  imports.dimColor = ftxui::Color::GrayDark;
  imports.outputLine.stringWidth = [](const std::string& s) {
    return static_cast<double>(s.size());
  };
  imports.outputLine.sliceAnsi = [](const std::string& s, double start,
                                     double end) {
    return s.substr(static_cast<std::size_t>(start),
                    static_cast<std::size_t>(end - start));
  };
  imports.outputLine.dim = [](const std::string& s) { return s; };
  imports.outputLine.expandHint = [] { return "(expand)"; };
  imports.backgroundShortcutHint = [] { return "(↓ manage)"; };
  BashResultCard result;
  result.stdoutText = "hello";
  result.stderrText = "bad";
  result.cwdResetWarning = "Shell cwd was reset to /tmp";
  result.timeoutMs = 5000;
  auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                      ftxui::Dimension::Fixed(4));
  ftxui::Render(screen, projectBashResult(result, imports));
  assert(screen.CellAt(5, 0).character == "h");
  assert(screen.CellAt(5, 1).character == "b");
  assert(screen.CellAt(5, 1).foreground_color == imports.outputPalette.error);
  assert(screen.CellAt(5, 2).character == "S");
  assert(screen.CellAt(5, 3).character == "(");
  BashResultCard background;
  background.noOutputText = "Running in the background";
  background.backgroundHint = true;
  auto shortScreen = ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                           ftxui::Dimension::Fixed(1));
  ftxui::Render(shortScreen, projectBashResult(background, imports));
  assert(shortScreen.CellAt(5, 0).character == "R");
  auto taggedScreen = ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                            ftxui::Dimension::Fixed(1));
  ftxui::Render(taggedScreen, projectUserBashOutput(
      "<bash-stdout><persisted-output>full</persisted-output></bash-stdout>",
      false, imports));
  assert(taggedScreen.CellAt(5, 0).character == "f");
  std::cout << "3 native FTXUI Bash-result composition cases pass\n";
}
