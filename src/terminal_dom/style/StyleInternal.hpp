#pragma once
#include "terminal_dom/style/StyleValues.hpp"

namespace batbox::terminal_dom::style::detail {
void applyPositionStyles(FullLayoutNode& node,const Styles& style);
void applyOverflowStyles(FullLayoutNode& node,const Styles& style);
void applyMarginStyles(FullLayoutNode& node,const Styles& style);
void applyPaddingStyles(FullLayoutNode& node,const Styles& style);
void applyFlexStyles(FullLayoutNode& node,const Styles& style);
void applyDimensionStyles(FullLayoutNode& node,const Styles& style);
void applyDisplayStyles(FullLayoutNode& node,const Styles& style);
void applyBorderStyles(FullLayoutNode& node,const Styles& style,
  const Styles* resolvedStyle);
void applyGapStyles(FullLayoutNode& node,const Styles& style);
} // namespace batbox::terminal_dom::style::detail
