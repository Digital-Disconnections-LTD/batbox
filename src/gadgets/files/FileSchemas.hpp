#pragma once
#include "runtime/JsonValue.hpp"
#include <string>

namespace batbox::gadgets::files {
Json fileGadgetInputSchema(const std::string& gadgetName);
Json fileGadgetOutputSchema(const std::string& gadgetName);
Json parseFileGadgetInput(const std::string& gadgetName,const Json& input);
} // namespace batbox::gadgets::files
