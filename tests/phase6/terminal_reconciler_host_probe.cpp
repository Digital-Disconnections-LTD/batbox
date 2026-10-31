#include "terminal_reconciler/HostConfig.hpp"
#include "terminal_dom/Focus.hpp"
#include "FakeFullLayout.hpp"
#include <iostream>

using namespace batbox;
namespace rec=batbox::terminal_reconciler;
namespace td=batbox::terminal_dom;
namespace {
struct Layout:FakeFullLayout {
  td::LayoutDisplay display=td::LayoutDisplay::Flex;
  td::LayoutDisplay getDisplay()const override{return display;}
  void setDisplay(td::LayoutDisplay value)override{display=value;}
};
td::ElementRef node(const std::string& type){
  auto result=std::make_shared<td::DOMElement>(type);
  result->yogaNode=std::make_shared<Layout>();return result;
}
std::string display(const td::DOMElement& node){
  const auto layout=std::static_pointer_cast<Layout>(node.yogaNode);
  return layout->display==td::LayoutDisplay::None?"none":"flex";
}
rec::PropsBundle props(Json values){
  rec::PropsBundle result;
  result.values=std::make_shared<rec::Props>(rec::Props::fromJson(values));
  const auto found=values.find("style");
  if(found!=values.end()&&found->is_object())
    result.style=std::make_shared<rec::Props>(
      rec::Props::fromJson(*found));
  return result;
}
}
int main(){
  Json log=Json::array(),rows=Json::array();
  auto dom=std::make_shared<td::DomServices>();
  dom->createLayoutNode=[](){return std::make_shared<Layout>();};
  td::NodeStash stash;
  terminal_hit_test::PointerHandlerStore pointerHandlers;
  rec::DebugRepaints debug([](){return std::optional<std::string>("false");});
  rec::HostConfigServices services;
  services.dom=dom;services.stash=&stash;services.debugRepaints=&debug;
  services.props.pointerHandlers=&pointerHandlers;
  services.props.setStyle=[&](td::DOMElement& node,
      const value::ValueRef& value){
    log.push_back(Json::array({"setStyle",value?*value:Json(nullptr)}));
    node.style=value;
  };
  services.props.applyStyles=[&](td::FullLayoutNode&,
      const value::ValueRef& value){
    log.push_back(Json::array({"applyStyles",value?*value:Json(nullptr),
      nullptr}));
  };
  services.props.setAttribute=[&](td::DOMElement& node,
      const std::string& key,const Json& value){
    log.push_back(Json::array({"setAttribute",key,value}));
    node.attributes[key]=value;
  };
  services.setTextStyles=[&](td::DOMElement& node,
      const value::ValueRef& value){
    log.push_back(Json::array({"setTextStyles",value?*value:Json(nullptr)}));
    node.textStyles=value;
  };
  services.applyStyleDiff=[&](td::FullLayoutNode&,
      const Json& changed,const Json* resolved){
    log.push_back(Json::array({"applyStyles",changed,
      resolved?*resolved:Json(nullptr)}));
  };
  rec::HostConfig host(services);
  auto rootCtx=host.getRootHostContext();
  auto inside=host.getChildHostContext(rootCtx,"ink-text");
  rows.push_back(Json{{"kind","context"},
    {"root",rootCtx->isInsideText},{"inside",inside->isInsideText},
    {"sameRoot",host.getChildHostContext(rootCtx,"ink-box")==rootCtx},
    {"sameInside",host.getChildHostContext(inside,"ink-link")==inside}});
  {
    log=Json::array();
    int pointerCalls=0;
    auto p=props(Json{{"id","one"},{"style",Json{{"width",4}}},
      {"textStyles",Json{{"bold",true}}},
      {"onClick",Json{{"$function",1}}}});
    p.handlers["onClick"]=terminal_events::HandlerValue(
      [](terminal_events::TerminalEvent&){});
    p.clickHandlers["onClick"]=[&](terminal_events::ClickEvent&){
      ++pointerCalls;};
    auto n=host.createInstance("ink-box",p,rootCtx);
    terminal_events::ClickEvent click(0,0,false);
    pointerHandlers.get(*n)->onClick(click);
    Json handlers=Json::array();for(const auto& [key,_]:n->eventHandlers)
      handlers.push_back(key);
    rows.push_back(Json{{"kind","create"},{"type",n->nodeName},
      {"attributes",n->attributes},{"style",*n->style},
      {"textStyles",*n->textStyles},{"handlers",handlers},
      {"pointerCalls",pointerCalls},{"log",log}});
  }
  {
    const auto n=host.createInstance("ink-text",props(Json::object()),inside);
    std::string error;
    try{host.createInstance("ink-box",props(Json::object()),inside);}
    catch(const std::exception& e){error=e.what();}
    rows.push_back(Json{{"kind","nested"},{"type",n->nodeName},
      {"error",error}});
  }
  {
    std::string error;
    try{host.createTextInstance("hello",rootCtx);}
    catch(const std::exception& e){error=e.what();}
    auto n=host.createTextInstance("hello",inside);
    host.hideTextInstance(*n);host.unhideTextInstance(*n,"world");
    rows.push_back(Json{{"kind","text"},{"error",error},
      {"value",n->nodeValue}});
  }
  {
    auto n=node("ink-box");host.hideInstance(*n);
    const Json hidden=Json::array({*n->isHidden,display(*n),n->dirty});
    host.unhideInstance(*n);
    const Json shown=Json::array({*n->isHidden,display(*n),n->dirty});
    rows.push_back(Json{{"kind","visibility"},{"hidden",hidden},
      {"shown",shown}});
  }
  {
    auto parent=node("ink-box"),a=node("ink-box"),b=node("ink-box");
    host.appendInitialChild(*parent,a);host.appendChild(*parent,b);
    rows.push_back(Json{{"kind","children"},
      {"length",parent->childNodes.size()},
      {"firstSame",parent->childNodes[0]==a},
      {"autoTrue",host.finalizeInitialChildren(props(
        Json{{"autoFocus",true}}))},
      {"autoOne",host.finalizeInitialChildren(props(
        Json{{"autoFocus",1}}))}});
  }
  {
    auto n=node("ink-box");log=Json::array();
    int pointerCalls=0;
    auto oldProps=props(Json{{"id","old"},{"style",Json{{"width",4}}},
      {"textStyles",Json{{"bold",false}}},
      {"onClick",Json{{"$function",1}}}});
    auto newProps=props(Json{{"id","new"},{"style",Json{{"width",5}}},
      {"textStyles",Json{{"bold",true}}},
      {"onClick",Json{{"$function",2}}}});
    oldProps.handlers["onClick"]=terminal_events::HandlerValue(
      [](terminal_events::TerminalEvent&){});
    newProps.handlers["onClick"]=terminal_events::HandlerValue(
      [](terminal_events::TerminalEvent&){});
    newProps.clickHandlers["onClick"]=[&](terminal_events::ClickEvent&){
      ++pointerCalls;};
    host.commitUpdate(*n,oldProps,newProps);
    terminal_events::ClickEvent click(0,0,false);
    pointerHandlers.get(*n)->onClick(click);
    Json handlers=Json::array();for(const auto& [key,_]:n->eventHandlers)
      handlers.push_back(key);
    rows.push_back(Json{{"kind","update"},
      {"attributes",n->attributes},{"style",*n->style},
      {"textStyles",*n->textStyles},{"handlers",handlers},
      {"pointerCalls",pointerCalls},{"log",log}});
  }
  {
    auto n=node("ink-box");
    auto oldProps=props(Json{{"onClick",Json{{"$function",1}}}});
    oldProps.clickHandlers["onClick"]=[](terminal_events::ClickEvent&){};
    auto newProps=props(Json::object());
    host.commitUpdate(*n,oldProps,newProps);
    const auto* value=pointerHandlers.get(*n);
    rows.push_back(Json{{"kind","removePointer"},
      {"handlerUndefined",value&&!value->onClick&&
        !static_cast<bool>(n->eventHandlers.at("onClick"))}});
  }
  {
    auto root=node("ink-box"),a=node("ink-box"),b=node("ink-box"),
      c=node("ink-box");
    td::FocusManager focus([](td::DOMElement&,
        terminal_events::FocusEvent&){return true;},[]{return 0;});
    root->focusManager=&focus;
    host.appendChild(*root,a);host.appendChild(*root,c);
    host.insertBefore(*root,b,c);host.commitMount(*b);
    rows.push_back(Json{{"kind","insertMount"},
      {"order",Json::array({"a","b","c"})},
      {"focused",focus.activeElement==b}});
    host.removeChildFromContainer(*root,b);
    rows.push_back(Json{{"kind","removeContainer"},
      {"order",Json::array({"a","c"})},
      {"cleaned",!b->yogaNode},{"detached",b->parentNode==nullptr},
      {"focused",!focus.activeElement}});
  }
  {
    auto root=node("ink-box"),parent=node("ink-box"),child=node("ink-box");
    td::FocusManager focus([](td::DOMElement&,
        terminal_events::FocusEvent&){return true;},[]{return 0;});
    root->focusManager=&focus;
    host.appendChild(*root,parent);host.appendChild(*parent,child);
    host.commitMount(*child);host.removeChild(*parent,child);
    rows.push_back(Json{{"kind","removeChild"},
      {"empty",parent->childNodes.empty()},
      {"cleaned",!child->yogaNode},
      {"detached",child->parentNode==nullptr},
      {"focused",!focus.activeElement}});
  }
  {
    auto parent=node("ink-box");
    auto child=host.createTextInstance("first",inside);
    host.appendChild(*parent,child);
    host.commitTextUpdate(*child,"second");host.removeChild(*parent,child);
    rows.push_back(Json{{"kind","textUpdateRemove"},
      {"text",child->nodeValue},{"empty",parent->childNodes.empty()},
      {"detached",child->parentNode==nullptr}});
  }
  std::cout<<rows.dump()<<'\n';
}
