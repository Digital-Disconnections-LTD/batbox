#pragma once

#include "interactive/cards_gadget_children/GadgetRejectProjection.hpp"
#include "interactive/cards_gadget_children/GadgetSuccessProjection.hpp"
#include "interactive/cards_gadget_result/GadgetResultProjection.hpp"
#include <memory>
#include <unordered_map>

namespace batbox::interactive::cards_gadget_binding {

class RetainedSuccessStates {
public:
  cards_gadget_children::GadgetSuccessState& stateFor(const std::string& stableCardKey);
  void unmount(const std::string& stableCardKey);
  std::size_t size()const{return states_.size();}
private:
  std::unordered_map<std::string,cards_gadget_children::GadgetSuccessState> states_;
};
struct GadgetChildBindingInputs {
  std::string stableCardKey;
  Json userMessage;
  Json progressMessages=Json::array();
  const MessageLookups* lookups=nullptr;
  const gadgets::Gadgets* gadgets=nullptr;
  int terminalColumns=80;
  std::optional<std::string> style;
  bool verbose=false,transcriptMode=false;
};
struct GadgetChildBindingServices {
  cards_gadget_children::SuccessServices success;
  cards_gadget_children::RejectServices rejection;
  cards_gadget_children::SuccessProjectionImports successProjection;
  ftxui::Color rejectedSubtle=ftxui::Color::Default;
  std::function<ftxui::Element(const Json&)> renderRejectedComponent;
};
struct GadgetChildBinding {
  cards_gadget_result::GadgetResultProjectionImports imports;
  // Invoke after the returned FTXUI view is committed, even if the success
  // child returned null. Calling it for another route has no effect.
  std::function<void()> committed;
};
GadgetChildBinding bindGadgetResultChildren(
    RetainedSuccessStates& retained,const GadgetChildBindingInputs& input,
    const GadgetChildBindingServices& services,
    cards_gadget_result::GadgetResultProjectionImports base);
}
