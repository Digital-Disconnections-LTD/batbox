#include "react_sdk/Context.hpp"
#include "react_sdk/Hooks.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::react_sdk;
static std::string describe(const ValueRef& value){
  if(!value||value->kind==Kind::Undefined)return "undefined";
  switch(value->kind){
    case Kind::Null:return "null";
    case Kind::Function:return "fn";
    case Kind::Array:return "array";
    case Kind::Context:return "context";
    case Kind::Object:return "object";
    default:return toString(value);
  }
}
class ProbeDispatcher:public Dispatcher {
public:
  Json trace=Json::array();
  ValueRef call(const std::string& method,
      const std::vector<ValueRef>& arguments)override{
    Json args=Json::array();
    for(const auto& value:arguments)args.push_back(describe(value));
    trace.push_back({{"method",method},{"args",args}});
    return string("result:"+method);
  }
};
int main(){
  ProbeDispatcher dispatcher;
  auto callback=function(std::make_shared<Function>());
  auto context=createContext(number(1));
  {
    DispatcherScope scope(&dispatcher);
    useContext(context);
    useState(number(5));
    useReducer(callback,number(7));
    useRef(nullValue());
    useEffect(callback,array({number(1)}));
    useInsertionEffect(callback);
    useLayoutEffect(callback,array({number(2)}));
    useCallback(callback,nullValue());
    useMemo(callback,array({number(3)}));
    useImperativeHandle(nullValue(),callback,array({number(4)}));
    const auto debugResult=useDebugValue(string("debug"));
    useTransition();
    useDeferredValue(string("value"));
    useId();
    useSyncExternalStore(callback,callback);
    useStashRefresh();
    use(context);
    useEffectEvent(callback);
    useOptimistic(number(5));
    useActionState(callback,number(0));
    compilerStash(4);
    dispatcher.trace.push_back({{"method","useDebugValueResult"},
      {"args",Json::array({describe(debugResult)})}});
  }
  try{useState(number(1));}
  catch(const TypeError& error){dispatcher.trace.push_back({
    {"method","invalidHook"},
    {"args",Json::array({"TypeError",error.what()})}});}
  std::cout<<dispatcher.trace.dump()<<'\n';
}
