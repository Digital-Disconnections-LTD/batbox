#include "interactive/cards_gadget_binding/GadgetChildBinding.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_gadget_binding {
namespace {
struct RenderState {
  RetainedSuccessStates* retained=nullptr;
  GadgetChildBindingInputs input;
  GadgetChildBindingServices services;
  bool mountedSuccess=false;
  std::string renderedGadgetUseID;
};
}

cards_gadget_children::GadgetSuccessState& RetainedSuccessStates::stateFor(
    const std::string& stableCardKey){
  if(stableCardKey.empty())throw std::invalid_argument("Stable Gadget card key required");
  return states_[stableCardKey];
}
void RetainedSuccessStates::unmount(const std::string& stableCardKey){
  states_.erase(stableCardKey);
}

GadgetChildBinding bindGadgetResultChildren(
    RetainedSuccessStates& retained,const GadgetChildBindingInputs& input,
    const GadgetChildBindingServices& services,
    cards_gadget_result::GadgetResultProjectionImports base){
  if(!input.lookups||!input.gadgets)
    throw std::logic_error("Gadget result lookups and Gadget Roster required");
  if(input.terminalColumns<=0)
    throw std::invalid_argument("Terminal columns required");
  if(input.stableCardKey.empty())
    throw std::invalid_argument("Stable Gadget card key required");
  auto state=std::make_shared<RenderState>();
  state->retained=&retained;
  state->input=input;
  state->services=services;
  base.renderSuccess=[state](const cards_gadget_result::GadgetResultCard& route,
                             const Json&){
    state->mountedSuccess=true;
    state->renderedGadgetUseID=route.gadgetUseID;
    auto& retainedState=state->retained->stateFor(state->input.stableCardKey);
    const auto card=retainedState.render(state->input.userMessage,
      route.gadgetUseID,state->input.progressMessages,*state->input.lookups,
      route.gadget,*state->input.gadgets,state->input.style,
      state->input.verbose,state->input.transcriptMode,
      state->services.success);
    auto projection=state->services.successProjection;
    projection.columns=state->input.terminalColumns-5;
    return cards_gadget_children::projectGadgetSuccess(card,projection);
  };
  base.renderRejected=[state](const cards_gadget_result::GadgetResultCard& route,
                              const Json& progress)
                              ->std::optional<ftxui::Element>{
    if(!route.gadgetUse)
      throw std::logic_error("Rejected Gadget-use input lookup required");
    const auto& use=route.gadgetUse->value();
    const auto found=use.find("input");
    const auto parsed=cards_gadget_children::classifyGadgetReject(
      found==use.end()?Json():*found,progress,route.gadget,
      *state->input.gadgets,state->input.style,state->input.verbose,
      state->input.transcriptMode,state->services.rejection);
    return cards_gadget_children::projectGadgetReject(parsed,
      state->services.rejectedSubtle,
      state->services.renderRejectedComponent);
  };
  return {std::move(base),[state]{
    if(state->mountedSuccess){
      state->retained->stateFor(state->input.stableCardKey).committed(
        state->renderedGadgetUseID,state->services.success);
    }
  }};
}
}
