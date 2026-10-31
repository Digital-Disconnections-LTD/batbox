#include "app_state/Host.hpp"
#include <stdexcept>

namespace batbox::app_state {
Host::Host(event_loop::Loop& loop,AppStateRef initial,
  AppStateChangeServices services):loop_(loop),
  store_(makeAppStateStore(std::move(initial),std::move(services))){
  requireOwner();
}
void Host::requireOwner()const{
  if(!loop_.isOwnerThread())
    throw std::logic_error("AppState host owner required");
}
AppStateRef Host::getState()const{
  requireOwner();return store_.getState();
}
Json Host::getSnapshot()const{
  return getState()->snapshot();
}
void Host::setState(const AppStateStore::Updater& updater){
  requireOwner();store_.setState(updater);
}
std::function<void()> Host::subscribe(AppStateStore::ListenerRef listener){
  requireOwner();return store_.subscribe(std::move(listener));
}
std::function<Json()> Host::snapshotGetter(){
  const std::weak_ptr<Host> weak=shared_from_this();
  return [weak]{
    const auto host=weak.lock();
    if(!host)throw std::logic_error("AppState host unavailable");
    return host->getSnapshot();
  };
}
std::function<void(const AppStateStore::Updater&)> Host::typedSetter(){
  const std::weak_ptr<Host> weak=shared_from_this();
  return [weak](const AppStateStore::Updater& updater){
    const auto host=weak.lock();
    if(!host)throw std::logic_error("AppState host unavailable");
    host->setState(updater);
  };
}
} // namespace batbox::app_state
