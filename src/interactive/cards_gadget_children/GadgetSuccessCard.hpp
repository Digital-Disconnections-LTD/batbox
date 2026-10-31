#pragma once

#include "interactive/MessageLookups.hpp"
#include "interactive/cards_gadget_result/HookProgressCard.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::interactive::cards_gadget_children {

struct SuccessRenderContext {
  std::optional<std::string> style;
  Json theme;
  const gadgets::Gadgets* gadgets = nullptr;
  bool verbose = false;
  bool transcriptMode = false;
  bool briefOnly = false;
  std::optional<Json> input;
};

struct SuccessServices {
  bool briefFeature = false;
  bool classifierApprovalFeature = false;
  bool classifierDenialFeature = false;
  std::function<Json()> readTheme;
  std::function<bool()> readBriefOnly;
  std::function<std::optional<std::string>(const std::string&)>
      getClassifierApproval;
  std::function<std::optional<std::string>(const std::string&)>
      getYoloClassifierApproval;
  std::function<void(const std::string&)> deleteClassifierApproval;
  std::function<std::optional<Json>(const gadgets::Gadget&,const Json&)>
      safeParseOutput;
  std::function<Json(const gadgets::Gadget&,const Json&,const Json&,
                     const SuccessRenderContext&)> renderGadgetResult;
  std::function<std::string(const gadgets::Gadget&)> userFacingName;
};

struct GadgetSuccessCard {
  bool visible = false;
  Json renderedMessage = Json();
  bool rendersAsAssistantText = false;
  std::optional<std::string> classifierRule;
  std::optional<std::string> yoloReason;
  cards_gadget_result::HookProgressCard hookProgress;
};

struct GadgetSuccessState {
  bool initialized = false;
  std::optional<std::string> classifierRule;
  std::optional<std::string> yoloReason;
  std::optional<std::string> lastCommittedGadgetUseID;

  GadgetSuccessCard render(const Json& userMessage,
                         const std::string& gadgetUseID,
                         const Json& progressMessages,
                         const MessageLookups& lookups,
                         const gadgets::Gadget* gadget,
                         const gadgets::Gadgets& gadgets,
                         std::optional<std::string> style,
                         bool verbose, bool transcriptMode,
                         const SuccessServices& services);
  void committed(const std::string& gadgetUseID,
                 const SuccessServices& services);
};

} // namespace batbox::interactive::cards_gadget_children
