#include "instructions/hooks/runtime/AsyncHookRoster.hpp"
#include <memory>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
struct RosterFinalize:std::enable_shared_from_this<RosterFinalize>{
  AsyncHookRoster& roster;
  event_loop::Deferred<event_loop::Unit> output;
  std::vector<std::shared_ptr<PendingAsyncHook>> hooks;
  std::size_t remaining=0;
  bool failed=false;
  explicit RosterFinalize(AsyncHookRoster& owner):roster(owner),output(owner.loop_){}
  void start(){
    for(const auto& id:roster.order_){
      const auto found=roster.pending_.find(id);
      if(found!=roster.pending_.end())hooks.push_back(found->second);
    }
    remaining=hooks.size();
    if(hooks.empty()){
      roster.pending_.clear();roster.order_.clear();output.resolve({});return;
    }
    for(const auto& hook:hooks)runOne(hook);
  }
  void settle(std::exception_ptr error){
    if(error&&!failed){failed=true;output.reject(error);}
    if(--remaining==0&&!failed){
      roster.pending_.clear();roster.order_.clear();output.resolve({});
    }
  }
  void afterCode(const std::shared_ptr<PendingAsyncHook>& hook,int code){
    try{
      roster.finalizeHook(*hook,code,code==0?"success":"error").then(
        [self=shared_from_this()](const event_loop::Unit*,
          const std::exception_ptr& error){self->settle(error);});
    }catch(...){settle(std::current_exception());}
  }
  void runOne(const std::shared_ptr<PendingAsyncHook>& hook){
    try{
      if(hook->process&&hook->process->status()=="completed"){
        hook->process->resultCode().then(
          [self=shared_from_this(),hook](const int* code,
            const std::exception_ptr& error){
            if(error){self->settle(error);return;}
            self->afterCode(hook,*code);
          });
        return;
      }
      if(hook->process&&hook->process->status()!="killed")
        hook->process->kill();
      roster.finalizeHook(*hook,1,"cancelled").then(
        [self=shared_from_this()](const event_loop::Unit*,
          const std::exception_ptr& error){self->settle(error);});
    }catch(...){settle(std::current_exception());}
  }
};
event_loop::Deferred<event_loop::Unit>
AsyncHookRoster::finalizePendingAsyncHooks(){
  if(!loop_.isOwnerThread())throw std::logic_error("Async hook registry owner required");
  auto run=std::make_shared<RosterFinalize>(*this);
  run->start();return run->output;
}
} // namespace batbox::instructions::hooks::runtime
