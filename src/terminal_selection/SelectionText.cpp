#include "terminal_selection/Selection.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>

namespace batbox::terminal_selection {
namespace {
bool typedOne(const terminal_screen::Screen& screen,double index){
  return std::isfinite(index)&&index>=0&&std::trunc(index)==index&&
    index<screen.noSelect->size()&&
    (*screen.noSelect)[static_cast<std::size_t>(index)]==1;
}
double softWrapAt(const terminal_screen::Screen& screen,double row){
  if(!std::isfinite(row)||row<0||std::trunc(row)!=row||
      row>=screen.softWrap->size())return 0;
  return (*screen.softWrap)[static_cast<std::size_t>(row)];
}
bool jsWhitespace(char16_t c){
  return (c>=0x0009&&c<=0x000D)||c==0x0020||c==0x00A0||
    c==0x1680||(c>=0x2000&&c<=0x200A)||c==0x2028||
    c==0x2029||c==0x202F||c==0x205F||c==0x3000||c==0xFEFF;
}
std::string trimRightJsWhitespace(const std::string& line){
  auto units=value::utf16CodeUnits(line);
  while(!units.empty()&&jsWhitespace(units.back()))units.pop_back();
  return value::fromUtf16(units);
}
std::optional<bool> flagAt(const std::vector<bool>& values,std::size_t i){
  return i<values.size()?std::optional<bool>(values[i]):std::nullopt;
}
}
std::string extractRowText(const terminal_screen::Screen& screen,double row,
    double colStart,double colEnd){
  const double rowOff=row*screen.width;
  const double contentEnd=row+1<screen.height?softWrapAt(screen,row+1):0;
  const double lastCol=contentEnd>0?std::min(colEnd,contentEnd-1):colEnd;
  std::string line;
  for(double col=colStart;col<=lastCol;col+=1){
    if(typedOne(screen,rowOff+col))continue;
    const auto cell=terminal_screen::cellAt(screen,col,row);
    if(!cell)continue;
    if(cell->width==terminal_screen::CellWidth::SpacerTail||
        cell->width==terminal_screen::CellWidth::SpacerHead)continue;
    line+=cell->character;
  }
  return contentEnd>0?line:trimRightJsWhitespace(line);
}
void joinRows(std::vector<std::string>& lines,const std::string& text,
    std::optional<bool> softWrap){
  if(softWrap.value_or(false)&&!lines.empty())lines.back()+=text;
  else lines.push_back(text);
}
std::string getSelectedText(const SelectionState& s,
    const terminal_screen::Screen& screen){
  const auto b=selectionBounds(s);
  if(!b)return {};
  const auto& start=*b->start;const auto& end=*b->end;
  std::vector<std::string> lines;
  for(std::size_t i=0;i<s.scrolledOffAbove->size();++i)
    joinRows(lines,(*s.scrolledOffAbove)[i],flagAt(*s.scrolledOffAboveSW,i));
  for(double row=start.row;row<=end.row;row+=1){
    const double rowStart=row==start.row?start.col:0;
    const double rowEnd=row==end.row?end.col:screen.width-1;
    joinRows(lines,extractRowText(screen,row,rowStart,rowEnd),
      softWrapAt(screen,row)>0);
  }
  for(std::size_t i=0;i<s.scrolledOffBelow->size();++i)
    joinRows(lines,(*s.scrolledOffBelow)[i],flagAt(*s.scrolledOffBelowSW,i));
  std::string result;
  for(std::size_t i=0;i<lines.size();++i){
    if(i)result+='\n';result+=lines[i];
  }
  return result;
}
void captureScrolledRows(SelectionState& s,
    const terminal_screen::Screen& screen,double firstRow,double lastRow,
    bool above){
  const auto b=selectionBounds(s);
  if(!b||firstRow>lastRow)return;
  const auto& start=*b->start;const auto& end=*b->end;
  const double lo=std::max(firstRow,start.row);
  const double hi=std::min(lastRow,end.row);
  if(lo>hi)return;
  const double width=screen.width;
  std::vector<std::string> captured;
  std::vector<bool> capturedSW;
  for(double row=lo;row<=hi;row+=1){
    const double colStart=row==start.row?start.col:0;
    const double colEnd=row==end.row?end.col:width-1;
    captured.push_back(extractRowText(screen,row,colStart,colEnd));
    capturedSW.push_back(softWrapAt(screen,row)>0);
  }
  if(above){
    s.scrolledOffAbove->insert(s.scrolledOffAbove->end(),
      captured.begin(),captured.end());
    s.scrolledOffAboveSW->insert(s.scrolledOffAboveSW->end(),
      capturedSW.begin(),capturedSW.end());
    if(s.anchor&&s.anchor->row==start.row&&lo==start.row){
      s.anchor=std::make_shared<Point>(Point{0,s.anchor->row});
      if(s.anchorSpan)
        s.anchorSpan=std::make_shared<AnchorSpan>(AnchorSpan{
          std::make_shared<Point>(Point{0,s.anchorSpan->lo->row}),
          std::make_shared<Point>(Point{width-1,s.anchorSpan->hi->row}),
          s.anchorSpan->kind});
    }
  }else{
    s.scrolledOffBelow->insert(s.scrolledOffBelow->begin(),
      captured.begin(),captured.end());
    s.scrolledOffBelowSW->insert(s.scrolledOffBelowSW->begin(),
      capturedSW.begin(),capturedSW.end());
    if(s.anchor&&s.anchor->row==end.row&&hi==end.row){
      s.anchor=std::make_shared<Point>(Point{width-1,s.anchor->row});
      if(s.anchorSpan)
        s.anchorSpan=std::make_shared<AnchorSpan>(AnchorSpan{
          std::make_shared<Point>(Point{0,s.anchorSpan->lo->row}),
          std::make_shared<Point>(Point{width-1,s.anchorSpan->hi->row}),
          s.anchorSpan->kind});
    }
  }
}
} // namespace batbox::terminal_selection
