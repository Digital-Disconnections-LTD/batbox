#include "terminal_reconciler/HostProps.hpp"
#include "FakeFullLayout.hpp"
#include <iostream>

using namespace batbox;
namespace rec=batbox::terminal_reconciler;
namespace td=batbox::terminal_dom;
int main(){
  Json log=Json::array();int calls=0;
  rec::HostPropImports imports;
  imports.setStyle=[&](td::DOMElement& node,const value::ValueRef& value){
    log.push_back(Json::array({"setStyle",*value}));node.style=value;
  };
  imports.applyStyles=[&](td::FullLayoutNode&,
      const value::ValueRef& value){
    log.push_back(Json::array({"applyStyles",*value}));
  };
  imports.setAttribute=[&](td::DOMElement& node,
      const std::string& key,const Json& value){
    log.push_back(Json::array({"setAttribute",key,value}));
    node.attributes[key]=value;
  };
  auto n=std::make_shared<td::DOMElement>("ink-box");
  n->yogaNode=std::make_shared<FakeFullLayout>();
  auto style=std::make_shared<Json>(Json{{"color","ansi:red"}});
  auto textStyles=std::make_shared<Json>(Json{{"bold",true}});
  rec::applyProp(*n,"children",{std::make_shared<Json>("ignored"),{}},imports);
  rec::applyProp(*n,"style",{style,{}},imports);
  rec::applyProp(*n,"textStyles",{textStyles,{}},imports);
  rec::applyProp(*n,"onClick",{std::make_shared<Json>(true),
    terminal_events::HandlerValue([&](terminal_events::TerminalEvent&){calls++;})},imports);
  rec::applyProp(*n,"onMouseEnter",{std::make_shared<Json>(true),
    terminal_events::HandlerValue([&](terminal_events::TerminalEvent&){calls+=2;})},imports);
  rec::applyProp(*n,"id",{std::make_shared<Json>("box"),{}},imports);
  terminal_events::TerminalEvent event("click",[]{return 0;});
  n->eventHandlers.at("onClick").invoke(event);
  n->eventHandlers.at("onMouseEnter").invoke(event);
  Json handlers=Json::array();for(const auto& [key,_]:n->eventHandlers)
    handlers.push_back(key);
  Json rows=Json::array({Json{{"kind","props"},{"log",log},
    {"styleSame",n->style==style},{"textStylesSame",n->textStyles==textStyles},
    {"handlers",handlers},{"calls",calls},{"attributes",n->attributes}}});
  std::cout<<rows.dump()<<'\n';
}
