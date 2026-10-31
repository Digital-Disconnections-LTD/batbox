#include "interactive/cards/UserTextBinding.hpp"
#include "interactive/cards/UserBashOutputProjection.hpp"
#include "interactive/cards/UserTextLeaves.hpp"
#include <stdexcept>

namespace batbox::interactive::cards {
namespace {
std::string string(const Json& value, const char* key) {
  const auto found = value.find(key);
  return found != value.end() && found->is_string() ?
      found->get<std::string>() : "";
}
} // namespace

std::optional<ftxui::Element> projectUserTextBlock(
    const Json& userMessage, const Json& textBlock, bool addMargin, bool verbose,
    int columns, const UserTextRenderServices& services) {
  const auto text = string(textBlock, "text");
  const auto card = classifyUserText(text, string(userMessage, "planContent"),
                                     services.protocol);
  switch (card.kind) {
    case UserTextKind::Hidden:
      return {};
    case UserTextKind::BashInput: {
      const auto leaf = bashInputLeaf(text, addMargin);
      if (!leaf) return {};
      return projectBashInput(*leaf, services.userPalette);
    }
    case UserTextKind::Command: {
      const auto leaf = commandLeaf(text, services.protocol.commandMessageTag,
                                     addMargin);
      if (!leaf) return {};
      return projectCommand(*leaf, services.userPalette,
                            services.highlight.pointerGlyph);
    }
    case UserTextKind::BashOutput:
      return projectUserBashOutput(text, verbose, services.bashResult);
    case UserTextKind::Poetry: {
      auto environment = services.poetry;
      const auto timestampField = userMessage.find("timestamp");
      const auto timestamp = timestampField != userMessage.end() &&
          timestampField->is_string() ?
          std::optional<std::string>{timestampField->get<std::string>()} :
          std::nullopt;
      const auto leaf = poetryLeaf(text, timestamp, addMargin, environment);
      if (!leaf) return {};
      const auto highlighted = highlightedThinkingText(leaf->displayText,
          leaf->briefLayout, leaf->timestamp, services.highlight);
      return projectPoetry(*leaf, highlighted, columns, services.poetryPalette);
    }
    default:
      if (!services.renderOtherCard)
        throw std::logic_error("User card renderer required for selected kind");
      return services.renderOtherCard(card.kind, userMessage, card);
  }
}
} // namespace batbox::interactive::cards
