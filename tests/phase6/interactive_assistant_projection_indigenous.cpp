#include "interactive/cards_assistant/AssistantProjection.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

using namespace batbox::interactive::cards_assistant;

namespace {
ftxui::Screen draw(const ftxui::Element& element, int height = 5) {
  auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                      ftxui::Dimension::Fixed(height));
  ftxui::Render(screen, element);
  return screen;
}
}

int main() {
  int markdownCalls = 0;
  int rateCalls = 0;
  int hintCalls = 0;
  ProjectionImports imports;
  imports.palette = {ftxui::Color::White, ftxui::Color::GrayDark,
                     ftxui::Color::RedLight, ftxui::Color::YellowLight,
                     ftxui::Color::Blue};
  imports.dotGlyph = "●";
  imports.renderMarkdown = [&](const std::string& value, bool dim) {
    ++markdownCalls;
    return ftxui::text((dim ? "DIM:" : "MD:") + value);
  };
  imports.renderRateLimit = [&](const std::string& value) {
    ++rateCalls;
    return ftxui::text("RATE:" + value);
  };
  imports.renderExpandHint = [&] {
    ++hintCalls;
    return ftxui::text("(expand)");
  };

  assert(!projectAssistantText({Kind::Hidden, {}, false, false, false, false},
                               imports));
  const auto markdown = projectAssistantText(
      {Kind::Markdown, "hello", false, true, true, true}, imports);
  assert(markdown && markdownCalls == 1);
  const auto markdownScreen = draw(*markdown);
  assert(markdownScreen.CellAt(0, 1).character == "●");
  assert(markdownScreen.CellAt(2, 1).character == "M");
  assert(markdownScreen.CellAt(2, 1).background_color ==
         ftxui::Color::Blue);

  const auto error = projectAssistantText(
      {Kind::ApiError, "bad key", true}, imports);
  assert(error && hintCalls == 1);
  const auto errorScreen = draw(*error);
  assert(errorScreen.CellAt(2, 0).character == "⎿");
  assert(errorScreen.CellAt(5, 0).character == "b");
  assert(errorScreen.CellAt(5, 1).character == "(");

  const auto rate = projectAssistantText(
      {Kind::RateLimit, "limit"}, imports);
  assert(rate && rateCalls == 1);
  const auto rateScreen = draw(*rate);
  assert(rateScreen.CellAt(0, 0).character == "R");

  const auto collapsed = projectThinking(
      {ThinkingKind::Collapsed, "private", true}, imports);
  assert(collapsed && hintCalls == 2);
  const auto collapsedScreen = draw(*collapsed);
  assert(collapsedScreen.CellAt(0, 1).character == "∴");

  const auto expanded = projectThinking(
      {ThinkingKind::Expanded, "private", false}, imports);
  assert(expanded && markdownCalls == 2);
  const auto expandedScreen = draw(*expanded);
  assert(expandedScreen.CellAt(0, 0).character == "∴");
  assert(expandedScreen.CellAt(2, 2).character == "D");

  const auto redacted = projectThinking(
      {ThinkingKind::Redacted, {}, false}, imports);
  assert(redacted);
  const auto redactedScreen = draw(*redacted);
  assert(redactedScreen.CellAt(0, 0).character == "✻");
  const auto rateCard = projectRateLimitCard(
      {"limited", "Opening your options…", true}, imports.palette);
  const auto rateCardScreen = draw(rateCard);
  assert(rateCardScreen.CellAt(5, 0).character == "l");
  assert(rateCardScreen.CellAt(5, 1).character == "O");
  std::cout << "8 native assistant projection cases pass\n";
}
