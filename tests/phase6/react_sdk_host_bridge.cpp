#include "react_sdk/HostBridge.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;
using namespace batbox::react_sdk;
int main(){
  auto port=std::make_shared<int>(42);
  auto portValue=hostObject(port);
  auto callback=std::make_shared<Function>();
  callback->name="onClick";
  callback->invoke=[port](const std::vector<ValueRef>& args){
    *port+=static_cast<int>(args.size());return number(*port);
  };
  auto callbackValue=function(callback);
  auto styleProps=props();styleProps->set("color",string("red"));
  auto style=object(styleProps);
  auto source=props();
  source->set("stdin",portValue);
  source->set("onClick",callbackValue);
  source->set("style",style);
  int specialCalls=0;
  HostBridge bridge({[&](const std::string& key,const ValueRef& value){
    ++specialCalls;
    terminal_reconciler::HostPropValue result;
    if(key=="stdin"){
      assert(value==portValue);
      assert(value->hostObject==port);
      result.value=std::make_shared<Json>("native-port");
    }else if(key=="onClick"){
      assert(value==callbackValue);
      assert(value->function->invoke({})->number==42);
      result.value=std::make_shared<Json>(Json::value_t::discarded);
    }else assert(false);
    return result;
  }});
  const auto first=bridge.project(source);
  const auto second=bridge.project(source);
  assert(first.values==second.values);
  assert(first.style==second.style);
  assert(first.style->get("color")==second.style->get("color"));
  assert(*first.values->get("stdin")=="native-port");
  assert(specialCalls==2);
  source.reset();
  const auto next=props();next->set("stdin",portValue);
  const auto third=bridge.project(next);
  assert(third.values!=first.values);
  assert(specialCalls==3);
  assert(first.values->get("stdin"));
  std::cout<<"1 native host prop identity and callback projection passed\n";
}
