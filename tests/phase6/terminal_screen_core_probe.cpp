#include "terminal_screen/Screen.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>

using namespace batbox;
namespace screen=batbox::terminal_screen;
namespace {
struct Pools {
  std::shared_ptr<screen::CharPool> chars=std::make_shared<screen::CharPool>();
  std::shared_ptr<screen::HyperlinkPool> links=std::make_shared<screen::HyperlinkPool>();
  std::shared_ptr<screen::StylePool> styles=std::make_shared<screen::StylePool>();
};
Json cell(const std::optional<screen::Cell>& value){
  if(!value)return nullptr;
  return Json{{"char",value->character},{"styleId",value->styleId},
    {"width",static_cast<int>(value->width)},
    {"hyperlink",value->hyperlink?Json(*value->hyperlink):Json(nullptr)}};
}
Json cell(const screen::Cell& value){return cell(std::optional(value));}
}
int main(){
  Json rows=Json::array();
  {
    Pools p;auto s=screen::createScreen(3,2,p.styles,p.chars,p.links,{});
    rows.push_back(Json{{"kind","create"},{"width",s->width},
      {"height",s->height},{"cells",s->cells->length32()},
      {"cells64",s->cells->length64()},{"sharedBuffer",true},
      {"noSelect",s->noSelect->size()},{"softWrap",s->softWrap->size()},
      {"empty",screen::isEmptyCellAt(*s,0,0)},
      {"first",cell(screen::cellAt(*s,0,0))},{"damage",nullptr}});
  }
  {
    Pools p;Json warnings=Json::array();
    auto s=screen::createScreen(-2.3,2.9,p.styles,p.chars,p.links,
      [&](const std::string& message,const std::string& level){
        warnings.push_back(Json::array({message,level}));});
    rows.push_back(Json{{"kind","normalize"},{"width",s->width},
      {"height",s->height},{"cells",s->cells->length32()},
      {"softWrap",s->softWrap->size()},{"warnings",warnings}});
  }
  {
    Pools p;auto s=screen::createScreen(2,2,p.styles,p.chars,p.links,{});
    auto oldCells=s->cells;
    auto oldNo=s->noSelect;
    auto oldWrap=s->softWrap;
    s->cells->setWord(0,p.chars->intern("A"));
    s->cells->setWord(1,screen::packWord1(2,0,1));
    (*s->noSelect)[0]=1;(*s->softWrap)[1]=2;
    s->damage=std::make_shared<screen::Rectangle>(screen::Rectangle{0,0,1,1});
    screen::resetScreen(*s,1,1,{});
    rows.push_back(Json{{"kind","resetShrink"},
      {"sameCells",s->cells==oldCells},{"sameCells64",s->cells==oldCells},
      {"sameNo",s->noSelect==oldNo},{"sameWrap",s->softWrap==oldWrap},
      {"width",s->width},{"height",s->height},
      {"capacity",s->cells->length64()},
      {"zeroFirst",s->cells->word(0)==0&&s->cells->word(1)==0},
      {"noFirst",(*s->noSelect)[0]},{"wrapFirst",(*s->softWrap)[0]},
      {"damage",nullptr}});
    screen::resetScreen(*s,4,3,{});
    rows.push_back(Json{{"kind","resetGrow"},
      {"sameCells",s->cells==oldCells},{"sameNo",s->noSelect==oldNo},
      {"sameWrap",s->softWrap==oldWrap},{"width",s->width},
      {"height",s->height},{"capacity",s->cells->length64()},
      {"noLength",s->noSelect->size()},{"wrapLength",s->softWrap->size()}});
  }
  {
    Pools p;auto s=screen::createScreen(2,1,p.styles,p.chars,p.links,{});
    auto charId=p.chars->intern("A"),linkId=p.links->intern("https://a");
    s->cells->setWord(0,charId);
    s->cells->setWord(1,screen::packWord1(2,linkId,1));
    auto nextChars=std::make_shared<screen::CharPool>();
    auto nextLinks=std::make_shared<screen::HyperlinkPool>();
    nextChars->intern("Z");nextLinks->intern("https://z");
    screen::migrateScreenPools(*s,nextChars,nextLinks);
    rows.push_back(Json{{"kind","migrate"},{"charId",*s->cells->word(0)},
      {"word1",*s->cells->word(1)},{"charPool",s->charPool==nextChars},
      {"linkPool",s->hyperlinkPool==nextLinks},
      {"cell",cell(screen::cellAt(*s,0,0))}});
    auto before=s->cells->word(0);
    screen::migrateScreenPools(*s,nextChars,nextLinks);
    rows.push_back(Json{{"kind","migrateSame"},
      {"sameId",s->cells->word(0)==before}});
  }
  {
    Pools p;auto s=screen::createScreen(3,1,p.styles,p.chars,p.links,{});
    s->cells->setWord(0,p.chars->intern("A"));
    s->cells->setWord(1,screen::packWord1(2,0,0));
    s->cells->setWord(2,1);
    s->cells->setWord(3,screen::packWord1(0,0,2));
    s->cells->setWord(4,0);
    s->cells->setWord(5,screen::packWord1(2,0,0));
    auto char0=screen::charInCellAt(*s,0,0);
    auto outside=screen::charInCellAt(*s,-1,0);
    rows.push_back(Json{{"kind","read"},
      {"cell0",cell(screen::cellAt(*s,0,0))},
      {"cellOutside",cell(screen::cellAt(*s,3,0))},
      {"char0",char0?Json(*char0):Json(nullptr)},
      {"charOutside",outside?Json(*outside):Json(nullptr)},
      {"empty0",screen::isEmptyCellAt(*s,0,0)},
      {"empty2",screen::isEmptyCellAt(*s,2,0)},
      {"visible0",cell(screen::visibleCellAtIndex(*s->cells,*p.chars,*p.links,0,-1))},
      {"visibleSpacer",cell(screen::visibleCellAtIndex(*s->cells,*p.chars,*p.links,1,-1))},
      {"visibleFgSame",cell(screen::visibleCellAtIndex(*s->cells,*p.chars,*p.links,2,2))},
      {"visibleFgDiff",cell(screen::visibleCellAtIndex(*s->cells,*p.chars,*p.links,2,4))}});
  }
  {
    Pools p;auto s=screen::createScreen(1,1,p.styles,p.chars,p.links,{});
    screen::Cell linked{" ",0,screen::CellWidth::Narrow,"https://a"};
    rows.push_back(Json{{"kind","cellEmpty"},
      {"empty",screen::isCellEmpty(*s,*screen::cellAt(*s,0,0))},
      {"linked",screen::isCellEmpty(*s,linked)}});
  }
  {
    Pools p;Json warnings=Json::array();
    auto s=screen::createScreen(std::numeric_limits<double>::infinity(),0,
      p.styles,p.chars,p.links,
      [&](const std::string& message,const std::string& level){
        warnings.push_back(Json::array({message,level}));});
    rows.push_back(Json{{"kind","infinityZero"},
      {"widthInfinite",std::isinf(s->width)},{"height",s->height},
      {"cells",s->cells->length32()},{"noSelect",s->noSelect->size()},
      {"softWrap",s->softWrap->size()},{"warnings",warnings}});
  }
  {
    Pools p;Json error=nullptr;
    try{screen::createScreen(268435456,1,p.styles,p.chars,p.links,{});}
    catch(const std::range_error& e){error=Json::array({"RangeError",e.what()});}
    rows.push_back(Json{{"kind","bufferWrapError"},{"error",error}});
  }
  {
    Pools p;Json error=nullptr;
    try{screen::createScreen(0,std::numeric_limits<double>::infinity(),
      p.styles,p.chars,p.links,
      [](const std::string&,const std::string&){});}
    catch(const std::range_error& e){error=Json::array({"RangeError",e.what()});}
    rows.push_back(Json{{"kind","rowInfinityError"},{"error",error}});
  }
  {
    Pools p;auto s=screen::createScreen(1,1,p.styles,p.chars,p.links,{});
    s->cells->setWord(0,p.chars->intern("A"));auto old=s->cells;
    screen::resetScreen(*s,std::numeric_limits<double>::infinity(),0,
      [](const std::string&,const std::string&){});
    rows.push_back(Json{{"kind","resetNaNSize"},
      {"widthInfinite",std::isinf(s->width)},{"height",s->height},
      {"sameCells",s->cells==old},{"first",*s->cells->word(0)},
      {"damage",nullptr}});
  }
  {
    Pools p;auto s=screen::createScreen(1,1,p.styles,p.chars,p.links,{});
    Json error=nullptr;auto old=s->cells;
    try{screen::resetScreen(*s,268435456,1,{});}
    catch(const std::range_error& e){error=Json::array({"RangeError",e.what()});}
    rows.push_back(Json{{"kind","resetWrapError"},{"error",error},
      {"sameCells",s->cells==old},{"width",s->width},
      {"height",s->height}});
  }
  {
    Json warnings=Json::array();
    const screen::DebugWarning diagnostic=
      [&](const std::string& message,const std::string& level){
        warnings.push_back(Json::array({message,level}));};
    screen::ifNotInteger(std::optional<double>{},"optional",diagnostic);
    screen::ifNotInteger(2.0,"integer",diagnostic);
    screen::ifNotInteger(1.5,"fraction",diagnostic);
    rows.push_back(Json{{"kind","warningOptional"},
      {"warnings",warnings}});
  }
  std::cout<<rows.dump()<<'\n';
}
