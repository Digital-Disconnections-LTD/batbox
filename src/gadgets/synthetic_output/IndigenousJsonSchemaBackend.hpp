#pragma once
#include "gadgets/synthetic_output/SyntheticOutputFactory.hpp"

namespace batbox::gadgets::synthetic_output {
AjvBackend makeIndigenousJsonSchemaBackend(
  std::function<void(const std::string&)> logWarning);
} // namespace batbox::gadgets::synthetic_output
