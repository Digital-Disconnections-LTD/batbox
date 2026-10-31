#include "interactive/cards_system/SystemAPIErrorProjection.hpp"
#include "runtime/Encoding.hpp"
#include <stdexcept>
#include <utility>
#include <vector>

namespace batbox::interactive::cards_system {

std::optional<ftxui::Element> projectSystemAPIError(
    const APIErrorCard& card, const APIErrorPalette& palette,
    const std::function<ftxui::Element()>& expandHint) {
  if (!card.visible) return {};
  std::vector<ftxui::Element> content;
  content.push_back(ftxui::paragraph(value::encodeUtf8(card.errorText)) |
                    ftxui::color(palette.error));
  if (card.truncated) {
    if (!expandHint)
      throw std::logic_error("Expand shortcut renderer required");
    content.push_back(expandHint());
  }
  content.push_back(ftxui::paragraph(value::encodeUtf8(card.retryText)) |
                    ftxui::color(palette.subtle) | ftxui::dim);
  return ftxui::hbox({
      ftxui::text("  ⎿  ") | ftxui::color(palette.subtle) | ftxui::dim,
      ftxui::vbox(std::move(content)) | ftxui::xflex
  });
}

} // namespace batbox::interactive::cards_system
