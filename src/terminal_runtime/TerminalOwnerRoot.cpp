#include "terminal_runtime/TerminalControls.hpp"
#include "terminal_dom/FullLayoutPort.hpp"
#include "indigenous_yoga/Node.hpp"
#include <stdexcept>

namespace batbox::terminal_runtime {
void TerminalControls::bindOwnerRoot(OwnerRootServices services){
  if(!services.rootNode||!services.focusManager||!services.profile||
     !services.performanceNow||!services.scheduleRender)
    throw std::invalid_argument("Owner root imports required");
  const auto self=weak_from_this();
  if(self.expired())
    throw std::logic_error("Terminal owner requires shared lifetime");
  if(!interaction_)interaction_.emplace();
  interaction_->rootNode=services.rootNode;
  interaction_->focusManager=services.focusManager;
  interaction_->scheduleRender=services.scheduleRender;
  services.rootNode->focusManager=services.focusManager;
  services.rootNode->onRender=services.scheduleRender;
  auto unmounted=ownerUnmounted_;
  services.rootNode->onImmediateRender=[self,unmounted]{
    const auto owner=self.lock();
    if(!owner){
      if(unmounted->load(std::memory_order_relaxed))return;
      throw std::logic_error("Terminal owner expired before unmount");
    }
    owner->onRender();
  };
  const std::weak_ptr<terminal_dom::DOMElement> rootWeak=services.rootNode;
  services.rootNode->onComputeLayout=[self,rootWeak,unmounted,
      now=std::move(services.performanceNow),
      profile=services.profile]{
    const auto owner=self.lock();
    if(!owner){
      if(unmounted->load(std::memory_order_relaxed))return;
      throw std::logic_error("Terminal owner expired before unmount");
    }
    if(owner->isUnmounted_)return;
    const auto root=rootWeak.lock();
    if(!root)throw std::logic_error("Terminal root expired");
    if(root->yogaNode){
      const auto yoga=std::dynamic_pointer_cast<terminal_dom::FullLayoutNode>(
        root->yogaNode);
      if(!yoga)throw std::logic_error("Full layout node required");
      const auto t0=now();
      yoga->setWidth(owner->terminalColumns_);
      yoga->calculateLayout(owner->terminalColumns_);
      const auto ms=now()-t0;
      profile->recordYogaMs(ms);
      const auto counters=indigenous_yoga::getYogaCounters();
      owner->lastYogaCounters_={ms,
        static_cast<double>(counters.visited),
        static_cast<double>(counters.measured),
        static_cast<double>(counters.stashHits),
        static_cast<double>(counters.live)};
    }
  };
}
} // namespace batbox::terminal_runtime
