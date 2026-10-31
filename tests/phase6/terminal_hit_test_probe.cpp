#include "terminal_hit_test/HitTest.hpp"
#include <iostream>

using namespace batbox;
namespace td=batbox::terminal_dom;
namespace th=batbox::terminal_hit_test;
namespace {
td::ElementRef element(const std::string& name){
  return std::make_shared<td::DOMElement>(name);
}
void rect(td::NodeStash& stash,td::DOMElement& node,double x,double y,
    double width,double height){
  stash.set(node,std::make_shared<td::StashedLayout>(
    td::StashedLayout{x,y,width,height,{}}));
}
std::string name(const td::DOMElement* node){
  return node?node->nodeName:std::string{};
}
Json hoveredNames(const th::Hovered& values){
  Json result=Json::array();
  for(const auto& n:values)result.push_back(n->nodeName);
  return result;
}
}
int main(){
  Json log=Json::array(),rows=Json::array();
  td::NodeStash stash;
  th::PointerHandlerStore handlers;
  auto root=element("root"),a=element("a"),b=element("b"),
    missing=element("missing"),nested=element("nested");
  td::bindFocusChild(*root,a);td::bindFocusChild(*root,b);
  td::bindFocusChild(*root,missing);td::bindFocusChild(*missing,nested);
  auto text=std::make_shared<td::TextNode>("text");
  td::bindFocusChild(*b,text);
  rect(stash,*root,0,0,10,5);rect(stash,*a,0,0,5,5);
  rect(stash,*b,2,0,5,5);rect(stash,*nested,3,0,1,1);
  td::FocusManager focus([&](td::DOMElement& n,
      terminal_events::FocusEvent& event){
    if(event.type()=="focus")log.push_back(Json::array({"focus",n.nodeName}));
    return true;
  },[]{return 0;});
  root->focusManager=&focus;b->attributes["tabIndex"]=0;
  th::PointerHandlers rootHandlers,bHandlers;
  rootHandlers.onClick=[&](terminal_events::ClickEvent& e){
    log.push_back(Json::array({"click","root",e.localCol,e.localRow,
      e.cellIsBlank}));
  };
  bool stop=true;
  bHandlers.onClick=[&](terminal_events::ClickEvent& e){
    log.push_back(Json::array({"click","b",e.localCol,e.localRow,
      e.cellIsBlank}));
    if(stop)e.stopImmediatePropagation();
  };
  handlers.set(root,rootHandlers);handlers.set(b,bHandlers);
  const auto hit=[&](double col,double row)->Json{
    const auto* result=th::hitTest(*root,col,row,stash);
    return result?Json(result->nodeName):Json(nullptr);
  };
  rows.push_back(Json{{"kind","hit"},{"overlap",hit(3,1)},
    {"other",hit(1,1)},{"gap",hit(8,1)},
    {"outside",hit(10,1)}});
  const auto click=[&](const char* kind,double col,double row,bool blank){
    const bool handled=th::dispatchClick(*root,col,row,blank,stash,handlers);
    rows.push_back(Json{{"kind",kind},{"handled",handled},
      {"focus",focus.activeElement?Json(focus.activeElement->nodeName):
        Json(nullptr)},{"log",log}});
    log=Json::array();
  };
  click("clickStopped",3,1,true);
  stop=false;click("clickBubble",3,1,false);
  const bool outside=th::dispatchClick(*root,10,1,false,stash,handlers);
  rows.push_back(Json{{"kind","clickOutside"},{"handled",outside},
    {"log",log}});log=Json::array();
  for(const auto& n:{root,a,b}){
    th::PointerHandlers h;
    if(n==root)h.onClick=rootHandlers.onClick;
    if(n==b)h.onClick=bHandlers.onClick;
    h.onMouseEnter=[&,name=n->nodeName]{
      log.push_back(Json::array({"enter",name}));};
    h.onMouseLeave=[&,name=n->nodeName]{
      log.push_back(Json::array({"leave",name}));};
    handlers.set(n,std::move(h));
  }
  th::Hovered hovered;
  const auto hover=[&](const char* kind,double col,double row){
    th::dispatchHover(*root,col,row,hovered,stash,handlers);
    rows.push_back(Json{{"kind",kind},{"hovered",hoveredNames(hovered)},
      {"log",log}});log=Json::array();
  };
  hover("hoverB",3,1);hover("hoverA",1,1);
  hover("hoverGap",8,1);hover("hoverOutside",10,1);
  hover("hoverBAgain",3,1);
  td::unbindFocusChild(*root,b);
  hover("hoverDetached",10,1);
  std::cout<<rows.dump()<<'\n';
}
