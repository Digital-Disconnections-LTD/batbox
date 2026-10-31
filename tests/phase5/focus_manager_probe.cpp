#include "terminal_dom/Focus.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
using batbox::Json;
using namespace batbox::terminal_dom;
namespace {
Json tabIndex(const Json& value){
  if(value.is_string()&&value.get<std::string>()=="NaN")
    return std::numeric_limits<double>::quiet_NaN();
  return value;
}
Json run(const Json& spec){
  std::map<std::string,NodeRef> nodes;
  for(const auto& item:spec.at("nodes")){
    const auto id=item.at("id").get<std::string>();
    const auto name=item.value("name",std::string("ink-box"));
    NodeRef node=name=="#text"?std::static_pointer_cast<DOMNode>(
      std::make_shared<TextNode>(std::string{})):
      std::static_pointer_cast<DOMNode>(std::make_shared<DOMElement>(name));
    if(const auto element=std::dynamic_pointer_cast<DOMElement>(node)){
      element->attributes["id"]=id;
      if(item.contains("tabIndex"))element->attributes["tabIndex"]=tabIndex(item.at("tabIndex"));
    }
    nodes[id]=std::move(node);
  }
  for(const auto& item:spec.at("nodes")){
    if(!item.contains("parent")||item.at("parent").is_null())continue;
    const auto parent=std::dynamic_pointer_cast<DOMElement>(
      nodes.at(item.at("parent").get<std::string>()));
    bindFocusChild(*parent,nodes.at(item.at("id").get<std::string>()));
  }
  const auto root=std::dynamic_pointer_cast<DOMElement>(
    nodes.at(spec.value("root",std::string("root"))));
  Json events=Json::array();FocusManager* managerPointer=nullptr;
  int throwRemaining=spec.contains("throwOn")?1:0;
  int reenterRemaining=spec.contains("reenterOn")?1:0;
  FocusManager manager([&](DOMElement& target,batbox::terminal_events::FocusEvent& event){
    const auto related=static_cast<DOMElement*>(event.relatedTarget());
    const Json relatedId=related?related->attributes.at("id"):Json();
    const Json activeId=managerPointer->activeElement?
      managerPointer->activeElement->attributes.at("id"):Json();
    events.push_back({{"target",target.attributes.at("id")},{"type",event.type()},
      {"related",relatedId},{"active",activeId},
      {"stackSize",managerPointer->stackSize()},{"bubbles",event.bubbles()},
      {"cancelable",event.cancelable()}});
    if(throwRemaining&&event.type()==spec.at("throwOn").at("type").get<std::string>()&&
       target.attributes.at("id")==spec.at("throwOn").at("target")){
      --throwRemaining;throw std::runtime_error("focus dispatch failure");
    }
    if(reenterRemaining&&event.type()==spec.at("reenterOn").at("type").get<std::string>()&&
       target.attributes.at("id")==spec.at("reenterOn").at("target")){
      --reenterRemaining;
      managerPointer->focus(std::dynamic_pointer_cast<DOMElement>(
        nodes.at(spec.at("reenterOn").at("focus").get<std::string>())));
    }
    return false;
  },[]{return 1.0;});
  managerPointer=&manager;
  if(spec.value("manager",true))root->focusManager=&manager;
  const auto element=[&](const Json& operation){
    return std::dynamic_pointer_cast<DOMElement>(
      nodes.at(operation.at("id").get<std::string>()));
  };
  const auto id=[](const ElementRef& node)->Json{
    return node?node->attributes.at("id"):Json();
  };
  Json result=Json::array();
  for(const auto& operation:spec.at("ops")){
    const auto op=operation.at("op").get<std::string>();Json value;
    try{
      if(op=="focus")manager.focus(element(operation));
      else if(op=="blur")manager.blur();
      else if(op=="auto")manager.handleAutoFocus(element(operation));
      else if(op=="click")manager.handleClickFocus(element(operation));
      else if(op=="enable")manager.enable();
      else if(op=="disable")manager.disable();
      else if(op=="next")manager.focusNext(*root);
      else if(op=="previous")manager.focusPrevious(*root);
      else if(op=="remove"||op=="unmount"){
        auto node=nodes.at(operation.at("id").get<std::string>());
        if(node->parentNode)
          unbindFocusChild(*static_cast<DOMElement*>(node->parentNode),node);
        if(op=="remove")manager.handleNodeRemoved(element(operation),*root);
      }else if(op=="handleRemoved")manager.handleNodeRemoved(element(operation),*root);
      else if(op=="reattach"){
        bindFocusChild(*std::dynamic_pointer_cast<DOMElement>(
          nodes.at(operation.at("parent").get<std::string>())),
          nodes.at(operation.at("id").get<std::string>()));
      }else if(op=="tabIndex")element(operation)->attributes["tabIndex"]=tabIndex(operation.at("value"));
      else if(op=="getRoot")value=getRootNode(*element(operation)).attributes.at("id");
      else if(op=="getManager")value=&getFocusManager(*element(operation))==&manager;
      else throw std::runtime_error("unknown operation");
    }catch(const std::exception& error){value=Json{{"error",error.what()}};}
    result.push_back({{"op",op},{"result",value},{"active",id(manager.activeElement)},
      {"stackSize",manager.stackSize()},{"events",events}});
  }
  return result;
}
}
int main(int argc,char** argv){
  if(argc!=2||std::string(argv[1])!="--probe")return 2;
  std::string line;
  while(std::getline(std::cin,line)){
    try{
      const auto value=batbox::value::parse(line);
      std::cout<<batbox::value::stringify(Json{{"result",run(value)}})<<'\n';
    }catch(const std::exception& error){
      std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
    }
  }
}
