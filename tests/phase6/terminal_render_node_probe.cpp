#include "terminal_render/RenderNode.hpp"
#include "terminal_dom/NodeStash.hpp"
#include "terminal_screen/Screen.hpp"
#include "FakeFullLayout.hpp"
#include <cmath>
#include <iostream>

using namespace batbox;
namespace tr=batbox::terminal_render;
namespace td=batbox::terminal_dom;
namespace to=batbox::terminal_output;
namespace ts=batbox::terminal_screen;
namespace {
using Elem=std::shared_ptr<td::DOMElement>;
struct HiddenLayout:FakeFullLayout {
  td::LayoutDisplay getDisplay()const override{return td::LayoutDisplay::None;}
};
struct PaddedLayout:FakeFullLayout {
  double getComputedPadding(const std::string& edge)const override{
    return edge=="left"?2:3;
  }
  double getComputedBorder(const std::string&)const override{return 1;}
};
Elem node(const std::string& name,double x,double y,double w,double h,
    Json style=Json::object()){
  auto result=std::make_shared<td::DOMElement>(name);
  result->style=std::make_shared<Json>(std::move(style));
  auto layout=std::make_shared<FakeFullLayout>();
  layout->left=x;layout->top=y;layout->width=w;layout->height=h;
  result->yogaNode=layout;result->dirty=true;
  return result;
}
void child(const Elem& parent,const td::NodeRef& value){
  parent->childNodes.push_back(value);value->parentNode=parent.get();
}
Json operation(const to::Operation& op){
  if(auto value=std::get_if<to::WriteOperation>(&op))
    return Json{{"op","write"},{"x",value->x},{"y",value->y},
      {"text",value->text},{"softWrap",value->softWrap?
        Json(*value->softWrap):Json(nullptr)}};
  if(auto value=std::get_if<to::BlitOperation>(&op))
    return Json{{"op","blit"},{"x",value->x},{"y",value->y},
      {"width",value->width},{"height",value->height}};
  if(auto value=std::get_if<to::ClearOperation>(&op))
    return Json{{"op","clear"},{"x",value->region->x},
      {"y",value->region->y},{"width",value->region->width},
      {"height",value->region->height},
      {"fromAbsolute",value->fromAbsolute?
        Json(*value->fromAbsolute):Json(nullptr)}};
  if(auto value=std::get_if<to::NoSelectOperation>(&op))
    return Json{{"op","noSelect"},{"x",value->region->x},
      {"y",value->region->y},{"width",value->region->width},
      {"height",value->region->height}};
  if(auto value=std::get_if<to::ClipOperation>(&op)){
    Json result={{"op","clip"}};
    if(value->clip->x1)result["x1"]=*value->clip->x1;
    if(value->clip->x2)result["x2"]=*value->clip->x2;
    if(value->clip->y1)result["y1"]=*value->clip->y1;
    if(value->clip->y2)result["y2"]=*value->clip->y2;
    return result;
  }
  if(std::holds_alternative<to::UnclipOperation>(op))
    return Json{{"op","unclip"}};
  const auto& shift=std::get<to::ShiftOperation>(op);
  return Json{{"op","shift"},{"top",shift.top},
    {"bottom",shift.bottom},{"n",shift.n}};
}
tr::RenderNodeImports imports(){
  tr::RenderNodeImports result;
  result.text.applyTextStyles=[](const std::string& value,
      const tr::StylesRef& styles){
    if(styles&&styles->is_object()&&styles->contains("backgroundColor")&&
        (*styles)["backgroundColor"].is_string()){
      const auto color=(*styles)["backgroundColor"].get<std::string>();
      return "<"+color+">"+value+"</>";
    }
    return value;
  };
  result.text.wrapText=[](const std::string& value,double,const std::string&){
    return value;
  };
  result.text.indentString=[](const std::string& value,double n){
    std::string result;std::size_t start=0;
    while(true){
      const auto end=value.find('\n',start);
      if(!result.empty())result+='\n';
      result+=std::string(static_cast<std::size_t>(n),' ')+
        value.substr(start,end==std::string::npos?end:end-start);
      if(end==std::string::npos)break;start=end+1;
    }
    return result;
  };
  result.widestLine=[](const std::string& value){
    std::size_t longest=0,start=0;
    while(true){const auto end=value.find('\n',start);
      longest=std::max(longest,(end==std::string::npos?value.size():end)-start);
      if(end==std::string::npos)break;start=end+1;}
    return static_cast<double>(longest);
  };
  result.termProgram=[](){return std::optional<std::string>{};};
  result.isXtermJs=[](){return false;};
  return result;
}
Json run(const std::string& kind,const Elem& elem){
  td::NodeStash stash;tr::RenderState state(stash);
  auto styles=std::make_shared<ts::StylePool>();
  auto chars=std::make_shared<ts::CharPool>();
  auto links=std::make_shared<ts::HyperlinkPool>();
  auto screen=ts::createScreen(30,10,styles,chars,links,{});
  to::Output output(30,10,styles,screen,{});
  if(kind=="cleanBlit"||kind=="hidden"||kind=="pendingClears"){
    stash.set(*elem,std::make_shared<td::StashedLayout>(
      td::StashedLayout{1,2,8,1,2}));
  }
  if(kind=="pendingClears")stash.addPendingClear(*elem,
    std::make_shared<td::StashedLayout>(td::StashedLayout{8,3,2,1,{}}),false);
  if(kind=="scrollFast"){
    stash.set(*elem,std::make_shared<td::StashedLayout>(
      td::StashedLayout{1,1,20,3,1}));
    stash.set(*std::static_pointer_cast<td::DOMElement>(elem->childNodes[0]),
      std::make_shared<td::StashedLayout>(td::StashedLayout{1,1,20,8,0}));
  }
  tr::RenderNodeRenderer renderer(state,imports());
  tr::RenderNodeOptions options;
  if(kind=="cleanBlit"||kind=="scrollFast")options.prevScreen=screen;
  renderer.render(*elem,output,options);
  Json ops=Json::array();
  for(const auto& value:output.operations)ops.push_back(operation(value));
  Json row={{"kind",kind},{"operations",ops},
    {"cached",bool(stash.get(*elem))},{"dirty",elem->dirty},
    {"shifted",state.layoutShifted},
    {"absolutes",state.absoluteRectsCur.size()}};
  if(kind=="scrollBasic"||kind=="scrollFast"){
    row["scrollTop"]=*elem->scrollTop;
    row["scrollHeight"]=*elem->scrollHeight;
    row["viewportTop"]=*elem->scrollViewportTop;
    row["pending"]=elem->pendingScrollDelta?
      Json(*elem->pendingScrollDelta):Json(nullptr);
    row["contentCached"]=bool(stash.get(*std::static_pointer_cast<
      td::DOMElement>(elem->childNodes[0])));
    row["hint"]=state.scrollHint?Json{{"top",state.scrollHint->top},
      {"bottom",state.scrollHint->bottom},
      {"delta",state.scrollHint->delta}}:Json(nullptr);
  }
  return row;
}
}
int main(){
  Json rows=Json::array();
  {
    auto n=node("ink-raw-ansi",1,2,8,1);n->attributes["rawText"]="raw";
    rows.push_back(run("raw",n));
  }
  {
    auto n=node("ink-text",2,3,8,1);
    child(n,std::make_shared<td::TextNode>("hello"));
    rows.push_back(run("text",n));
  }
  {
    auto n=node("ink-root",0,0,30,5);
    auto a=node("ink-raw-ansi",1,0,3,1);
    auto b=node("ink-raw-ansi",2,1,4,1);
    a->attributes["rawText"]="one";b->attributes["rawText"]="two";
    child(n,a);child(n,b);rows.push_back(run("rootChildren",n));
  }
  {
    auto n=node("ink-box",2,1,12,3,Json{{"overflow","hidden"},
      {"noSelect","from-left-edge"},{"opaque",true}});
    auto a=node("ink-raw-ansi",1,1,4,1);a->attributes["rawText"]="child";
    child(n,a);rows.push_back(run("boxClip",n));
  }
  rows.push_back(run("boxColor",node("ink-box",1,1,5,2,
    Json{{"backgroundColor","red"}})));
  {
    auto n=node("ink-raw-ansi",1,-3,3,1,Json{{"position","absolute"}});
    n->attributes["rawText"]="abs";
    rows.push_back(run("absoluteNegative",n));
  }
  {
    auto n=node("ink-raw-ansi",1,2,8,1);n->dirty=false;
    n->attributes["rawText"]="ignored";
    rows.push_back(run("cleanBlit",n));
  }
  {
    auto n=node("ink-raw-ansi",1,2,8,1);
    auto layout=std::make_shared<HiddenLayout>();
    layout->left=1;layout->top=2;layout->width=8;layout->height=1;
    n->yogaNode=layout;rows.push_back(run("hidden",n));
  }
  {
    auto n=node("ink-raw-ansi",1,2,8,1);
    n->attributes["rawText"]="changed";
    rows.push_back(run("pendingClears",n));
  }
  {
    auto c=node("ink-box",0,0,20,8);
    auto a=node("ink-raw-ansi",0,0,5,1);
    auto b=node("ink-raw-ansi",0,2,5,1);
    auto d=node("ink-raw-ansi",0,6,5,1);
    a->attributes["rawText"]="top";
    b->attributes["rawText"]="middle";
    d->attributes["rawText"]="bottom";
    child(c,a);child(c,b);child(c,d);
    auto n=node("ink-box",1,1,20,3,Json{{"overflowY","scroll"}});
    child(n,c);n->scrollTop=2;n->stickyScroll=false;
    n->scrollHeight=8;n->scrollViewportHeight=3;
    rows.push_back(run("scrollBasic",n));
  }
  {
    auto edge=node("ink-raw-ansi",0,4,5,1);
    edge->attributes["rawText"]="edge";
    auto c=node("ink-box",0,0,20,8);child(c,edge);
    auto n=node("ink-box",1,1,20,3,Json{{"overflowY","scroll"}});
    child(n,c);n->scrollTop=2;n->stickyScroll=false;
    n->scrollHeight=8;n->scrollViewportHeight=3;
    rows.push_back(run("scrollFast",n));
  }
  {
    PaddedLayout yoga;yoga.width=20;
    rows.push_back(Json{{"kind","maxWidth"},
      {"value",tr::getMaxWidth(yoga)}});
  }
  std::cout<<rows.dump()<<'\n';
}
