#pragma once
#include "event_loop/AsyncContext.hpp"
namespace batbox::runtime {
// Indigenous asynchronous-local storage with run/getStore operations.
// Continuation registration captures a snapshot; stores retain object identity.
template<class T>class AsyncLocalStorage {
public:
  using Ref=std::shared_ptr<T>;
  Ref getStore()const{return std::static_pointer_cast<T>(currentAsyncContextValue(key_));}
  AsyncContextScope scope(Ref store)const{
    return AsyncContextScope(extendAsyncContext(key_,std::move(store)));
  }
  template<class F,class... Args>decltype(auto) run(Ref store,F&& callback,Args&&... args)const{
    auto active=scope(std::move(store));
    return std::invoke(std::forward<F>(callback),std::forward<Args>(args)...);
  }
private:
  AsyncContextKeyRef key_=std::make_shared<AsyncContextKey>();
};
}
