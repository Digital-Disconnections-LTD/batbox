#pragma once
#include "bats/BatGadgetPresentation.hpp"
#include "gadgets/Gadget.hpp"

namespace batbox::bats {
struct BatGadgetDescriptorImports {
  std::string fileReadGadgetName="Read",bashGadgetName="Bash";
  std::function<std::string(const Json&)> userFacingName;
  std::function<Json(const Json&)> userFacingNameBackgroundColor;
  std::function<Json(const Json&,const Json&)> renderGadgetUseMessage,
    renderGadgetUseProgressMessage,
    renderGadgetUseRejectedMessage,renderGadgetUseErrorMessage;
  std::function<Json(const Json&,const Json&,const Json&)> renderGadgetResultMessage;
  std::function<Json(const Json&)> renderGadgetUseTag;
  std::function<Json(const Json&,const Json&)> renderGroupedGadgetUse;
};
// Functional BatGadget descriptor fields and constant methods. Input/output
// schemas are configured separately.
void configureBatGadgetDescriptor(gadgets::Gadget& gadget,
  const BatGadgetDescriptorImports& imports);
} // namespace batbox::bats
