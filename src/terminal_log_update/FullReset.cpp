#include "terminal_log_update/Internal.hpp"

namespace batbox::terminal_log_update {
Diff fullResetSequence(const Frame& frame,FlickerReason reason,
    StylePool& styles,
    std::optional<terminal_renderer::ClearTerminalPatch::Debug> debug){
  VirtualScreen screen({0,0},frame.viewport.width);
  renderFrame(screen,frame,styles);
  Diff result;
  result.reserve(screen.diff.size()+1);
  result.push_back(terminal_renderer::ClearTerminalPatch{reason,
    std::move(debug)});
  result.insert(result.end(),screen.diff.begin(),screen.diff.end());
  return result;
}
void renderFrame(VirtualScreen& screen,const Frame& frame,StylePool& styles){
  renderFrameSlice(screen,frame,0,frame.screen->height,styles);
}
} // namespace batbox::terminal_log_update
