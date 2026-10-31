#include "interactive/cards_gadget_result/GadgetResultProjection.hpp"
#include "interactive/cards_gadget_result/FallbackErrorCard.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_gadget_result {

std::optional<ftxui::Element> projectGadgetResultBlock(
    const Json& param, const Json& progressMessages,
    const MessageLookups& lookups, const gadgets::Gadgets& gadgets,
    bool verbose, const GadgetResultProjectionImports& imports) {
  const auto card = classifyGadgetResult(param,lookups,gadgets,
                                       imports.resultProtocol);
  switch (card.kind) {
    case GadgetResultKind::Hidden:
      return {};
    case GadgetResultKind::Canceled:
      return projectCanceledGadgetResult(imports.palette.subtle);
    case GadgetResultKind::Rejected:
      if (!imports.renderRejected)
        throw std::logic_error("Rejected Gadget card renderer required");
      return imports.renderRejected(card,progressMessages);
    case GadgetResultKind::Success:
      if (!imports.renderSuccess)
        throw std::logic_error("Successful Gadget card renderer required");
      return imports.renderSuccess(card,param);
    case GadgetResultKind::Error:
      break;
  }
  const auto content=param.find("content");
  const Json errorContent=content==param.end()?Json():*content;
  const auto error=classifyGadgetError(errorContent,imports.classifierFeature,
                                     imports.errorProtocol);
  if (const auto simple=projectSimpleGadgetError(error,imports.palette.subtle))
    return simple;
  if (error.kind==GadgetErrorKind::RejectedPlan) {
    if (!imports.renderRejectedPlan)
      throw std::logic_error("Rejected plan renderer required");
    return imports.renderRejectedPlan(error.planContent);
  }
  if (card.gadget && card.gadget->renderGadgetUseErrorMessage) {
    if (!imports.renderGadgetError)
      throw std::logic_error("Gadget error view adapter required");
    if (auto rendered=imports.renderGadgetError(card,errorContent,
            gadgets::filterGadgetProgressMessages(progressMessages)))
      return rendered;
  }
  return projectFallbackError(
      fallbackErrorCard(errorContent,verbose,imports.shortcutDisplay),
      imports.columns,imports.palette);
}

} // namespace batbox::interactive::cards_gadget_result
