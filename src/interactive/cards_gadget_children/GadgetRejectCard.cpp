#include "interactive/cards_gadget_children/GadgetRejectCard.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_gadget_children {
namespace {
void require(const auto& callback,const char* name){
  if(!callback)throw std::logic_error(std::string(name)+" import required");
}
}

GadgetRejectCard classifyGadgetReject(const Json& input,
    const Json& progressMessages,const gadgets::Gadget* gadget,
    const gadgets::Gadgets& availableGadgets,std::optional<std::string> style,
    bool verbose,bool transcriptMode,const RejectServices& services){
  require(services.readColumns,"terminal columns");
  const int columns=services.readColumns();
  require(services.readTheme,"theme");
  const auto theme=services.readTheme();
  if(!gadget||!gadget->renderGadgetUseRejectedMessage)return {};
  require(services.safeParseInput,"Gadget input schema");
  const auto parsed=services.safeParseInput(*gadget,input);
  if(!parsed)return {};
  require(services.renderRejected,"Gadget rejected renderer");
  RejectRenderContext context{columns,theme,&availableGadgets,verbose,
      transcriptMode,std::move(style),
      gadgets::filterGadgetProgressMessages(progressMessages)};
  const auto rendered=services.renderRejected(*gadget,*parsed,context);
  if(rendered.is_null())return {};
  return {false,rendered};
}

} // namespace batbox::interactive::cards_gadget_children
