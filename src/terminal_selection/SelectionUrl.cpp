#include "terminal_selection/Selection.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>

namespace batbox::terminal_selection {
namespace {
bool noSelectAt(const terminal_screen::Screen& screen,double index){
  return std::isfinite(index)&&index>=0&&std::trunc(index)==index&&
    index<screen.noSelect->size()&&
    (*screen.noSelect)[static_cast<std::size_t>(index)]==1;
}
bool isUrlChar(const std::string& value){
  if(value::stringLength(value)!=1||value.size()!=1)return false;
  const unsigned char code=value[0];
  return code>=0x21&&code<=0x7e&&
    value!="<"&&value!=">"&&value!="\""&&value!="'"&&
    value!="`"&&value!=" ";
}
std::size_t schemeLength(const std::string& token,std::size_t at){
  if(token.compare(at,7,"http://")==0)return 7;
  if(token.compare(at,8,"https://")==0)return 8;
  if(token.compare(at,7,"file://")==0)return 7;
  return 0;
}
}
std::optional<std::string> findPlainTextUrlAt(
    const terminal_screen::Screen& screen,double col,double row){
  if(row<0||row>=screen.height)return {};
  const double width=screen.width,rowOff=row*width;
  double c=col;
  if(c>0){
    const auto cell=terminal_screen::cellAt(screen,c,row);
    if(cell&&cell->width==terminal_screen::CellWidth::SpacerTail)c-=1;
  }
  if(c<0||c>=width||noSelectAt(screen,rowOff+c))return {};
  const auto start=terminal_screen::cellAt(screen,c,row);
  if(!start||!isUrlChar(start->character))return {};
  double lo=c;
  while(lo>0){
    const double prev=lo-1;
    if(noSelectAt(screen,rowOff+prev))break;
    const auto pc=terminal_screen::cellAt(screen,prev,row);
    if(!pc||pc->width!=terminal_screen::CellWidth::Narrow||
        !isUrlChar(pc->character))break;
    lo=prev;
  }
  double hi=c;
  while(hi<width-1){
    const double next=hi+1;
    if(noSelectAt(screen,rowOff+next))break;
    const auto nc=terminal_screen::cellAt(screen,next,row);
    if(!nc||nc->width!=terminal_screen::CellWidth::Narrow||
        !isUrlChar(nc->character))break;
    hi=next;
  }
  std::string token;
  for(double i=lo;i<=hi;i+=1)
    token+=terminal_screen::cellAt(screen,i,row)->character;
  const double clickIdx=c-lo;
  std::ptrdiff_t urlStart=-1;
  std::size_t urlEnd=token.size();
  for(std::size_t i=0;i<token.size();){
    const auto length=schemeLength(token,i);
    if(!length){++i;continue;}
    if(i>clickIdx){urlEnd=i;break;}
    urlStart=static_cast<std::ptrdiff_t>(i);
    i+=length;
  }
  if(urlStart<0)return {};
  std::string url=token.substr(static_cast<std::size_t>(urlStart),
    urlEnd-static_cast<std::size_t>(urlStart));
  while(!url.empty()){
    const char last=url.back();
    if(last=='.'||last==','||last==';'||last==':'||last=='!'||last=='?'){
      url.pop_back();continue;
    }
    char opener=0;
    if(last==')')opener='(';
    else if(last==']')opener='[';
    else if(last=='}')opener='{';
    if(!opener)break;
    std::size_t opens=0,closes=0;
    for(const char ch:url){
      if(ch==opener)++opens;
      else if(ch==last)++closes;
    }
    if(closes>opens)url.pop_back();
    else break;
  }
  if(clickIdx>=urlStart+url.size())return {};
  return url;
}
} // namespace batbox::terminal_selection
