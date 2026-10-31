#pragma once
#include "gadgets/Gadget.hpp"
#include <string>

namespace batbox::gadgets::synthetic_output {
bool isEnabledForNonInteractiveFlight(bool isNonInteractiveFlight);
Gadget makeSyntheticOutputGadget(const std::string& protocolName);
Gadget makeHookStructuredOutputGadget(const Gadget& base);
} // namespace batbox::gadgets::synthetic_output
