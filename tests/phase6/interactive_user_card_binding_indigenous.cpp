#include "interactive/cards/UserTextBinding.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>
using batbox::Json;
using namespace batbox::interactive::cards;

int main() {
  UserTextRenderServices services;
  services.protocol = {"__NO_CONTENT__", "__INTERRUPT__",
      "__INTERRUPT_TOOL__", "command-message", "local-command-caveat",
      "tick", "teammate-message", "task-notification", "GITHUB",
      "FORK", "INBOX", "CHANNEL", "CHANNELS",
      [](const std::string&) { return false; }, [] { return false; }};
  services.userPalette = {ftxui::Color::YellowLight, ftxui::Color::White,
    ftxui::Color::GrayDark, ftxui::Color::Blue, ftxui::Color::Cyan};
  services.poetryPalette = {ftxui::Color::Cyan, ftxui::Color::YellowLight,
    [](const std::string&) { return ftxui::Color::White; }};
  services.poetry.logError = [](const std::string&) { assert(false); };
  services.highlight.pointerGlyph = ">";
  services.bashResult.columns = 30;
  services.bashResult.outputPalette = {ftxui::Color::GrayDark,
    ftxui::Color::White, ftxui::Color::RedLight, ftxui::Color::YellowLight};
  services.bashResult.dimColor = ftxui::Color::GrayDark;
  services.bashResult.outputLine.stringWidth = [](const std::string& s) {
    return static_cast<double>(s.size());
  };
  services.bashResult.outputLine.sliceAnsi = [](const std::string& s,
      double start, double end) {
    return s.substr(static_cast<std::size_t>(start),
                    static_cast<std::size_t>(end - start));
  };
  services.bashResult.outputLine.dim = [](const std::string& s) {return s;};
  services.bashResult.outputLine.expandHint = [] {return "(expand)";};
  int otherCalls = 0;
  services.renderOtherCard = [&](UserTextKind kind, const Json&,
                                  const UserTextCard&) {
    assert(kind == UserTextKind::Plan);
    ++otherCalls;
    return ftxui::text("PLAN");
  };
  const Json user{{"type", "user"}};
  const auto poetry = projectUserTextBlock(user, Json{{"text", "hello"}},
                                            false, false, 30, services);
  assert(poetry);
  auto poetryScreen = ftxui::Screen::Create(ftxui::Dimension::Fixed(30),
                                             ftxui::Dimension::Fixed(1));
  ftxui::Render(poetryScreen, *poetry);
  assert(poetryScreen.CellAt(0, 0).character == ">");
  const auto bash = projectUserTextBlock(user,
      Json{{"text", "<bash-input>pwd</bash-input>"}},
      false, false, 30, services);
  assert(bash);
  auto bashScreen = ftxui::Screen::Create(ftxui::Dimension::Fixed(30),
                                           ftxui::Dimension::Fixed(1));
  ftxui::Render(bashScreen, *bash);
  assert(bashScreen.CellAt(0, 0).character == "!");
  const auto output = projectUserTextBlock(user,
      Json{{"text", "<bash-stdout>done</bash-stdout>"}},
      false, false, 30, services);
  assert(output);
  auto outputScreen = ftxui::Screen::Create(ftxui::Dimension::Fixed(30),
                                             ftxui::Dimension::Fixed(1));
  ftxui::Render(outputScreen, *output);
  assert(outputScreen.CellAt(5, 0).character == "d");
  const auto hidden = projectUserTextBlock(user,
      Json{{"text", "__NO_CONTENT__"}}, false, false, 30, services);
  assert(!hidden);
  const Json planUser{{"type", "user"}, {"planContent", "plan"}};
  const auto plan = projectUserTextBlock(planUser, Json{{"text", "hello"}},
                                          false, false, 30, services);
  assert(plan && otherCalls == 1);
  std::cout << "5 native user-card binding cases pass\n";
}
