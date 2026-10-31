#pragma once
#include "app_state/Binding.hpp"
#include "event_loop/Loop.hpp"

namespace batbox::app_state {
class Host:public std::enable_shared_from_this<Host> {
public:
  Host(event_loop::Loop& loop,AppStateRef initial,
    AppStateChangeServices services);
  AppStateRef getState()const;
  Json getSnapshot()const;
  void setState(const AppStateStore::Updater& updater);
  std::function<void()> subscribe(AppStateStore::ListenerRef listener);
  std::function<Json()> snapshotGetter();
  std::function<void(const AppStateStore::Updater&)> typedSetter();
private:
  event_loop::Loop& loop_;
  AppStateStore store_;
  void requireOwner()const;
};
} // namespace batbox::app_state
