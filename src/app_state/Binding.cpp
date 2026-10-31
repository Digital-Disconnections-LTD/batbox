#include "app_state/Binding.hpp"

namespace batbox::app_state {
void bindFlightLifecycleNotifications(AppStateChangeServices& services,
  flights::Lifecycle& lifecycle){
  services.notifyFlightMetadataChanged=[&lifecycle](value::RecordRef metadata){
    lifecycle.notifyFlightMetadataChanged(std::move(metadata));
  };
  services.notifyPermissionModeChanged=[&lifecycle](const std::string& mode){
    lifecycle.notifyPermissionModeChanged(mode);
  };
}
AppStateStore makeAppStateStore(AppStateRef initial,
  AppStateChangeServices services){
  return createStore<AppStateRef>(std::move(initial),
    [services=std::move(services)](const AppStateStore::Change& change){
      onChangeAppState(change.newState,change.oldState,services);
    });
}
} // namespace batbox::app_state
