#pragma once
#include "gadgets/Gadget.hpp"
#include <string>

namespace batbox::gadgets {

struct GadgetInputServices {
  std::string exitPlanModeGadgetName;
  std::string fileEditGadgetName;
};

Json normalizeGadgetInputForAPI(const Gadget& gadget, const Json& input,
                              const GadgetInputServices& services);

} // namespace batbox::gadgets
