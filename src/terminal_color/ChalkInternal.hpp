#pragma once
#include "terminal_color/ChalkIndigenous.hpp"

namespace batbox::terminal_color {
std::string chalkWrap(const std::string& text,const std::string& open,
  const std::string& close,double level);
} // namespace batbox::terminal_color
