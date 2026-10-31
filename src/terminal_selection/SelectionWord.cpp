#include "terminal_selection/Selection.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <unicode/uchar.h>
#include <unicode/utf16.h>

namespace batbox::terminal_selection {
namespace {
PointRef point(double col,double row){
  return std::make_shared<Point>(Point{col,row});
}
bool noSelectAt(const terminal_screen::Screen& screen,double index){
  return std::isfinite(index)&&index>=0&&std::trunc(index)==index&&
    index<screen.noSelect->size()&&
    (*screen.noSelect)[static_cast<std::size_t>(index)]==1;
}
int charClass(const std::string& value){
  if(value==" "||value.empty())return 0;
  const auto units=value::utf16CodeUnits(value);
  int32_t i=0;
  while(i<static_cast<int32_t>(units.size())){
    UChar32 code;
    U16_NEXT(units.data(),i,static_cast<int32_t>(units.size()),code);
    const auto category=u_charType(code);
    if(category>=U_UPPERCASE_LETTER&&category<=U_OTHER_LETTER)return 1;
    if(category==U_DECIMAL_DIGIT_NUMBER||category==U_LETTER_NUMBER||
        category==U_OTHER_NUMBER)return 1;
    if(code=='_'||code=='/'||code=='.'||code=='-'||code=='+'||
        code=='~'||code=='\\')return 1;
  }
  return 2;
}
}
std::optional<std::pair<double,double>> wordBoundsAt(
    const terminal_screen::Screen& screen,double col,double row){
  if(row<0||row>=screen.height)return {};
  const double width=screen.width,rowOff=row*width;
  double c=col;
  if(c>0){
    const auto cell=terminal_screen::cellAt(screen,c,row);
    if(cell&&cell->width==terminal_screen::CellWidth::SpacerTail)c-=1;
  }
  if(c<0||c>=width||noSelectAt(screen,rowOff+c))return {};
  const auto startCell=terminal_screen::cellAt(screen,c,row);
  if(!startCell)return {};
  const int cls=charClass(startCell->character);
  double lo=c;
  while(lo>0){
    const double prev=lo-1;
    if(noSelectAt(screen,rowOff+prev))break;
    const auto pc=terminal_screen::cellAt(screen,prev,row);
    if(!pc)break;
    if(pc->width==terminal_screen::CellWidth::SpacerTail){
      if(prev==0||noSelectAt(screen,rowOff+prev-1))break;
      const auto head=terminal_screen::cellAt(screen,prev-1,row);
      if(!head||charClass(head->character)!=cls)break;
      lo=prev-1;continue;
    }
    if(charClass(pc->character)!=cls)break;
    lo=prev;
  }
  double hi=c;
  while(hi<width-1){
    const double next=hi+1;
    if(noSelectAt(screen,rowOff+next))break;
    const auto nc=terminal_screen::cellAt(screen,next,row);
    if(!nc)break;
    if(nc->width==terminal_screen::CellWidth::SpacerTail){hi=next;continue;}
    if(charClass(nc->character)!=cls)break;
    hi=next;
  }
  return std::pair{lo,hi};
}
void selectWordAt(SelectionState& s,const terminal_screen::Screen& screen,
    double col,double row){
  const auto bounds=wordBoundsAt(screen,col,row);
  if(!bounds)return;
  const auto lo=point(bounds->first,row),hi=point(bounds->second,row);
  s.anchor=lo;s.focus=hi;s.isDragging=true;
  s.anchorSpan=std::make_shared<AnchorSpan>(AnchorSpan{lo,hi,SpanKind::Word});
}
void selectLineAt(SelectionState& s,const terminal_screen::Screen& screen,
    double row){
  if(row<0||row>=screen.height)return;
  const auto lo=point(0,row),hi=point(screen.width-1,row);
  s.anchor=lo;s.focus=hi;s.isDragging=true;
  s.anchorSpan=std::make_shared<AnchorSpan>(AnchorSpan{lo,hi,SpanKind::Line});
}
void extendSelection(SelectionState& s,const terminal_screen::Screen& screen,
    double col,double row){
  if(!s.isDragging||!s.anchorSpan)return;
  const auto span=s.anchorSpan;
  PointRef mLo,mHi;
  if(span->kind==SpanKind::Word){
    const auto b=wordBoundsAt(screen,col,row);
    mLo=point(b?b->first:col,row);
    mHi=point(b?b->second:col,row);
  }else{
    const auto r=terminal_screen::clamp(row,0,screen.height-1);
    mLo=point(0,r);mHi=point(screen.width-1,r);
  }
  if(comparePoints(*mHi,*span->lo)<0){
    s.anchor=span->hi;s.focus=mLo;
  }else if(comparePoints(*mLo,*span->hi)>0){
    s.anchor=span->lo;s.focus=mHi;
  }else{
    s.anchor=span->lo;s.focus=span->hi;
  }
}
} // namespace batbox::terminal_selection
