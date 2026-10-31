#include "terminal_log_update/Internal.hpp"
#include "terminal_log_update/PatchJson.hpp"
#include "ansi_tokenize/Types.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
namespace lu=batbox::terminal_log_update;
namespace tr=batbox::terminal_renderer;
namespace ts=batbox::terminal_screen;
namespace {
Json cursor(const lu::VirtualScreen& s){
  return Json{{"x",s.cursor.x},{"y",s.cursor.y}};
}
}
int main(){
  Json rows=Json::array();
  for(const auto& value:std::vector<std::string>{
      "A","界","🫠","❤️","😀","🇺🇸"})
    rows.push_back(Json{{"kind","compensation"},{"value",value},
      {"needed",lu::needsWidthCompensation(value)}});
  struct WriteCase {const char* kind;double x,y,width;
    const char* character;ts::CellWidth cellWidth;const char* style;};
  for(const auto& c:std::vector<WriteCase>{
      {"wideSkip",5,0,6,"界",ts::CellWidth::Wide,"STYLE"},
      {"wideWrite",3,0,6,"界",ts::CellWidth::Wide,"STYLE"},
      {"emojiCompensate",1,0,10,"❤️",ts::CellWidth::Wide,"STYLE"},
      {"emojiAtEdge",4,0,6,"❤️",ts::CellWidth::Wide,"STYLE"},
      {"flagAtEdge",4,0,6,"🇺🇸",ts::CellWidth::Wide,"STYLE"},
      {"narrowPending",10,2,10,"A",ts::CellWidth::Narrow,""}}){
    lu::VirtualScreen s({c.x,c.y},c.width);
    const bool written=lu::writeCellWithStyleStr(s,
      ts::Cell{c.character,0,c.cellWidth,{}},c.style);
    rows.push_back(Json{{"kind",c.kind},{"written",written},
      {"cursor",cursor(s)},{"diff",lu::patchesToJson(s.diff)}});
  }
  struct MoveCase {const char* kind;double x,y,width,tx,ty;};
  for(const auto& c:std::vector<MoveCase>{
      {"moveSame",3,0,10,5,0},{"moveLine",3,0,10,1,2},
      {"moveWrap",10,0,10,2,1}}){
    lu::VirtualScreen s({c.x,c.y},c.width);
    lu::moveCursorTo(s,c.tx,c.ty);
    rows.push_back(Json{{"kind",c.kind},{"cursor",cursor(s)},
      {"diff",lu::patchesToJson(s.diff)}});
  }
  {
    tr::Diff diff;
    auto current=lu::transitionHyperlink(diff,{},std::string("a"));
    current=lu::transitionHyperlink(diff,current,std::string("a"));
    current=lu::transitionHyperlink(diff,current,{});
    rows.push_back(Json{{"kind","hyperlink"},
      {"diff",lu::patchesToJson(diff)},{"current",nullptr}});
  }
  {
    ts::StylePool styles;
    auto codes=std::make_shared<ansi_tokenize::Codes>();
    codes->push_back(std::make_shared<ansi_tokenize::AnsiCode>(
      ansi_tokenize::AnsiCode{"ansi","\x1b[1m","\x1b[22m"}));
    const auto bold=styles.intern(codes);
    tr::Diff diff;
    auto current=lu::transitionStyle(diff,styles,styles.none(),bold);
    current=lu::transitionStyle(diff,styles,current,bold);
    current=lu::transitionStyle(diff,styles,current,styles.none());
    rows.push_back(Json{{"kind","style"},
      {"diff",lu::patchesToJson(diff)},{"current",current}});
  }
  std::cout<<rows.dump()<<'\n';
}
