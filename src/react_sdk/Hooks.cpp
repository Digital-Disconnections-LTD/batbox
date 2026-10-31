#include "react_sdk/Hooks.hpp"

namespace batbox::react_sdk {
namespace {
thread_local Dispatcher* hooks=nullptr;
thread_local Dispatcher* stash=nullptr;
ValueRef forward(const char* method,std::vector<ValueRef> arguments){
  if(!hooks)throw TypeError(std::string(
    "Cannot read properties of null (reading '")+method+"')");
  return hooks->call(method,arguments);
}
}
DispatcherScope::DispatcherScope(Dispatcher* nextHooks,Dispatcher* nextStash):
  previousHooks_(hooks),previousStash_(stash){
  hooks=nextHooks;stash=nextStash;
}
DispatcherScope::~DispatcherScope(){hooks=previousHooks_;stash=previousStash_;}
Dispatcher* currentHookDispatcher(){return hooks;}
Dispatcher* currentStashDispatcher(){return stash;}
ValueRef getStashForType(ValueRef resourceType){
  if(!stash){
    if(!resourceType||resourceType->kind!=Kind::Function||
        !resourceType->function||!resourceType->function->invoke)
      throw TypeError("resourceType is not a function");
    return resourceType->function->invoke({});
  }
  return stash->call("getCacheForType",{resourceType});
}
ValueRef useContext(ValueRef context){return forward("useContext",{context});}
ValueRef useState(ValueRef initialState){return forward("useState",{initialState});}
ValueRef useReducer(ValueRef reducer,ValueRef initialArg,ValueRef init){
  return forward("useReducer",{reducer,initialArg,init});}
ValueRef useRef(ValueRef initialValue){return forward("useRef",{initialValue});}
ValueRef useEffect(ValueRef create,ValueRef deps){return forward("useEffect",{create,deps});}
ValueRef useInsertionEffect(ValueRef create,ValueRef deps){
  return forward("useInsertionEffect",{create,deps});}
ValueRef useLayoutEffect(ValueRef create,ValueRef deps){
  return forward("useLayoutEffect",{create,deps});}
ValueRef useCallback(ValueRef callback,ValueRef deps){
  return forward("useCallback",{callback,deps});}
ValueRef useMemo(ValueRef create,ValueRef deps){
  return forward("useMemo",{create,deps});}
ValueRef useImperativeHandle(ValueRef ref,ValueRef create,ValueRef deps){
  return forward("useImperativeHandle",{ref,create,deps});}
ValueRef useDebugValue(ValueRef,ValueRef){return undefined();}
ValueRef useTransition(){return forward("useTransition",{});}
ValueRef useDeferredValue(ValueRef value,ValueRef initialValue){
  return forward("useDeferredValue",{value,initialValue});}
ValueRef useId(){return forward("useId",{});}
ValueRef useSyncExternalStore(ValueRef subscribe,ValueRef getSnapshot,
    ValueRef getServerSnapshot){
  return forward("useSyncExternalStore",
    {subscribe,getSnapshot,getServerSnapshot});}
ValueRef useStashRefresh(){return forward("useCacheRefresh",{});}
ValueRef use(ValueRef usable){return forward("use",{usable});}
ValueRef useMemoStash(double size){return forward("useMemoCache",{number(size)});}
ValueRef compilerStash(double size){return useMemoStash(size);}
ValueRef useEffectEvent(ValueRef callback){return forward("useEffectEvent",{callback});}
ValueRef useOptimistic(ValueRef passthrough,ValueRef reducer){
  return forward("useOptimistic",{passthrough,reducer});}
ValueRef useActionState(ValueRef action,ValueRef initialState,
    ValueRef permalink){
  return forward("useActionState",{action,initialState,permalink});}
}
