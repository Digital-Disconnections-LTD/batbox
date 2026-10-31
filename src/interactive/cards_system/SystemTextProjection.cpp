#include "interactive/cards_system/SystemTextProjection.hpp"
#include "runtime/Encoding.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::interactive::cards_system {
namespace {
ftxui::Element withMargin(ftxui::Element content, bool margin) {
  return margin ? ftxui::vbox({ftxui::text(""), std::move(content)}) : content;
}
}

std::optional<ftxui::Element> projectSystemText(
    const Json& message, const SystemTextCard& card, bool verbose,
    const SystemProjectionImports& imports) {
  using K = SystemTextKind;
  if (card.kind == K::Hidden) return {};
  if (card.kind == K::AwaySummary) {
    if (imports.referenceMark.empty())
      throw std::logic_error("Reference mark required");
    auto content = ftxui::hbox({
      ftxui::text(value::encodeUtf8(imports.referenceMark)) |
        ftxui::color(imports.palette.subtle) |
        ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 2),
      ftxui::paragraph(value::encodeUtf8(card.content)) |
        ftxui::color(imports.palette.subtle) | ftxui::xflex
    }) | ftxui::bgcolor(imports.palette.selectedBackground);
    return withMargin(std::move(content), card.addMargin);
  }
  if (card.kind == K::BatsKilled) {
    if (imports.errorDot.empty())
      throw std::logic_error("System error dot required");
    auto content = ftxui::hbox({
      ftxui::text(value::encodeUtf8(imports.errorDot)) |
        ftxui::color(imports.palette.error) |
        ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 2),
      ftxui::text(card.content) | ftxui::color(imports.palette.subtle)
    }) | ftxui::bgcolor(imports.palette.selectedBackground);
    return withMargin(std::move(content), card.addMargin);
  }
  if (card.kind == K::ScheduledMission) {
    if (imports.eventMark.empty())
      throw std::logic_error("System event mark required");
    auto content = ftxui::paragraph(value::encodeUtf8(imports.eventMark +
        " " + card.content)) | ftxui::color(imports.palette.subtle) |
        ftxui::bgcolor(imports.palette.selectedBackground);
    return withMargin(std::move(content), card.addMargin);
  }
  if (card.kind == K::PermissionRetry) {
    if (imports.eventMark.empty())
      throw std::logic_error("System event mark required");
    auto content = ftxui::hbox({
      ftxui::text(value::encodeUtf8(imports.eventMark + " ")) |
        ftxui::color(imports.palette.subtle),
      ftxui::text("Allowed ") | ftxui::color(imports.palette.text),
      ftxui::paragraph(value::encodeUtf8(card.content)) | ftxui::bold |
        ftxui::color(imports.palette.text) | ftxui::xflex
    }) | ftxui::bgcolor(imports.palette.selectedBackground);
    return withMargin(std::move(content), card.addMargin);
  }
  if (!imports.renderSpecial)
    throw std::logic_error("System child-card renderer required");
  return imports.renderSpecial(card.kind, message, card, verbose);
}

std::optional<ftxui::Element> projectSystemTextMessage(
    const Json& message, bool addMargin, bool verbose, bool transcriptMode,
    const SystemProjectionImports& imports) {
  const auto card = classifySystemText(message, addMargin, verbose,
                                        transcriptMode);
  return projectSystemText(message, card, verbose, imports);
}

} // namespace batbox::interactive::cards_system
