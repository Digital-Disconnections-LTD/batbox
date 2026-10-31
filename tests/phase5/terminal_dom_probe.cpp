#include "terminal_dom/Dom.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace batbox;
namespace td=batbox::terminal_dom;
namespace {
struct Layout:td::LayoutNode {
  int id;
  Json& trace;
  std::vector<Layout*> children;
  td::MeasureFunction measure;
  td::LayoutDisplay display=td::LayoutDisplay::Flex;
  double top=0,height=10;
  Layout(int id,Json& trace):id(id),trace(trace){}
  void setMeasureFunc(td::MeasureFunction fn)override{
    measure=std::move(fn);trace.push_back("measureFunc:"+std::to_string(id));}
  void insertChild(td::LayoutNode& child,std::size_t index)override{
    auto* other=static_cast<Layout*>(&child);
    children.insert(children.begin()+static_cast<std::ptrdiff_t>(index),other);
    trace.push_back("insert:"+std::to_string(id)+":"+
      std::to_string(other->id)+":"+std::to_string(index));
  }
  void removeChild(td::LayoutNode& child)override{
    auto* other=static_cast<Layout*>(&child);
    const auto found=std::find(children.begin(),children.end(),other);
    if(found!=children.end())children.erase(found);
    trace.push_back("remove:"+std::to_string(id)+":"+std::to_string(other->id));
  }
  std::size_t getChildCount()const override{return children.size();}
  void markDirty()override{trace.push_back("markYoga:"+std::to_string(id));}
  td::LayoutDisplay getDisplay()const override{return display;}
  double getComputedTop()const override{return top;}
  double getComputedHeight()const override{return height;}
};
struct Fixture {
  Json trace=Json::array();int nextLayout=0;
  td::NodeStash stash;
  td::DomServicesRef services=std::make_shared<td::DomServices>();
  Fixture(){
    services->createLayoutNode=[this]{
      return std::make_shared<Layout>(++nextLayout,trace);};
    services->expandTabs=[this](const std::string& input){
      trace.push_back("tabs:"+value::stringify(Json(input)));
      std::string result;for(char c:input)
        result+=c=='\t'?"        ":std::string(1,c);
      return result;};
    services->measureText=[this](const std::string& input,double width){
      trace.push_back("measure:"+value::stringify(Json(input))+":"+
        value::numberToString(width));
      return terminal_screen::Size{double(input.size()),
        double(std::count(input.begin(),input.end(),'\n')+1)};};
    services->wrapText=[this](const std::string& input,double width,
        const std::string& mode){
      trace.push_back("wrap:"+mode+":"+value::numberToString(width));
      const auto size=std::max(0.0,std::floor(width));
      return input.substr(0,static_cast<std::size_t>(size));};
  }
  td::ElementRef create(const std::string& name){
    return td::createNode(name,services);}
  static Layout& layout(td::DOMNode& node){
    return static_cast<Layout&>(*node.yogaNode);}
};
Json run(const Json& spec){
  Fixture f;
  const auto mode=spec.at("mode").get<std::string>();
  if(mode=="tree"){
    auto root=f.create("ink-root"),box=f.create("ink-box");
    auto virtualNode=f.create("ink-virtual-text");
    auto text=f.create("ink-text"),raw=f.create("ink-raw-ansi");
    td::setStyle(*box,std::make_shared<Json>(Json{{"position","absolute"}}));
    f.stash.set(*box,std::make_shared<td::StashedLayout>(1,2,3,4));
    f.stash.set(*text,std::make_shared<td::StashedLayout>(5,6,7,8));
    td::appendChildNode(*root,box,f.stash);
    td::appendChildNode(*root,virtualNode,f.stash);
    td::appendChildNode(*box,text,f.stash);
    td::insertBeforeNode(*root,raw,virtualNode,f.stash);
    td::insertBeforeNode(*root,box,virtualNode,f.stash);
    Json children=Json::array(),yoga=Json::array(),clears=Json::array();
    for(const auto& item:root->childNodes)children.push_back(item->nodeName);
    for(const auto* item:Fixture::layout(*root).children)yoga.push_back(item->id);
    if(const auto pending=f.stash.pendingClears(*root))
      for(const auto& item:*pending)clears.push_back(item->x);
    const auto flag1=f.stash.consumeAbsoluteRemovedFlag();
    const auto flag2=f.stash.consumeAbsoluteRemovedFlag();
    return {{"trace",f.trace},{"children",children},{"yoga",yoga},
      {"boxParent",box->parentNode==root.get()},{"clears",clears},
      {"flag1",flag1},{"flag2",flag2},
      {"boxCached",bool(f.stash.get(*box))},
      {"textCached",bool(f.stash.get(*text))},
      {"rootDirty",root->dirty},{"textDirty",text->dirty}};
  }
  if(mode=="props"){
    auto root=f.create("ink-root"),text=f.create("ink-text");
    td::appendChildNode(*root,text,f.stash);
    root->onRender=[&]{f.trace.push_back("render");};
    td::setAttribute(*text,"children","ignored");
    td::setAttribute(*text,"id","a");
    td::setAttribute(*text,"id","a");
    td::setStyle(*text,std::make_shared<Json>(Json{{"textWrap","wrap"}}));
    td::setStyle(*text,std::make_shared<Json>(Json{{"textWrap","wrap"}}));
    td::setTextStyles(*text,std::make_shared<Json>(Json{{"bold",true}}));
    td::setTextStyles(*text,std::make_shared<Json>(Json{{"bold",true}}));
    auto leaf=td::createTextNode("a\tb\nc");
    td::appendChildNode(*text,leaf,f.stash);
    td::setTextNodeValue(*leaf,"a\tb\nc");
    td::setTextNodeValue(*leaf,"a\tb\nd");
    const auto name=spec.at("modeName").get<std::string>();
    const auto layoutMode=name=="undefined"?
      td::LayoutMeasureMode::Undefined:td::LayoutMeasureMode::Exactly;
    const auto measured=Fixture::layout(*text).measure(
      spec.at("width").get<double>(),layoutMode);
    td::scheduleRenderFrom(leaf.get());
    return {{"trace",f.trace},{"measured",{{"width",*measured.width},
      {"height",*measured.height}}},{"id",text->attributes.at("id")},
      {"leaf",leaf->nodeValue},{"rootDirty",root->dirty},
      {"textDirty",text->dirty},{"childCount",text->childNodes.size()}};
  }
  if(mode=="owner"){
    auto root=f.create("ink-root"),box=f.create("ink-box");
    auto hidden=f.create("ink-box"),child=f.create("ink-box");
    td::appendChildNode(*root,box,f.stash);
    td::appendChildNode(*root,hidden,f.stash);
    td::appendChildNode(*box,child,f.stash);
    root->debugOwnerChain=std::vector<std::string>{"root"};
    box->debugOwnerChain=std::vector<std::string>{"box"};
    child->debugOwnerChain=std::vector<std::string>{"child"};
    hidden->debugOwnerChain=std::vector<std::string>{"hidden"};
    Fixture::layout(*root).height=20;
    Fixture::layout(*box).top=2;Fixture::layout(*box).height=10;
    Fixture::layout(*child).top=3;Fixture::layout(*child).height=4;
    Fixture::layout(*hidden).top=12;Fixture::layout(*hidden).height=5;
    Fixture::layout(*hidden).display=td::LayoutDisplay::None;
    Json hits=Json::array();
    for(const auto& row:spec.at("rows"))
      hits.push_back(td::findOwnerChainAtRow(*root,row.get<double>()));
    td::clearYogaNodeReferences(*root);
    return {{"trace",f.trace},{"hits",hits},
      {"cleared",!root->yogaNode&&!box->yogaNode&&
        !hidden->yogaNode&&!child->yogaNode}};
  }
  if(mode=="textCoercion"){
    auto node=td::createTextNode("start");
    Json error=nullptr;
    try{td::setTextNodeValueDynamic(*node,spec.at("value"));}
    catch(const std::exception& e){error=e.what();}
    return {{"trace",f.trace},{"value",node->nodeValue},{"error",error}};
  }
  if(mode=="edges"){
    auto root=f.create("ink-root"),virtualNode=f.create("ink-virtual-text");
    auto box=f.create("ink-box"),before=f.create("ink-link");
    auto raw=f.create("ink-raw-ansi");
    td::appendChildNode(*root,virtualNode,f.stash);
    td::insertBeforeNode(*root,box,before,f.stash);
    td::setAttribute(*raw,"rawWidth",12);
    td::setAttribute(*raw,"rawHeight",2);
    const auto measured=Fixture::layout(*raw).measure(0,
      td::LayoutMeasureMode::Undefined);
    td::removeChildNode(*root,virtualNode,f.stash);
    td::markDirty(nullptr);td::scheduleRenderFrom(nullptr);
    Json children=Json::array(),yoga=Json::array();
    for(const auto& item:root->childNodes)children.push_back(item->nodeName);
    for(const auto* item:Fixture::layout(*root).children)yoga.push_back(item->id);
    return {{"trace",f.trace},{"children",children},{"yoga",yoga},
      {"measured",{{"width",*measured.width},{"height",*measured.height}}},
      {"virtualParent",virtualNode->parentNode==nullptr},
      {"rootDirty",root->dirty},{"rawDirty",raw->dirty}};
  }
  throw std::runtime_error("unknown mode");
}
}
int main(){const std::string data((std::istreambuf_iterator<char>(std::cin)),{});
  const auto cases=value::parse(data);Json results=Json::array();
  for(const auto& item:cases)results.push_back(run(item));
  std::cout<<value::stringify(results)<<'\n';}
