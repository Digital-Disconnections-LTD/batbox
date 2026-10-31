#pragma once
#include "terminal_screen/Screen.hpp"
#include "terminal_screen/StylePool.hpp"
#include <string>

namespace batbox::terminal_runtime {
bool applySearchHighlight(terminal_screen::Screen& screen,
  const std::string& query,terminal_screen::StylePool& stylePool);
}
