#include "terminal_selection/Selection.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
namespace sel=batbox::terminal_selection;
namespace ts=batbox::terminal_screen;
namespace {
Json point(const sel::PointRef& p){
  return p?Json{{"col",p->col},{"row",p->row}}:Json(nullptr);
}
Json span(const sel::SpanRef& s){
  return s?Json{{"lo",point(s->lo)},{"hi",point(s->hi)},
    {"kind",s->kind==sel::SpanKind::Word?"word":"line"}}:Json(nullptr);
}
ts::ScreenRef screen(const std::vector<std::string>& lines,
    const std::shared_ptr<ts::StylePool>& styles,
    const std::shared_ptr<ts::CharPool>& chars,
    const std::shared_ptr<ts::HyperlinkPool>& links){
  auto result=ts::createScreen(lines.at(0).size(),lines.size(),
    styles,chars,links,[](const std::string&,const std::string&){});
  for(std::size_t row=0;row<lines.size();++row)
    for(std::size_t col=0;col<lines[row].size();++col)
      ts::setCellAt(*result,col,row,ts::Cell{
        std::string(1,lines[row][col]),styles->none(),
        ts::CellWidth::Narrow,{}});
  return result;
}
template<class T>Json array(const std::vector<T>& values){
  Json result=Json::array();for(const auto& value:values)result.push_back(value);
  return result;
}
}
int main(){
  auto styles=std::make_shared<ts::StylePool>();
  auto chars=std::make_shared<ts::CharPool>();
  auto links=std::make_shared<ts::HyperlinkPool>();
  Json rows=Json::array();
  auto w=screen({"abc  def  ","/usr/bin/b","          "},styles,chars,links);
  ts::setCellAt(*w,0,2,ts::Cell{"界",styles->none(),
    ts::CellWidth::Wide,{}});
  auto s=sel::createSelectionState();sel::selectWordAt(s,*w,1,0);
  rows.push_back(Json{{"kind","word"},{"anchor",point(s.anchor)},
    {"focus",point(s.focus)},{"span",span(s.anchorSpan)},
    {"alias",s.anchor==s.anchorSpan->lo}});
  sel::extendSelection(s,*w,7,0);
  rows.push_back(Json{{"kind","extendWord"},{"anchor",point(s.anchor)},
    {"focus",point(s.focus)},{"span",span(s.anchorSpan)}});
  sel::extendSelection(s,*w,0,0);
  rows.push_back(Json{{"kind","returnWord"},{"anchor",point(s.anchor)},
    {"focus",point(s.focus)},{"span",span(s.anchorSpan)}});
  s=sel::createSelectionState();sel::selectWordAt(s,*w,4,0);
  rows.push_back(Json{{"kind","spaceWord"},{"anchor",point(s.anchor)},
    {"focus",point(s.focus)}});
  s=sel::createSelectionState();sel::selectWordAt(s,*w,1,2);
  rows.push_back(Json{{"kind","wideTail"},{"anchor",point(s.anchor)},
    {"focus",point(s.focus)}});
  (*w->noSelect)[1]=1;
  s=sel::createSelectionState();sel::selectWordAt(s,*w,1,0);
  rows.push_back(Json{{"kind","excludedWord"},{"anchor",point(s.anchor)},
    {"focus",point(s.focus)}});
  s=sel::createSelectionState();sel::selectLineAt(s,*w,1);
  sel::extendSelection(s,*w,4,0);
  rows.push_back(Json{{"kind","extendLineBackward"},
    {"anchor",point(s.anchor)},{"focus",point(s.focus)},
    {"span",span(s.anchorSpan)}});
  auto u=screen({"https://a.com,https://b.com.           "},
    styles,chars,links);
  const auto url=[&](double col)->Json{
    const auto value=sel::findPlainTextUrlAt(*u,col,0);
    return value?Json(*value):Json(nullptr);
  };
  rows.push_back(Json{{"kind","urls"},{"first",url(8)},
    {"second",url(22)},{"period",url(26)},{"gap",url(31)}});
  auto punct=screen({"https://site/wiki/Foo_(bar).       "},
    styles,chars,links);
  const auto punctUrl=[&](double col)->Json{
    const auto value=sel::findPlainTextUrlAt(*punct,col,0);
    return value?Json(*value):Json(nullptr);
  };
  rows.push_back(Json{{"kind","balancedUrl"},
    {"inside",punctUrl(20)},{"period",punctUrl(28)}});
  auto unicode=screen({"     "},styles,chars,links);
  for(const auto& [i,value]:std::vector<std::pair<int,std::string>>{
      {0,"α"},{1,"β"},{2,"γ"}})
    ts::setCellAt(*unicode,i,0,ts::Cell{value,styles->none(),
      ts::CellWidth::Narrow,{}});
  s=sel::createSelectionState();sel::selectWordAt(s,*unicode,1,0);
  rows.push_back(Json{{"kind","unicodeWord"},
    {"anchor",point(s.anchor)},{"focus",point(s.focus)}});
  auto t=screen({"hello wo","rld  ok ","done    "},styles,chars,links);
  (*t->softWrap)[1]=8;(*t->noSelect)[2*8]=1;
  s=sel::createSelectionState();sel::startSelection(s,0,0);
  sel::updateSelection(s,7,2);
  rows.push_back(Json{{"kind","selectedText"},
    {"text",sel::getSelectedText(s,*t)}});
  sel::captureScrolledRows(s,*t,0,0,true);
  rows.push_back(Json{{"kind","captureAbove"},
    {"text",sel::getSelectedText(s,*t)},
    {"above",array(*s.scrolledOffAbove)},
    {"aboveSW",array(*s.scrolledOffAboveSW)},
    {"anchor",point(s.anchor)}});
  s=sel::createSelectionState();sel::startSelection(s,5,2);
  sel::updateSelection(s,1,0);sel::captureScrolledRows(s,*t,2,2,false);
  rows.push_back(Json{{"kind","captureBelow"},
    {"below",array(*s.scrolledOffBelow)},
    {"belowSW",array(*s.scrolledOffBelowSW)},
    {"anchor",point(s.anchor)}});
  auto overlay=screen({"abc  def  ","/usr/bin/b","          "},
    styles,chars,links);
  ts::setCellAt(*overlay,0,2,ts::Cell{"界",styles->none(),
    ts::CellWidth::Wide,{}});
  (*overlay->noSelect)[1]=1;
  s=sel::createSelectionState();sel::startSelection(s,0,0);
  sel::updateSelection(s,2,0);
  sel::applySelectionOverlay(*overlay,s,*styles);
  Json changed=Json::array();
  for(int col=0;col<10;++col)
    changed.push_back(ts::cellAt(*overlay,col,0)->styleId!=styles->none());
  rows.push_back(Json{{"kind","overlay"},{"changed",changed}});
  std::cout<<rows.dump()<<'\n';
}
