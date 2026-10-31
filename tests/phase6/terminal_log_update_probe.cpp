#include "terminal_log_update/LogUpdate.hpp"
#include "terminal_log_update/PatchJson.hpp"
#include "terminal_log_update/Internal.hpp"
#include "ansi_tokenize/Types.hpp"
#include <iostream>

using namespace batbox;
namespace lu=batbox::terminal_log_update;
namespace ts=batbox::terminal_screen;
namespace tr=batbox::terminal_renderer;
namespace {
struct Pools {
  std::shared_ptr<ts::StylePool> styles=std::make_shared<ts::StylePool>();
  std::shared_ptr<ts::CharPool> chars=std::make_shared<ts::CharPool>();
  std::shared_ptr<ts::HyperlinkPool> links=
    std::make_shared<ts::HyperlinkPool>();
};
ts::ScreenRef screen(double w,double h,const Pools& p){
  return ts::createScreen(w,h,p.styles,p.chars,p.links,
    [](const std::string&,const std::string&){});
}
void put(ts::Screen& s,double x,double y,const std::string& c){
  ts::setCellAt(s,x,y,ts::Cell{c,0,ts::CellWidth::Narrow,{}});
}
void putRow(ts::Screen& s,int row,const std::string& text){
  for(std::size_t i=0;i<text.size();++i)
    put(s,i,row,std::string(1,text[i]));
}
tr::Frame frame(ts::ScreenRef s,double vw=10,double vh=4,
    double cy=-1,double cx=0,bool visible=true){
  return {s,{vw,vh},{cx,cy<0?s->height:cy,visible},{},{}};
}
Json prevLine(const tr::Frame& f){
  std::string result;
  for(double x=0;x<f.screen->width;x+=1)
    result+=ts::charInCellAt(*f.screen,x,0).value_or(" ");
  return lu::trimEndJs(result);
}
}
int main(){
  Json rows=Json::array();
  auto run=[&](const char* kind,const tr::Frame& prev,
      const tr::Frame& next,const Pools& p,bool tty=true,
      bool alt=false,bool safe=true,std::vector<double> times={0,1}){
    std::vector<std::string> logs;
    auto current=times.begin();
    lu::Services services;
    services.performanceNow=[&](){
      return current==times.end()?1:*current++;
    };
    services.logForDebugging=[&](const std::string& value){
      logs.push_back(value);
    };
    services.oscModule=std::make_shared<termio::OscModule>(
      [](){return "xterm";});
    lu::LogUpdate update({tty,p.styles},services);
    const auto diff=update.render(prev,next,alt,safe);
    rows.push_back(Json{{"kind",kind},{"diff",lu::patchesToJson(diff)},
      {"logs",logs},{"prevLine",prevLine(prev)}});
  };
  {
    Pools p;run("empty",frame(screen(0,0,p)),frame(screen(0,0,p)),p);
  }
  {
    Pools p;auto a=screen(0,0,p),b=screen(3,2,p);
    putRow(*b,0,"abc");putRow(*b,1,"def");
    run("grow",frame(a),frame(b),p);
  }
  {
    Pools p;auto a=screen(3,1,p),b=screen(3,1,p);
    putRow(*a,0,"abc");putRow(*b,0,"axc");
    run("update",frame(a,10,4,0),frame(b,10,4,0),p);
  }
  {
    Pools p;auto a=screen(3,2,p),b=screen(3,1,p);
    putRow(*a,0,"abc");putRow(*a,1,"def");putRow(*b,0,"abc");
    run("shrink",frame(a),frame(b),p);
  }
  {
    Pools p;auto a=screen(3,1,p),b=screen(3,1,p);put(*b,0,0,"X");
    run("resize",frame(a,10,4),frame(b,9,4),p);
  }
  {
    Pools p;auto a=screen(3,3,p),b=screen(3,3,p);
    put(*a,0,0,"A");put(*b,0,0,"B");
    run("offscreen",frame(a,3,2,3),frame(b,3,2,3),p);
  }
  {
    Pools p;auto a=screen(3,2,p),b=screen(3,2,p);
    put(*a,0,0,"A");put(*a,0,1,"B");
    put(*b,0,0,"B");put(*b,0,1,"C");
    auto prev=frame(a,3,4,0),next=frame(b,3,4,0);
    next.scrollHint=std::optional<tr::ScrollHint>(tr::ScrollHint{0,1,1});
    run("altScroll",prev,next,p,true,true);
  }
  {
    Pools p;auto a=screen(3,2,p),b=screen(3,2,p);
    put(*a,0,0,"A");put(*a,0,1,"B");
    put(*b,0,0,"B");put(*b,0,1,"C");
    auto prev=frame(a,3,4,0),next=frame(b,3,4,0);
    next.scrollHint=std::optional<tr::ScrollHint>(tr::ScrollHint{0,1,1});
    run("altUnsafe",prev,next,p,true,true,false);
  }
  {
    Pools p;auto a=screen(3,4,p),b=screen(3,3,p);
    put(*a,0,0,"A");put(*b,0,0,"B");
    run("shrinkBelow",frame(a,3,3,4),frame(b,3,3,3),p);
  }
  {
    Pools p;auto a=screen(3,5,p),b=screen(3,1,p);
    put(*a,0,0,"A");put(*b,0,0,"B");
    run("clearTooMany",frame(a,3,2,0),frame(b,3,2,0),p);
  }
  {
    Pools p;auto a=screen(0,0,p),b=screen(3,1,p);
    putRow(*b,0,"abc");run("nonTTY",frame(a),frame(b),p,false);
  }
  {
    Pools p;auto a=screen(0,0,p),b=screen(2,1,p);
    auto codes=std::make_shared<ansi_tokenize::Codes>();
    codes->push_back(std::make_shared<ansi_tokenize::AnsiCode>(
      ansi_tokenize::AnsiCode{"ansi","\x1b[1m","\x1b[22m"}));
    const auto bold=p.styles->intern(codes);
    ts::setCellAt(*b,0,0,ts::Cell{"A",bold,ts::CellWidth::Narrow,
      std::string("https://x")});
    put(*b,1,0,"B");
    run("nonTTYStyledLink",frame(a),frame(b),p,false);
  }
  {
    Pools p;auto a=screen(3,1,p),b=screen(3,1,p);put(*b,0,0,"Z");
    run("slow",frame(a,10,4,0),frame(b,10,4,0),p,
      true,false,true,{100,160});
  }
  {
    Pools p;auto a=screen(0,0,p);
    lu::Services services;services.oscModule=
      std::make_shared<termio::OscModule>([](){return "xterm";});
    lu::LogUpdate tty({true,p.styles},services);
    rows.push_back(Json{{"kind","doneHidden"},
      {"diff",lu::patchesToJson(tty.renderPreviousOutput_DEPRECATED(
        frame(a,10,4,0,0,false)))}});
    rows.push_back(Json{{"kind","doneVisible"},
      {"diff",lu::patchesToJson(tty.renderPreviousOutput_DEPRECATED(
        frame(a,10,4,0,0,true)))}});
    lu::LogUpdate nonTTY({false,p.styles},services);
    rows.push_back(Json{{"kind","doneNonTTY"},
      {"diff",lu::patchesToJson(nonTTY.renderPreviousOutput_DEPRECATED(
        frame(a,10,4,0,0,false)))}});
  }
  std::cout<<rows.dump()<<'\n';
}
