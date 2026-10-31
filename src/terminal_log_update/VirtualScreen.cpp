#include "terminal_log_update/Internal.hpp"

namespace batbox::terminal_log_update {
void VirtualScreen::txn(const std::function<std::pair<Diff,Delta>(
    const terminal_screen::Point&)>& fn){
  auto [patches,next]=fn(cursor);
  for(auto& patch:patches)diff.push_back(std::move(patch));
  cursor.x+=next.dx;cursor.y+=next.dy;
}
std::optional<std::string> transitionHyperlink(Diff& diff,
    const std::optional<std::string>& current,
    const std::optional<std::string>& target){
  if(current!=target){
    diff.push_back(terminal_renderer::HyperlinkPatch{target.value_or("")});
    return target;
  }
  return current;
}
std::int32_t transitionStyle(Diff& diff,StylePool& styles,
    std::int32_t currentId,std::int32_t targetId){
  const auto str=styles.transition(currentId,targetId);
  if(!str.empty())diff.push_back(terminal_renderer::StyleStringPatch{str});
  return targetId;
}
void moveCursorTo(VirtualScreen& screen,double targetX,double targetY){
  screen.txn([&screen,targetX,targetY](const terminal_screen::Point& prev){
    const double dx=targetX-prev.x,dy=targetY-prev.y;
    const bool inPendingWrap=prev.x>=screen.viewportWidth;
    if(inPendingWrap){
      return std::pair<Diff,Delta>{
        {terminal_renderer::CarriageReturnPatch{},
          terminal_renderer::CursorMovePatch{targetX,dy}},
        {dx,dy}};
    }
    if(dy!=0){
      return std::pair<Diff,Delta>{
        {terminal_renderer::CarriageReturnPatch{},
          terminal_renderer::CursorMovePatch{targetX,dy}},
        {dx,dy}};
    }
    return std::pair<Diff,Delta>{{terminal_renderer::CursorMovePatch{dx,dy}},
      {dx,dy}};
  });
}
} // namespace batbox::terminal_log_update
