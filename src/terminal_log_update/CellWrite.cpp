#include "terminal_log_update/Internal.hpp"
#include "runtime/JsonValue.hpp"
#include <unicode/utf16.h>

namespace batbox::terminal_log_update {
bool needsWidthCompensation(const std::string& character){
  const auto units=value::utf16CodeUnits(character);
  if(units.empty())return false;
  int32_t index=0;UChar32 cp;
  U16_NEXT(units.data(),index,static_cast<int32_t>(units.size()),cp);
  if((cp>=0x1fa70&&cp<=0x1faff)||
      (cp>=0x1fb00&&cp<=0x1fbff))return true;
  if(units.size()>=2)
    for(const auto code:units)if(code==0xfe0f)return true;
  return false;
}
bool writeCellWithStyleStr(VirtualScreen& screen,const Cell& cell,
    const std::string& styleStr){
  const double cellWidth=cell.width==terminal_screen::CellWidth::Wide?2:1;
  const double px=screen.cursor.x,vw=screen.viewportWidth;
  if(cellWidth==2&&px<vw){
    const double threshold=value::stringLength(cell.character)>2?vw:vw+1;
    if(px+2>=threshold)return false;
  }
  auto& diff=screen.diff;
  if(!styleStr.empty())
    diff.push_back(terminal_renderer::StyleStringPatch{styleStr});
  const bool compensate=cellWidth==2&&
    needsWidthCompensation(cell.character);
  if(compensate&&px+1<vw){
    diff.push_back(terminal_renderer::CursorToPatch{px+2});
    diff.push_back(terminal_renderer::StdoutPatch{" "});
    diff.push_back(terminal_renderer::CursorToPatch{px+1});
  }
  diff.push_back(terminal_renderer::StdoutPatch{cell.character});
  if(compensate)
    diff.push_back(terminal_renderer::CursorToPatch{px+cellWidth+1});
  if(px>=vw){screen.cursor.x=cellWidth;screen.cursor.y++;}
  else screen.cursor.x=px+cellWidth;
  return true;
}
} // namespace batbox::terminal_log_update
