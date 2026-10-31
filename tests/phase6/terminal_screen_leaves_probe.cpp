#include "terminal_screen/Screen.hpp"
#include "terminal_screen/HyperlinkStyles.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <iostream>

using namespace batbox;
namespace screen=batbox::terminal_screen;
namespace ansi=batbox::ansi_tokenize;
namespace {
ansi::CodeRef code(const std::string& value){
  return std::make_shared<ansi::AnsiCode>(ansi::AnsiCode{"ansi",value,""});
}
Json styleRow(const std::string& kind,const ansi::Codes& input){
  const auto filtered=screen::filterOutHyperlinkStyles(input);
  Json codes=Json::array();bool refs=true;
  for(const auto& value:filtered){
    codes.push_back(value->code);
    if(std::find(input.begin(),input.end(),value)==input.end())refs=false;
  }
  const auto uri=screen::extractHyperlinkFromStyles(input);
  return Json{{"kind",kind},{"extract",uri?Json(*uri):Json(nullptr)},
    {"filtered",codes},{"retainedRefs",refs}};
}
Json marks(const screen::Screen& s){
  Json output=Json::array();
  for(const auto value:*s.noSelect)output.push_back(value);
  return output;
}
}
int main(){
  Json rows=Json::array();
  const auto ansiColor=code("\x1b[31m");
  const auto valid=code("\x1b]8;;https://a\x07");
  const auto empty=code("\x1b]8;;\x07");
  const auto malformed=code("\x1b]8;id=x;https://bad\x07");
  const auto embedded=code("\x1b]8;;https://x\x07" "bad\x07");
  const auto unicode=code("\x1b]8;;https://é\x07");
  rows.push_back(styleRow("valid",{ansiColor,valid,empty,malformed,embedded,unicode}));
  rows.push_back(styleRow("empty",{empty}));
  rows.push_back(styleRow("malformed",{malformed,embedded}));
  rows.push_back(styleRow("unicode",{unicode}));
  rows.push_back(styleRow("none",{ansiColor}));
  auto chars=std::make_shared<screen::CharPool>();
  auto links=std::make_shared<screen::HyperlinkPool>();
  auto styles=std::make_shared<screen::StylePool>();
  auto s=screen::createScreen(4,3,styles,chars,links,{});
  auto damage=s->damage;
  screen::markNoSelectRegion(*s,-1,1,4,2);
  rows.push_back(Json{{"kind","mark"},{"marks",marks(*s)},
    {"sameDamage",s->damage==damage}});
  screen::markNoSelectRegion(*s,3,2,3,2);
  rows.push_back(Json{{"kind","markClip"},{"marks",marks(*s)}});
  screen::markNoSelectRegion(*s,1,0,0,2);
  rows.push_back(Json{{"kind","markZero"},{"marks",marks(*s)}});
  {
    auto next=screen::createScreen(4,3,styles,
      std::make_shared<screen::CharPool>(),
      std::make_shared<screen::HyperlinkPool>(),{});
    screen::markNoSelectRegion(*next,0,0,-1,1);
    rows.push_back(Json{{"kind","markNegativeEnd"},
      {"marks",marks(*next)}});
  }
  {
    auto next=screen::createScreen(4,3,styles,
      std::make_shared<screen::CharPool>(),
      std::make_shared<screen::HyperlinkPool>(),{});
    screen::markNoSelectRegion(*next,1.8,0.5,2.5,2.2);
    rows.push_back(Json{{"kind","markFractional"},
      {"marks",marks(*next)}});
  }
  std::cout<<rows.dump()<<'\n';
}
