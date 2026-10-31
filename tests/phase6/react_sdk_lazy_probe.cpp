#include "react_sdk/Children.hpp"
#include "react_sdk/Lazy.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::react_sdk;
static ValueRef module(ValueRef value){
  auto p=props();p->set("default",value);return object(p);
}
int main(){
  Json rows=Json::array();
  auto component=function(std::make_shared<Function>());
  auto immediate=lazyComponent([&]{
    auto promise=std::make_shared<LazyThenable>();
    promise->then=[&](auto resolve,auto){resolve(module(component));};
    return promise;
  });
  rows.push_back({{"kind","immediate"},
    {"same",initializeLazy(immediate)==component},
    {"status",lazyPayload(immediate)->status}});
  std::function<void(ValueRef)> resolvePending;
  auto pending=std::make_shared<LazyThenable>();
  pending->then=[&](auto resolve,auto){resolvePending=resolve;};
  auto later=lazyComponent([pending]{return pending;});
  ValueRef first,second;
  try{(void)initializeLazy(later);}catch(const ThrownValue& error){
    first=error.value();}
  try{(void)initializeLazy(later);}catch(const ThrownValue& error){
    second=error.value();}
  rows.push_back({{"kind","pending"},
    {"same",first&&first->hostObject==pending},
    {"again",first==second},{"status",lazyPayload(later)->status}});
  resolvePending(module(component));
  rows.push_back({{"kind","resolved"},
    {"same",initializeLazy(later)==component},
    {"status",lazyPayload(later)->status}});
  auto reason=hostObject(std::make_shared<std::string>("failed"));
  auto rejected=lazyComponent([&]{
    auto promise=std::make_shared<LazyThenable>();
    promise->then=[&](auto,auto reject){reject(reason);};
    return promise;
  });
  ValueRef thrown;
  try{(void)initializeLazy(rejected);}catch(const ThrownValue& error){
    thrown=error.value();}
  rows.push_back({{"kind","rejected"},{"same",thrown==reason},
    {"status",lazyPayload(rejected)->status}});
  int tries=0;
  auto retry=lazyComponent([&]{
    if(++tries==1)throw std::runtime_error("ctor");
    auto promise=std::make_shared<LazyThenable>();
    promise->then=[&](auto resolve,auto){resolve(module(component));};
    return promise;
  });
  try{(void)initializeLazy(retry);}catch(const std::exception& error){
    rows.push_back({{"kind","constructorThrow"},
      {"message",error.what()},{"status",lazyPayload(retry)->status}});
  }
  rows.push_back({{"kind","constructorRetry"},
    {"same",initializeLazy(retry)==component},{"tries",tries},
    {"status",lazyPayload(retry)->status}});
  auto child=element(jsx(string("child"),props()));
  auto lazyChild=lazyComponent([&]{
    auto promise=std::make_shared<LazyThenable>();
    promise->then=[&](auto resolve,auto){resolve(module(child));};
    return promise;
  });
  ChildrenImports imports;
  imports.resolveLazy=[](const ValueRef& value){return initializeLazy(value);};
  rows.push_back({{"kind","children"},
    {"key",toArray(lazyChild,imports)[0]->element->key.value()}});
  std::cout<<rows.dump()<<'\n';
}
