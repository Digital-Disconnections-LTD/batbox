#include "terminal_screen/StringPools.hpp"
#include "terminal_screen/StylePool.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <limits>

using namespace batbox;
namespace screen=batbox::terminal_screen;
namespace ansi=batbox::ansi_tokenize;
namespace {
ansi::CodeRef code(std::string start,std::string end){
  return std::make_shared<ansi::AnsiCode>(ansi::AnsiCode{
    "ansi",std::move(start),std::move(end)});
}
screen::StylesRef styles(std::initializer_list<ansi::CodeRef> codes){
  return std::make_shared<ansi::Codes>(codes);
}
Json codes(const screen::StylesRef& source){
  Json result=Json::array();
  for(const auto& value:*source)result.push_back(value->code);
  return result;
}
Json chars(){
  screen::CharPool pool;Json ids=Json::array(),values=Json::array();
  for(const auto& value:std::vector<std::string>{" ","","A","A","é","😀",
      "e\u0301",std::string(1,'\0'),"é"})ids.push_back(pool.intern(value));
  for(const auto id:std::vector<double>{0,1,2,3,4,5,6,999,-1,1.5,
      std::numeric_limits<double>::quiet_NaN()})
    values.push_back(pool.get(id));
  return Json{{"kind","chars"},{"ids",ids},{"values",values}};
}
Json sharedCharIds(){
  screen::CharPool first,second;
  return Json{{"kind","sharedCharIds"},{"first",first.intern("A")},
    {"second",second.intern("A")},{"other",second.intern("B")}};
}
Json hyperlinks(){
  screen::HyperlinkPool pool;Json ids=Json::array(),values=Json::array();
  for(const auto& value:std::vector<std::optional<std::string>>{
      {},"","https://a","https://a","https://b"})
    ids.push_back(pool.intern(value));
  for(const auto id:std::vector<double>{0,1,2,3,-1,1.5,
      std::numeric_limits<double>::quiet_NaN()}){
    const auto value=pool.get(id);
    values.push_back(value?Json(*value):Json(nullptr));
  }
  return Json{{"kind","hyperlinks"},{"ids",ids},{"values",values}};
}
void styleRows(Json& rows){
  const auto fg=code("\x1b[31m","\x1b[39m");
  const auto bg=code("\x1b[44m","\x1b[49m");
  screen::StylePool pool;
  auto empty=styles({}),foreground=styles({fg}),background=styles({bg});
  const auto fgId=pool.intern(foreground),bgId=pool.intern(background);
  rows.push_back(Json{{"kind","styleIntern"},{"none",pool.none()},
    {"emptySame",pool.get(pool.none())==empty},{"fgId",fgId},
    {"bgId",bgId},
    {"repeat",pool.intern(styles({code("\x1b[31m","\x1b[39m")}))},
    {"fgSame",pool.get(fgId)==foreground},
    {"bgSame",pool.get(bgId)==background},
    {"invalidNew",pool.get(999)!=pool.get(999)},
    {"visible",Json::array({screen::hasVisibleSpaceEffect(*styles({})),
      screen::hasVisibleSpaceEffect(*styles({fg})),
      screen::hasVisibleSpaceEffect(*styles({bg})),
      screen::hasVisibleSpaceEffect(*styles({code("x","\x1b[24m")}))})}});
  const auto same=pool.transition(fgId,fgId);
  const auto fgToBg=pool.transition(fgId,bgId);
  const auto repeated=pool.transition(fgId,bgId);
  rows.push_back(Json{{"kind","transition"},{"same",same},
    {"fgToBg",fgToBg},{"repeated",repeated}});
  const auto inverse=pool.withInverse(fgId);
  const auto inverseAgain=pool.withInverse(inverse);
  rows.push_back(Json{{"kind","inverse"},{"inverse",inverse},
    {"inverseAgain",inverseAgain},{"codes",codes(pool.get(inverse))}});
  const auto current=pool.withCurrentMatch(bgId);
  rows.push_back(Json{{"kind","currentMatch"},{"id",current},
    {"repeat",pool.withCurrentMatch(bgId)},
    {"codes",codes(pool.get(current))}});
  const auto fallback=pool.withSelectionBg(fgId);
  pool.setSelectionBg(code("\x1b[42m","\x1b[49m"));
  const auto green=pool.withSelectionBg(bgId);
  pool.setSelectionBg(code("\x1b[42m","different-end"));
  const auto sameCode=pool.withSelectionBg(bgId);
  pool.setSelectionBg(code("\x1b[45m","\x1b[49m"));
  const auto purple=pool.withSelectionBg(bgId);
  rows.push_back(Json{{"kind","selection"},{"fallback",fallback},
    {"green",green},{"sameCode",sameCode},{"purple",purple},
    {"greenCodes",codes(pool.get(green))},
    {"purpleCodes",codes(pool.get(purple))}});
  screen::StylePool second;
  auto original=styles({fg});
  const auto id=second.intern(original);
  original->push_back(code("\x1b[1m","\x1b[22m"));
  rows.push_back(Json{{"kind","styleIdentity"},{"id",id},
    {"length",original->size()},{"same",second.get(id)==original},
    {"oldKeyId",second.intern(styles({fg}))},
    {"newKeyId",second.intern(styles({fg,original->back()}))}});
}
}
int main(){
  Json rows=Json::array();
  rows.push_back(chars());rows.push_back(sharedCharIds());
  rows.push_back(hyperlinks());styleRows(rows);
  std::cout<<rows.dump()<<'\n';
}
