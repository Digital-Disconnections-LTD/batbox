#include "terminal_output/OutputRender.hpp"
#include "terminal_screen/Screen.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <algorithm>

using namespace batbox;
namespace out=batbox::terminal_output;
namespace scr=batbox::terminal_screen;
namespace {
struct Fixture {
  std::shared_ptr<scr::CharPool> chars=std::make_shared<scr::CharPool>();
  std::shared_ptr<scr::HyperlinkPool> links=std::make_shared<scr::HyperlinkPool>();
  std::shared_ptr<scr::StylePool> styles=std::make_shared<scr::StylePool>();
  scr::ScreenRef make(double w,double h){
    return scr::createScreen(w,h,styles,chars,links,{});
  }
};
double width(const std::string& text){
  double result=0;
  for(const unsigned char c:text)if(c>0x1f)result++;
  return result;
}
out::OutputRenderServices services(){
  out::OutputRenderServices s;
  s.line.tokenize=ansi_tokenize::tokenize;
  s.line.styledCharsFromTokens=ansi_tokenize::styledCharsFromTokens;
  auto segmenter=std::make_shared<intl::Segmenter>();
  segmenter->segment=[](const std::string& input){
    auto index=std::make_shared<std::size_t>(0);
    return intl::SegmentCursor{[input,index]()->std::optional<intl::Segment>{
      if(*index>=input.size())return {};
      auto text=input.substr((*index)++,1);
      return intl::Segment{text,input,*index-1,{}};
    }};
  };
  s.line.getGraphemeSegmenter=[segmenter]{return segmenter;};
  s.line.stringWidth=width;
  s.line.reorderBidi=[](const out::ClusteredCharsRef& value){return value;};
  s.widestLine=[](const std::string& text){
    double widest=0;std::size_t start=0;
    while(true){
      const auto pos=text.find('\n',start);
      widest=std::max(widest,width(text.substr(start,
        pos==std::string::npos?pos:pos-start)));
      if(pos==std::string::npos)break;
      start=pos+1;
    }
    return widest;
  };
  s.sliceAnsi=[](const std::string& text,double from,double to){
    const auto a=std::min(text.size(),static_cast<std::size_t>(std::max(0.0,from)));
    const auto b=std::min(text.size(),static_cast<std::size_t>(std::max(0.0,to)));
    return b>a?text.substr(a,b-a):std::string{};
  };
  s.logForDebugging=[](const std::string&){};
  return s;
}
Json snapshot(const scr::Screen& s){
  Json cells=Json::array(),wrap=Json::array(),marks=Json::array();
  for(int y=0;y<s.height;y++)for(int x=0;x<s.width;x++){
    const auto c=*scr::cellAt(s,x,y);
    cells.push_back(Json::array({c.character,c.styleId,
      static_cast<int>(c.width),c.hyperlink?Json(*c.hyperlink):Json(nullptr)}));
  }
  for(int y=0;y<s.height;y++)wrap.push_back(s.softWrap->at(y));
  for(int i=0;i<s.width*s.height;i++)marks.push_back(s.noSelect->at(i));
  Json damage=nullptr;
  if(s.damage)damage=Json{{"x",s.damage->x},{"y",s.damage->y},
    {"width",s.damage->width},{"height",s.damage->height}};
  return Json{{"cells",cells},{"softWrap",wrap},{"noSelect",marks},
    {"damage",damage}};
}
Json row(const char* kind,const scr::Screen& screen){
  auto result=snapshot(screen);result["kind"]=kind;return result;
}
}
int main(){
  Json rows=Json::array();auto render=services();
  {
    Fixture f;auto s=f.make(8,1);
    out::Output o(8,1,f.styles,s,{});
    o.write(0,0,"A\tB");o.get(render);
    auto r=row("tab",*s);r["cache"]=o.charStash.size();rows.push_back(r);
    o.reset(8,1,s);o.write(0,0,"A\tB");o.get(render);
    r=row("cache",*s);r["cache"]=o.charStash.size();rows.push_back(r);
  }
  {
    Fixture f;auto s=f.make(7,1);out::Output o(7,1,f.styles,s,{});
    o.write(0,0,"A\x1b[2JB\rC");o.get(render);
    rows.push_back(row("escape",*s));
  }
  {
    Fixture f;auto s=f.make(6,3);out::Output o(6,3,f.styles,s,{});
    o.clip(std::make_shared<out::Clip>(out::Clip{2,5,1,3}));
    o.write(0,0,"abcdef\nxyz",std::make_shared<std::vector<bool>>(
      std::initializer_list<bool>{false,true}));
    o.unclip();o.get(render);rows.push_back(row("clipWrap",*s));
  }
  {
    Fixture f;auto src=f.make(4,2),s=f.make(4,2);
    scr::setCellAt(*src,0,0,scr::Cell{"X",0,scr::CellWidth::Narrow,{}});
    scr::setCellAt(*src,0,1,scr::Cell{"Y",0,scr::CellWidth::Narrow,{}});
    out::Output o(4,2,f.styles,s,{});
    o.blit(src,0,0,4,2);
    o.clear(std::make_shared<scr::Rectangle>(scr::Rectangle{0,0,4,1}),true);
    o.noSelect(std::make_shared<scr::Rectangle>(scr::Rectangle{0,1,2,1}));
    o.get(render);rows.push_back(row("absoluteClear",*s));
  }
  {
    Fixture f;auto s=f.make(3,1);out::Output o(3,1,f.styles,s,{});
    o.write(0,0,"abc");
    o.noSelect(std::make_shared<scr::Rectangle>(scr::Rectangle{1,0,1,1}));
    o.get(render);rows.push_back(row("noSelectLast",*s));
  }
  {
    Fixture f;auto s=f.make(3,1);out::Output o(3,1,f.styles,s,{});
    auto chars=std::make_shared<out::ClusteredChars>();
    for(const auto& [value,w]:std::vector<std::pair<std::string,double>>{
        {"A",1},{"\u200b",0},{"B",1},{"本",2}})
      chars->push_back(std::make_shared<out::ClusteredChar>(
        out::ClusteredChar{value,w,0,{}}));
    o.charStash["cached"]=chars;o.write(0,0,"cached");o.get(render);
    rows.push_back(row("cachedWide",*s));
  }
  {
    Fixture f;auto src=f.make(2,3),s=f.make(2,3);
    for(int y=0;y<3;y++)scr::setCellAt(*src,0,y,scr::Cell{
      std::string(1,"ABC"[y]),0,scr::CellWidth::Narrow,{}});
    out::Output o(2,3,f.styles,s,{});
    o.blit(src,0,0,2,3);o.shift(0,2,1);o.get(render);
    rows.push_back(row("shiftAfterBlit",*s));
  }
  {
    auto red=std::make_shared<ansi_tokenize::AnsiCode>(
      ansi_tokenize::AnsiCode{"ansi","\x1b[31m",""});
    auto open=std::make_shared<ansi_tokenize::AnsiCode>(
      ansi_tokenize::AnsiCode{"ansi","\x1b]8;;https://a\x07",""});
    auto close=std::make_shared<ansi_tokenize::AnsiCode>(
      ansi_tokenize::AnsiCode{"ansi","\x1b]8;;\x07",""});
    ansi_tokenize::StyledChars chars{
      {"char","a",false,{red,open}},
      {"char","b",false,{red,open}},
      {"char","c",false,{red,close}},
      {"char","d",false,{}}};
    scr::StylePool pool;
    const auto clustered=out::styledCharsWithGraphemeClustering(
      chars,pool,render.line);
    Json values=Json::array();
    for(const auto& c:*clustered)values.push_back(Json{
      {"value",c->value},{"width",c->width},{"styleId",c->styleId},
      {"hyperlink",c->hyperlink?Json(*c->hyperlink):Json(nullptr)}});
    rows.push_back(Json{{"kind","clusters"},{"characters",values},
      {"sameStyles",out::stylesEqual(chars[0].styles,chars[1].styles)},
      {"differentStyles",out::stylesEqual(chars[1].styles,chars[2].styles)}});
  }
  {
    Fixture f;auto s=f.make(1001,1);
    out::Output o(1001,1,f.styles,s,{});
    Json logs=Json::array();
    auto debugRender=render;
    debugRender.logForDebugging=[&logs](const std::string& message){
      logs.push_back(message);};
    o.write(0,0,std::string(1001,'a'));o.get(debugRender);
    Json damage=nullptr;
    if(s->damage)damage=Json{{"x",s->damage->x},{"y",s->damage->y},
      {"width",s->damage->width},{"height",s->damage->height}};
    rows.push_back(Json{{"kind","debugRatio"},{"logs",logs},
      {"damage",damage}});
  }
  std::cout<<rows.dump()<<'\n';
}
