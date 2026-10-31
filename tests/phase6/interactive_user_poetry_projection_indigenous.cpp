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
      if (role == "briefLabelYou") return ftxui::Color::YellowLight;
      return ftxui::Color::White;
    }
  };
  HighlightOptions options;
  options.pointerGlyph = ">";
  options.formatBriefTimestamp = [](const std::string& s) { return s; };
  auto plain = poetryLeaf("abcdefgh", {}, true,
      PoetryEnvironment{false, false, false, false, false, false,
        {}, {}, {}, [](const std::string&) { assert(false); }});
  assert(plain);
  auto highlighted = highlightedThinkingText(plain->displayText, false, {}, options);
  auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(6),
                                      ftxui::Dimension::Fixed(4));
  ftxui::Render(screen, projectPoetry(*plain, highlighted, 6, palette));
  assert(screen.CellAt(0, 0).character.empty());
  assert(screen.CellAt(0, 1).character == ">");
  assert(screen.CellAt(2, 1).character == "a");
  assert(screen.CellAt(0, 2).character == "d");
  assert(screen.CellAt(0, 1).foreground_color == ftxui::Color::GrayDark);
  assert(screen.CellAt(2, 1).background_color == ftxui::Color::Blue);
  auto briefLeaf = PoetryLeaf{"hello", true, {}, PoetryBackground::None,
                               false, 0};
  auto brief = highlightedThinkingText("hello", true, {}, options);
  auto briefScreen = ftxui::Screen::Create(ftxui::Dimension::Fixed(12),
                                           ftxui::Dimension::Fixed(2));
  ftxui::Render(briefScreen, projectPoetry(briefLeaf, brief, 12, palette));
  assert(briefScreen.CellAt(2, 0).character == "Y");
  assert(briefScreen.CellAt(2, 1).character == "h");
  assert(briefScreen.CellAt(2, 0).foreground_color == ftxui::Color::YellowLight);
  auto emojiLeaf = PoetryLeaf{"🙂x", false, {}, PoetryBackground::User,
                               false, 1};
  auto emoji = highlightedThinkingText(emojiLeaf.displayText, false, {}, options);
  auto emojiScreen = ftxui::Screen::Create(ftxui::Dimension::Fixed(6),
                                           ftxui::Dimension::Fixed(1));
  ftxui::Render(emojiScreen, projectPoetry(emojiLeaf, emoji, 6, palette));
  assert(emojiScreen.CellAt(2, 0).character == "🙂");
  assert(emojiScreen.CellAt(3, 0).character.empty());
  assert(emojiScreen.CellAt(4, 0).character == "x");
  std::cout << "3 native FTXUI user-prompt projection cases pass\n";
}
