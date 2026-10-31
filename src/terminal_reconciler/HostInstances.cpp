#include "terminal_reconciler/HostConfig.hpp"
#include "terminal_dom/Focus.hpp"
#include "terminal_dom/FullLayoutPort.hpp"
#include <stdexcept>

namespace batbox::terminal_reconciler {
namespace {
terminal_dom::FullLayoutRef fullLayout(terminal_dom::DOMNode& node){
  const auto full=std::dynamic_pointer_cast<terminal_dom::FullLayoutNode>(
    node.yogaNode);
  if(node.yogaNode&&!full)
    throw std::invalid_argument("Full Yoga layout import required");
  return full;
}
}
HostConfig::HostConfig(HostConfigServices services):
  services_(std::move(services)){}
HostContextRef HostConfig::getRootHostContext()const{
  return std::make_shared<HostContext>(HostContext{false});
}
HostContextRef HostConfig::getChildHostContext(
    const HostContextRef& parent,const std::string& type)const{
  if(!parent)throw std::invalid_argument("Host context required");
  const auto previous=parent->isInsideText;
  const auto inside=type=="ink-text"||type=="ink-virtual-text"||
    type=="ink-link";
  if(previous==inside)return parent;
  return std::make_shared<HostContext>(HostContext{inside});
}
terminal_dom::ElementRef HostConfig::createInstance(
    const std::string& originalType,const PropsBundle& props,
    const HostContextRef& context,const OwnerFiberRef& internalHandle){
  if(!context)throw std::invalid_argument("Host context required");
  if(context->isInsideText&&originalType=="ink-box")
    throw std::runtime_error("<Box> can't be nested inside <Text> component");
  const auto type=originalType=="ink-text"&&context->isInsideText?
    "ink-virtual-text":originalType;
  auto node=terminal_dom::createNode(type,services_.dom);
  if(services_.commitLogEnabled){
    if(!services_.recordCreatedNode)
      throw std::invalid_argument("Commit create-count import required");
    services_.recordCreatedNode();
  }
  if(!props.values)throw std::invalid_argument("Host props required");
  for(const auto& key:props.values->keys()){
    HostPropValue value{props.values->get(key),{}};
    const auto found=props.handlers.find(key);
    if(found!=props.handlers.end())value.handler=found->second;
    const auto click=props.clickHandlers.find(key);
    if(click!=props.clickHandlers.end())value.onClick=click->second;
    const auto hover=props.hoverHandlers.find(key);
    if(hover!=props.hoverHandlers.end())value.onHover=hover->second;
    applyProp(*node,key,value,services_.props);
  }
  if(!services_.debugRepaints)
    throw std::invalid_argument("Debug repaint state import required");
  if(services_.debugRepaints->enabled())
    node->debugOwnerChain=getOwnerChain(internalHandle);
  return node;
}
std::shared_ptr<terminal_dom::TextNode> HostConfig::createTextInstance(
    const std::string& text,const HostContextRef& context){
  if(!context||!context->isInsideText)
    throw std::runtime_error("Text string \""+text+
      "\" must be rendered inside <Text> component");
  return terminal_dom::createTextNode(text);
}
void HostConfig::hideTextInstance(terminal_dom::TextNode& node){
  terminal_dom::setTextNodeValue(node,"");
}
void HostConfig::unhideTextInstance(terminal_dom::TextNode& node,
    const std::string& text){terminal_dom::setTextNodeValue(node,text);}
void HostConfig::hideInstance(terminal_dom::DOMElement& node){
  node.isHidden=true;
  if(const auto yoga=fullLayout(node))
    yoga->setDisplay(terminal_dom::LayoutDisplay::None);
  terminal_dom::markDirty(&node);
}
void HostConfig::unhideInstance(terminal_dom::DOMElement& node){
  node.isHidden=false;
  if(const auto yoga=fullLayout(node))
    yoga->setDisplay(terminal_dom::LayoutDisplay::Flex);
  terminal_dom::markDirty(&node);
}
void HostConfig::appendInitialChild(terminal_dom::DOMElement& node,
    const terminal_dom::NodeRef& child){appendChild(node,child);}
void HostConfig::appendChild(terminal_dom::DOMElement& node,
    const terminal_dom::NodeRef& child){
  if(!services_.stash)throw std::invalid_argument("DOM node cache required");
  terminal_dom::appendChildNode(node,child,*services_.stash);
}
void HostConfig::insertBefore(terminal_dom::DOMElement& node,
    const terminal_dom::NodeRef& child,
    const terminal_dom::NodeRef& before){
  if(!services_.stash)throw std::invalid_argument("DOM node cache required");
  terminal_dom::insertBeforeNode(node,child,before,*services_.stash);
}
bool HostConfig::finalizeInitialChildren(const PropsBundle& props)const{
  if(!props.values)throw std::invalid_argument("Host props required");
  const auto value=props.values->get("autoFocus");
  return value&&value->is_boolean()&&value->get<bool>();
}
void HostConfig::commitMount(terminal_dom::DOMElement& node){
  terminal_dom::getFocusManager(node).handleAutoFocus(
    std::static_pointer_cast<terminal_dom::DOMElement>(node.shared_from_this()));
}
void HostConfig::removeChildFromContainer(terminal_dom::DOMElement& node,
    const terminal_dom::NodeRef& removeNode){
  if(!services_.stash)throw std::invalid_argument("DOM node cache required");
  terminal_dom::removeChildNode(node,removeNode,*services_.stash);
  cleanupYogaNode(*removeNode);
  terminal_dom::getFocusManager(node).handleNodeRemoved(
    std::static_pointer_cast<terminal_dom::DOMElement>(removeNode),node);
}
void HostConfig::removeChild(terminal_dom::DOMElement& node,
    const terminal_dom::NodeRef& removeNode){
  if(!services_.stash)throw std::invalid_argument("DOM node cache required");
  terminal_dom::removeChildNode(node,removeNode,*services_.stash);
  cleanupYogaNode(*removeNode);
  if(removeNode->nodeName!="#text"){
    auto& root=terminal_dom::getRootNode(node);
    if(!root.focusManager)
      throw std::invalid_argument("Root FocusManager required");
    root.focusManager->handleNodeRemoved(
      std::static_pointer_cast<terminal_dom::DOMElement>(removeNode),root);
  }
}
void HostConfig::commitTextUpdate(terminal_dom::TextNode& node,
    const std::string& newText){
  terminal_dom::setTextNodeValue(node,newText);
}
} // namespace batbox::terminal_reconciler
