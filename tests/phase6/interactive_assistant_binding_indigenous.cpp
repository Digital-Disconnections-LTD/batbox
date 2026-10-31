#include "interactive/cards_assistant/AssistantBinding.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

using namespace batbox::interactive::cards_assistant;
using batbox::Json;

namespace {
ftxui::Screen draw(const ftxui::Element& element) {
  auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                      ftxui::Dimension::Fixed(4));
  ftxui::Render(screen, element);
  return screen;
}
}

int main() {
  AssistantRenderServices services;
  services.protocol = {"__NO_CONTENT__", "__NO_RESPONSE__",
      "Prompt is too long", "__CREDIT__", "low credit", "__KEY__",
      "__EXTERNAL_KEY__", "__ORG__", "__ORG_AUTH__", "__TOKEN__",
      "__TIMEOUT__", "__MODEL_OFF__", "__ABORT__", "API Error",
      {"You've hit your"}};
  services.textImports.contextUpgradeHint = [] { return std::string{}; };
  services.textImports.isKeychainLocked = [] { return false; };
  services.textImports.fallbackModelName = [] { return "Fallback"; };
  services.textImports.startsWithApiErrorPrefix =
      [](const std::string& text) {return text.starts_with("API Error");};
  services.projection.palette = {ftxui::Color::White, ftxui::Color::GrayDark,
      ftxui::Color::RedLight, ftxui::Color::YellowLight,
      ftxui::Color::Blue};
  services.projection.dotGlyph = "●";
  services.projection.renderMarkdown = [](const std::string& text, bool dim) {
    return ftxui::text((dim ? "TH:" : "MD:") + text);
  };
  services.projection.renderRateLimit = [](const std::string& text) {
    return ftxui::text("RATE:" + text);
  };
  services.projection.renderExpandHint = [] {return ftxui::text("(expand)");};

  const auto hidden = projectAssistantTextBlock(
      Json{{"type", "text"}, {"text", "__NO_RESPONSE__"}},
      false, false, false, false, services);
  assert(!hidden);
  const auto markdown = projectAssistantTextBlock(
      Json{{"type", "text"}, {"text", "ready"}},
      false, true, true, true, services);
  assert(markdown);
  auto screen = draw(*markdown);
  assert(screen.CellAt(0, 1).character == "●");
  assert(screen.CellAt(2, 1).character == "M");
  const auto error = projectAssistantTextBlock(
      Json{{"type", "text"}, {"text", "API Error: failed"}},
      false, false, false, false, services);
  assert(error);
  screen = draw(*error);
  assert(screen.CellAt(5, 0).character == "A");
  const auto thinking = projectAssistantThinkingBlock(
      Json{{"type", "thinking"}, {"thinking", "private"}},
      false, false, true, false, services.projection);
  assert(thinking);
  screen = draw(*thinking);
  assert(screen.CellAt(2, 2).character == "T");
  const auto erased = projectAssistantThinkingBlock(
      Json{{"type", "thinking"}, {"thinking", "private"}},
      false, true, true, true, services.projection);
  assert(!erased);
  std::cout << "5 native assistant metadata bindings pass\n";
}
