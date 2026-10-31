#include "interactive/cards_collapsed/CollapsedBinding.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_collapsed {

std::optional<ftxui::Element> projectCollapsedWithIndigenousGadgets(
    const CollapsedCard& card,const gadgets::Gadgets& availableGadgets,
    const MessageLookups& lookups,const GadgetIDs& inProgressGadgetUseIDs,
    const VerboseGadgetUseServices& gadgetServices,
    const VerboseProjectionImports& gadgetProjection,
    CollapsedProjectionImports imports){
  imports.renderVerboseGadgetUse=[&](const Json& message){
    if(!message.is_object()||!message.contains("message"))
      throw std::invalid_argument("Verbose collapsed Gadget message required");
    const auto& inner=message["message"];
    if(!inner.is_object()||!inner.contains("content")||
       !inner["content"].is_array()||inner["content"].empty())
      throw std::invalid_argument("Verbose collapsed Gadget content required");
    const auto cardForGadget=classifyVerboseGadgetUse(inner["content"][0],
      availableGadgets,lookups,inProgressGadgetUseIDs,card.shouldAnimate,
      card.theme,gadgetServices);
    auto projected=projectVerboseGadgetUse(cardForGadget,gadgetProjection);
    return projected?std::move(*projected):ftxui::text("");
  };
  return projectCollapsedReadSearch(card,imports);
}
}
