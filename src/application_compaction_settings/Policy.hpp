#pragma once

#include "application/Config.hpp"
#include "app_state/State.hpp"
#include "event_loop/Deferred.hpp"
#include <atomic>
#include <filesystem>
#include <memory>

namespace batbox::application_compaction_settings {

// One policy for /context, /config and all engine generations. Values change
// only after the indigenous configuration file has been written successfully.
class Policy final:public std::enable_shared_from_this<Policy> {
public:
  static std::shared_ptr<Policy> create(event_loop::Loop& owner,
    const application::Config& config,
    std::shared_ptr<app_state::AppStateStore> store);
  bool enabled()const{return enabled_.load(std::memory_order_acquire);}
  std::size_t compactAt()const{return compactAt_.load(std::memory_order_acquire);}
  application::Config withCurrentThreshold(application::Config config)const{
    config.compactAt=compactAt();return config;
  }
  event_loop::Deferred<event_loop::Unit> setEnabled(bool value);
  event_loop::Deferred<event_loop::Unit> setThreshold(std::size_t value);
  // /config has already persisted compact_at when this is called.
  void applyExternalThreshold(std::size_t value);
private:
  Policy(event_loop::Loop& owner,const application::Config& config,
    std::shared_ptr<app_state::AppStateStore> store);
  event_loop::Deferred<event_loop::Unit> change(const char* key,Json value);
  void projectEnabled(bool value);
  void projectThreshold(std::size_t value);
  event_loop::Loop& owner_;
  std::shared_ptr<app_state::AppStateStore> store_;
  std::filesystem::path path_,fallbackPath_;
  std::atomic_bool enabled_;
  std::atomic<std::size_t> compactAt_;
  bool busy_=false;
};

} // namespace batbox::application_compaction_settings
