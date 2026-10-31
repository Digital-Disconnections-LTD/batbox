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
void put(screen::Screen& s,int x,int y,const std::string& value,
    screen::CellWidth width=screen::CellWidth::Narrow){
  screen::setCellAt(s,x,y,screen::Cell{value,0,width,{}});
}
Json snapshot(const screen::Screen& s){
  Json words=Json::array(),no=Json::array(),wrap=Json::array();
  for(std::size_t i=0;i<s.cells->length32();i++)words.push_back(*s.cells->word(i));
  for(const auto value:*s.noSelect)no.push_back(value);
  for(const auto value:*s.softWrap)wrap.push_back(value);
  Json damage=nullptr;
  if(s.damage)damage=Json{{"x",s.damage->x},{"y",s.damage->y},
    {"width",s.damage->width},{"height",s.damage->height}};
  return Json{{"cells",words},{"noSelect",no},
    {"softWrap",wrap},{"damage",damage}};
}
void append(Json& rows,const std::string& kind,
    const screen::Screen& s,std::optional<bool> sameDamage={}){
  auto value=snapshot(s);
  value["kind"]=kind;
  if(sameDamage)value["sameDamage"]=*sameDamage;
  rows.push_back(value);
}
}
int main(){
  Json rows=Json::array();
  {
    Pools p;auto src=p.make(3,2),dst=p.make(3,2);
    put(*src,0,0,"A");put(*src,2,1,"B");
    (*src->noSelect)[1]=1;(*src->noSelect)[5]=1;(*src->softWrap)[1]=2;
    screen::blitRegion(*dst,*src,0,0,3,2);
    append(rows,"blitFull",*dst);
  }
  {
    Pools p;auto src=p.make(3,1),dst=p.make(4,1);
    put(*src,1,0,"😀",screen::CellWidth::Wide);
    (*src->noSelect)[1]=1;(*src->softWrap)[0]=2;
    screen::blitRegion(*dst,*src,1,0,2,1);
    append(rows,"blitPartialWide",*dst);
  }
  {
    Pools p;auto s=p.make(4,1);put(*s,1,0,"😀",screen::CellWidth::Wide);
    (*s->noSelect)[2]=1;auto old=s->damage;
    screen::clearRegion(*s,2,0,1,1);
    append(rows,"clearLeft",*s,s->damage==old);
  }
  {
    Pools p;auto s=p.make(4,1);put(*s,2,0,"😀",screen::CellWidth::Wide);
    auto old=s->damage;screen::clearRegion(*s,2,0,1,1);
    append(rows,"clearRight",*s,s->damage==old);
  }
  {
    Pools p;auto s=p.make(2,2);put(*s,0,0,"A");put(*s,1,1,"B");
    (*s->noSelect)[0]=1;(*s->softWrap)[1]=1;
    screen::clearRegion(*s,0,0,2,2);
    append(rows,"clearFull",*s);
  }
  for(const auto& [kind,n]:
      std::vector<std::pair<std::string,int>>{{"shiftUp",1},
        {"shiftDown",-1},{"shiftAll",3}}){
    Pools p;auto s=p.make(2,3);
    put(*s,0,0,"A");put(*s,0,1,"B");put(*s,0,2,"C");
    (*s->noSelect)[0]=1;(*s->noSelect)[2]=2;(*s->noSelect)[4]=3;
    (*s->softWrap)[0]=1;(*s->softWrap)[1]=2;(*s->softWrap)[2]=3;
    auto old=s->damage;screen::shiftRows(*s,0,2,n);
    append(rows,kind,*s,s->damage==old);
  }
  {
    Pools p;auto src=p.make(4,3),dst=p.make(4,3);
    put(*src,0,0,"A");put(*src,1,1,"B");put(*src,2,2,"C");
    (*src->noSelect)[1]=1;(*src->noSelect)[6]=2;(*src->softWrap)[1]=3;
    screen::blitRegion(*dst,*src,0.5,0.5,2.8,2.8);
    append(rows,"fractionBlit",*dst);
  }
  {
    Pools p;auto s=p.make(4,3);
    put(*s,0,0,"A");put(*s,1,1,"B");put(*s,2,2,"C");
    screen::clearRegion(*s,0.5,0.5,2.3,2.3);
    append(rows,"fractionClear",*s);
  }
  {
    Pools p;auto s=p.make(2,3);
    put(*s,0,0,"A");put(*s,0,1,"B");put(*s,0,2,"C");
    (*s->noSelect)[0]=1;(*s->noSelect)[2]=2;(*s->noSelect)[4]=3;
    (*s->softWrap)[0]=1;(*s->softWrap)[1]=2;(*s->softWrap)[2]=3;
    screen::shiftRows(*s,0.5,2.5,0.5);
    append(rows,"fractionShift",*s);
  }
  {
    Pools p;auto src=p.make(3,1),dst=p.make(2,1);
    Json error=nullptr;
    try{screen::blitRegion(*dst,*src,1,0,3,1);}
    catch(const std::range_error& e){
      error=Json::array({"RangeError",e.what()});
    }
    auto value=snapshot(*dst);
    value["kind"]="blitError";value["error"]=error;
    rows.push_back(value);
  }
  std::cout<<rows.dump()<<'\n';
}
