#include "terminal_renderer/Frame.hpp"

namespace batbox::terminal_renderer {
Frame emptyFrame(double rows,double columns,
    std::shared_ptr<terminal_screen::StylePool> styles,
    std::shared_ptr<terminal_screen::CharPool> chars,
    std::shared_ptr<terminal_screen::HyperlinkPool> links,
    const terminal_screen::DebugWarning& debug){
  return {terminal_screen::createScreen(0,0,styles,chars,links,debug),
    {columns,rows},{0,0,true},{},{}};
}
std::optional<FlickerReason> shouldClearScreen(const Frame& previous,
    const Frame& frame){
  const bool resized=frame.viewport.height!=previous.viewport.height||
    frame.viewport.width!=previous.viewport.width;
  if(resized)return FlickerReason::Resize;
  const bool currentOverflows=frame.screen->height>=frame.viewport.height;
  const bool previousOverflowed=
    previous.screen->height>=previous.viewport.height;
  if(currentOverflows||previousOverflowed)return FlickerReason::Offscreen;
  return {};
}
} // namespace batbox::terminal_renderer
