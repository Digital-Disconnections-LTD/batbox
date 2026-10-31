#include "terminal_dom/Dom.hpp"
#include "runtime/ArrayJoin.hpp"
#include <stdexcept>

namespace batbox::terminal_dom {
namespace {
bool primitiveStrictEqual(const Json& a,const Json& b){
  if(a.is_number()&&b.is_number())return a.get<double>()==b.get<double>();
  if(a.type()!=b.type())return false;
  if(a.is_null()||a.is_boolean()||a.is_string())return a==b;
  // Styles/TextStyles properties are primitive values. Distinct
  // nested JSON values cannot express shared JavaScript object identity.
  return false;
}
}
bool shallowEqual(const std::shared_ptr<Json>& a,
    const std::shared_ptr<Json>& b){
  if(a==b)return true;
  if(!a||!b)return false;
  if(!a->is_object()||!b->is_object())return false;
  if(a->size()!=b->size())return false;
  for(auto it=a->begin();it!=a->end();++it){
    const auto other=b->find(it.key());
    if(other==b->end()||!primitiveStrictEqual(it.value(),other.value()))
      return false;
  }
  return true;
}
void setAttribute(DOMElement& node,const std::string& key,const Json& value){
  if(key=="children")return;
  const auto found=node.attributes.find(key);
  if(found!=node.attributes.end()&&primitiveStrictEqual(*found,value))return;
  node.attributes[key]=value;
  markDirty(&node);
}
void setStyle(DOMNode& node,const std::shared_ptr<Json>& style){
  if(shallowEqual(node.style,style))return;
  node.style=style;
  markDirty(&node);
}
void setTextStyles(DOMElement& node,
    const std::shared_ptr<Json>& textStyles){
  if(shallowEqual(node.textStyles,textStyles))return;
  node.textStyles=textStyles;
  markDirty(&node);
}
void setTextNodeValue(TextNode& node,const std::string& text){
  if(node.nodeValue==text)return;
  node.nodeValue=text;
  markDirty(&node);
}
void setTextNodeValueDynamic(TextNode& node,const Json& value){
  if(value.is_string()){setTextNodeValue(node,value.get<std::string>());return;}
  if(value.is_discarded()){setTextNodeValue(node,std::string("undefined"));return;}
  if(value.is_null()){setTextNodeValue(node,std::string("null"));return;}
  if(value.is_boolean()){
    setTextNodeValue(node,std::string(value.get<bool>()?"true":"false"));return;
  }
  if(value.is_number()){
    setTextNodeValue(node,value::numberToString(value.get<double>()));return;
  }
  if(value.is_array()){setTextNodeValue(node,value::arrayJoin(value));return;}
  if(value.contains("toString"))
    throw std::invalid_argument("Cannot convert object to primitive value");
  setTextNodeValue(node,std::string("[object Object]"));
}
} // namespace batbox::terminal_dom
