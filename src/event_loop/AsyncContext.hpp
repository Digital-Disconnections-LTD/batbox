#pragma once
#include <functional>
#include <memory>
#include <utility>
namespace batbox::runtime {
struct AsyncContextKey {};
using AsyncContextKeyRef=std::shared_ptr<const AsyncContextKey>;
struct AsyncContextSnapshot;
using AsyncContextRef=std::shared_ptr<const AsyncContextSnapshot>;
AsyncContextRef captureAsyncContext();
AsyncContextRef extendAsyncContext(const AsyncContextKeyRef& key,std::shared_ptr<void> value);
std::shared_ptr<void> currentAsyncContextValue(const AsyncContextKeyRef& key);
class AsyncContextScope {
public:
  explicit AsyncContextScope(AsyncContextRef context);
  ~AsyncContextScope();
  AsyncContextScope(const AsyncContextScope&)=delete;
  AsyncContextScope& operator=(const AsyncContextScope&)=delete;
private:
  AsyncContextRef previous_;
};
template<class F>auto bindAsyncContext(F callback){
  return [context=captureAsyncContext(),callback=std::move(callback)](auto&&... args)mutable->decltype(auto){
    AsyncContextScope scope(context);
    return std::invoke(callback,std::forward<decltype(args)>(args)...);
  };
}
}
