#include "event_loop/AsyncContext.hpp"
#include <map>
#include <stdexcept>
namespace batbox::runtime {
struct AsyncContextSnapshot {
  std::map<AsyncContextKeyRef,std::shared_ptr<void>,std::owner_less<AsyncContextKeyRef>> values;
};
namespace {thread_local AsyncContextRef activeContext;}
AsyncContextRef captureAsyncContext(){return activeContext;}
AsyncContextRef extendAsyncContext(const AsyncContextKeyRef& key,std::shared_ptr<void> value){
  if(!key)throw std::invalid_argument("Async context storage key required");
  auto snapshot=activeContext?std::make_shared<AsyncContextSnapshot>(*activeContext):
    std::make_shared<AsyncContextSnapshot>();
  snapshot->values[key]=std::move(value);return snapshot;
}
std::shared_ptr<void> currentAsyncContextValue(const AsyncContextKeyRef& key){
  if(!activeContext)return {};
  const auto found=activeContext->values.find(key);
  return found==activeContext->values.end()?std::shared_ptr<void>{}:found->second;
}
AsyncContextScope::AsyncContextScope(AsyncContextRef context):previous_(std::move(activeContext)){
  activeContext=std::move(context);
}
AsyncContextScope::~AsyncContextScope(){activeContext=std::move(previous_);}
}
