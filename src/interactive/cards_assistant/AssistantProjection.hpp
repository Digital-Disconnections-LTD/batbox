#pragma once

#include "interactive/cards_assistant/AssistantTextCard.hpp"
#include "interactive/cards_assistant/ThinkingCard.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include <functional>
#include <optional>

namespace batbox::interactive::cards_assistant {

struct Palette {
  ftxui::Color text;
  ftxui::Color subtle;
  ftxui::Color error;
  ftxui::Color suggestion;
  ftxui::Color selectedBackground;
};

struct ProjectionImports {
  Palette palette;
  std::string dotGlyph;
  std::function<ftxui::Element(const std::string&, bool dim)> renderMarkdown;
  std::function<ftxui::Element(const std::string&)> renderRateLimit;
  std::function<ftxui::Element()> renderExpandHint;
};

std::optional<ftxui::Element> projectAssistantText(
    const Card& card, const ProjectionImports& imports);
// The "∴ Thinking…" heading of an expanded reasoning block.
ftxui::Element thinkingHeading(const ProjectionImports& imports);
std::optional<ftxui::Element> projectThinking(
    const ThinkingCard& card, const ProjectionImports& imports);

} // namespace batbox::interactive::cards_assistant
