#include "terminal_log_update/PatchJson.hpp"

namespace batbox::terminal_log_update {
namespace {
Json patchToJson(const terminal_renderer::Patch& patch){
  using namespace terminal_renderer;
  if(const auto p=std::get_if<StdoutPatch>(&patch))
    return Json{{"type","stdout"},{"content",p->content}};
  if(const auto p=std::get_if<ClearPatch>(&patch))
    return Json{{"type","clear"},{"count",p->count}};
  if(const auto p=std::get_if<ClearTerminalPatch>(&patch)){
    Json result={{"type","clearTerminal"},{"reason",
      p->reason==FlickerReason::Resize?"resize":
      p->reason==FlickerReason::Offscreen?"offscreen":"clear"}};
    if(p->debug)result["debug"]={{"triggerY",p->debug->triggerY},
      {"prevLine",p->debug->prevLine},{"nextLine",p->debug->nextLine}};
    return result;
  }
  if(std::holds_alternative<CursorHidePatch>(patch))
    return Json{{"type","cursorHide"}};
  if(std::holds_alternative<CursorShowPatch>(patch))
    return Json{{"type","cursorShow"}};
  if(const auto p=std::get_if<CursorMovePatch>(&patch))
    return Json{{"type","cursorMove"},{"x",p->x},{"y",p->y}};
  if(const auto p=std::get_if<CursorToPatch>(&patch))
    return Json{{"type","cursorTo"},{"col",p->col}};
  if(std::holds_alternative<CarriageReturnPatch>(patch))
    return Json{{"type","carriageReturn"}};
  if(const auto p=std::get_if<HyperlinkPatch>(&patch))
    return Json{{"type","hyperlink"},{"uri",p->uri}};
  return Json{{"type","styleStr"},
    {"str",std::get<StyleStringPatch>(patch).str}};
}
}
Json patchesToJson(const terminal_renderer::Diff& diff){
  Json result=Json::array();
  for(const auto& patch:diff)result.push_back(patchToJson(patch));
  return result;
}
} // namespace batbox::terminal_log_update
