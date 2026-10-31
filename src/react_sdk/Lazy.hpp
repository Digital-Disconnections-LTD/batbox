#pragma once
#include "react_sdk/Value.hpp"
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>

namespace batbox::react_sdk {
class ThrownValue:public std::exception {
public:
  explicit ThrownValue(ValueRef value):value_(std::move(value)){}
  const ValueRef& value()const{return value_;}
  const char* what()const noexcept override{return "React lazy value thrown";}
private:
  ValueRef value_;
};
struct LazyThenable {
  std::function<void(std::function<void(ValueRef)>,
    std::function<void(ValueRef)>)> then;
};
using LazyThenableRef=std::shared_ptr<LazyThenable>;
using LazyConstructor=std::function<LazyThenableRef()>;
struct LazyPayload {
  int status=-1;
  LazyConstructor constructor;
  LazyThenableRef pending;
  ValueRef pendingValue;
  ValueRef module;
  ValueRef reason;
};
ValueRef lazyComponent(LazyConstructor constructor);
ValueRef initializeLazy(const ValueRef& lazyType);
std::shared_ptr<LazyPayload> lazyPayload(const ValueRef& lazyType);
} // namespace batbox::react_sdk
