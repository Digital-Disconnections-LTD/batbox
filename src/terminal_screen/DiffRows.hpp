#pragma once
#include "terminal_screen/Screen.hpp"

namespace batbox::terminal_screen {
bool diffSameWidth(const Screen& previous,const Screen& next,
  double startX,double endX,double startY,double endY,
  const DiffCallback& callback);
bool diffDifferentWidth(const Screen& previous,const Screen& next,
  double startX,double endX,double startY,double endY,
  const DiffCallback& callback);
} // namespace batbox::terminal_screen
