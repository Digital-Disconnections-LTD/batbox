#include "terminal_render/ScreenSearch.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
namespace scr=batbox::terminal_screen;
namespace rend=batbox::terminal_render;
namespace {
Json positions(const std::vector<rend::MatchPosition>& values){
  Json result=Json::array();
  for(const auto& v:values)result.push_back(Json{
    {"row",v.row},{"col",v.col},{"len",v.len}});
  return result;
}
}
int main(){
  auto chars=std::make_shared<scr::CharPool>();
  auto links=std::make_shared<scr::HyperlinkPool>();
  auto styles=std::make_shared<scr::StylePool>();
  auto s=scr::createScreen(8,2,styles,chars,links,{});
  auto put=[&](double x,double y,std::string value,
      scr::CellWidth width=scr::CellWidth::Narrow){
    scr::setCellAt(*s,x,y,scr::Cell{std::move(value),0,width,{}});
  };
  for(int x=0;x<5;x++)put(x,0,std::string(1,"ababa"[x]));
  put(0,1,"İ");put(1,1,"I");put(2,1,"😀",scr::CellWidth::Wide);
  put(4,1,"x");put(5,1,"a");put(6,1,"b");put(7,1,"c");
  scr::markNoSelectRegion(*s,6,1,1,1);
  double clock=0;
  rend::ScanState scan{0,[&]{return clock+=3;}};
  Json rows=Json::array();
  auto add=[&](const char* kind,const std::string& query){
    rows.push_back(Json{{"kind",kind},{"matches",positions(
      rend::scanPositions(*s,query,scan))},{"clock",clock}});
  };
  add("empty","");add("overlap","ba");add("emoji","😀x");
  add("lowerExpand","i\u0307");
  const auto selected=rend::scanPositions(*s,"ac",scan);
  rows.push_back(Json{{"kind","noSelect"},
    {"matches",positions(selected)},{"clock",clock}});
  std::vector<std::int32_t> before;
  for(std::size_t i=0;i<s->cells->length32();i++)
    before.push_back(s->cells->word(i).value_or(0));
  const bool highlight=rend::applyPositionedHighlight(*s,*styles,selected,0,0);
  Json afterStyles=Json::array();
  for(int x=5;x<=7;x++)afterStyles.push_back(
    scr::cellAtIndex(*s,8+x).styleId);
  bool changed=false;
  for(std::size_t i=0;i<before.size();i++)
    if(before[i]!=s->cells->word(i).value_or(0)){changed=true;break;}
  const bool outOfRange=rend::applyPositionedHighlight(*s,*styles,
    selected,0,1);
  rows.push_back(Json{{"kind","highlight"},{"highlight",highlight},
    {"styles",afterStyles},{"changed",changed},
    {"outOfRange",outOfRange}});
  std::cout<<rows.dump()<<'\n';
}
