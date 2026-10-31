#include "terminal_runtime/TerminalControls.hpp"
#include <stdexcept>

namespace batbox::terminal_runtime {
void TerminalControls::bindProcessLifecycle(
    ProcessLifecycleServices services){
  if(unsubscribeExit_)
    throw std::logic_error("Process lifecycle already bound");
  if(!services.onExit||
     (stdout_.isTTY()&&(!services.onStdoutResize||
       !services.offStdoutResize||!services.onSigcont||
       !services.offSigcont)))
    throw std::invalid_argument("Process lifecycle imports required");
  const auto self=weak_from_this();
  if(self.expired())
    throw std::logic_error("Terminal owner requires shared lifetime");
  auto unmounted=ownerUnmounted_;
  auto exitListener=std::make_shared<Listener>(Listener{
    [self,unmounted]{
      const auto owner=self.lock();
      if(!owner){
        if(unmounted->load(std::memory_order_relaxed))return;
        throw std::logic_error("Terminal owner expired before unmount");
      }
      owner->unmount();
    }});
  unsubscribeExit_=services.onExit(exitListener,false);
  if(!unsubscribeExit_)
    throw std::invalid_argument("Exit unsubscribe required");
  if(stdout_.isTTY()){
    handleResizeListener_=std::make_shared<Listener>(Listener{
      [self,unmounted]{
        const auto owner=self.lock();
        if(!owner){
          if(unmounted->load(std::memory_order_relaxed))return;
          throw std::logic_error("Terminal owner expired before unmount");
        }
        owner->handleResize();
      }});
    handleResumeListener_=std::make_shared<Listener>(Listener{
      [self,unmounted]{
        const auto owner=self.lock();
        if(!owner){
          if(unmounted->load(std::memory_order_relaxed))return;
          throw std::logic_error("Terminal owner expired before unmount");
        }
        owner->handleResume();
      }});
    services.onStdoutResize(handleResizeListener_);
    services.onSigcont(handleResumeListener_);
    unsubscribeTTYHandlers_=[services,
        resize=handleResizeListener_,resume=handleResumeListener_]{
      services.offStdoutResize(resize);
      services.offSigcont(resume);
    };
  }
}
} // namespace batbox::terminal_runtime
