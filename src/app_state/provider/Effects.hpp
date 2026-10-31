#pragma once
#include "app_state/Host.hpp"
#include "app_state/provider/PermissionHelpers.hpp"
#include "settings/persistence/ChangeEvents.hpp"
#include <memory>

namespace batbox::app_state::provider {
struct MountServices {
  BypassPolicyServices policy;
  std::function<void(const std::string&)> debug;
};
void runMountPermissionCheck(Host& host,const MountServices& services);

struct SettingsListenerServices {
  std::function<Json()> getSettingsDeprecated;
  std::function<void(settings::Source,Host&)> applySettingsChange;
};
class SettingsSubscription {
public:
  SettingsSubscription(event_loop::Loop& loop,
    settings::persistence::ChangeEvents& detector,
    std::shared_ptr<Host> host,SettingsListenerServices services);
  SettingsSubscription(const SettingsSubscription&)=delete;
  SettingsSubscription& operator=(const SettingsSubscription&)=delete;
  ~SettingsSubscription();
  void close();
private:
  event_loop::Loop& loop_;
  std::function<void()> unsubscribe_;
};
} // namespace batbox::app_state::provider
