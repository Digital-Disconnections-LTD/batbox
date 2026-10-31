#include "bats/BatMissionRuntime.hpp"

namespace batbox::bats {
void BatMissionRuntime::remember(const std::string& id,const RegisteredLocalBat& mission){
  std::lock_guard lock(mutex_);
  entries_[id]={mission.abortOwner,mission.parentAbortLink,mission.unregisterCleanup};
}
std::shared_ptr<std::stop_source> BatMissionRuntime::abortOwner(const std::string& id) const{
  std::lock_guard lock(mutex_);
  const auto it=entries_.find(id);
  return it==entries_.end()?nullptr:it->second.abortOwner;
}
void BatMissionRuntime::bind(BatMissionStateServices& services){
  services.abortMission=[this](const std::string& id){
    auto owner=abortOwner(id);
    if(owner)owner->request_stop();
  };
  services.unregisterCleanup=[this](const std::string& id){
    std::function<void()> unregister;
    {
      std::lock_guard lock(mutex_);
      const auto it=entries_.find(id);
      if(it==entries_.end())return;
      unregister=std::move(it->second.unregisterCleanup);
      entries_.erase(it);
    }
    if(unregister)unregister();
  };
}
} // namespace batbox::bats
