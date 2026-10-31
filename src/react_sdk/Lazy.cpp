#include "react_sdk/Lazy.hpp"

namespace batbox::react_sdk {
ValueRef lazyComponent(LazyConstructor constructor){
  auto payload=std::make_shared<LazyPayload>();
  payload->constructor=std::move(constructor);
  return lazy(payload);
}
std::shared_ptr<LazyPayload> lazyPayload(const ValueRef& lazyType){
  if(!lazyType||lazyType->kind!=Kind::Lazy||!lazyType->hostObject)
    throw std::invalid_argument("React lazy type required");
  return std::static_pointer_cast<LazyPayload>(lazyType->hostObject);
}
ValueRef initializeLazy(const ValueRef& lazyType){
  const auto payload=lazyPayload(lazyType);
  if(payload->status==-1){
    const auto constructor=payload->constructor;
    const auto thenable=constructor();
    if(!thenable||!thenable->then)
      throw std::invalid_argument("React lazy thenable required");
    const std::weak_ptr<LazyPayload> weak=payload;
    thenable->then(
      [weak](ValueRef module){
        if(const auto state=weak.lock();state&&
            (state->status==0||state->status==-1)){
          state->status=1;state->module=std::move(module);
        }
      },
      [weak](ValueRef error){
        if(const auto state=weak.lock();state&&
            (state->status==0||state->status==-1)){
          state->status=2;state->reason=std::move(error);
        }
      });
    if(payload->status==-1){
      payload->status=0;payload->pending=thenable;
      payload->pendingValue=thenable?react_sdk::thenable(thenable):nullptr;
    }
  }
  if(payload->status==1){
    if(payload->module&&payload->module->kind==Kind::Object&&
        payload->module->object)
      return payload->module->object->get("default");
    return undefined();
  }
  if(payload->status==2)throw ThrownValue(payload->reason);
  throw ThrownValue(payload->pendingValue);
}
} // namespace batbox::react_sdk
