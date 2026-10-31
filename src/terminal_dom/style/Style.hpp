#pragma once
#include "runtime/JsonValue.hpp"
#include "terminal_dom/FullLayoutPort.hpp"
#include <stdexcept>

namespace batbox::terminal_dom::style {
// A JSON object preserves property presence and insertion order from the
// reconciler. A {"$undefined":true} value is the indigenous boundary for an
// explicitly present undefined property in fixtures.
using Styles=Json;
using TextStyles=Json;
struct TypeError:std::runtime_error {using std::runtime_error::runtime_error;};
void applyStyles(FullLayoutNode& node,const Styles& style=Styles::object(),
  const Styles* resolvedStyle=nullptr);
} // namespace batbox::terminal_dom::style
