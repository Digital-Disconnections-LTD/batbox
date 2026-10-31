#include "terminal_screen/Screen.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
namespace screen=batbox::terminal_screen;
namespace {
struct Pools {
  std::shared_ptr<screen::CharPool> chars=std::make_shared<screen::CharPool>();
  std::shared_ptr<screen::HyperlinkPool> links=std::make_shared<screen::HyperlinkPool>();
  std::shared_ptr<screen::StylePool> styles=std::make_shared<screen::StylePool>();
  screen::ScreenRef make(int width,int height){
    return screen::createScreen(width,height,styles,chars,links,{});
  }
};
void put(screen::Screen& s,int x,int y,const std::string& character){
  screen::setCellAt(s,x,y,screen::Cell{character,0,screen::CellWidth::Narrow,{}});
}
Json cell(const std::optional<screen::Cell>& value){
  if(!value)return nullptr;
  return Json::array({value->character,value->styleId,
    static_cast<int>(value->width),
    value->hyperlink?Json(*value->hyperlink):Json(nullptr)});
}
Json changes(const screen::Screen& previous,const screen::Screen& next){
  Json output=Json::array();
  for(const auto& entry:screen::diff(previous,next))
    output.push_back(Json::array({entry.point.x,entry.point.y,
      cell(entry.removed),cell(entry.added)}));
  return output;
}
}
int main(){
  Json rows=Json::array();
  {
    Pools p;auto prev=p.make(0,0),next=p.make(3,1);
    put(*next,1,0,"A");
    rows.push_back(Json{{"kind","initial"},{"diff",changes(*prev,*next)}});
  }
  {
    Pools p;auto prev=p.make(2,1),next=p.make(2,1);
    put(*prev,0,0,"A");put(*next,0,0,"B");
    rows.push_back(Json{{"kind","sameWidth"},{"diff",changes(*prev,*next)}});
  }
  {
    Pools p;auto prev=p.make(2,1),next=p.make(2,1);
    next->cells->setWord(0,next->charPool->intern("A"));
    rows.push_back(Json{{"kind","noDamage"},{"diff",changes(*prev,*next)}});
  }
  {
    Pools p;auto prev=p.make(2,2),next=p.make(2,1);
    put(*prev,0,0,"A");put(*next,0,0,"A");
    rows.push_back(Json{{"kind","heightShrink"},{"diff",changes(*prev,*next)}});
  }
  {
    Pools p;auto prev=p.make(3,1),next=p.make(2,1);
    put(*prev,2,0,"A");
    rows.push_back(Json{{"kind","widthShrink"},{"diff",changes(*prev,*next)}});
  }
  {
    Pools p;auto prev=p.make(2,1),next=p.make(3,1);
    put(*prev,0,0,"A");put(*next,0,0,"A");put(*next,2,0,"B");
    rows.push_back(Json{{"kind","widthGrow"},{"diff",changes(*prev,*next)}});
  }
  {
    Pools p;auto prev=p.make(2,1),next=p.make(2,1);
    put(*next,0,0,"A");put(*next,1,0,"B");
    int calls=0;
    const auto stopped=screen::diffEach(*prev,*next,
      [&](double,double,const screen::Cell*,const screen::Cell*){
        calls++;return true;});
    rows.push_back(Json{{"kind","early"},{"stopped",stopped},
      {"calls",calls}});
  }
  {
    Pools p;auto prev=p.make(2,1),next=p.make(2,1);
    put(*prev,0,0,"A");put(*prev,1,0,"B");
    put(*next,0,0,"C");put(*next,1,0,"D");
    const screen::Cell* previousRef=nullptr;
    const screen::Cell* nextRef=nullptr;
    bool reused=false;Json firstAfter=nullptr;int calls=0;
    const auto stopped=screen::diffEach(*prev,*next,
      [&](double,double,const screen::Cell* removed,
          const screen::Cell* added){
        calls++;
        if(calls==1){previousRef=removed;nextRef=added;}
        else{
          reused=removed==previousRef&&added==nextRef;
          firstAfter=Json::array({previousRef->character,nextRef->character});
        }
        return false;
      });
    rows.push_back(Json{{"kind","reuse"},{"stopped",stopped},
      {"calls",calls},{"reused",reused},{"firstAfter",firstAfter}});
  }
  {
    Pools p;auto prev=p.make(2,2),next=p.make(2,2);
    put(*next,1,1,"A");
    next->damage=std::make_shared<screen::Rectangle>(
      screen::Rectangle{0.5,0.5,1.7,1.7});
    rows.push_back(Json{{"kind","fractionSameWidth"},
      {"diff",changes(*prev,*next)}});
  }
  {
    Pools p;auto prev=p.make(2,2),next=p.make(3,2);
    put(*next,2,1,"B");
    next->damage=std::make_shared<screen::Rectangle>(
      screen::Rectangle{0.5,0.5,2.2,1.7});
    rows.push_back(Json{{"kind","fractionDifferentWidth"},
      {"diff",changes(*prev,*next)}});
  }
  std::cout<<rows.dump()<<'\n';
}
