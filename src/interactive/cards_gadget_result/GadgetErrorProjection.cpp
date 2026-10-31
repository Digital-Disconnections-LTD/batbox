#include "interactive/cards_gadget_result/GadgetErrorProjection.hpp"
#include "runtime/Encoding.hpp"
#include <utility>

namespace batbox::interactive::cards_gadget_result {
namespace {
ftxui::Element response(const std::string& text, ftxui::Color subtle) {
  return ftxui::hbox({
      ftxui::text("  ⎿  ") | ftxui::color(subtle) | ftxui::dim,
      ftxui::paragraph(value::encodeUtf8(text)) |
        ftxui::color(subtle) | ftxui::dim | ftxui::xflex
  }) | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 1);
}
}

ftxui::Element projectCanceledGadgetResult(ftxui::Color subtle) {
  return response("Interrupted · What should Batbox do instead?",subtle);
}

std::optional<ftxui::Element> projectSimpleGadgetError(
    const GadgetErrorCard& card, ftxui::Color subtle) {
  switch (card.kind) {
    case GadgetErrorKind::Interrupted:
      return projectCanceledGadgetResult(subtle);
    case GadgetErrorKind::RejectedWithReason:
      return response("Gadget use rejected",subtle);
    case GadgetErrorKind::ClassifierDenial:
      return response("Denied by auto mode classifier · /feedback if incorrect",
                      subtle);
    case GadgetErrorKind::RejectedPlan:
    case GadgetErrorKind::GadgetRenderer:
      return {};
  }
  return {};
}

ftxui::Element projectFallbackError(const FallbackErrorCard& card,
                                    int columns,
                                    const GadgetErrorPalette& palette) {
  auto body = cards::projectOutputLine(
      {card.errorText, std::optional<std::string>{"error"}},
      columns,palette.output);
  if (!card.showExpand) return body;
  const auto prefix = "… +" + std::to_string(card.extraLines) +
      (card.extraLines == 1 ? " line (" : " lines (");
  auto hint = ftxui::hbox({
    ftxui::text("     "),
    ftxui::text(value::encodeUtf8(prefix)) |
      ftxui::color(palette.subtle) | ftxui::dim,
    ftxui::text(value::encodeUtf8(card.shortcut)) |
      ftxui::color(palette.subtle) | ftxui::dim | ftxui::bold,
    ftxui::text(" to see all)") |
      ftxui::color(palette.subtle) | ftxui::dim
  });
  return ftxui::vbox({std::move(body),std::move(hint)});
}

} // namespace batbox::interactive::cards_gadget_result
