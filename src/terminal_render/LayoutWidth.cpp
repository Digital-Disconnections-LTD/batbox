#include "terminal_render/RenderNode.hpp"

namespace batbox::terminal_render {
double getMaxWidth(const terminal_dom::FullLayoutNode& yogaNode){
  return yogaNode.getComputedWidth()-
    yogaNode.getComputedPadding("left")-
    yogaNode.getComputedPadding("right")-
    yogaNode.getComputedBorder("left")-
    yogaNode.getComputedBorder("right");
}
} // namespace batbox::terminal_render
