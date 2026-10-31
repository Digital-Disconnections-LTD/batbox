#include "interactive/cards_gadget_children/GadgetRejectProjection.hpp"
#include "interactive/cards_gadget_result/GadgetErrorProjection.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_gadget_children {

ftxui::Element projectGadgetReject(const GadgetRejectCard& card,
    ftxui::Color subtle,
    const std::function<ftxui::Element(const Json&)>& renderGadgetMessage){
  if(card.fallback)return cards_gadget_result::projectCanceledGadgetResult(subtle);
  if(!renderGadgetMessage)
    throw std::logic_error("Gadget rejected component renderer required");
  return renderGadgetMessage(card.renderedMessage);
}

} // namespace batbox::interactive::cards_gadget_children
