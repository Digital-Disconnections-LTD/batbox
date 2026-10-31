#include "app_state/provider/Effects.hpp"
#include <stdexcept>

namespace batbox::app_state::provider {
void runMountPermissionCheck(Host& host,const MountServices& services){
  const auto state=host.getState();
  if(!state||!state->permissionContext)
    throw std::invalid_argument("AppState permission context required");
  const auto available=state->permissionContext->get(
    "isBypassPermissionsModeAvailable");
  if(!available||!value::truthy(*available))return;
  if(!isBypassPermissionsModeDisabled(services.policy))return;
  if(!services.debug)
    throw std::invalid_argument("Mount diagnostic import required");
  services.debug("Disabling bypass permissions mode on mount (remote settings loaded before mount)");
  host.setState([&services](const AppStateRef& previous){
    auto next=std::make_shared<AppState>(previous->shallowCopy());
    next->setPermissionContext(createDisabledBypassPermissionsContext(
      previous->permissionContext,services.policy));
    return next;
  });
}
SettingsSubscription::SettingsSubscription(event_loop::Loop& loop,
    settings::persistence::ChangeEvents& detector,
    std::shared_ptr<Host> host,SettingsListenerServices services):loop_(loop){
  if(!loop_.isOwnerThread())throw std::logic_error("AppState provider owner required");
  if(!host||!services.getSettingsDeprecated||!services.applySettingsChange)
    throw std::invalid_argument("Settings listener imports required");
  const auto dispatcher=loop_.dispatcher();
  unsubscribe_=detector.subscribe([dispatcher,host=std::move(host),
      services=std::move(services)](
      settings::Source source){
    if(!dispatcher.isOwnerThread())
      throw std::logic_error("AppState provider owner required");
    // Read the settings stash even though the AppState callback
    // ignores the second argument.
    (void)services.getSettingsDeprecated();
    services.applySettingsChange(source,*host);
  });
}
SettingsSubscription::~SettingsSubscription(){
  if(!unsubscribe_)return;
  auto cancel=std::move(unsubscribe_);
  if(loop_.isOwnerThread())cancel();
  else {
    try{loop_.postMission(cancel);}
    catch(...){cancel();}
  }
}
void SettingsSubscription::close(){
  if(!unsubscribe_)return;
  if(!loop_.isOwnerThread())
    throw std::logic_error("AppState provider owner required");
  auto cancel=std::move(unsubscribe_);
  cancel();
}
} // namespace batbox::app_state::provider
