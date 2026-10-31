#include "react_sdk/Context.hpp"
#include <stdexcept>

namespace batbox::react_sdk {
ValueRef createContext(ValueRef defaultValue){
  if(!defaultValue)defaultValue=undefined();
  auto context=std::make_shared<Value>();
  context->kind=Kind::Context;
  context->context=std::make_shared<ContextData>();
  context->context->defaultValue=defaultValue;
  context->context->currentValue=defaultValue;
  context->context->currentValue2=defaultValue;
  context->context->threadCount=0;
  auto consumer=std::make_shared<Value>();
  consumer->kind=Kind::Consumer;
  consumer->contextBackref=context;
  context->context->consumer=consumer;
  return context;
}
ValueRef contextProvider(const ValueRef& context){
  if(!context||context->kind!=Kind::Context)
    throw std::invalid_argument("React Context required");
  return context;
}
ValueRef contextConsumer(const ValueRef& context){
  if(!context||context->kind!=Kind::Context||!context->context)
    throw std::invalid_argument("React Context required");
  return context->context->consumer;
}
ValueRef createRef(){
  auto record=props();record->set("current",nullValue());
  return object(std::move(record));
}
}
