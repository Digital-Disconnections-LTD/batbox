#pragma once
#include "terminal_dom/Node.hpp"

namespace batbox::terminal_runtime_roster {
struct MeasuredElement {double width=0,height=0;};
MeasuredElement measureElement(const terminal_dom::DOMElement& node);
}
