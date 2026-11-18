#include "interactive/cards/UserTextProjection.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>
using namespace batbox::interactive::cards;

int main() {
  const PoetryPalette palette{
    ftxui::Color::Blue, ftxui::Color::Cyan,
    [](const std::string& role) {
      if (role == "subtle") return ftxui::Color::GrayDark;
      return ftxui::Color::White;
    }
  };
  HighlightOptions options;
  options.pointerGlyph = ">";
  auto plain = poetryLeaf("abcdefgh", true,
      PoetryEnvironment{false, [](const std::string&) { assert(false); }});
  assert(plain);
  auto highlighted = highlightedThinkingText(plain->displayText, options);
  auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(6),
                                      ftxui::Dimension::Fixed(4));
  ftxui::Render(screen, projectPoetry(*plain, highlighted, 6, palette));
  assert(screen.CellAt(0, 0).character.empty());
  assert(screen.CellAt(0, 1).character == ">");
  assert(screen.CellAt(2, 1).character == "a");
  assert(screen.CellAt(0, 2).character == "d");
  assert(screen.CellAt(0, 1).foreground_color == ftxui::Color::GrayDark);
  assert(screen.CellAt(2, 1).background_color == ftxui::Color::Blue);
  auto emojiLeaf = PoetryLeaf{"🙂x", PoetryBackground::User, false, 1};
  auto emoji = highlightedThinkingText(emojiLeaf.displayText, options);
  auto emojiScreen = ftxui::Screen::Create(ftxui::Dimension::Fixed(6),
                                           ftxui::Dimension::Fixed(1));
  ftxui::Render(emojiScreen, projectPoetry(emojiLeaf, emoji, 6, palette));
  assert(emojiScreen.CellAt(2, 0).character == "🙂");
  assert(emojiScreen.CellAt(3, 0).character.empty());
  assert(emojiScreen.CellAt(4, 0).character == "x");
  std::cout << "2 native FTXUI user-prompt projection cases pass\n";
}
