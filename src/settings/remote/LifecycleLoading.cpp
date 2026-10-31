#include "settings/remote/Lifecycle.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::settings::remote {
Lifecycle::Lifecycle(event_loop::Loop& loop,LifecycleServices services):
  loop_(loop),services_(std::move(services)){
  if(!services_.eligibility||!services_.stash||!services_.fetchAndLoad||
      !services_.unlinkSettings||!services_.debug||
      !services_.notifyPolicySettings||!services_.setTimeout||
      !services_.setInterval||
      !services_.registerCleanup)
    throw std::invalid_argument("Missing remote settings lifecycle binding");
}
void Lifecycle::resolveLoading(){
  if(!loadingResolvePending_||!loadingComplete_)return;
  loadingResolvePending_=false;loadingComplete_->resolve({});
}
void Lifecycle::initializeRemoteManagedSettingsLoadingPromise(){
  if(loadingComplete_)return;
  if(!services_.eligibility->isRemoteManagedSettingsEligible())return;
  loadingComplete_.emplace(loop_);
  loadingResolvePending_=true;
  services_.setTimeout(services_.loadingTimeoutMs,[this](){
    if(loadingResolvePending_){
      services_.debug("Remote settings: Loading promise timed out, "
        "resolving anyway");
      resolveLoading();
    }
  });
}
bool Lifecycle::isEligibleForRemoteManagedSettings(){
  return services_.eligibility->isRemoteManagedSettingsEligible();
}
event_loop::Deferred<event_loop::Unit>
Lifecycle::waitForRemoteManagedSettingsToLoad(){
  event_loop::Deferred<event_loop::Unit> output(loop_);
  if(!loadingComplete_){output.resolve({});return output;}
  loadingComplete_->then([output](const event_loop::Unit* value,
      const std::exception_ptr& error){
    if(error)output.reject(error);else output.resolve(*value);
  });
  return output;
}
event_loop::Deferred<event_loop::Unit>
Lifecycle::clearRemoteManagedSettingsStash(){
  event_loop::Deferred<event_loop::Unit> output(loop_);
  stopBackgroundPolling();
  services_.eligibility->resetSyncStash();
  loadingComplete_.reset();loadingResolvePending_=false;
  try{
    services_.unlinkSettings(services_.stash->getSettingsPath())
      .then([output](const event_loop::Unit*,const std::exception_ptr&){
        output.resolve({});
      });
  }catch(...){output.resolve({});}
  return output;
}
event_loop::Deferred<event_loop::Unit> Lifecycle::loadRemoteManagedSettings(){
  event_loop::Deferred<event_loop::Unit> output(loop_);
  try{
    if(services_.eligibility->isRemoteManagedSettingsEligible()&&
        !loadingComplete_){
      loadingComplete_.emplace(loop_);loadingResolvePending_=true;
    }
    if(services_.stash->getRemoteManagedSettingsSyncFromStash()&&
        loadingResolvePending_)resolveLoading();
    services_.fetchAndLoad().then([this,output](
        const std::optional<Json>* settings,const std::exception_ptr& error){
      try{
        if(error){resolveLoading();output.reject(error);return;}
        if(services_.eligibility->isRemoteManagedSettingsEligible())
          startBackgroundPolling();
        if(*settings)services_.notifyPolicySettings();
        resolveLoading();output.resolve({});
      }catch(...){resolveLoading();output.reject(std::current_exception());}
    });
  }catch(...){resolveLoading();output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<event_loop::Unit>
Lifecycle::refreshRemoteManagedSettings(){
  event_loop::Deferred<event_loop::Unit> output(loop_);
  try{
    clearRemoteManagedSettingsStash().then([this,output](
        const event_loop::Unit*,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        if(!services_.eligibility->isRemoteManagedSettingsEligible()){
          services_.notifyPolicySettings();output.resolve({});return;
        }
        services_.fetchAndLoad().then([this,output](
            const std::optional<Json>*,const std::exception_ptr& fetchError){
          if(fetchError){output.reject(fetchError);return;}
          try{
            services_.debug("Remote settings: Refreshed after auth change");
            services_.notifyPolicySettings();output.resolve({});
          }catch(...){output.reject(std::current_exception());}
        });
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
}
