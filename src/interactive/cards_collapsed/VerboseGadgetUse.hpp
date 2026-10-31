#pragma once

#include "interactive/MessageLookups.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>

namespace batbox::interactive::cards_collapsed {
struct VerboseGadgetUseCard {
  bool visible=false,resolved=false,error=false,inProgress=false,shouldAnimate=false;
  Json selectedBackground=Json();
  std::string name;
  Json useMessage=Json(),tag=Json(),resultMessage=Json();
  bool hasResultMessage=false;
};
struct VerboseGadgetUseServices {
  std::function<Json()> readSelectedBackground;
  std::function<gadgets::Gadgets()> primitiveGadgets;
  std::function<std::optional<Json>(const gadgets::Gadget&,const Json&)> safeParseInput;
  std::function<std::optional<Json>(const gadgets::Gadget&,const std::optional<Json>&)>
      safeParseOutput;
  std::function<std::string(const gadgets::Gadget&,const std::optional<Json>&)>
      userFacingName;
  std::function<Json(const gadgets::Gadget&,const Json&,const Json&)> renderUse;
  std::function<Json(const gadgets::Gadget&,const Json&)> renderTag;
  std::function<Json(const gadgets::Gadget&,const Json&,const gadgets::Gadgets&,
                     const Json&)> renderResult;
};
VerboseGadgetUseCard classifyVerboseGadgetUse(const Json& content,
    const gadgets::Gadgets& availableGadgets,const MessageLookups& lookups,
    const GadgetIDs& inProgressGadgetUseIDs,bool shouldAnimate,
    const Json& theme,const VerboseGadgetUseServices& services);
}
