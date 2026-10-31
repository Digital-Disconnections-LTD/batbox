#pragma once
#include "terminal_dom/Dom.hpp"

namespace batbox::terminal_dom {
// Binds DOM text helpers to the indigenous implementations.
// The caller supplies the concrete LayoutNode factory from the Yoga adapter.
DomServicesRef indigenousDomServices(LayoutFactory layoutFactory,
  bool bunAvailable=true);
} // namespace batbox::terminal_dom
