#include "terminal_screen/Screen.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
namespace screen=batbox::terminal_screen;
namespace {
struct Fixture {
  std::shared_ptr<screen::CharPool> chars=std::make_shared<screen::CharPool>();
  std::shared_ptr<screen::HyperlinkPool> links=std::make_shared<screen::HyperlinkPool>();
  std::shared_ptr<screen::StylePool> styles=std::make_shared<screen::StylePool>();
  screen::ScreenRef s=screen::createScreen(4,1,styles,chars,links,{});
};
screen::Cell cell(std::string c,screen::CellWidth w=screen::CellWidth::Narrow,
    int style=0,std::optional<std::string> link={}){
  return {std::move(c),style,w,std::move(link)};
}
Json view(const screen::Screen& s){
  Json out=Json::array();
  for(int y=0;y<s.height;y++)for(int x=0;x<s.width;x++){
    const auto c=*screen::cellAt(s,x,y);
    out.push_back(Json::array({c.character,c.styleId,
      static_cast<int>(c.width),c.hyperlink?Json(*c.hyperlink):Json(nullptr)}));
  }
  return out;
}
Json damage(const screen::Screen& s){
  if(!s.damage)return nullptr;
  return Json{{"x",s.damage->x},{"y",s.damage->y},
    {"width",s.damage->width},{"height",s.damage->height}};
}
Json words(const screen::Screen& s){
  Json out=Json::array();
  for(std::size_t i=0;i<s.cells->length32();i++)out.push_back(*s.cells->word(i));
  return out;
}
}
int main(){
  Json rows=Json::array();
  {
    Fixture f;screen::setCellAt(*f.s,1,0,cell("A",screen::CellWidth::Narrow,2,"https://a"));
    rows.push_back(Json{{"kind","narrow"},{"view",view(*f.s)},
      {"damage",damage(*f.s)},{"cells",words(*f.s)}});
    auto old=f.s->damage;
    screen::setCellAt(*f.s,3,0,cell("B"));
    rows.push_back(Json{{"kind","expand"},{"sameDamage",f.s->damage==old},
      {"damage",damage(*f.s)}});
    screen::setCellAt(*f.s,8,0,cell("X"));
    rows.push_back(Json{{"kind","outOfBounds"},
      {"sameDamage",f.s->damage==old},{"damage",damage(*f.s)}});
  }
  {
    Fixture f;screen::setCellAt(*f.s,1,0,cell("😀",screen::CellWidth::Wide,3));
    rows.push_back(Json{{"kind","wide"},{"view",view(*f.s)},
      {"damage",damage(*f.s)}});
    screen::setCellAt(*f.s,1,0,cell("X"));
    rows.push_back(Json{{"kind","replaceHead"},{"view",view(*f.s)},
      {"damage",damage(*f.s)}});
  }
  {
    Fixture f;screen::setCellAt(*f.s,1,0,cell("😀",screen::CellWidth::Wide));
    screen::setCellAt(*f.s,2,0,cell("X"));
    rows.push_back(Json{{"kind","replaceTail"},{"view",view(*f.s)},
      {"damage",damage(*f.s)}});
  }
  {
    Fixture f;screen::setCellAt(*f.s,2,0,cell("😀",screen::CellWidth::Wide));
    screen::setCellAt(*f.s,1,0,cell("本",screen::CellWidth::Wide));
    rows.push_back(Json{{"kind","wideOverWide"},{"view",view(*f.s)},
      {"damage",damage(*f.s)}});
  }
  {
    Fixture f;screen::setCellAt(*f.s,1,0,cell("A",screen::CellWidth::Narrow,2,"https://a"));
    auto old=f.s->damage;
    screen::setCellStyleId(*f.s,1,0,7);
    rows.push_back(Json{{"kind","restyle"},{"view",view(*f.s)},
      {"sameDamage",f.s->damage==old},{"damage",damage(*f.s)}});
    screen::setCellAt(*f.s,2,0,cell("本",screen::CellWidth::Wide));
    auto oldWords=words(*f.s);
    auto before=f.s->damage;
    screen::setCellStyleId(*f.s,3,0,9);
    rows.push_back(Json{{"kind","skipSpacer"},
      {"sameCells",words(*f.s)==oldWords},
      {"sameDamage",f.s->damage==before}});
  }
  std::cout<<rows.dump()<<'\n';
}
