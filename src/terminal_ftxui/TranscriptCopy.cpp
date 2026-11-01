#include "terminal_ftxui/TranscriptCopy.hpp"
#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace batbox::terminal_ftxui {
namespace {
std::string join(const std::vector<std::string>& cells,std::size_t first,
    std::size_t end){
  std::string text;
  for(auto i=first;i<end&&i<cells.size();++i)text+=cells[i];
  return text;
}
std::size_t normalizedStart(const std::vector<std::string>& cells,
    std::size_t first){
  return first<cells.size()&&cells[first].empty()&&first>0?first-1:first;
}
bool before(CopyPosition left,CopyPosition right){
  return left.row<right.row||
    (left.row==right.row&&left.column<=right.column);
}
}
TranscriptCopy::TranscriptCopy(const interactive::PresentationState& state,
    int columns,int rows){resize(state,columns,rows);top_=lines_.size();clampTop();}
void TranscriptCopy::resize(const interactive::PresentationState& state,
    int columns,int rows){
  if(columns<=0||rows<3)throw std::invalid_argument("Copy view needs terminal space");
  columns_=columns;rows_=rows;lines_=projectCopyLines(state,columns);
  anchor_.reset();end_.reset();dragging_=moved_=false;clampTop();
}
int TranscriptCopy::bodyHeight()const{return rows_-2;}
void TranscriptCopy::clampTop(){
  const auto maximum=lines_.size()>static_cast<std::size_t>(bodyHeight())?
    lines_.size()-static_cast<std::size_t>(bodyHeight()):0;
  top_=std::min(top_,maximum);
}
void TranscriptCopy::scrollBy(int rows){
  const auto old=top_;
  if(rows<0)top_-=std::min(top_,static_cast<std::size_t>(-rows));
  else top_+=static_cast<std::size_t>(rows);
  clampTop();
  if(dragging_&&old!=top_){end_=position(mouseX_,mouseY_);moved_=true;}
}
CopyPosition TranscriptCopy::position(int x,int y)const{
  if(lines_.empty())return {};
  const auto row=std::min(lines_.size()-1,top_+
    static_cast<std::size_t>(std::clamp(y-1,0,bodyHeight()-1)));
  return {row,std::clamp(x,0,columns_-1)};
}
void TranscriptCopy::press(int x,int y){
  if(y<1||y>bodyHeight()||lines_.empty())return;
  mouseX_=x;mouseY_=y;anchor_=end_=position(x,y);
  dragging_=true;moved_=false;
}
void TranscriptCopy::move(int x,int y){
  if(!dragging_)return;
  mouseX_=x;mouseY_=y;
  const auto next=position(x,y);
  if(!end_||next.row!=end_->row||next.column!=end_->column)moved_=true;
  end_=next;
}
std::optional<std::string> TranscriptCopy::release(int x,int y){
  if(!dragging_)return {};
  move(x,y);dragging_=false;
  if(!moved_)return {};
  auto text=selectedText();
  if(text.empty())return {};
  return text;
}
std::pair<CopyPosition,CopyPosition> TranscriptCopy::ordered()const{
  if(!anchor_||!end_)return {};
  return before(*anchor_,*end_)?std::pair{*anchor_,*end_}:
    std::pair{*end_,*anchor_};
}
std::string TranscriptCopy::selectedText()const{
  if(!anchor_||!end_||lines_.empty())return {};
  const auto [first,last]=ordered();
  std::string result;
  for(auto row=first.row;row<=last.row;++row){
    if(row>first.row&&lines_[row-1].hardBreakAfter)result+='\n';
    const auto& cells=lines_[row].cells;
    const auto begin=normalizedStart(cells,row==first.row?
      static_cast<std::size_t>(first.column):0);
    const auto end=row==last.row?
      std::min(cells.size(),static_cast<std::size_t>(last.column+1)):
      cells.size();
    if(begin<end)result+=join(cells,begin,end);
  }
  return result;
}
int TranscriptCopy::edgeDirection()const{
  if(!dragging_)return 0;
  if(mouseY_<1)return -1;
  if(mouseY_>bodyHeight())return 1;
  return 0;
}
ftxui::Element TranscriptCopy::render()const{
  ftxui::Elements rows;
  rows.push_back(ftxui::text("COPY TRANSCRIPT  ·  drag and scroll to select")|
    ftxui::bold|ftxui::size(ftxui::HEIGHT,ftxui::EQUAL,1));
  const auto [first,last]=ordered();
  for(int visible=0;visible<bodyHeight();++visible){
    const auto row=top_+static_cast<std::size_t>(visible);
    if(row>=lines_.size()){
      rows.push_back(ftxui::text("")|ftxui::size(ftxui::HEIGHT,ftxui::EQUAL,1));
      continue;
    }
    const auto& cells=lines_[row].cells;
    const bool selected=anchor_&&end_&&row>=first.row&&row<=last.row;
    if(!selected){
      rows.push_back(ftxui::text(join(cells,0,cells.size()))|
        ftxui::size(ftxui::HEIGHT,ftxui::EQUAL,1));
      continue;
    }
    const auto begin=normalizedStart(cells,row==first.row?
      static_cast<std::size_t>(first.column):0);
    const auto end=row==last.row?
      std::min(cells.size(),static_cast<std::size_t>(last.column+1)):
      cells.size();
    rows.push_back(ftxui::hbox({
      ftxui::text(join(cells,0,begin)),
      ftxui::text(join(cells,begin,end))|ftxui::inverted,
      ftxui::text(join(cells,end,cells.size()))
    })|ftxui::size(ftxui::HEIGHT,ftxui::EQUAL,1));
  }
  rows.push_back(ftxui::text("Wheel / PgUp / PgDn scroll  ·  Enter copies  ·  Esc returns")|
    ftxui::dim|ftxui::size(ftxui::HEIGHT,ftxui::EQUAL,1));
  return ftxui::vbox(std::move(rows))|
    ftxui::size(ftxui::WIDTH,ftxui::EQUAL,columns_)|
    ftxui::size(ftxui::HEIGHT,ftxui::EQUAL,rows_);
}
} // namespace batbox::terminal_ftxui
