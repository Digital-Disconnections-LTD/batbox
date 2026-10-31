#pragma once
#include "terminal_dom/Node.hpp"
#include "terminal_events/Dispatcher.hpp"
#include <functional>
#include <memory>
#include <optional>

namespace batbox::event_loop {class Loop;}
namespace batbox::terminal_reconciler {
using TimeoutRef=std::shared_ptr<void>;
struct HostStaticImports {
  terminal_events::Dispatcher* dispatcher=nullptr;
  std::function<TimeoutRef(std::function<void()>,double)> scheduleTimeout;
  std::function<void(const TimeoutRef&)> cancelTimeout;
};
class HostStatic {
public:
  explicit HostStatic(HostStaticImports imports):imports_(std::move(imports)){}
  static constexpr bool isPrimaryRenderer=true,supportsMutation=true,
    supportsPersistence=false,supportsHydration=false;
  static constexpr int noTimeout=-1;
  std::nullptr_t preparePortalMount()const{return nullptr;}
  bool clearContainer()const{return false;}
  bool shouldSetTextContent()const{return false;}
  void resetTextContent()const{}
  terminal_dom::NodeRef getPublicInstance(
    const terminal_dom::NodeRef& instance)const{return instance;}
  int getCurrentUpdatePriority()const;
  void beforeActiveInstanceBlur()const{}
  void afterActiveInstanceBlur()const{}
  void detachDeletedInstance()const{}
  std::nullptr_t getInstanceFromNode()const{return nullptr;}
  void prepareScopeUpdate()const{}
  std::nullptr_t getInstanceFromScope()const{return nullptr;}
  bool maySuspendCommit()const{return false;}
  bool preloadInstance()const{return true;}
  void startSuspendingCommit()const{}
  void suspendInstance()const{}
  std::nullptr_t waitForCommitToBeReady()const{return nullptr;}
  std::nullptr_t notPendingTransition()const{return nullptr;}
  void setCurrentUpdatePriority(int priority)const;
  int resolveUpdatePriority()const;
  void resetFormInstance()const{}
  void requestPostPaintCallback()const{}
  bool shouldAttemptEagerTransition()const{return false;}
  void trackSchedulerEvent()const{}
  std::optional<std::string> resolveEventType()const;
  double resolveEventTimeStamp()const;
  TimeoutRef scheduleTimeout(std::function<void()> callback,double delay)const;
  void cancelTimeout(const TimeoutRef& timer)const;
private:
  terminal_events::Dispatcher& dispatcher()const;
  HostStaticImports imports_;
};
HostStaticImports indigenousHostStaticImports(
  terminal_events::Dispatcher& dispatcher,event_loop::Loop& loop);
} // namespace batbox::terminal_reconciler
