#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>

namespace batbox::gadgets::synthetic_output {
void emitAjvStrictTypeWarnings(const Json& schema,
  const std::function<void(const std::string&)>& warn);
} // namespace batbox::gadgets::synthetic_output
