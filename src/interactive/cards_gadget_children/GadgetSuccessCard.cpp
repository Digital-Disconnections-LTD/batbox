#include "interactive/cards_gadget_children/GadgetSuccessCard.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_gadget_children {
namespace {
void require(const auto& callback,const char* name) {
  if (!callback) throw std::logic_error(std::string(name)+" import required");
}
}

GadgetSuccessCard GadgetSuccessState::render(
    const Json& userMessage,const std::string& gadgetUseID,
    const Json& progressMessages,const MessageLookups& lookups,
    const gadgets::Gadget* gadget,const gadgets::Gadgets& availableGadgets,
    std::optional<std::string> style,bool verbose,bool transcriptMode,
    const SuccessServices& services) {
  require(services.readTheme,"theme");
  const auto theme=services.readTheme();
  bool briefOnly=false;
  if (services.briefFeature) {
    require(services.readBriefOnly,"brief mode");
    briefOnly=services.readBriefOnly();
  }
  if (!initialized) {
    require(services.getClassifierApproval,"classifier approval lookup");
    require(services.getYoloClassifierApproval,"auto-mode approval lookup");
    classifierRule=services.getClassifierApproval(gadgetUseID);
    yoloReason=services.getYoloClassifierApproval(gadgetUseID);
    initialized=true;
  }
  const auto result=userMessage.find("toolUseResult");
  if (result==userMessage.end() || !value::truthy(*result) || !gadget)
    return {};
  Json parsed=*result;
  if (!gadget->outputSchema.is_null()) {
    require(services.safeParseOutput,"Gadget output schema");
    const auto validated=services.safeParseOutput(*gadget,*result);
    if (!validated) return {};
    parsed=*validated;
  }
  if (!gadget->renderGadgetResultMessage) return {};
  require(services.renderGadgetResult,"Gadget result renderer");
  SuccessRenderContext context{std::move(style),theme,&availableGadgets,
      verbose,transcriptMode,briefOnly,{}};
  const auto use=lookups.gadgetUseByGadgetUseID.find(gadgetUseID);
  if (use!=lookups.gadgetUseByGadgetUseID.end()) {
    const auto& block=use->second.value();
    const auto input=block.find("input");
    if (input!=block.end()) context.input=*input;
  }
  auto rendered=services.renderGadgetResult(*gadget,parsed,
      gadgets::filterGadgetProgressMessages(progressMessages),context);
  if (rendered.is_null()) return {};
  require(services.userFacingName,"Gadget display name");
  const bool assistantText=services.userFacingName(*gadget).empty();
  GadgetSuccessCard card;
  card.visible=true;
  card.renderedMessage=std::move(rendered);
  card.rendersAsAssistantText=assistantText;
  if (services.classifierApprovalFeature && classifierRule &&
      !classifierRule->empty())
    card.classifierRule=classifierRule;
  if (services.classifierDenialFeature && yoloReason &&
      !yoloReason->empty())
    card.yoloReason=yoloReason;
  card.hookProgress=cards_gadget_result::classifyHookProgress(
      "PostToolUse",gadgetUseID,transcriptMode,lookups);
  return card;
}

void GadgetSuccessState::committed(const std::string& gadgetUseID,
                                 const SuccessServices& services) {
  if (lastCommittedGadgetUseID==gadgetUseID) return;
  require(services.deleteClassifierApproval,"classifier approval deletion");
  lastCommittedGadgetUseID=gadgetUseID;
  services.deleteClassifierApproval(gadgetUseID);
}

} // namespace batbox::interactive::cards_gadget_children
