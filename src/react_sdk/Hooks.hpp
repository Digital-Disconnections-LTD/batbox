#pragma once
#include "react_sdk/Value.hpp"
#include <stdexcept>

namespace batbox::react_sdk {
class TypeError:public std::runtime_error {
public:using std::runtime_error::runtime_error;
};
// ReactSharedInternals.H/A are installed by the renderer. This interface
// forwards hook calls and does not implement a substitute Fiber scheduler.
class Dispatcher {
public:
  virtual ~Dispatcher()=default;
  virtual ValueRef call(const std::string& method,
    const std::vector<ValueRef>& arguments)=0;
};
class DispatcherScope {
public:
  DispatcherScope(Dispatcher* hooks,Dispatcher* stash=nullptr);
  ~DispatcherScope();
  DispatcherScope(const DispatcherScope&)=delete;
  DispatcherScope& operator=(const DispatcherScope&)=delete;
private:
  Dispatcher* previousHooks_;
  Dispatcher* previousStash_;
};
Dispatcher* currentHookDispatcher();
Dispatcher* currentStashDispatcher();
ValueRef getStashForType(ValueRef resourceType);
ValueRef useContext(ValueRef context);
ValueRef useState(ValueRef initialState);
ValueRef useReducer(ValueRef reducer,ValueRef initialArg,
  ValueRef init=undefined());
ValueRef useRef(ValueRef initialValue);
ValueRef useEffect(ValueRef create,ValueRef deps=undefined());
ValueRef useInsertionEffect(ValueRef create,ValueRef deps=undefined());
ValueRef useLayoutEffect(ValueRef create,ValueRef deps=undefined());
ValueRef useCallback(ValueRef callback,ValueRef deps=undefined());
ValueRef useMemo(ValueRef create,ValueRef deps=undefined());
ValueRef useImperativeHandle(ValueRef ref,ValueRef create,
  ValueRef deps=undefined());
ValueRef useDebugValue(ValueRef value,ValueRef formatter=undefined());
ValueRef useTransition();
ValueRef useDeferredValue(ValueRef value,
  ValueRef initialValue=undefined());
ValueRef useId();
ValueRef useSyncExternalStore(ValueRef subscribe,ValueRef getSnapshot,
  ValueRef getServerSnapshot=undefined());
ValueRef useStashRefresh();
ValueRef use(ValueRef usable);
ValueRef useMemoStash(double size);
ValueRef compilerStash(double size);
ValueRef useEffectEvent(ValueRef callback);
ValueRef useOptimistic(ValueRef passthrough,
  ValueRef reducer=undefined());
ValueRef useActionState(ValueRef action,ValueRef initialState,
  ValueRef permalink=undefined());
}
