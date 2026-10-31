#include "react_sdk/Element.hpp"
#include <stdexcept>

namespace batbox::react_sdk {
ElementRef reactElement(ValueRef type,std::optional<std::string> key,
    PropsRef elementProps,ValueRef owner){
  if(!elementProps)throw std::invalid_argument("Element props required");
  auto ref=elementProps->get("ref");
  if(isUndefined(ref))ref=nullValue();
  return std::make_shared<Element>(Element{std::move(type),std::move(key),
    std::move(elementProps),std::move(ref),std::move(owner)});
}
ElementRef jsx(ValueRef type,PropsRef config,
    std::optional<ValueRef> maybeKey){
  if(!config)throw std::invalid_argument("JSX config required");
  std::optional<std::string> key;
  if(maybeKey&&!isUndefined(*maybeKey))key=toString(*maybeKey);
  const auto configKey=config->get("key");
  if(!isUndefined(configKey))key=toString(configKey);
  PropsRef elementProps;
  if(!config->has("key"))elementProps=config;
  else{
    elementProps=props();
    for(const auto& name:config->enumerableKeys())
      if(name!="key")elementProps->set(name,config->get(name));
  }
  return reactElement(std::move(type),std::move(key),
    std::move(elementProps));
}
ElementRef createElement(ValueRef type,PropsRef config,
    std::vector<ValueRef> children){
  auto elementProps=props();
  std::optional<std::string> key;
  if(config){
    const auto configKey=config->get("key");
    if(!isUndefined(configKey))key=toString(configKey);
    for(const auto& name:config->ownEnumerableKeys())
      if(name!="key"&&name!="__self"&&name!="__source")
        elementProps->set(name,config->getOwn(name));
  }
  if(children.size()==1)elementProps->set("children",children[0]);
  else if(children.size()>1)
    elementProps->set("children",array(std::move(children)));

  PropsRef defaults;
  if(truthy(type)){
    if(type->kind==Kind::Function&&type->function)
      defaults=type->function->defaultProps;
    else if(type->kind==Kind::Object&&type->object){
      const auto defaultValue=type->object->get("defaultProps");
      if(defaultValue&&defaultValue->kind==Kind::Object)
        defaults=defaultValue->object;
    }
  }
  if(defaults)for(const auto& name:defaults->enumerableKeys())
    if(isUndefined(elementProps->get(name)))
      elementProps->set(name,defaults->get(name));
  return reactElement(std::move(type),std::move(key),
    std::move(elementProps));
}
ElementRef cloneAndReplaceKey(const ElementRef& oldElement,
    const std::string& newKey){
  if(!oldElement)throw std::invalid_argument("Element required");
  return reactElement(oldElement->type,newKey,oldElement->props);
}
ElementRef cloneElement(const ValueRef& original,PropsRef config,
    std::vector<ValueRef> children){
  if(isUndefined(original)||original->kind==Kind::Null)
    throw std::runtime_error("The argument must be a React element, but you passed " +
      toString(original)+".");
  if(original->kind!=Kind::Element||!original->element)
    throw std::invalid_argument("React element import required");
  const auto& old=*original->element;
  auto clonedProps=props();
  if(old.props)for(const auto& name:old.props->ownEnumerableKeys())
    clonedProps->set(name,old.props->getOwn(name));
  auto key=old.key;
  if(config){
    const auto newKey=config->get("key");
    if(!isUndefined(newKey))key=toString(newKey);
    for(const auto& name:config->ownEnumerableKeys()){
      if(name=="key"||name=="__self"||name=="__source")continue;
      const auto value=config->getOwn(name);
      if(name=="ref"&&isUndefined(value))continue;
      clonedProps->set(name,value);
    }
  }
  if(children.size()==1)clonedProps->set("children",children[0]);
  else if(children.size()>1)
    clonedProps->set("children",array(std::move(children)));
  return reactElement(old.type,std::move(key),std::move(clonedProps));
}
bool isValidElement(const ValueRef& value){
  return value&&value->kind==Kind::Element&&value->element!=nullptr;
}
}
