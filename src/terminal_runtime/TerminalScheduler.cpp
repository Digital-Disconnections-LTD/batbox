#include "terminal_runtime/TerminalControls.hpp"
#include <stdexcept>

namespace batbox::terminal_runtime {
void TerminalControls::bindIndigenousRenderScheduler(
    lodash_throttle::Services services){
  if(!services_.ownerLoop)
    throw std::logic_error("Render scheduler owner loop required");
  if(renderThrottle_)
    throw std::logic_error("Render scheduler already bound");
  if(!services.now&&!services.setTimeout)
    services=lodash_throttle::indigenousServices(*services_.ownerLoop);
  if(!services.now||!services.setTimeout)
    throw std::invalid_argument("Complete throttle timer imports required");
  const auto self=weak_from_this();
  if(self.expired())
    throw std::logic_error("Terminal owner requires shared lifetime");
  auto unmounted=ownerUnmounted_;
  auto* loop=services_.ownerLoop;
  auto deferredRender=[self,unmounted,loop]{
    loop->queueMicrotask([self,unmounted]{
      const auto owner=self.lock();
      if(!owner){
        if(unmounted->load(std::memory_order_relaxed))return;
        throw std::logic_error("Terminal owner expired before unmount");
      }
      owner->onRender();
    });
  };
  renderThrottle_=std::make_shared<lodash_throttle::Throttle>(
    std::move(deferredRender),16,std::move(services));
  if(!interaction_)interaction_.emplace();
  interaction_->scheduleRender=[throttle=renderThrottle_]{throttle->call();};
  services_.cancelScheduledRender=[throttle=renderThrottle_]{
    throttle->cancel();};
}
} // namespace batbox::terminal_runtime
