#include "interactive/cards_gadget_result/HookProgressProjection.hpp"
#include "runtime/Encoding.hpp"

namespace batbox::interactive::cards_gadget_result {

std::optional<ftxui::Element> projectHookProgress(
    const HookProgressCard& card, ftxui::Color subtle) {
  if (card.kind == HookProgressKind::Hidden) return {};
  const auto leading = card.kind == HookProgressKind::TranscriptSummary ?
      std::to_string(card.inProgressCount) + " " : "Running ";
  const auto trailing = card.kind == HookProgressKind::TranscriptSummary ?
      card.inProgressCount == 1 ? " hook ran" : " hooks ran" :
      card.inProgressCount == 1 ? " hook…" : " hooks…";
  auto content = ftxui::hbox({
      ftxui::text(value::encodeUtf8(leading)),
      ftxui::text(value::encodeUtf8(card.hookEvent)) | ftxui::bold,
      ftxui::text(value::encodeUtf8(trailing))
  }) | ftxui::color(subtle) | ftxui::dim;
  return ftxui::hbox({
      ftxui::text("  ⎿  ") | ftxui::color(subtle) | ftxui::dim,
      std::move(content)
  });
}

} // namespace batbox::interactive::cards_gadget_result
