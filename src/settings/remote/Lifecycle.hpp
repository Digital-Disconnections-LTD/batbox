#pragma once
#include "settings/remote/Eligibility.hpp"
#include "settings/remote/FetchAndLoad.hpp"

namespace batbox::settings::remote {
using CancelInterval=std::function<void()>;
struct LifecycleServices {
  Eligibility* eligibility=nullptr;
  SyncStash* stash=nullptr;
  std::function<event_loop::Deferred<std::optional<Json>>()> fetchAndLoad;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&)> unlinkSettings;
  std::function<void(const std::string&)> debug;
  std::function<void()> notifyPolicySettings;
  std::function<void(double,std::function<void()>)> setTimeout;
  std::function<CancelInterval(double,std::function<void()>,bool)> setInterval;
  std::function<void(std::function<event_loop::Deferred<event_loop::Unit>()>)>
    registerCleanup;
  double loadingTimeoutMs=30000;
  double pollingIntervalMs=60.0*60.0*1000.0;
};
class Lifecycle {
public:
  Lifecycle(event_loop::Loop& loop,LifecycleServices services);
  void initializeRemoteManagedSettingsLoadingPromise();
  bool isEligibleForRemoteManagedSettings();
  event_loop::Deferred<event_loop::Unit> waitForRemoteManagedSettingsToLoad();
  event_loop::Deferred<event_loop::Unit> clearRemoteManagedSettingsStash();
  event_loop::Deferred<event_loop::Unit> loadRemoteManagedSettings();
  event_loop::Deferred<event_loop::Unit> refreshRemoteManagedSettings();
  event_loop::Deferred<event_loop::Unit> pollRemoteSettings();
  void startBackgroundPolling();
  void stopBackgroundPolling();
private:
  event_loop::Loop& loop_;
  LifecycleServices services_;
  std::optional<CancelInterval> pollingInterval_;
  std::optional<event_loop::Deferred<event_loop::Unit>> loadingComplete_;
  bool loadingResolvePending_=false;
  void resolveLoading();
};
}
