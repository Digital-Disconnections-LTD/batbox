#include "interactive/cards_gadget_children/GadgetSuccessProjection.hpp"
#include "interactive/cards_gadget_result/HookProgressProjection.hpp"
#include "runtime/Encoding.hpp"
#include <stdexcept>
#include <utility>
#include <vector>

namespace batbox::interactive::cards_gadget_children {
namespace {
ftxui::Element response(ftxui::Element child,ftxui::Color subtle) {
  return ftxui::hbox({
    ftxui::text("  ⎿  ") | ftxui::color(subtle) | ftxui::dim,
    std::move(child)
  }) | ftxui::size(ftxui::HEIGHT,ftxui::EQUAL,1);
}
}

std::optional<ftxui::Element> projectGadgetSuccess(
    const GadgetSuccessCard& card,const SuccessProjectionImports& imports) {
  if (!card.visible) return {};
  if (!imports.renderGadgetMessage)
    throw std::logic_error("Gadget result component renderer required");
  if (imports.columns<=0)
    throw std::invalid_argument("Positive Gadget result width required");
  std::vector<ftxui::Element> messageRows;
  messageRows.push_back(imports.renderGadgetMessage(card.renderedMessage));
  if (card.classifierRule) {
    if (imports.tickGlyph.empty())
      throw std::logic_error("Approval tick glyph required");
    auto approval=ftxui::hbox({
      ftxui::text(value::encodeUtf8(imports.tickGlyph)) |
        ftxui::color(imports.success),
      ftxui::text(" Auto-approved · matched \"") |
        ftxui::color(imports.subtle) | ftxui::dim,
      ftxui::text(value::encodeUtf8(*card.classifierRule)) |
        ftxui::color(imports.subtle) | ftxui::dim,
      ftxui::text("\"") | ftxui::color(imports.subtle) | ftxui::dim
    });
    messageRows.push_back(response(std::move(approval),imports.subtle));
  }
  if (card.yoloReason)
    messageRows.push_back(response(
      ftxui::text("Allowed by auto mode classifier") |
        ftxui::color(imports.subtle) | ftxui::dim,imports.subtle));
  auto message=ftxui::vbox(std::move(messageRows));
  if (!card.rendersAsAssistantText)
    message=message | ftxui::size(ftxui::WIDTH,ftxui::EQUAL,
                                  imports.columns);
  std::vector<ftxui::Element> rows;
  rows.push_back(std::move(message));
  if (auto progress=cards_gadget_result::projectHookProgress(
      card.hookProgress,imports.subtle))
    rows.push_back(std::move(*progress));
  return ftxui::vbox(std::move(rows));
}

} // namespace batbox::interactive::cards_gadget_children
