#include "terminal_render/Border.hpp"
#include "FakeFullLayout.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
namespace rend=batbox::terminal_render;
namespace out=batbox::terminal_output;
namespace scr=batbox::terminal_screen;
namespace {
rend::BorderImports imports(){
  rend::BorderImports result;
  result.stringWidth=[](const std::string& text){
    return static_cast<double>(value::stringLength(text));};
  result.applyColor=[](const std::string& text,
      const std::optional<std::string>& color){
    return color&&!color->empty()?"<"+*color+">"+text+"</"+*color+">":text;};
  result.dim=[](const std::string& text){return "<dim>"+text+"</dim>";};
  result.resolveBox=rend::selectedCliBox;
  return result;
}
Json writes(const out::Output& output){
  Json result=Json::array();
  for(const auto& value:output.operations){
    if(const auto write=std::get_if<out::WriteOperation>(&value))
      result.push_back(Json{{"x",write->x},{"y",write->y},
        {"text",write->text}});
  }
  return result;
}
Json render(const char* kind,const Json& style,double width,double height){
  auto styles=std::make_shared<scr::StylePool>();
  auto chars=std::make_shared<scr::CharPool>();
  auto links=std::make_shared<scr::HyperlinkPool>();
  auto target=scr::createScreen(12,8,styles,chars,links,{});
  out::Output output(12,8,styles,target,{});
  terminal_dom::DOMElement node("ink-box");
  node.style=std::make_shared<Json>(style);
  auto yoga=std::make_shared<FakeFullLayout>();
  yoga->width=width;yoga->height=height;
  node.yogaNode=yoga;
  rend::renderBorder(2,3,node,output,imports());
  return Json{{"kind",kind},{"writes",writes(output)}};
}
}
int main(){
  Json rows=Json::array();const auto i=imports();
  const auto center=rend::embedTextInBorder("┌───┐","Hi","center",0,"─",i);
  rows.push_back(Json{{"kind","embedCenter"},
    {"value",Json::array({center[0],center[1],center[2]})}});
  const auto longText=rend::embedTextInBorder("┌───┐","HELLO","start",0,"─",i);
  rows.push_back(Json{{"kind","embedLong"},
    {"value",Json::array({longText[0],longText[1],longText[2]})}});
  rows.push_back(Json{{"kind","style"},{"value",
    rend::styleBorderLine("x","ansi:red",true,i)}});
  rows.push_back(render("single",Json{{"borderStyle","single"}},5,3));
  rows.push_back(render("dashedHidden",Json{{"borderStyle","dashed"},
    {"borderTop",false},{"borderRight",false}},4,3));
  rows.push_back(render("label",Json{{"borderStyle","round"},
    {"borderColor","ansi:red"},{"borderDimColor","ansi:blue"},
    {"borderText",Json{{"position","top"},{"content","Hi"},
      {"align","center"}}}},8,3));
  rows.push_back(render("custom",Json{{"borderStyle",Json{
    {"topLeft","a"},{"top","-"},{"topRight","b"},{"right","r"},
    {"bottomRight","d"},{"bottom","="},{"bottomLeft","c"},
    {"left","l"}}},{"borderBottom",false}},4,2));
  std::cout<<rows.dump()<<'\n';
}
