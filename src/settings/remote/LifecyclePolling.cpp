#include "settings/remote/Lifecycle.hpp"

namespace batbox::settings::remote {
event_loop::Deferred<event_loop::Unit> Lifecycle::pollRemoteSettings(){
  event_loop::Deferred<event_loop::Unit> output(loop_);
  if(!services_.eligibility->isRemoteManagedSettingsEligible()){
    output.resolve({});return output;
  }
  const auto before=services_.stash->getRemoteManagedSettingsSyncFromStash();
  const std::optional<std::string> snapshot=before?
    std::optional(value::stringify(*before)):std::nullopt;
  try{
    services_.fetchAndLoad().then([this,output,snapshot](
        const std::optional<Json>*,const std::exception_ptr& error){
      if(error){output.resolve({});return;}
      try{
        const auto current=services_.stash->getRemoteManagedSettingsSyncFromStash();
        const std::optional<std::string> now=current?
          std::optional(value::stringify(*current)):std::nullopt;
        if(now!=snapshot){
          services_.debug("Remote settings: Changed during background poll");
          services_.notifyPolicySettings();
        }
      }catch(...){/* Polling failures leave the current settings available. */}
      output.resolve({});
    });
  }catch(...){output.resolve({});}
  return output;
}
void Lifecycle::startBackgroundPolling(){
  if(pollingInterval_)return;
  if(!services_.eligibility->isRemoteManagedSettingsEligible())return;
  pollingInterval_=services_.setInterval(services_.pollingIntervalMs,
    [this](){(void)pollRemoteSettings();},true);
  services_.registerCleanup([this](){
    stopBackgroundPolling();
    event_loop::Deferred<event_loop::Unit> done(loop_);done.resolve({});
    return done;
  });
}
void Lifecycle::stopBackgroundPolling(){
  if(pollingInterval_){(*pollingInterval_)();pollingInterval_.reset();}
}
}
