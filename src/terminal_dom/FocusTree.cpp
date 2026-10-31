#include "terminal_dom/Focus.hpp"
#include <stdexcept>

namespace batbox::terminal_dom {
namespace {
void walkTree(DOMElement& node,std::vector<ElementRef>& result){
  if(node.attributes.is_object()){
    const auto found=node.attributes.find("tabIndex");
    if(found!=node.attributes.end()&&found->is_number()&&found->get<double>()>=0)
      result.push_back(std::static_pointer_cast<DOMElement>(node.shared_from_this()));
  }
  for(const auto& child:node.childNodes){
    if(child->nodeName=="#text")continue;
    const auto element=std::dynamic_pointer_cast<DOMElement>(child);
    if(!element)throw std::runtime_error("Expected DOMElement child");
    walkTree(*element,result);
  }
}
}
std::vector<ElementRef> collectTabbable(DOMElement& root){
  std::vector<ElementRef> result;walkTree(root,result);return result;
}
bool isInTree(const DOMElement& node,const DOMElement& root){
  const DOMElement* current=&node;
  while(current){
    if(current==&root)return true;
    current=parentElement(*current);
  }
  return false;
}
DOMElement& getRootNode(DOMElement& node){
  DOMElement* current=&node;
  while(current){
    if(current->focusManager)return *current;
    current=parentElement(*current);
  }
  throw std::runtime_error("Node is not in a tree with a FocusManager");
}
FocusManager& getFocusManager(DOMElement& node){
  return *getRootNode(node).focusManager;
}
}
