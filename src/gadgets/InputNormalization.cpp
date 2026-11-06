#include "gadgets/InputNormalization.hpp"

namespace batbox::gadgets {

Json normalizeGadgetInputForAPI(const Gadget& gadget, const Json& input,
                              const GadgetInputServices& services) {
  if (gadget.name == services.exitPlanModeGadgetName && input.is_object() &&
      (input.contains("plan") || input.contains("planFilePath"))) {
    Json out = input; out.erase("plan"); out.erase("planFilePath"); return out;
  }
  if (gadget.name == services.fileEditGadgetName && input.is_object() && input.contains("edits")) {
    Json out = input;
    out.erase("old_string"); out.erase("new_string"); out.erase("replace_all");
    return out;
  }
  return input;
}

} // namespace batbox::gadgets
