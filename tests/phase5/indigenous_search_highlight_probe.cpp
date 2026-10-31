#include "terminal_runtime/SearchHighlight.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
namespace scr=batbox::terminal_screen;
int main(){
  const std::vector<std::string> queries={"","ba","ab","aa","😀x",
    "i\u0307","abc","A","İ","x"};
  Json rows=Json::array();
  for(const auto& query:queries){
    auto styles=std::make_shared<scr::StylePool>();
    auto chars=std::make_shared<scr::CharPool>();
    auto links=std::make_shared<scr::HyperlinkPool>();
    auto screen=scr::createScreen(8,2,styles,chars,links,{});
    const auto put=[&](double x,double y,std::string value,
        scr::CellWidth width=scr::CellWidth::Narrow){
      scr::setCellAt(*screen,x,y,scr::Cell{std::move(value),0,width,{}});
    };
    for(int x=0;x<5;x++)put(x,0,std::string(1,"ababa"[x]));
    put(0,1,"İ");put(1,1,"I");put(2,1,"😀",scr::CellWidth::Wide);
    put(4,1,"x");put(5,1,"a");put(6,1,"b");put(7,1,"c");
    scr::markNoSelectRegion(*screen,6,1,1,1);
    const bool applied=terminal_runtime::applySearchHighlight(
      *screen,query,*styles);
    Json changed=Json::array();
    for(int i=0;i<16;i++)
      if(scr::cellAtIndex(*screen,i).styleId!=0)changed.push_back(i);
    rows.push_back({{"query",query},{"applied",applied},
      {"changed",changed}});
  }
  std::cout<<value::stringify(rows)<<'\n';
}
